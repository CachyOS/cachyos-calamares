# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
if(NOT WITH_CACHYOS_INSTALLER)
    calamares_skip_module( "gucc" )
    return()
endif()

calamares_add_plugin(gucc
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        GuccJob.cpp
    SHARED_LIB
)
target_link_libraries(calamares_job_gucc PRIVATE cachyos::gucc)

calamares_add_test(guccjobtest SOURCES Tests.cpp GuccJob.cpp LIBRARIES cachyos::gucc)
