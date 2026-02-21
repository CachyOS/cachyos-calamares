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
    Gets the job-configuration for btrfs subvolumes, or if there is
    none given, returns a default configuration that matches
    the setup (/ and /home) from before configurability was introduced.

    @param partitions
        The partitions (from the partitioning module) that will exist on disk.
        This is used to filter out subvolumes that don't need to be created
        because they get a dedicated partition instead.
    """
    btrfs_subvolumes = libcalamares.job.configuration.get("btrfsSubvolumes", None)
    # Warn if there's no configuration at all, and empty configurations are
    # replaced by a simple root-only layout.
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

def err(error_message, active_mounts):
    for tmp_dir in sorted(active_mounts, reverse=True):
        if os.path.ismount(tmp_dir):
            if subprocess.call(["umount", "-v", tmp_dir]) != 0:
                subprocess.call(["umount", "-v", "-l", tmp_dir])
    raise Exception(error_message)

def mount_partition(root_mount_point, partition, partitions, mount_options, mount_options_list, efi_location, active_mounts):
    """
    Do a single mount of @p partition inside @p root_mount_point.

    :param root_mount_point: A string containing the root of the install
    :param partition: A dict containing information about the partition
    :param partitions: The full list of partitions used to filter out btrfs subvols which have duplicate mountpoints
    :param mount_options: The mount options from the config file
    :param mount_options_list: A list of options for each mountpoint to be placed in global storage for future modules
    :param efi_location: A string holding the location of the EFI partition or None
    :param active_mounts A list of strings
    :return:
    """
    # Create mount point with `+` rather than `os.path.join()` because
    # `partition["mountPoint"]` starts with a '/'.
    raw_mount_point = partition["mountPoint"]
    if not raw_mount_point:
        return

    mount_point = root_mount_point + raw_mount_point

    # Ensure that the created directory has the correct SELinux context on
    # SELinux-enabled systems.

    os.makedirs(mount_point, exist_ok=True)

    try:
        subprocess.call(['chcon', '--reference=' + raw_mount_point, mount_point])
    except FileNotFoundError as e:
        libcalamares.utils.warning(str(e))
    except OSError:
        libcalamares.utils.error("Cannot run 'chcon' normally.")
        raise

    fstype = partition.get("fs", "").lower()
    if fstype == "unformatted":
        return


    am = active_mounts
    am.append(mount_point)
    device = partition["device"]

    # Only allow fat32 on boot path and block incompatible
    if fstype in ["fat16", "fat32", "ntfs", "ext2", "exfat"]:
        is_boot = raw_mount_point in ["/boot", "/boot/efi"]
        if not (is_boot and fstype == "fat32"):
            err(f"Unsupported {fstype} partition on {raw_mount_point}", am)
        fstype = "vfat"

    if "luksMapperName" in partition:
        device = os.path.join("/dev/mapper", partition["luksMapperName"])

    if fstype == "zfs":
        mount_zfs(root_mount_point, partition)
        return


    mount_options_string = get_mount_options(fstype, mount_options, partition, efi_location)

    # Standard mount for everything EXCEPT Btrfs root (this catches other btrfs partitions)
    if not (fstype == "btrfs" and raw_mount_point == '/'):
        if libcalamares.utils.mount(device, mount_point, fstype, mount_options_string) != 0:
            err(f"Cannot mount {device}", am)

        # Verify that the install target is empty
        is_virtual = any(raw_mount_point.startswith(v) for v in ["/sys", "/proc", "/dev", "/run"])
        if not is_virtual and raw_mount_point not in ["/home", "/srv", "/boot", "/boot/efi"]:
            ignored_metadata = [
                "lost+found", ".Trash-1000", "$RECYCLE.BIN", 
                "System Volume Information", ".fseventsd", 
                ".Spotlight-V100"
                ]
            contents = [f for f in os.listdir(mount_point) if f not in ignored_metadata]
            if contents:
                err((
                    f"Device {device} at {raw_mount_point} not empty. "
                    "Only /home or /srv allowed."), am)

        mount_options_list.append({"mountpoint": raw_mount_point, "option_string": mount_options_string})
        return


    # Btrfs Setup
    btrfs_subvolumes = get_btrfs_subvolumes(partitions)

    # Ensure every entry has a subvolume name
    for s in btrfs_subvolumes:
        if not s.get("mountPoint") or not s.get("subvolume"):
            err(f"Btrfs config error: entry missing mountPoint or subvolume name", am)

    # Ensure root subvolume with mountpoint / exists 
    root_sub = next((s for s in btrfs_subvolumes if s["mountPoint"] == "/"), None)
    if not root_sub:
        err("Btrfs config error: root subvolume not found", am)

    # Mount raw partition to create subvolumes
    with tempfile.TemporaryDirectory(prefix="calam-btrfs-") as setup_dir:
        am.append(setup_dir)
        if libcalamares.utils.mount(device, setup_dir, fstype, "defaults") != 0:
            err(f"Cannot mount btrfs for subvolume creation {device}", am)
        try:
            for s in btrfs_subvolumes:
                sub_path = setup_dir + s["subvolume"]
                if os.path.exists(sub_path):
                    err((
                        f"Subvolume {s['subvolume']} already exists on {device}. "
                        "Only /home or /srv allowed."), am)
            for s in btrfs_subvolumes:
                sub_path = setup_dir + s["subvolume"]
                os.makedirs(os.path.dirname(sub_path), exist_ok=True)
                subprocess.check_call(["btrfs", "subvolume", "create", sub_path])
                # Set secure permissions for /root subvolume (750 instead of default 755)
                if s["mountPoint"] == "/root":
                    os.chmod(sub_path, 0o750)
        finally:
            if os.path.ismount(setup_dir):
                subprocess.call(["umount", "-v", setup_dir])
            if setup_dir in am:
                am.remove(setup_dir)

    # Mount the specific @ root subvolume
    root_opts = f"subvol={root_sub['subvolume']},{mount_options_string}"

    if libcalamares.utils.mount(device, root_mount_point, fstype, root_opts) != 0:
        err(f"Failed to mount root subvolume {device}", am)

    # Mount remaining subvolumes
    for s in btrfs_subvolumes:
        if s["mountPoint"] == "/":
            continue

        # Prepare subvolume mount options and target mount point
        sub_opts = f"subvol={s['subvolume']},{mount_options_string}" 
        sub_path = root_mount_point + s["mountPoint"]
        os.makedirs(sub_path, exist_ok=True)

        if libcalamares.utils.mount(device, sub_path, fstype, sub_opts) == 0:
            mount_options_list.append({"mountpoint": s["mountPoint"], "option_string": mount_options_string})
        else:
            err(f"Failed to mount subvolume {s['subvolume']}", am)
    
    # Fstab module uses btrfsRootSubvolume to inject subvol=/@ into generic / entry
    mount_options_list.append({"mountpoint": raw_mount_point, "option_string": mount_options_string})
    libcalamares.globalstorage.insert("btrfsRootSubvolume", root_sub['subvolume'])
    libcalamares.globalstorage.insert("btrfsSubvolumes", btrfs_subvolumes)


def enable_swap_partition(devices):
    try:
        for d in devices:
            libcalamares.utils.host_env_process_output(["swapon", d])
    except subprocess.CalledProcessError:
        libcalamares.utils.warning(f"Failed to enable swap for devices: {devices}")


def run():
    """
    Mount all the partitions from GlobalStorage and from the job configuration.
    Partitions are mounted in-lexical-order of their mountPoint.
    """

    partitions = libcalamares.globalstorage.value("partitions")

    if not partitions:
        libcalamares.utils.warning("partitions is empty, {!s}".format(partitions))
        return (_("Configuration Error"),
                _("No partitions are defined for <pre>{!s}</pre> to use.").format("mount"))

    # Find existing swap partitions that are part of the installation and enable them now
    claimed_swap_partitions = [p for p in partitions if p["fs"] == "linuxswap" and p.get("claimed", False)]
    plain_swap = [p for p in claimed_swap_partitions if p["fsName"] == "linuxswap"]
    luks_swap = [p for p in claimed_swap_partitions if p["fsName"] == "luks" or p["fsName"] == "luks2"]
    swap_devices = [p["device"] for p in plain_swap] + ["/dev/mapper/" + p["luksMapperName"] for p in luks_swap]

    enable_swap_partition(swap_devices)

    root_mount_point = tempfile.mkdtemp(prefix="calamares-root-")

    # Get the mountOptions, if this is None, that is OK and will be handled later
    mount_options = libcalamares.job.configuration.get("mountOptions")

    # Guard against missing keys (generally a sign that the config file is bad)
    extra_mounts = libcalamares.job.configuration.get("extraMounts") or []
    if not extra_mounts:
        libcalamares.utils.warning("No extra mounts defined. Does mount.conf exist?")

    efi_location = None
    if libcalamares.globalstorage.value("firmwareType") == "efi":
        efi_location = libcalamares.globalstorage.value("efiSystemPartition")
    else:
        extra_mounts = [m for m in extra_mounts if not m.get("efi")]

    # Add extra mounts to the partitions list and sort by mount points.
    # This way, we ensure / is mounted before the rest, and every mount point
    # is created on the right partition (e.g. if a partition is to be mounted
    # under /tmp, we make sure /tmp is mounted before the partition)

    # mount_options_list will be inserted into global storage for use in fstab later
    mount_options_list = []
    active_mounts = []

    # Lexical Sort: mount / before sub-paths  
    physical = [p for p in partitions if "mountPoint" in p and p["mountPoint"]]
    physical.sort(key=lambda x: x["mountPoint"])

    try:
        for p in physical:
            mount_partition(root_mount_point, p, partitions, mount_options, mount_options_list, efi_location, active_mounts)
         
        # Bind/Virtual: After creating Btrfs subvolumes
        extra = [p for p in extra_mounts if "mountPoint" in p and p["mountPoint"]]
        extra.sort(key=lambda x: x["mountPoint"])

        for p in extra:
            mount_partition(root_mount_point, p, partitions, mount_options, mount_options_list, efi_location, active_mounts)

    except Exception as e:
        err(str(e), active_mounts)

    libcalamares.globalstorage.insert("rootMountPoint", root_mount_point)
    libcalamares.globalstorage.insert("mountOptionsList", mount_options_list)

    # Remember the extra mounts for the unpackfs module
    libcalamares.globalstorage.insert("extraMounts", extra_mounts)
