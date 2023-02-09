/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "BootloaderModel.h"

#include "Branding.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include <QFileInfo>

static QPixmap
loadScreenshot( const QString& path )
{
    if ( QFileInfo::exists( path ) )
    {
        return QPixmap( path );
    }

    const auto* branding = Calamares::Branding::instance();
    if ( !branding )
    {
        return QPixmap();
    }
    return QPixmap( branding->componentDirectory() + QStringLiteral( "/" ) + path );
}

BootloaderItem::BootloaderItem() = default;

BootloaderItem::BootloaderItem( const QString& a_id, const QString& a_name, const QString& a_description )
    : id( a_id )
    , name( a_name )
    , description( a_description )
{
}

BootloaderItem::BootloaderItem( const QString& a_id,
                          const QString& a_name,
                          const QString& a_description,
                          const QString& screenshotPath )
    : id( a_id )
    , name( a_name )
    , description( a_description )
    , screenshot( screenshotPath )
{
}

BootloaderItem::BootloaderItem( const QVariantMap& item_map )
    : id( CalamaresUtils::getString( item_map, "id" ) )
    , name( CalamaresUtils::Locale::TranslatedString( item_map, "name" ) )
    , description( CalamaresUtils::Locale::TranslatedString( item_map, "description" ) )
    , screenshot( loadScreenshot( CalamaresUtils::getString( item_map, "screenshot" ) ) )
    , supportedFS( CalamaresUtils::getStringList( item_map, "supported-fs" ) )
{
    if ( name.isEmpty() && id.isEmpty() )
    {
        name = QObject::tr( "No bootloader" );
    }
    else if ( name.isEmpty() )
    {
        cWarning() << "BootloaderChooser item" << id << "has an empty name.";
    }
    if ( description.isEmpty() )
    {
        description = QObject::tr( "No description provided." );
    }
}

BootloaderListModel::BootloaderListModel( QObject* parent )
    : QAbstractListModel( parent )
{
}

BootloaderListModel::BootloaderListModel( BootloaderList&& items, QObject* parent )
    : QAbstractListModel( parent )
    , m_bootloaders( std::move( items ) )
{
}

BootloaderListModel::~BootloaderListModel() = default;

void
BootloaderListModel::addBootloader( BootloaderItem&& bootloaderitem )
{
    // Only add valid bootloaders
    if ( bootloaderitem.isValid() )
    {
        const int32_t bootloader_count = m_bootloaders.count();
        beginInsertRows( QModelIndex(), bootloader_count, bootloader_count );
        m_bootloaders.append( std::move( bootloaderitem ) );
        endInsertRows();
    }
}

QStringList
BootloaderListModel::getSupportedFSForName( const QString& id ) const
{
    auto found = std::find_if( m_bootloaders.begin(), m_bootloaders.end(), [&id](auto&& bootloader){ return bootloader.id == id; } );
    return ( found != m_bootloaders.end() ) ? found->supportedFS : QStringList();
}

QStringList
BootloaderListModel::getSupportedFSForNames( const QStringList& ids ) const
{
    QStringList l;
    for ( const auto& p : qAsConst( m_bootloaders ) )
    {
        if ( ids.contains( p.id ) )
        {
            l.append( p.supportedFS );
        }
    }
    return l;
}

int
BootloaderListModel::rowCount( const QModelIndex& index ) const
{
    // For lists, valid indexes have zero children; only the root index has them
    return index.isValid() ? 0 : m_bootloaders.count();
}

QVariant
BootloaderListModel::data( const QModelIndex& index, int role ) const
{
    if ( !index.isValid() )
    {
        return QVariant();
    }
    int row = index.row();
    if ( row >= m_bootloaders.count() || row < 0 )
    {
        return QVariant();
    }

    if ( role == Qt::DisplayRole /* Also BootloaderNameRole */ )
    {
        return m_bootloaders[ row ].name.get();
    }
    else if ( role == DescriptionRole )
    {
        return m_bootloaders[ row ].description.get();
    }
    else if ( role == ScreenshotRole )
    {
        return m_bootloaders[ row ].screenshot;
    }
    else if ( role == IdRole )
    {
        return m_bootloaders[ row ].id;
    }

    return QVariant();
}
