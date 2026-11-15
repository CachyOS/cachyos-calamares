/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2022 Evan James <dalto@fastmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "InitcpioJob.h"

#include "CalamaresConfig.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/UMask.h"
#include "utils/Variant.h"

#include <QDir>
#include <QFile>

#ifdef WITH_CACHYOS_INSTALLER
#include "GlobalStorage.h"
#include "JobQueue.h"

#include <gucc/error.hpp>
#include <gucc/io_utils.hpp>
#endif

InitcpioJob::InitcpioJob( QObject* parent )
    : Calamares::CppJob( parent )
{
}

InitcpioJob::~InitcpioJob() {}

QString
InitcpioJob::prettyName() const
{
    return tr( "Creating initramfs with mkinitcpio…", "@status" );
}

/** @brief Sets secure permissions on each initramfs
 *
 * Iterates over each initramfs contained directly in the directory @p d.
 * For each initramfs found, the permissions are set to owner read/write only.
 *
 */
void
fixPermissions( const QDir& d )
{
    const auto initramList = d.entryInfoList( { "initramfs*" }, QDir::Files );
    for ( const auto& fi : initramList )
    {
        QFile f( fi.absoluteFilePath() );
        if ( f.exists() )
        {
            cDebug() << "initcpio setting permissions for" << f.fileName();
            f.setPermissions( QFileDevice::ReadOwner | QFileDevice::WriteOwner );
        }
    }
}

Calamares::JobResult
InitcpioJob::exec()
{
    Calamares::UMask m( Calamares::UMask::Safe );

    if ( m_unsafe )
    {
        cDebug() << "Skipping mitigations for unsafe initramfs permissions.";
    }
    else
    {
        QDir d( Calamares::System::instance()->targetPath( "/boot" ) );
        if ( d.exists() )
        {
            fixPermissions( d );
        }
    }

#ifdef WITH_CACHYOS_INSTALLER
    if ( m_gucc )
    {
        const auto root = Calamares::JobQueue::instance()->globalStorage()->value( "rootMountPoint" ).toString();
        if ( const auto res = gucc::utils::arch_chroot_or_error( "mkinitcpio -P", root.toStdString(), "mkinitcpio" );
             !res )
        {
            return Calamares::JobResult::error( tr( "Could not create initramfs" ),
                                                QString::fromStdString( gucc::to_string( res.error() ) ) );
        }
        return Calamares::JobResult::ok();
    }
#endif

    // If the kernel option isn't set to a specific kernel, run mkinitcpio on all kernels
    QStringList command = { "mkinitcpio" };
    if ( m_kernel.isEmpty() || m_kernel == "all" )
    {
        command.append( "-P" );
    }
    else
    {
        command.append( { "-p", m_kernel } );
    }

    cDebug() << "Updating initramfs with kernel" << m_kernel;
    auto r = Calamares::System::instance()->targetEnvCommand( command, QString(), QString() /* no timeout , 0 */ );
    return r.explainProcess( "mkinitcpio", std::chrono::seconds( 10 ) /* fake timeout */ );
}

void
InitcpioJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_kernel = Calamares::getString( configurationMap, "kernel" );

    m_unsafe = Calamares::getBool( configurationMap, "be_unsafe", false );

    m_gucc = Calamares::getBool( configurationMap, "gucc", false );
#ifdef WITH_CACHYOS_INSTALLER
    if ( m_gucc && !( m_kernel.isEmpty() || m_kernel == "all" ) )
    {
        cWarning() << "Initcpio: gucc needs kernel: all, using the built-in code.";
        m_gucc = false;
    }
#endif
}

CALAMARES_PLUGIN_FACTORY_DEFINITION( InitcpioJobFactory, registerPlugin< InitcpioJob >(); )
