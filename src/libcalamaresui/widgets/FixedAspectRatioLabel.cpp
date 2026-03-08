/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "FixedAspectRatioLabel.h"

#include <QMovie>
#include <QHideEvent>
#include <QShowEvent>


FixedAspectRatioLabel::FixedAspectRatioLabel( QWidget* parent )
    : QLabel( parent )
{
}


FixedAspectRatioLabel::~FixedAspectRatioLabel()
{
    stopAnimation();
}


void
FixedAspectRatioLabel::setPixmap( const QPixmap& pixmap )
{
    stopAnimation();
    m_pixmap = pixmap;
    m_pixmap.setDevicePixelRatio( devicePixelRatio() );
    QLabel::setPixmap( m_pixmap.scaled(
        contentsRect().size() * m_pixmap.devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}


void
FixedAspectRatioLabel::setAnimatedImage( const QString& path )
{
    stopAnimation();

    m_movie = new QMovie( path, QByteArray(), this );
    if ( !m_movie->isValid() )
    {
        delete m_movie;
        m_movie = nullptr;
        return;
    }

    connect( m_movie, &QMovie::frameChanged, this, &FixedAspectRatioLabel::updateAnimatedFrame );
    m_movie->start();
}


void
FixedAspectRatioLabel::updateAnimatedFrame()
{
    if ( !m_movie )
    {
        return;
    }

    QPixmap frame = m_movie->currentPixmap();
    frame.setDevicePixelRatio( devicePixelRatio() );
    QLabel::setPixmap( frame.scaled(
        contentsRect().size() * frame.devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}


void
FixedAspectRatioLabel::stopAnimation()
{
    if ( m_movie )
    {
        m_movie->stop();
        delete m_movie;
        m_movie = nullptr;
    }
}


void
FixedAspectRatioLabel::resizeEvent( QResizeEvent* event )
{
    Q_UNUSED( event )
    if ( m_movie )
    {
        updateAnimatedFrame();
    }
    else
    {
        QLabel::setPixmap( m_pixmap.scaled(
            contentsRect().size() * m_pixmap.devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    }
}


void
FixedAspectRatioLabel::hideEvent( QHideEvent* event )
{
    if ( m_movie && m_movie->state() == QMovie::Running )
    {
        m_movie->setPaused( true );
    }
    QLabel::hideEvent( event );
}


void
FixedAspectRatioLabel::showEvent( QShowEvent* event )
{
    if ( m_movie && m_movie->state() == QMovie::Paused )
    {
        m_movie->setPaused( false );
    }
    QLabel::showEvent( event );
}
