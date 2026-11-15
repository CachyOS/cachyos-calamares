/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "GuccJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/NamedEnum.h"
#include "utils/Variant.h"

#include <gucc/btrfs.hpp>
#include <gucc/error.hpp>
#include <gucc/firewall.hpp>
#include <gucc/hwclock.hpp>
#include <gucc/initcpio.hpp>
#include <gucc/pacman.hpp>
#include <gucc/zfs.hpp>

#include <string>

namespace
{
using Action = GuccJob::Action;

const NamedEnumTable< Action >&
actionNames()
{
    // clang-format off
    static const NamedEnumTable< Action > names {
        { QStringLiteral( "install-mk-hook" ), Action::InstallMkHook },
        { QStringLiteral( "remove-mk-hook" ), Action::RemoveMkHook },
        { QStringLiteral( "btrfs-snapshot" ), Action::BtrfsSnapshot },
        { QStringLiteral( "enable-ufw" ), Action::EnableUfw },
        { QStringLiteral( "hwclock" ), Action::Hwclock },
        { QStringLiteral( "zfs-hostid" ), Action::ZfsHostid },
    };
    // clang-format on
    return names;
}

gucc::Result< void >
runAction( Action action, const std::string& root, const Calamares::GlobalStorage& gs )
{
    switch ( action )
    {
    case Action::InstallMkHook:
        return gucc::initcpio::install_deferral_hook( root );
    case Action::RemoveMkHook:
        return gucc::initcpio::remove_deferral_hook( root );
    case Action::BtrfsSnapshot:
        return gucc::fs::create_btrfs_installation_snapshot( root );
    case Action::EnableUfw:
        if ( !gucc::pacman::is_installed( "ufw", root ) )
        {
            return { };
        }
        return gucc::firewall::enable_ufw( root, gucc::pacman::is_installed( "kdeconnect", root ) );
    case Action::Hwclock:
        if ( const auto res = gucc::hwclock::set_hwclock_utc( root ); !res )
        {
            cWarning() << "gucc: setting the hardware clock failed:"
                       << QString::fromStdString( gucc::to_string( res.error() ) );
        }
        return { };
    case Action::ZfsHostid:
        if ( gs.value( "zfsDatasets" ).toList().isEmpty() )
        {
            return { };
        }
        return gucc::fs::copy_hostid_to_target( root );
    }
    return { };
}
}  // namespace

GuccJob::GuccJob( QObject* parent )
    : Calamares::CppJob( parent )
{
}

GuccJob::~GuccJob() { }

QString
GuccJob::prettyName() const
{
    return m_name ? m_name->get() : tr( "Running an install step…", "@status" );
}

Calamares::JobResult
GuccJob::exec()
{
    Calamares::GlobalStorage* gs = Calamares::JobQueue::instance()->globalStorage();
    if ( !gs || !gs->contains( "rootMountPoint" ) )
    {
        return Calamares::JobResult::internalError( tr( "Configuration Error" ),
                                                    tr( "No root mount point is set." ),
                                                    Calamares::JobResult::InvalidConfiguration );
    }
    if ( !m_action )
    {
        return Calamares::JobResult::internalError( tr( "Configuration Error" ),
                                                    tr( "No valid action is set." ),
                                                    Calamares::JobResult::InvalidConfiguration );
    }

    const auto root = gs->value( "rootMountPoint" ).toString().toStdString();
    if ( const auto res = runAction( *m_action, root, *gs ); !res )
    {
        return Calamares::JobResult::error( prettyName(), QString::fromStdString( gucc::to_string( res.error() ) ) );
    }
    return Calamares::JobResult::ok();
}

void
GuccJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    const auto name = Calamares::getString( configurationMap, "action" );

    // TODO(vnepogodin): hacky.. but fine we remove it for instlib instead of raw gucc in the future
    bool ok = false;
    const auto action = actionNames().find( name, ok );
    m_action = ok ? std::optional { action } : std::nullopt;
    if ( !ok )
    {
        cError() << "gucc: unknown action" << name;
    }

    bool labels_ok = false;
    const auto labels = Calamares::getSubMap( configurationMap, "i18n", labels_ok );
    m_name = labels_ok && labels.contains( "name" )
        ? std::make_unique< Calamares::Locale::TranslatedString >( labels, "name" )
        : nullptr;
}

CALAMARES_PLUGIN_FACTORY_DEFINITION( GuccJobFactory, registerPlugin< GuccJob >(); )
