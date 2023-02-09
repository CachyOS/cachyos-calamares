# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

calamares_add_plugin(bootloaderchooser
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Config.cpp
        BootloaderChooserPage.cpp
        BootloaderChooserViewStep.cpp
        BootloaderModel.cpp
    RESOURCES
        bootloaderchooser.qrc
    UI
        page_bootloader.ui
    SHARED_LIB
)
