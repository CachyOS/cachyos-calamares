#!/usr/bin/env python3

import os
import subprocess
import shutil
import time

import libcalamares
from libcalamares.utils import gettext_path, gettext_languages

import gettext

_translation = gettext.translation("calamares-python",
                                   localedir=gettext_path(),
                                   languages=gettext_languages(),
                                   fallback=True)
_ = _translation.gettext
_n = _translation.ngettext

custom_status_message = None
status_update_time = 0


class PacmanError(Exception):
    """Exception raised when the call to pacman returns a non-zero exit code

    Attributes:
        message -- explanation of the error
    """

    def __init__(self, message):
        self.message = message


def pretty_name():
    return _("Install base system")


def pretty_status_message():
    if custom_status_message is not None:
        return custom_status_message


def line_cb(line):
    """
    Writes every line to the debug log and displays it in calamares
    :param line: The line of output text from the command
    """
    global custom_status_message
    global status_update_time
    custom_status_message = line.strip()
    libcalamares.utils.debug("pacstrap: " + line.strip())
    if (time.time() - status_update_time) > 0.5:
        libcalamares.job.setprogress(0)
        status_update_time = time.time()


def run_in_host(command, line_func):
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True,
                            bufsize=1)
    for line in proc.stdout:
        if line.strip():
            line_func(line)
    proc.wait()
    if proc.returncode != 0:
        raise PacmanError("Failed to run pacman")


def run():
    """
    Installs the base system packages and copies files post-installation

    """
    root_mount_point = libcalamares.globalstorage.value("rootMountPoint")

    if not root_mount_point:
        return ("No mount point for root partition in globalstorage",
                "globalstorage does not contain a \"rootMountPoint\" key, "
                "doing nothing")

    if not os.path.exists(root_mount_point):
        return ("Bad mount point for root partition in globalstorage",
                "globalstorage[\"rootMountPoint\"] is \"{}\", which does not "
                "exist, doing nothing".format(root_mount_point))

    if libcalamares.job.configuration:
        if "basePackages" in libcalamares.job.configuration:
            base_packages = libcalamares.job.configuration["basePackages"]
        else:
            return "Package List Missing", "Cannot continue without list of packages to install"
    else:
        return "No configuration found", "Aborting due to missing configuration"


    bootloader = libcalamares.globalstorage.value("packagechooser_bootloader")

    if not bootloader:
        libcalamares.utils.warning("Failed to determine bootloader type")
        return "No bootloader selected", "Aborting due to missing configuration"

    libcalamares.utils.debug("Current bootloader: {!s}".format(bootloader))

    curr_filesystem = subprocess.run(["findmnt", "-ln", "-o", "FSTYPE", root_mount_point], stdout=subprocess.PIPE).stdout.decode('utf-8')
    libcalamares.utils.debug("Current filesystem: {!s}".format(curr_filesystem))
    is_root_on_zfs = (curr_filesystem == "zfs\n")
    is_root_on_btrfs = (curr_filesystem == "btrfs\n")
    is_root_on_bcachefs = (curr_filesystem == "bcachefs\n")

    if bootloader == "grub":
        base_packages += ["grub", "grub-hook", "cachyos-grub-theme", "os-prober"]
    elif bootloader == "limine":
        base_packages += ["limine", "limine-entry-tool"]
    elif bootloader == "refind" or bootloader == "refind-ai":
        base_packages += ["refind"]
    elif bootloader == "systemd-boot":
        base_packages += ["systemd-boot-manager"]

    # Detect CPU vendor and add the correct microcode package
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("vendor_id"):
                    if "GenuineIntel" in line:
                        base_packages.append("intel-ucode")
                    else:
                        base_packages.append("amd-ucode")
                    break
    except Exception as e:
        libcalamares.utils.warning("Failed to detect CPU vendor for microcode: {!s}".format(e))

    if (is_root_on_zfs):
        base_packages += ["zfs-utils", "linux-cachyos-zfs", "linux-cachyos-lts-zfs"]
    elif is_root_on_btrfs:
        libcalamares.utils.debug("Root on BTRFS")
        if bootloader == "limine":
            base_packages += ["snapper", "btrfs-assistant", "limine-snapper-sync" ]
        elif bootloader == "grub":
            base_packages += ["snapper", "btrfs-assistant", "grub-btrfs-support"]
        elif bootloader == "refind" or bootloader == "refind-ai":
            base_packages += ["snapper", "btrfs-assistant", "refind-btrfs" ]

    elif is_root_on_bcachefs:
        libcalamares.utils.debug("Root on BCACHEFS")
        base_packages += ["bcachefs-tools", "bcachefs-dkms"]


    # Clean up stale ucode files from reused ESP to avoid pacman "exists in filesystem" conflicts
    boot_path = os.path.join(root_mount_point, "boot")
    for ucode_img in ["intel-ucode.img", "amd-ucode.img"]:
        ucode_path = os.path.join(boot_path, ucode_img)
        if os.path.exists(ucode_path):
            try:
                os.remove(ucode_path)
                libcalamares.utils.debug("Removed stale ucode file: {!s}".format(ucode_path))
            except Exception as e:
                libcalamares.utils.warning("Failed to remove stale ucode file {!s}: {!s}".format(ucode_path, e))

    # run the pacstrap
    pacstrap_command = ["/etc/calamares/scripts/pacstrap_calamares", "-c", root_mount_point] + base_packages

    try:
        run_in_host(pacstrap_command, line_cb)
    except subprocess.CalledProcessError as cpe:
        return "Failed to run pacstrap", "Pacstrap failed with error {!s}".format(cpe.stderr)
    except PacmanError as pe:
        return "Failed to run pacstrap", format(pe)

    # copy files post install
    if "postInstallFiles" in libcalamares.job.configuration:
        files_to_copy = libcalamares.job.configuration["postInstallFiles"]

        if bootloader == "grub":
            files_to_copy.append("/etc/default/grub")

        for source_file in files_to_copy:
            if os.path.exists(source_file):
                try:
                    libcalamares.utils.debug("Copying file {!s}".format(source_file))
                    dest = os.path.normpath(root_mount_point + source_file)
                    os.makedirs(os.path.dirname(dest), exist_ok=True)
                    shutil.copy2(source_file, dest)
                except Exception as e:
                    libcalamares.utils.warning("Failed to copy file {!s}, error {!s}".format(source_file, e))

    libcalamares.globalstorage.insert("online", True)

    libcalamares.job.setprogress(1.0)

    return None
