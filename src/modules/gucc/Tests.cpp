/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 Vladislav Nepogodin <vnepogodin@cachyos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "GuccJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class GuccTests : public QObject
{
    Q_OBJECT
public:
    GuccTests() { }
    ~GuccTests() override { }

private Q_SLOTS:
    void initTestCase();
    void testConfig();
    void testNoAction();
    void testZfsHostidSkipsNonZfs();
    void testMkHook();

private:
    Calamares::GlobalStorage* globalStorage();
};

void
GuccTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
}

Calamares::GlobalStorage*
GuccTests::globalStorage()
{
    if ( !Calamares::JobQueue::instance() )
    {
        // test test test. ignore
        (void)new Calamares::JobQueue();
    }
    return Calamares::JobQueue::instance()->globalStorage();
}

void
GuccTests::testConfig()
{
    GuccJob job;
    job.setConfigurationMap( { { "action", "btrfs-snapshot" } } );
    QCOMPARE( job.action(), std::optional { GuccJob::Action::BtrfsSnapshot } );
    QVERIFY( !job.prettyName().isEmpty() );

    const QVariantMap labels { { "name", "Snapshotting" }, { "name[de]", "Schnappschuss" } };
    job.setConfigurationMap( { { "action", "btrfs-snapshot" }, { "i18n", labels } } );
    QCOMPARE( job.prettyName(), QStringLiteral( "Snapshotting" ) );

    job.setConfigurationMap( { { "action", "frobnicate" } } );
    QVERIFY( !job.action() );
    job.setConfigurationMap( { } );
    QVERIFY( !job.action() );
}

void
GuccTests::testNoAction()
{
    QTemporaryDir root;
    globalStorage()->insert( "rootMountPoint", root.path() );

    GuccJob job;
    job.setConfigurationMap( { { "action", "frobnicate" } } );
    QVERIFY( !job.exec() );
}

void
GuccTests::testZfsHostidSkipsNonZfs()
{
    QTemporaryDir root;
    auto* gs = globalStorage();
    gs->insert( "rootMountPoint", root.path() );
    gs->remove( "zfsDatasets" );

    GuccJob job;
    job.setConfigurationMap( { { "action", "zfs-hostid" } } );
    QVERIFY( job.exec() );
    QVERIFY( !QFile::exists( root.filePath( "etc/hostid" ) ) );
}

void
GuccTests::testMkHook()
{
    QTemporaryDir root;
    globalStorage()->insert( "rootMountPoint", root.path() );
    const auto hook = root.filePath( "etc/pacman.d/hooks/90-mkinitcpio-install.hook" );
    const auto script = root.filePath( "usr/share/libalpm/scripts/mkinitcpio-install-calamares" );

    GuccJob install;
    install.setConfigurationMap( { { "action", "install-mk-hook" } } );
    QVERIFY( install.exec() );
    QVERIFY( QFile::exists( hook ) );
    QVERIFY( QFileInfo( script ).isExecutable() );

    GuccJob remove;
    remove.setConfigurationMap( { { "action", "remove-mk-hook" } } );
    QVERIFY( remove.exec() );
    QVERIFY( !QFile::exists( hook ) );
    QVERIFY( !QFile::exists( script ) );
}

QTEST_GUILESS_MAIN( GuccTests )

#include "utils/moc-warnings.h"

#include "Tests.moc"
