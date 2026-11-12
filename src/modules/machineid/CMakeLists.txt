# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
calamares_add_plugin(machineid
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        MachineIdJob.cpp
        Workers.cpp
    SHARED_LIB
)

# gucc lib
if(WITH_CACHYOS_INSTALLER)
    target_link_libraries(calamares_job_machineid PRIVATE cachyos::gucc)
    set(_gucc cachyos::gucc)
endif()

calamares_add_test(machineidtest SOURCES Tests.cpp MachineIdJob.cpp Workers.cpp LIBRARIES ${_gucc})
