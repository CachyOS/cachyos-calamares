/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Bill Auger
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef UTILS_PASTE_H
#define UTILS_PASTE_H

#include "DllMacro.h"

#include <QString>

class QObject;
class QWidget;

namespace Calamares
{
namespace Paste
{
/** @brief Send the current log file to a pastebin
 *
 * Returns the (string) URL that the pastebin gives us.
 */
UIDLLEXPORT QString doLogUpload( QObject* parent );

/** @brief Send the current log file to a pastebin
 *
 * As doLogUpload(), but also sets the clipboard and displays
 * a message saying it's been done.
 */
UIDLLEXPORT QString doLogUploadUI( QWidget* parent );

/** @brief Is paste enabled?
 *
 * Checks the branding instance if paste can be done.
 */
UIDLLEXPORT bool isEnabled();
}  // namespace Paste

}  // namespace Calamares

#endif
