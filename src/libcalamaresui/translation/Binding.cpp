/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */
#include "Binding.h"

#include "utils/Retranslator.h"

namespace Calamares
{
namespace Translation
{
Binding::Binding( QWidget* parent )
    : m_parent( parent )
{
    connect( CalamaresUtils::Retranslator::instance(),
             &CalamaresUtils::Retranslator::languageChanged,
             this,
             &Binding::update );
};
Binding::~Binding()
{
    for ( auto* p : m_labels )
    {
        delete p;
    }
}

void
Binding::update()
{
    std::for_each( m_labels.begin(), m_labels.end(), [ parent = m_parent ]( BaseUpdater* p ) { p->update( parent ); } );
}


Binding::BaseUpdater::~BaseUpdater() {}

QString
Binding::BaseUpdater::tr( QWidget* parent ) const
{
    QString text = parent->tr( untranslated_string );
    if ( args.has_value() )
    {
        for ( const QString& s : args.value() )
        {
            text = text.arg( s );
        }
    }
    return text;
}


}  // namespace Translation
}  // namespace Calamares
