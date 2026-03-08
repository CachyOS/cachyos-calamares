/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef FIXEDASPECTRATIOLABEL_H
#define FIXEDASPECTRATIOLABEL_H

#include "DllMacro.h"

#include <QLabel>
#include <QPixmap>

class QMovie;

class UIDLLEXPORT FixedAspectRatioLabel : public QLabel
{
    Q_OBJECT
public:
    explicit FixedAspectRatioLabel( QWidget* parent = nullptr );
    ~FixedAspectRatioLabel() override;

public slots:
    void setPixmap( const QPixmap& pixmap );
    void setAnimatedImage( const QString& path );
    void resizeEvent( QResizeEvent* event ) override;
    void hideEvent( QHideEvent* event ) override;
    void showEvent( QShowEvent* event ) override;

private slots:
    void updateAnimatedFrame();

private:
    void stopAnimation();

    QPixmap m_pixmap;
    QMovie* m_movie = nullptr;
};

#endif  // FIXEDASPECTRATIOLABEL_H
