/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef BOOTLOADERMODEL_H
#define BOOTLOADERMODEL_H

#include "locale/TranslatableConfiguration.h"
#include "utils/NamedEnum.h"

#include <QAbstractListModel>
#include <QObject>
#include <QPixmap>
#include <QVector>


struct BootloaderItem
{
    QString id;
    CalamaresUtils::Locale::TranslatedString name;
    CalamaresUtils::Locale::TranslatedString description;
    QPixmap screenshot;
    QStringList supportedFS;

    /// @brief Create blank BootloaderItem
    BootloaderItem();
    /** @brief Creates a BootloaderItem from given strings
     *
     * This constructor sets all the text members,
     * but leaves the screenshot blank. Set that separately.
     */
    BootloaderItem( const QString& id, const QString& name, const QString& description );

    /** @brief Creates a BootloaderItem from given strings.
     *
     * Set all the text members and load the screenshot from the given
     * @p screenshotPath, which may be a QRC path (:/path/in/qrc) or
     * a filesystem path, whatever QPixmap understands.
     */
    BootloaderItem( const QString& id, const QString& name, const QString& description, const QString& screenshotPath );

    /** @brief Creates a BootloaderItem from a QVariantMap
     *
     * This is intended for use when loading BootloaderItems from a
     * configuration map. It will look up the various keys in the map
     * and handle translation strings as well.
     *
     * The following keys are used:
     *  - *id*: the identifier for this item.
     *  - *name* (and *name[lang]*): for the name and its translations
     *  - *description* (and *description[lang]*)
     *  - *screenshot*: a path to a screenshot for this entry
     *  - *supported-fs*: a list of supported fs
     */
    BootloaderItem( const QVariantMap& map );

    /** @brief Is this item valid?
     *
     * A valid item has an untranslated name available.
     */
    bool isValid() const { return !name.isEmpty(); }

    /** @brief Is this a (the) No-Bootloader bootloader?
     *
     * There should be at most one No-Bootloader item in a collection
     * of BootloaderItems. That one will be used to describe a
     * "no bootloader" situation.
     */
    bool isNoneBootloader() const { return id.isEmpty(); }
};

using BootloaderList = QVector< BootloaderItem >;

class BootloaderListModel : public QAbstractListModel
{
public:
    BootloaderListModel( BootloaderList&& items, QObject* parent );
    BootloaderListModel( QObject* parent );
    ~BootloaderListModel() override;

    /** @brief Add a bootloader @p to the model
     *
     * Only valid bootloaders are added -- that is, they must have a name.
     */
    void addBootloader( BootloaderItem&& p );

    int rowCount( const QModelIndex& index ) const override;
    QVariant data( const QModelIndex& index, int role ) const override;

    /// @brief Direct (non-abstract) access to bootloader data
    const BootloaderItem& bootloaderData( int r ) const { return m_bootloaders[ r ]; }
    /// @brief Direct (non-abstract) count of bootloader data
    int bootloaderCount() const { return m_bootloaders.count(); }

    /** @brief Does a name lookup (based on id) and returns the supported fs
     */
    QStringList getSupportedFSForName( const QString& id ) const;
    /** @brief Name-lookup all the @p ids and returns the supported fs
     *
     * Concatenates installBootloadersForName() for each id in @p ids.
     */
    QStringList getSupportedFSForNames( const QStringList& ids ) const;

    enum Roles : int
    {
        NameRole = Qt::DisplayRole,
        DescriptionRole = Qt::UserRole,
        ScreenshotRole,
        IdRole
    };

private:
    BootloaderList m_bootloaders;
};

#endif
