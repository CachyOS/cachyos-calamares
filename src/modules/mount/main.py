#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
#   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
#   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
#   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
#   SPDX-FileCopyrightText: 2019-2020 Collabora Ltd
#   SPDX-License-Identifier: GPL-3.0-or-later
#
#   Calamares is Free Software: see the License-Identifier above.
#

import tempfile
import subprocess
import os
import re

import libcalamares

import gettext

_ = gettext.translation("calamares-python",
                        localedir=libcalamares.utils.gettext_path(),
                        languages=libcalamares.utils.gettext_languages(),
                        fallback=True).gettext


class ZfsException(Exception):
    """Exception raised when there is a problem with zfs

    Attributes:
        message -- explanation of the error
    """

    def __init__(self, message):
        self.message = message


def pretty_name():
    return _("Mounting partitions.")


def disk_name_for_partition(partition):
    """ Returns disk name for each found partition.

    :param partition:
    :return:
    """
    name = os.path.basename(partition["device"])

    if name.startswith("mmcblk") or name.startswith("nvme"):
        return re.sub("p[0-9]+$", "", name)

    return re.sub("[0-9]+$", "", name)


def is_ssd_disk(partition):
    """ Checks if given partition is on an ssd disk.

    :param partition: A dict containing the partition information
    :return: True is the partition in on an ssd, False otherwise
    """

    try:
        disk_name = disk_name_for_partition(partition)
        filename = os.path.join("/sys/block", disk_name, "queue/rotational")

        with open(filename) as sysfile:
            return sysfile.read() == "0\n"
    except:
        return False


def get_mount_options(filesystem, mount_options, partition, efi_location = None):
    """
    Returns the mount options for the partition object and filesystem

    :param filesystem: A string containing the filesystem
    :param mount_options: A list of dicts that descripes the mount options for each mountpoint
    :param partition: A dict containing information about the partition
    :param efi_location: A string holding the location of the EFI partition or None
    :return: A comma seperated string containing the mount options suitable for passing to mount
    """

    # Extra mounts can optionally have "options" set, in this case, they override other all other settings
    if "options" in partition:
        return ",".join(partition["options"])

    # If there are no mount options defined then we use the defaults
    if mount_options is None:
        return "defaults"

    # The EFI partition uses special mounting options
    if efi_location and partition["mountPoint"] == efi_location:
        effective_filesystem = "efi"
    else:
        effective_filesystem = filesystem

    options = next((x for x in mount_options if x["filesystem"] == effective_filesystem), None)

    # If there is no match then check for default options
    if options is None:
        options = next((x for x in mount_options if x["filesystem"] == "default"), None)

    # If it is still None, then fallback to returning defaults
    if options is None:
        return "defaults"

    option_items = options.get("options", []).copy()

    # Append the appropriate options for ssd or hdd if set
    if is_ssd_disk(partition):
        name = os.path.basename(partition["device"])
        if name.startswith("nvme"):
            option_items.extend(options.get("nvmeOptions", []))
        else:
            option_items.extend(options.get("ssdOptions", []))
    else:
        option_items.extend(options.get("hddOptions", []))

    if option_items:
        return ",".join(option_items)
    else:
        return "defaults"

def get_btrfs_subvolumes(partitions):
    """
    Gets the job-configuration for btrfs subvolumes.
    """
    btrfs_subvolumes = libcalamares.job.configuration.get("btrfsSubvolumes", None)
    if btrfs_subvolumes is None:
        libcalamares.utils.warning("No configuration for btrfsSubvolumes")
    if not btrfs_subvolumes:
        btrfs_subvolumes = [dict(mountPoint="/", subvolume="/@"), dict(mountPoint="/home", subvolume="/@home")]

    # Identify dedicated partitions (excluding root)
    non_root_partition_mounts = [m for m in [p.get("mountPoint", None) for p in partitions] if
                                 m is not None and m != '/']

    # Filter: Skip subvolume if it IS a partition OR is INSIDE a partition
    btrfs_subvolumes = [
        s for s in btrfs_subvolumes 
        if s["mountPoint"] == "/" or not any(
            m and (s["mountPoint"] == m or s["mountPoint"].startswith(m + "/"))
            for m in non_root_partition_mounts
        )
    ]

    # If we have a swap **file**, give it a separate subvolume.
    swap_choice = libcalamares.globalstorage.value("partitionChoices")
    if swap_choice and swap_choice.get("swap", None) == "file":
        swap_subvol = libcalamares.job.configuration.get("btrfsSwapSubvol", "/@swap")
        btrfs_subvolumes.append({'mountPoint': '/swap', 'subvolume': swap_subvol})
        libcalamares.globalstorage.insert("btrfsSwapSubvol", swap_subvol)

    return btrfs_subvolumes
    
def mount_zfs(root_mount_point, partition):
    """ Mounts a zfs partition at @p root_mount_point

    :param root_mount_point: The absolute path to the root of the install
    :param partition: The partition map from global storage for this partition
    :return:
    """
    # Get the list of zpools from global storage
    zfs_pool_list = libcalamares.globalstorage.value("zfsPoolInfo")
    if not zfs_pool_list:
        libcalamares.utils.warning("Failed to locate zfsPoolInfo data in global storage")
        raise ZfsException(_("Internal error mounting zfs datasets"))

    # Find the zpool matching this partition
    for zfs_pool in zfs_pool_list:
        if zfs_pool["mountpoint"] == partition["mountPoint"]:
            pool_name = zfs_pool["poolName"]
            ds_name = zfs_pool["dsName"]

    # import the zpool
    try:
        libcalamares.utils.host_env_process_output(["zpool", "import", "-N", "-R", root_mount_point, pool_name], None)
    except subprocess.CalledProcessError:
        raise ZfsException(_("Failed to import zpool"))

    # Get the encrpytion information from global storage
    zfs_info_list = libcalamares.globalstorage.value("zfsInfo")
    encrypt = False
    if zfs_info_list:
        for zfs_info in zfs_info_list:
            if zfs_info["mountpoint"] == partition["mountPoint"] and zfs_info["encrypted"] is True:
                encrypt = True
                passphrase = zfs_info["passphrase"]

    if encrypt is True:
        # The zpool is encrypted, we need to unlock it
        try:
            libcalamares.utils.host_env_process_output(["zfs", "load-key", pool_name], None, passphrase)
        except subprocess.CalledProcessError:
            raise ZfsException(_("Failed to unlock zpool"))

    if partition["mountPoint"] == '/':
        # Get the zfs dataset list from global storage
        zfs = libcalamares.globalstorage.value("zfsDatasets")

        if not zfs:
            libcalamares.utils.warning("Failed to locate zfs dataset list")
            raise ZfsException(_("Internal error mounting zfs datasets"))

        zfs.sort(key=lambda x: x["mountpoint"])
        for dataset in zfs:
            try:
                if dataset["canMount"] == "noauto" or dataset["canMount"] is True:
                    libcalamares.utils.host_env_process_output(["zfs", "mount",
                                                                dataset["zpool"] + '/' + dataset["dsName"]])
            except subprocess.CalledProcessError:
                raise ZfsException(_("Failed to set zfs mountpoint"))
    else:
        try:
            libcalamares.utils.host_env_process_output(["zfs", "mount", pool_name + '/' + ds_name])
        except subprocess.CalledProcessError:
            raise ZfsException(_("Failed to set zfs mountpoint"))


def mount_partition(root_mount_point, partition, partitions, mount_options, mount_options_list, efi_location):
    raw_mount_point = partition["mountPoint"]
    if not raw_mount_point:
        return

    mount_point = root_mount_point + raw_mount_point
    os.makedirs(mount_point, exist_ok=True)
    fstype = partition.get("fs", "").lower()

    is_virtual = any(raw_mount_point.startswith(v) for v in ["/sys", "/proc", "/dev", "/run"])
    if not is_virtual and fstype != "unformatted":
        try:
            os.chmod(mount_point, 0o755)
        except OSError as e:
            libcalamares.utils.warning(f"Could not chmod {mount_point}: {e}")

    try:
        subprocess.call(['chcon', '--reference=' + raw_mount_point, mount_point])
    except:
        pass

    if fstype == "unformatted":
        return
    if fstype in ["fat16", "fat32"]:
        fstype = "vfat"

    device = partition["device"]
    if "luksMapperName" in partition:
        device = os.path.join("/dev/mapper", partition["luksMapperName"])

    if fstype == "zfs":
        mount_zfs(root_mount_point, partition)
        return

    mount_options_string = get_mount_options(fstype, mount_options, partition, efi_location)
    mount_options_list.append({"mountpoint": raw_mount_point, "option_string": mount_options_string})
    # Standard mount for everything EXCEPT Btrfs root
    if not (fstype == "btrfs" and raw_mount_point == '/'):
        if libcalamares.utils.mount(device, mount_point, fstype, mount_options_string) != 0:
            libcalamares.utils.warning(f"Cannot mount {device}")

        return

    # Btrfs Root "Magic Trick" Logic
    btrfs_subvolumes = get_btrfs_subvolumes(partitions)
    libcalamares.globalstorage.insert("btrfsSubvolumes", btrfs_subvolumes)

    # Step 1: Create subvolumes via a private "backdoor" mount
    with tempfile.TemporaryDirectory(prefix="calam-btrfs-") as setup_dir:
        libcalamares.utils.mount(device, setup_dir, fstype, "defaults")
        try:  # <--- You need this line!
            for s in btrfs_subvolumes:
                if s["subvolume"]:
                    os.makedirs(setup_dir + os.path.dirname(s["subvolume"]), exist_ok=True)
                    subprocess.check_call(["btrfs", "subvolume", "create", setup_dir + s["subvolume"]])
        finally:
            # UNMOUNT 1: Close the "backdoor" so the temp directory can be cleaned up
            # We must clear this so we can remount using the '@' subvolume specifically.
            subprocess.check_call(["umount", "-v", setup_dir])
    
    # Step 2: Prepare for the real mount
    try:
        # UNMOUNT 2: Remove the "flat" partition mount Calamares created earlier.
        subprocess.call(["umount", "-l", root_mount_point])
    except Exception:
        pass
        
    root_sub = next((s for s in btrfs_subvolumes if s["mountPoint"] == "/"), None)
    if not root_sub:
        raise Exception("No root (/) subvolume defined!")

    root_opts = f"subvol={root_sub['subvolume']},{mount_options_string}"
    if libcalamares.utils.mount(device, root_mount_point, fstype, root_opts) != 0:
        raise Exception(f"Failed to mount root subvolume {root_sub['subvolume']}")
        
    # Step 3: Mount remaining subvolumes (like /home)
    for s in btrfs_subvolumes:
        if s["mountPoint"] == "/":
            libcalamares.globalstorage.insert("btrfsRootSubvolume", s["subvolume"])
            continue
            
        # This builds the path INSIDE your new root
        sub_path = root_mount_point + s["mountPoint"]
        os.makedirs(sub_path, exist_ok=True)
        # This tells Linux: "Put this specific subvolume here"
        sub_opts = f"subvol={s['subvolume']},{mount_options_string}"
        
        if libcalamares.utils.mount(device, sub_path, fstype, sub_opts) == 0:
            mount_options_list.append({"mountpoint": s["mountPoint"], "option_string": mount_options_string})
        else:
            libcalamares.utils.warning(f"Failed to mount subvolume {s['subvolume']}")
            
def enable_swap_partition(devices):
    try:
        for d in devices:
            libcalamares.utils.host_env_process_output(["swapon", d])
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to enable swap for devices: {devices}")


def run():
    partitions = libcalamares.globalstorage.value("partitions")
    if not partitions:
        libcalamares.utils.warning("partitions is empty")
        return (_("Configuration Error"), _("No partitions defined."))

    # 1. Restore Swap Activation (Physical swap first)
    claimed_swap = [p for p in partitions if p["fs"] == "linuxswap" and p.get("claimed", False)]
    swap_devices = [p["device"] if p["fsName"] == "linuxswap" else 
                    "/dev/mapper/" + p["luksMapperName"] for p in claimed_swap]
    enable_swap_partition(swap_devices)

    # 2. Setup Environment
    root_mount_point = tempfile.mkdtemp(prefix="calamares-root-")
    mount_options = libcalamares.job.configuration.get("mountOptions")
    extra_mounts = libcalamares.job.configuration.get("extraMounts") or []
    mount_options_list = []

    # 3. EFI Logic (Filtered properly)
    efi_location = None
    if libcalamares.globalstorage.value("firmwareType") == "efi":
        efi_location = libcalamares.globalstorage.value("efiSystemPartition")
    else:
        extra_mounts = [m for m in extra_mounts if not m.get("efi")]

    # 4. Phase One: Physical (Depth Sort: / before /var)  
    physical = [p for p in partitions if "mountPoint" in p and p["mountPoint"]]
    physical.sort(key=lambda x: x["mountPoint"])

    try:
        for p in physical:
            mount_partition(root_mount_point, p, partitions, mount_options, mount_options_list, efi_location)
         
        # 5. Phase Two: Bind/Virtual (After Btrfs subvolumes exist)
        extra = [p for p in extra_mounts if "mountPoint" in p and p["mountPoint"]]
        extra.sort(key=lambda x: x["mountPoint"])

        for p in extra:
            mount_partition(root_mount_point, p, partitions, mount_options, mount_options_list, efi_location)

    except ZfsException as ze:
        return _("zfs mounting error"), ze.message

    # 6. Global Storage Persistence
    libcalamares.globalstorage.insert("rootMountPoint", root_mount_point)
    libcalamares.globalstorage.insert("mountOptionsList", mount_options_list)
    libcalamares.globalstorage.insert("extraMounts", extra_mounts)
