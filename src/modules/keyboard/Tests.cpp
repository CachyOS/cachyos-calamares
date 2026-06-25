/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */
#include "AdditionalLayoutInfo.h"
#include "utils/Logger.h"

#include <QtTest/QtTest>

// Internals of SetKeyboardLayoutJob.cpp
extern QString findLegacyKeymap( const QString& layout, const QString& model, const QString& variant );
extern QStringList variantList( const AdditionalLayoutInfo& additionalLayoutInfo, const QString& variant );
extern QStringList
vconsoleXkbLines( const QString& model, const QStringList& layouts, const QStringList& variants, const QString& options );

class KeyboardLayoutTests : public QObject
{
    Q_OBJECT
public:
    KeyboardLayoutTests() {}
    ~KeyboardLayoutTests() override {}

private Q_SLOTS:
    void initTestCase();

    void testSimpleLayoutLookup_data();
    void testSimpleLayoutLookup();
    void testVariantList_data();
    void testVariantList();
    void testVConsoleXkbLines_data();
    void testVConsoleXkbLines();
};

void
KeyboardLayoutTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
}

void
KeyboardLayoutTests::testSimpleLayoutLookup_data()
{
    QTest::addColumn< QString >( "layout" );
    QTest::addColumn< QString >( "model" );
    QTest::addColumn< QString >( "variant" );
    QTest::addColumn< QString >( "vconsole" );

    QTest::newRow( "us" ) << QString( "us" ) << QString() << QString() << QString( "us" );
    QTest::newRow( "turkish default" ) << QString( "tr" ) << QString() << QString() << QString( "trq" );
    QTest::newRow( "turkish alt-q" ) << QString( "tr" ) << QString() << QString( "alt" ) << QString( "trq" );
    QTest::newRow( "turkish f" ) << QString( "tr" ) << QString() << QString( "f" ) << QString( "trf" );
    QTest::newRow( "italian" ) << QString( "it" ) << QString( "pc105" ) << QString() << QString( "it" );
}


void
KeyboardLayoutTests::testSimpleLayoutLookup()
{
    QFETCH( QString, layout );
    QFETCH( QString, model );
    QFETCH( QString, variant );
    QFETCH( QString, vconsole );

    QCOMPARE( findLegacyKeymap( layout, model, variant ), vconsole );
}

void
KeyboardLayoutTests::testVariantList_data()
{
    QTest::addColumn< QString >( "additionalLayout" );
    QTest::addColumn< QString >( "additionalVariant" );
    QTest::addColumn< QString >( "variant" );
    QTest::addColumn< QStringList >( "expected" );

    QTest::newRow( "selected variant only" ) << QString() << QString() << QStringLiteral( "dvorak" )
                                             << QStringList { QStringLiteral( "dvorak" ) };
    QTest::newRow( "no layouts or variants" ) << QString() << QString() << QString() << QStringList {};
    QTest::newRow( "ignore additional variant without layout" ) << QString() << QStringLiteral( "intl" )
                                                                << QStringLiteral( "dvorak" )
                                                                << QStringList { QStringLiteral( "dvorak" ) };
    QTest::newRow( "additional layout with selected variant" ) << QStringLiteral( "us" ) << QString()
                                                              << QStringLiteral( "phonetic" )
                                                              << QStringList { QString(), QStringLiteral( "phonetic" ) };
    QTest::newRow( "additional layout with additional variant" ) << QStringLiteral( "us" ) << QStringLiteral( "intl" )
                                                                << QString()
                                                                << QStringList { QStringLiteral( "intl" ), QString() };
    QTest::newRow( "additional layout with no variants" ) << QStringLiteral( "us" ) << QString() << QString()
                                                          << QStringList {};
}

void
KeyboardLayoutTests::testVariantList()
{
    QFETCH( QString, additionalLayout );
    QFETCH( QString, additionalVariant );
    QFETCH( QString, variant );
    QFETCH( QStringList, expected );

    AdditionalLayoutInfo additionalLayoutInfo;
    additionalLayoutInfo.additionalLayout = additionalLayout;
    additionalLayoutInfo.additionalVariant = additionalVariant;

    QCOMPARE( variantList( additionalLayoutInfo, variant ), expected );
}

void
KeyboardLayoutTests::testVConsoleXkbLines_data()
{
    QTest::addColumn< QString >( "model" );
    QTest::addColumn< QStringList >( "layouts" );
    QTest::addColumn< QStringList >( "variants" );
    QTest::addColumn< QString >( "options" );
    QTest::addColumn< QStringList >( "expected" );

    QTest::newRow( "simple layout" ) << QStringLiteral( "pc105" ) << QStringList { QStringLiteral( "de" ) }
                                     << QStringList {} << QString()
                                     << QStringList { QStringLiteral( "XKBLAYOUT=de" ),
                                                      QStringLiteral( "XKBMODEL=pc105" ) };
    QTest::newRow( "additional layout" )
        << QStringLiteral( "pc105" ) << QStringList { QStringLiteral( "us" ), QStringLiteral( "ua" ) }
        << QStringList { QString(), QStringLiteral( "phonetic" ) } << QStringLiteral( "grp:alt_shift_toggle" )
        << QStringList { QStringLiteral( "XKBLAYOUT=us,ua" ),
                         QStringLiteral( "XKBMODEL=pc105" ),
                         QStringLiteral( "XKBVARIANT=,phonetic" ),
                         QStringLiteral( "XKBOPTIONS=grp:alt_shift_toggle" ) };
}

void
KeyboardLayoutTests::testVConsoleXkbLines()
{
    QFETCH( QString, model );
    QFETCH( QStringList, layouts );
    QFETCH( QStringList, variants );
    QFETCH( QString, options );
    QFETCH( QStringList, expected );

    QCOMPARE( vconsoleXkbLines( model, layouts, variants, options ), expected );
}


QTEST_GUILESS_MAIN( KeyboardLayoutTests )

#include "utils/moc-warnings.h"

#include "Tests.moc"
