/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "packages/Globals.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

const NamedEnumTable< BootloaderChooserMode >&
bootloaderChooserModeNames()
{
    static const NamedEnumTable< BootloaderChooserMode > names {
        { "optional", BootloaderChooserMode::Optional },
        { "required", BootloaderChooserMode::Required },
        { "optionalmultiple", BootloaderChooserMode::OptionalMultiple },
        { "requiredmultiple", BootloaderChooserMode::RequiredMultiple },
        // and a bunch of aliases
        { "zero-or-one", BootloaderChooserMode::Optional },
        { "radio", BootloaderChooserMode::Required },
        { "one", BootloaderChooserMode::Required },
        { "set", BootloaderChooserMode::OptionalMultiple },
        { "zero-or-more", BootloaderChooserMode::OptionalMultiple },
        { "multiple", BootloaderChooserMode::RequiredMultiple },
        { "one-or-more", BootloaderChooserMode::RequiredMultiple }
    };
    return names;
}

const NamedEnumTable< BootloaderChooserMethod >&
BootloaderChooserMethodNames()
{
    static const NamedEnumTable< BootloaderChooserMethod > names {
        { "legacy", BootloaderChooserMethod::Legacy },
        { "custom", BootloaderChooserMethod::Legacy },
        { "contextualprocess", BootloaderChooserMethod::Legacy },
    };
    return names;
}

Config::Config( QObject* parent )
    : Calamares::ModuleSystem::Config( parent )
    , m_model( new BootloaderListModel( this ) )
    , m_mode( BootloaderChooserMode::Required )
{
}

Config::~Config() = default;

const BootloaderItem&
Config::introductionBootloader() const
{
    for ( int i = 0; i < m_model->bootloaderCount(); ++i )
    {
        const auto& bootloaderitem = m_model->bootloaderData( i );
        if ( bootloaderitem.isNoneBootloader() )
        {
            return bootloaderitem;
        }
    }

    static BootloaderItem* defaultIntroduction = nullptr;
    if ( !defaultIntroduction )
    {
        const auto name = QT_TR_NOOP( "Bootloader Selection" );
        const auto description
            = QT_TR_NOOP( "Please pick a bootloader from the list." );
        defaultIntroduction = new BootloaderItem( QString(), name, description );
        defaultIntroduction->screenshot = QPixmap( QStringLiteral( ":/images/no-selection.png" ) );
        defaultIntroduction->name = CalamaresUtils::Locale::TranslatedString( name, metaObject()->className() );
        defaultIntroduction->description
            = CalamaresUtils::Locale::TranslatedString( description, metaObject()->className() );
    }
    return *defaultIntroduction;
}

static inline QString
make_gs_key( const Calamares::ModuleSystem::InstanceKey& key )
{
    return QStringLiteral( "bootloaderchooser_" ) + key.id();
}

void
Config::updateGlobalStorage( const QStringList& selected ) const
{
    if ( m_bootloaderChoice.has_value() )
    {
        cWarning() << "Inconsistent package choices -- both model and single-selection QML";
    }
    if ( m_method == BootloaderChooserMethod::Legacy )
    {
        QString value = selected.join( ',' );
        Calamares::JobQueue::instance()->globalStorage()->insert( make_gs_key( m_defaultId ), value );
        cDebug() << m_defaultId << "selected" << value;
    }
    else
    {
        cWarning() << "Unknown bootloaderchooser method" << smash( m_method );
    }
}

void
Config::updateGlobalStorage() const
{
    if ( m_model->bootloaderCount() > 0 )
    {
        cWarning() << "Inconsistent package choices -- both model and single-selection QML";
    }
    if ( m_method == BootloaderChooserMethod::Legacy )
    {
        auto* gs = Calamares::JobQueue::instance()->globalStorage();
        if ( m_bootloaderChoice.has_value() )
        {
            gs->insert( make_gs_key( m_defaultId ), m_bootloaderChoice.value() );
        }
        else
        {
            gs->remove( make_gs_key( m_defaultId ) );
        }
    }
    else
    {
        cWarning() << "Unknown bootloaderchooser method" << smash( m_method );
    }
}


void
Config::setBootloaderChoice( const QString& bootloaderChoice )
{
    if ( bootloaderChoice.isEmpty() )
    {
        m_bootloaderChoice.reset();
    }
    else
    {
        m_bootloaderChoice = bootloaderChoice;
    }
    emit bootloaderChoiceChanged( m_bootloaderChoice.value_or( QString() ) );
}

QString
Config::prettyName() const
{
    return m_stepName ? m_stepName->get() : tr( "Bootloader" );
}

QString
Config::prettyStatus() const
{
    return tr( "Install option: <strong>%1</strong>" ).arg( m_bootloaderChoice.value_or( tr( "None" ) ) );
}

static void
fillModel( BootloaderListModel* model, const QVariantList& items )
{
    if ( items.isEmpty() )
    {
        cWarning() << "No *items* for BootloaderChooser module.";
        return;
    }

    cDebug() << "Loading BootloaderChooser model items from config";
    int item_index = 0;
    for ( const auto& item_it : items )
    {
        ++item_index;
        QVariantMap item_map = item_it.toMap();
        if ( item_map.isEmpty() )
        {
            cWarning() << "BootloaderChooser entry" << item_index << "is not valid.";
            continue;
        }

        if ( item_map.contains( "appdata" ) )
        {
            cWarning() << "Loading AppData XML is not supported.";
        }
        else if ( item_map.contains( "appstream" ) )
        {
            cWarning() << "Loading AppStream data is not supported.";
        }
        else
        {
            model->addBootloader( BootloaderItem( item_map ) );
        }
    }
    cDebug() << Logger::SubEntry << "Loaded BootloaderChooser with" << model->bootloaderCount() << "entries.";
}

void
Config::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_mode = bootloaderChooserModeNames().find( CalamaresUtils::getString( configurationMap, "mode" ),
                                             BootloaderChooserMode::Required );
    m_method = BootloaderChooserMethodNames().find( CalamaresUtils::getString( configurationMap, "method" ),
                                                 BootloaderChooserMethod::Legacy );

    if ( m_method == BootloaderChooserMethod::Legacy )
    {
        cDebug() << "Using module ID" << m_defaultId;
    }

    if ( configurationMap.contains( "items" ) )
    {
        fillModel( m_model, configurationMap.value( "items" ).toList() );

        QString default_item_id = CalamaresUtils::getString( configurationMap, "default" );
        if ( !default_item_id.isEmpty() )
        {
            for ( int item_n = 0; item_n < m_model->bootloaderCount(); ++item_n )
            {
                QModelIndex item_idx = m_model->index( item_n, 0 );
                QVariant item_id = m_model->data( item_idx, BootloaderListModel::IdRole );

                if ( item_id.toString() == default_item_id )
                {
                    m_defaultModelIndex = item_idx;
                    break;
                }
            }
        }
    }
    else
    {
        setBootloaderChoice( CalamaresUtils::getString( configurationMap, "bootloaderChoice" ) );
        if ( m_method != BootloaderChooserMethod::Legacy )
        {
            cWarning() << "Single-selection QML module must use 'Legacy' method.";
        }
    }

    bool labels_ok = false;
    auto labels = CalamaresUtils::getSubMap( configurationMap, "labels", labels_ok );
    if ( labels_ok )
    {
        if ( labels.contains( "step" ) )
        {
            m_stepName = new CalamaresUtils::Locale::TranslatedString( labels, "step" );
        }
    }
}
