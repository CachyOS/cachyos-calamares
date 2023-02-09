/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef BOOTLOADERCHOOSERPAGE_H
#define BOOTLOADERCHOOSERPAGE_H

#include "Config.h"
#include "BootloaderModel.h"

#include <QAbstractItemModel>
#include <QWidget>

namespace Ui
{
class BootloaderChooserPage;
}  // namespace Ui

class BootloaderChooserPage : public QWidget
{
    Q_OBJECT
public:
    explicit BootloaderChooserPage( BootloaderChooserMode mode, QWidget* parent = nullptr );

    /// @brief Sets the data model for the listview
    void setModel( QAbstractItemModel* model );

    /// @brief Sets the introductory (no-package-selected) texts
    void setIntroduction( const BootloaderItem& item );
    /// @brief Selects a listview item
    void setSelection( const QModelIndex& index );
    /// @brief Is anything selected?
    bool hasSelection() const;
    /** @brief Get the list of selected ids
     *
     * This list may be empty (if none is selected).
     */
    QStringList selectedBootloaderIds() const;

public slots:
    void currentChanged( const QModelIndex& index );
    void updateLabels();

signals:
    void selectionChanged();

private:
    Ui::BootloaderChooserPage* ui;
    BootloaderItem m_introduction;
};

#endif  // BOOTLOADERCHOOSERPAGE_H
