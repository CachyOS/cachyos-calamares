/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "BootloaderChooserViewStep.h"

#include "Config.h"
#include "BootloaderChooserPage.h"
#include "BootloaderModel.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "locale/TranslatableConfiguration.h"
#include "utils/CalamaresUtilsSystem.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include <QDesktopServices>
#include <QVariantMap>

CALAMARES_PLUGIN_FACTORY_DEFINITION( BootloaderChooserViewStepFactory, registerPlugin< BootloaderChooserViewStep >(); )

BootloaderChooserViewStep::BootloaderChooserViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( nullptr )
{
    emit nextStatusChanged( false );
}


BootloaderChooserViewStep::~BootloaderChooserViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
BootloaderChooserViewStep::prettyName() const
{
    return m_config->prettyName();
}


QWidget*
BootloaderChooserViewStep::widget()
{
    if ( !m_widget )
    {
        m_widget = new BootloaderChooserPage( m_config->mode(), nullptr );
        connect( m_widget,
                 &BootloaderChooserPage::selectionChanged,
                 [ = ]() { emit nextStatusChanged( this->isNextEnabled() ); } );
        hookupModel();
    }
    return m_widget;
}


bool
BootloaderChooserViewStep::isNextEnabled() const
{
    if ( !m_widget )
    {
        // No way to have changed anything
        return true;
    }

    switch ( m_config->mode() )
    {
    case BootloaderChooserMode::Optional:
    case BootloaderChooserMode::OptionalMultiple:
        // zero or one OR zero or more
        return true;
    case BootloaderChooserMode::Required:
    case BootloaderChooserMode::RequiredMultiple:
        // exactly one OR one or more
        return m_widget->hasSelection();
    }
    __builtin_unreachable();
}


bool
BootloaderChooserViewStep::isBackEnabled() const
{
    return true;
}


bool
BootloaderChooserViewStep::isAtBeginning() const
{
    return true;
}


bool
BootloaderChooserViewStep::isAtEnd() const
{
    return true;
}

void
BootloaderChooserViewStep::onActivate()
{
    if ( !m_widget->hasSelection() )
    {
        m_widget->setSelection( m_config->defaultSelectionIndex() );
    }
}

void
BootloaderChooserViewStep::onLeave()
{
    m_config->updateGlobalStorage( m_widget->selectedBootloaderIds() );
}

Calamares::JobList
BootloaderChooserViewStep::jobs() const
{
    Calamares::JobList l;
    return l;
}

void
BootloaderChooserViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setDefaultId( moduleInstanceKey() );
    m_config->setConfigurationMap( configurationMap );

    if ( m_widget )
    {
        hookupModel();
    }
}


void
BootloaderChooserViewStep::hookupModel()
{
    if ( !m_config->model() || !m_widget )
    {
        cError() << "Can't hook up model until widget and model both exist.";
        return;
    }

    m_widget->setModel( m_config->model() );
    m_widget->setIntroduction( m_config->introductionBootloader() );
}
