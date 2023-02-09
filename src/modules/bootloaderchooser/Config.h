/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef BOOTLOADER_CONFIG_H
#define BOOTLOADER_CONFIG_H

#include "BootloaderModel.h"

#include "modulesystem/Config.h"
#include "modulesystem/InstanceKey.h"

#include <memory>
#include <optional>

enum class BootloaderChooserMode
{
    Optional,  // zero or one
    Required,  // exactly one
    OptionalMultiple,  // zero or more
    RequiredMultiple  // one or more
};

const NamedEnumTable< BootloaderChooserMode >& bootloaderChooserModeNames();

enum class BootloaderChooserMethod
{
    Legacy,  // use contextualprocess or other custom
};

const NamedEnumTable< BootloaderChooserMethod >& BootloaderChooserMethodNames();

class Config : public Calamares::ModuleSystem::Config
{
    Q_OBJECT

    /** @brief This is the single-select bootloader-choice
     *
     * For (QML) modules that support only a single selection and
     * just want to do things in a straightforward pick-this-one
     * way, the bootloaderChoice property is a (the) way to go.
     *
     * Writing to this property means that any other form of bootloader-
     * choice or selection is ignored.
     */
    Q_PROPERTY( QString bootloaderChoice READ bootloaderChoice WRITE setBootloaderChoice NOTIFY bootloaderChoiceChanged )
    Q_PROPERTY( QString prettyStatus READ prettyStatus NOTIFY prettyStatusChanged FINAL )

public:
    Config( QObject* parent = nullptr );
    ~Config() override;

    /** @brief Sets the default Id for this Config
     *
     * The default Id is the (owning) module identifier for the config,
     * and it is used when the Id read from the config file is empty.
     * The **usual** configuration when using method *bootloaders* is
     * to rely on the default Id.
     */
    void setDefaultId( const Calamares::ModuleSystem::InstanceKey& defaultId ) { m_defaultId = defaultId; }
    void setConfigurationMap( const QVariantMap& ) override;

    BootloaderChooserMode mode() const { return m_mode; }
    BootloaderListModel* model() const { return m_model; }
    QModelIndex defaultSelectionIndex() const { return m_defaultModelIndex; }

    /** @brief Returns an "introductory bootloader" which describes bootloaderchooser
     *
     * If the model contains a "none" bootloader, returns that one on
     * the assumption that it is one to describe the whole; otherwise
     * returns a totally generic description.
     */
    const BootloaderItem& introductionBootloader() const;

    /** @brief Write selection to global storage
     *
     * Updates the GS keys for this bootloaderchooser, marking all
     * (and only) the bootloaders in @p selected as selected.
     */
    void updateGlobalStorage( const QStringList& selected ) const;
    /** @brief Write selection to global storage
     *
     * Updates the GS keys for this bootloaderchooser, marking **only**
     * the bootloader choice as selected. This assumes that the single-
     * selection QML code is in use.
     */
    void updateGlobalStorage() const;

    QString bootloaderChoice() const { return m_bootloaderChoice.value_or( QString() ); }
    void setBootloaderChoice( const QString& bootloaderChoice );

    QString prettyName() const;
    QString prettyStatus() const;

signals:
    void bootloaderChoiceChanged( QString bootloaderChoice );
    void prettyStatusChanged();

private:
    BootloaderListModel* m_model = nullptr;
    QModelIndex m_defaultModelIndex;

    /// Selection mode for this module
    BootloaderChooserMode m_mode = BootloaderChooserMode::Optional;
    /// How this module stores to GS
    BootloaderChooserMethod m_method = BootloaderChooserMethod::Legacy;
    /// Value to use for id if none is set in the config file
    Calamares::ModuleSystem::InstanceKey m_defaultId;
    /** @brief QML selection (for single-selection approaches)
     *
     * If there is no value, then there has been no selection.
     * Reading the property will return an empty QString.
     */
    std::optional< QString > m_bootloaderChoice;
    CalamaresUtils::Locale::TranslatedString* m_stepName;  // As it appears in the sidebar
};


#endif
