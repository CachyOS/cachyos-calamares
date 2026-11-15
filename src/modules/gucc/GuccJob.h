/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef GUCCJOB_H
#define GUCCJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "locale/TranslatableConfiguration.h"
#include "utils/PluginFactory.h"

#include <memory>
#include <optional>

/** @brief One install action per config
 *
 * Meant to be used as temporarly replacement, in place of the
 * shellprocess or Python.
 */
class PLUGINDLLEXPORT GuccJob : public Calamares::CppJob
{
    Q_OBJECT

public:
    enum class Action
    {
        InstallMkHook,
        RemoveMkHook,
        BtrfsSnapshot,
        EnableUfw,
        Hwclock,
        ZfsHostid,
    };

    explicit GuccJob( QObject* parent = nullptr );
    ~GuccJob() override;

    QString prettyName() const override;

    Calamares::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

    std::optional< Action > action() const { return m_action; }

private:
    std::optional< Action > m_action;
    std::unique_ptr< Calamares::Locale::TranslatedString > m_name;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( GuccJobFactory )

#endif  // GUCCJOB_H
