#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2016 Artoo <artoo@manjaro.org>
#   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
#   SPDX-FileCopyrightText: 2018 Gabriel Craciunescu <crazy@frugalware.org>
#   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: GPL-3.0-or-later
#
#   Calamares is Free Software: see the License-Identifier above.
#

import subprocess
import os
import fileinput
import libcalamares

from libcalamares.utils import debug, warning, target_env_call

import gettext
_ = gettext.translation("calamares-python",
                        localedir=libcalamares.utils.gettext_path(),
                        languages=libcalamares.utils.gettext_languages(),
                        fallback=True).gettext


def pretty_name():
    return _("Configure Plymouth theme")


def detect_plymouth():
    """
    Checks existence (runnability) of plymouth in the target system.

    @return True if plymouth exists in the target, False otherwise
    """
    # Used to only check existence of path /usr/bin/plymouth in target
    return target_env_call(["sh", "-c", "which plymouth"]) == 0


def detect_amdgpu():
    """
    Checks if an AMD GPU is present using chwd.

    @return True if an AMD GPU is detected, False otherwise
    """
    try:
        result = subprocess.run(
            ["chwd", "--check", "amd"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            debug("Detected AMD GPU via chwd")
            return True
    except OSError:
        debug("Could not run chwd for AMD GPU detection")
    return False


class PlymouthController:

    def __init__(self):
        self.__root = libcalamares.globalstorage.value('rootMountPoint')

    @property
    def root(self):
        return self.__root

    def setTheme(self):
        config = libcalamares.job.configuration
        plymouth_theme = config["plymouth_theme"]

        if config.get("plymouth_theme_amdgpu") and detect_amdgpu():
            plymouth_theme = config["plymouth_theme_amdgpu"]
            debug("Using AMD GPU plymouth theme: {}".format(plymouth_theme))

        target_env_call(["plymouth-set-default-theme", plymouth_theme])

    def setUseSimpledrm(self):
        conf_path = os.path.join(self.__root, "etc", "plymouth", "plymouthd.conf")

        try:
            with fileinput.input(conf_path, inplace=True) as config:
                for line in config:
                    if line.startswith("#[Daemon]") or line.startswith("[Daemon]"):
                        line = line.lstrip("#")
                        line = line + "UseSimpledrm=1\n"

                    print(line, end='')
        except Exception as e:
            warning("Failed to set UseSimpledrm=1: {!s}".format(e))

    def run(self):
        if detect_plymouth():
            if (("plymouth_theme" in libcalamares.job.configuration) and
               (libcalamares.job.configuration["plymouth_theme"] is not None)):
                self.setTheme()

            if libcalamares.job.configuration.get("plymouth_simpledrm", False):
                self.setUseSimpledrm()
        return None


def run():
    pc = PlymouthController()
    return pc.run()
