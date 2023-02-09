/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "BootloaderChooserPage.h"

#include "ui_page_bootloader.h"

#include "utils/CalamaresUtilsGui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"

#include <QLabel>

BootloaderChooserPage::BootloaderChooserPage( BootloaderChooserMode mode, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::BootloaderChooserPage )
    , m_introduction( QString(),
                      QString(),
                      tr( "Bootloader Selection" ),
                      tr( "Please pick a bootloader from the list" ) )
{
    m_introduction.screenshot = QPixmap( QStringLiteral( ":/images/no-selection.png" ) );

    ui->setupUi( this );
    CALAMARES_RETRANSLATE( updateLabels(); );

    switch ( mode )
    {
    case BootloaderChooserMode::Optional:
        [[fallthrough]];
    case BootloaderChooserMode::Required:
        ui->products->setSelectionMode( QAbstractItemView::SingleSelection );
        break;
    case BootloaderChooserMode::OptionalMultiple:
        [[fallthrough]];
    case BootloaderChooserMode::RequiredMultiple:
        ui->products->setSelectionMode( QAbstractItemView::ExtendedSelection );
    }

    ui->products->setMinimumWidth( 10 * CalamaresUtils::defaultFontHeight() );
}

void
BootloaderChooserPage::currentChanged( const QModelIndex& index )
{
    if ( !index.isValid() || !ui->products->selectionModel()->hasSelection() )
    {
        ui->productName->setText( m_introduction.name.get() );
        ui->productScreenshot->setPixmap( m_introduction.screenshot );
        ui->productDescription->setText( m_introduction.description.get() );
    }
    else
    {
        const auto* model = ui->products->model();

        ui->productName->setText( model->data( index, BootloaderListModel::NameRole ).toString() );
        ui->productDescription->setText( model->data( index, BootloaderListModel::DescriptionRole ).toString() );

        QPixmap currentScreenshot = model->data( index, BootloaderListModel::ScreenshotRole ).value< QPixmap >();
        if ( currentScreenshot.isNull() )
        {
            ui->productScreenshot->setPixmap( m_introduction.screenshot );
        }
        else
        {
            ui->productScreenshot->setPixmap( currentScreenshot );
        }
    }
}

void
BootloaderChooserPage::updateLabels()
{
    if ( ui && ui->products && ui->products->selectionModel() )
    {
        currentChanged( ui->products->selectionModel()->currentIndex() );
    }
    else
    {
        currentChanged( QModelIndex() );
    }
    emit selectionChanged();
}

void
BootloaderChooserPage::setModel( QAbstractItemModel* model )
{
    ui->products->setModel( model );
    currentChanged( QModelIndex() );
    connect( ui->products->selectionModel(),
             &QItemSelectionModel::selectionChanged,
             this,
             &BootloaderChooserPage::updateLabels );
}

void
BootloaderChooserPage::setSelection( const QModelIndex& index )
{
    if ( index.isValid() )
    {
        ui->products->selectionModel()->select( index, QItemSelectionModel::Select );
    }
    currentChanged( index );
}

bool
BootloaderChooserPage::hasSelection() const
{
    return ui && ui->products && ui->products->selectionModel() && ui->products->selectionModel()->hasSelection();
}

QStringList
BootloaderChooserPage::selectedBootloaderIds() const
{
    if ( !( ui && ui->products && ui->products->selectionModel() ) )
    {
        return QStringList();
    }

    const auto* model = ui->products->model();
    QStringList ids;
    for ( const auto& index : ui->products->selectionModel()->selectedIndexes() )
    {
        QString pid = model->data( index, BootloaderListModel::IdRole ).toString();
        if ( !pid.isEmpty() )
        {
            ids.append( pid );
        }
    }
    return ids;
}

void
BootloaderChooserPage::setIntroduction( const BootloaderItem& item )
{
    m_introduction.name = item.name;
    m_introduction.description = item.description;
    m_introduction.screenshot = item.screenshot;
}
