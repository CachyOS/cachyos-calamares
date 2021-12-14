/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Caio Jordão Carvalho <caiojcarvalho@gmail.com>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "ResizeVolumeGroupDialog.h"

#include "gui/ListPhysicalVolumeWidgetItem.h"

#include <kpmcore/core/lvmdevice.h>
#include <kpmcore/util/capacity.h>

#include <QComboBox>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QSpinBox>

ResizeVolumeGroupDialog::ResizeVolumeGroupDialog( LvmDevice* device,
                                                  const PartitionVector& availablePVs,
                                                  QWidget* parent )
    : VolumeGroupBaseDialog( parent, device->name(), device->physicalVolumes() )
{
    setWindowTitle( tr( "Resize Volume Group" ) );

    for ( int i = 0; i < pvListWidget()->count(); i++ )
    {
        pvListWidget()->item( i )->setCheckState( Qt::Checked );
    }

    for ( const Partition* p : availablePVs )
    {
        pvListWidget()->addItem( new ListPhysicalVolumeWidgetItem( p, false ) );
    }

    peSizeWidget()->setValue(
        static_cast< int >( device->peSize() / Capacity::unitFactor( Capacity::Unit::Byte, Capacity::Unit::MiB ) ) );

    vgNameWidget()->setEnabled( false );
    peSizeWidget()->setEnabled( false );
    vgTypeWidget()->setEnabled( false );

    setUsedSizeValue( device->allocatedPE() * device->peSize() );
    setLVQuantity( device->partitionTable()->children().count() );
}
