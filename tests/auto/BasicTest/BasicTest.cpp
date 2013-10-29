/****************************************************************************
**
** Copyright (C) 2013, HicknHack Software
** All rights reserved.
** Contact: http://www.hicknhack-software.com/contact
**
** This file is part of the QSqlMigrator
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
****************************************************************************/
#include "BasicTest.h"

#include "CommandExecution/CommandExecutionContext.h"
#include "CommandExecution/CommandExecutionService.h"

#include "MigrationExecution/LocalSchemeMigrationExecutionContext.h"
#include "MigrationExecution/LocalSchemeMigrationExecutionService.h"
#include "LocalSchemeMigrator/LocalSchemeMigrator.h"
#include "LocalSchemeMigrator/LocalSchemeComparisonContext.h"
#include "LocalSchemeMigrator/LocalSchemeComparisonService.h"

#include <QScopedPointer>
#include <QSqlQuery>
#include <QSqlError>

using namespace Structure;
using namespace Migrations;
using namespace MigrationExecution;

BasicTest::BasicTest(const QString &driverName, const QString &testDatabaseName
                     , bool (*buildContext)(MigrationExecution::MigrationExecutionContext &, QSqlDatabase)
              , const QString &structureDatabase, const QString &hostName
            , const int hostPort, const QString &userName, const QString &password)
    : m_driverName(driverName)
    , m_testDatabaseName(testDatabaseName)
    , m_buildContext(buildContext)
    , m_structureDatabase(structureDatabase)
    , m_hostName(hostName)
    , m_hostPort(hostPort)
    , m_userName(userName)
    , m_password(password)
    , m_context(QMap<QString, const Migration*>())
{
}

/* Important note: it's not possible to delete a database while connected to it in some DBMS
 * so to delete a database one needs to be connected to another meanwhile.
 * Database POSTGRESQL_STRUCTURE_DATABASE is used for this purpose. */

void BasicTest::initTestCase()
{
    const QString applicationPath = QCoreApplication::applicationDirPath();
    const QString absoluteApplicationPath = QDir(applicationPath).absolutePath();
    QCoreApplication::addLibraryPath( absoluteApplicationPath ); // wichtig damit die Treiber gefunden werden

    m_structure_database = QSqlDatabase::addDatabase(m_driverName);
    m_structure_database.setHostName(m_hostName);
    m_structure_database.setPort(m_hostPort);
    m_structure_database.setUserName(m_userName);
    m_structure_database.setPassword(m_password);
    m_structure_database.setDatabaseName(m_structureDatabase);

    m_structure_database.database().open();
    QSqlQuery query;
    if (!query.exec(QString("DROP DATABASE IF EXISTS %1").arg(m_testDatabaseName))) {
        ::qDebug() << query.lastError();
    }
    m_structure_database.database().close();
}

void BasicTest::cleanupTestCase()
{
    if (m_context.database().isOpen()) {
        m_context.database().close();
    }

    m_structure_database.database().open();
    QSqlQuery query;
    if (!query.exec(QString("DROP DATABASE IF EXISTS %1").arg(m_testDatabaseName))) {
         ::qDebug() << query.lastError();
    }
    m_structure_database.database().close();
}

void BasicTest::init()
{
    ::qDebug() << "running test for" << m_structureDatabase;

    m_structure_database.database().open();
    QSqlQuery query;
    if (!query.exec(QString("CREATE DATABASE %1").arg(m_testDatabaseName))) {
        ::qDebug() << query.lastError();
    }
    m_structure_database.database().close();

    QSqlDatabase database;
    if(!QSqlDatabase::contains("context_connection")) {
        database = QSqlDatabase::addDatabase(m_driverName, "context_connection");
        database.setHostName(m_hostName);
        database.setPort(m_hostPort);
        database.setUserName(m_userName);
        database.setPassword(m_password);
        database.setDatabaseName(m_testDatabaseName);
    }
    else
        database =  m_context.database();

    bool buildContextSuccess = m_buildContext(m_context, database);
    QVERIFY2(buildContextSuccess, "context should correctly builded");
}

void BasicTest::cleanup()
{
    if (m_context.database().isOpen()) {
        m_context.database().close();
    }
    m_structure_database.database().open();
    QSqlQuery query;
    if (!query.exec(QString("DROP DATABASE IF EXISTS %1").arg(m_testDatabaseName))) {
         ::qDebug() << query.lastError();
    }
    m_structure_database.database().close();
}

void BasicTest::testCreateTable()
{
    Commands::CommandPtr command(
                new Commands::CreateTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    ));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");
}

void BasicTest::testDropTable()
{
    Commands::CommandPtr command(
                new Commands::CreateTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    ));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    Commands::CommandPtr command2(
                new Commands::DropTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    ));

    execution.execute(command2, m_context.commandServiceRepository(), serviceContext);

    tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(!tables.contains("testtable1"), "testtable should be droped during migration!");
}

void BasicTest::testTransaction()
{
    if (!m_context.baseMigrationTableService()->canRevertStrucuturalChangesUsingTransactions()) {
        QSKIP("database driver does not support transactions for structural changes, SKIPPING TEST!"
              , SkipSingle);
    }

    Migration m;
    m.add(new Commands::CreateTable(
              Table("testtable1")
              .add(Column("ID", QVariant::Int, Column::Primary))
              .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
              ));

    Migration m2;
    m2.add(new Commands::CreateTable(
               Table("testtable2")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    m2.add(new Commands::CreateTable(
               Table("testtable1")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    MigrationExecutionService migrator;
    QMap<QString, const Migration*> migrationMap;
    migrationMap["Migration No1"] = &m;
    migrationMap["Migration No2"] = &m2;
    QScopedPointer<MigrationExecutionConfig> migrationConfig(new MigrationExecutionConfig);
    MigrationExecutionContext migrationContext(migrationMap, *migrationConfig);
    migrationContext.setDatabase(m_context.database());
    migrationContext.setBaseMigrationTableService(m_context.baseMigrationTableService());
    migrationContext.setCommandServiceRepository(m_context.commandServiceRepository());
    migrationContext.setHelperRepository(m_context.helperRepository());

    bool success = migrator.execute("Migration No1", migrationContext);
    QVERIFY2(success, "migration should work!");

    // migration 'Migration No2' should fail, because table already exists
    QTest::ignoreMessage(QtWarningMsg, "table allready exists! ");
    success = migrator.execute("Migration No2", migrationContext);
    QVERIFY2(!success, "migration should fail!");
    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1")
             , "testtable should be created during migrtaion No 1!");
    QVERIFY2(!tables.contains("testtable2")
             , "testtable should be deleted during rollback!");
}

void BasicTest::testUndoCreateTable()
{
    if (m_context.baseMigrationTableService()->canRevertStrucuturalChangesUsingTransactions()) {
        QSKIP("database driver should support transactions for structural changes, SKIPPING TEST!"
              , SkipSingle);
    }

    Migration m;
    m.add(new Commands::CreateTable(
              Table("testtable1")
              .add(Column("ID", QVariant::Int, Column::Primary))
              .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
              ));

    Migration m2;
    m2.add(new Commands::CreateTable(
               Table("testtable2")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    m2.add(new Commands::CreateTable(
               Table("testtable1")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    //QTest::ignoreMessage();
    MigrationExecutionService migrator;
    QMap<QString, const Migration*> migrationMap;
    migrationMap["Migration No1"] = &m;
    migrationMap["Migration No2"] = &m2;
    MigrationExecutionConfig *migrationConfig = new MigrationExecutionConfig;
    MigrationExecutionContext migrationContext(migrationMap, *migrationConfig);
    migrationContext.setDatabase(m_context.database());
    migrationContext.setBaseMigrationTableService(m_context.baseMigrationTableService());
    migrationContext.setCommandServiceRepository(m_context.commandServiceRepository());
    migrationContext.setHelperRepository(m_context.helperRepository());

    bool success = migrator.execute("Migration No1", migrationContext);
    QVERIFY2(success, "migration should work!");

    // migration 'Migration No2' should fail, because table 'testtable1' already exists
    QTest::ignoreMessage(QtWarningMsg, "table allready exists! ");
    success = migrator.execute("Migration No2", migrationContext);
    QVERIFY2(!success, "migration should fail!");
    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1")
             , "testtable should be created during migrtaion No 1!");
    QVERIFY2(!tables.contains("testtable2")
             , "testtable should be deleted during rollback!");
}

void BasicTest::testUndoDropTable()
{
    if (m_context.baseMigrationTableService()->canRevertStrucuturalChangesUsingTransactions()) {
        QSKIP("database driver should support transactions for structural changes, SKIPPING TEST!"
              , SkipSingle);
    }

    //TODO run dropTable with tableName only to test TableRecovery in DropTable-Command

    Table testtable1 = Table("testtable1")
            .add(Column("ID", QVariant::Int, Column::Primary))
            .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable));

    Migration m;
    m.add(new Commands::CreateTable(testtable1));

    //TODO add some data to the table to check if undo will reinsert the data..

    Migration m2;
    m2.add(new Commands::DropTable(testtable1));

    m2.add(new Commands::CreateTable(
               Table("testtable2")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    m2.add(new Commands::CreateTable(
               Table("testtable2")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    MigrationExecutionService migrator;
    QMap<QString, const Migration*> migrationMap;
    migrationMap["Migration No1"] = &m;
    migrationMap["Migration No2"] = &m2;
    QScopedPointer<MigrationExecutionConfig> migrationConfig(new MigrationExecutionConfig);
    MigrationExecutionContext migrationContext(migrationMap, *migrationConfig);
    migrationContext.setDatabase(m_context.database());
    migrationContext.setBaseMigrationTableService(m_context.baseMigrationTableService());
    migrationContext.setCommandServiceRepository(m_context.commandServiceRepository());
    migrationContext.setHelperRepository(m_context.helperRepository());

    bool success = migrator.execute("Migration No1", migrationContext);
    QVERIFY2(success, "migration should work!");

    // migration 'Migration No2' should fail, because table 'testtable2' will be created twice
    QTest::ignoreMessage(QtWarningMsg, "table allready exists! ");
    success = migrator.execute("Migration No2", migrationContext);
    QVERIFY2(!success, "migration should fail!");
    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1")
             , "testtable should be re-created during undo!");
    QVERIFY2(!tables.contains("testtable2")
             , "testtable should be deleted during rollback!");
}

void BasicTest::testMigrationDirections()
{
    Migration m;
    m.add(new Commands::CreateTable(
              Table("testtable1")
              .add(Column("ID", QVariant::Int, Column::Primary))
              .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
              ));

    Migration m2;
    m2.add(new Commands::CreateTable(
               Table("testtable2")
               .add(Column("ID", QVariant::Int, Column::Primary))
               .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
               ));

    MigrationExecutionService migrator;
    QMap<QString, const Migration*> migrationMap;
    migrationMap["Migration No1"] = &m;
    migrationMap["Migration No2"] = &m2;
    QScopedPointer<MigrationExecutionConfig> migrationConfig(new MigrationExecutionConfig);
    MigrationExecutionContext migrationContext(migrationMap, *migrationConfig);
    migrationContext.setDatabase(m_context.database());
    migrationContext.setBaseMigrationTableService(m_context.baseMigrationTableService());
    migrationContext.setCommandServiceRepository(m_context.commandServiceRepository());
    migrationContext.setHelperRepository(m_context.helperRepository());

    bool success = migrator.execute("Migration No1", migrationContext);
    QVERIFY2(success, "Migration should work!");
    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    success = migrator.execute("Migration No2", migrationContext);
    QVERIFY2(success, "Migration should work!");
    tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable2"), "testtable should be created during migration!");

    success = migrator.execute("Migration No2", migrationContext, MigrationExecution::MigrationExecutionService::Down);
    QVERIFY2(success, "Migration should work!");
    tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(!tables.contains("testtable2"), "testtable2 should be deleted during migration!");
}

void BasicTest::testDropTableRevert()
{
    Migration m;
    Table table1 = Table("testtable1")
            .add(Column("ID", QVariant::Int, Column::Primary))
            .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable));
    m.add(new Commands::CreateTable(table1));

    Migration m2;
    m2.add(new Commands::DropTable("testtable1"));
    m2.add(new Commands::DropTable(Table(table1)));    //second DropTable should fail

    MigrationExecutionService migrator;
    QMap<QString, const Migration*> migrationMap;
    migrationMap["Migration No1"] = &m;
    migrationMap["Migration No2"] = &m2;
    QScopedPointer<MigrationExecutionConfig> migrationConfig(new MigrationExecutionConfig);
    MigrationExecutionContext migrationContext(migrationMap, *migrationConfig);
    migrationContext.setDatabase(m_context.database());
    migrationContext.setBaseMigrationTableService(m_context.baseMigrationTableService());
    migrationContext.setCommandServiceRepository(m_context.commandServiceRepository());
    migrationContext.setHelperRepository(m_context.helperRepository());

    bool success = migrator.execute("Migration No1", migrationContext);
    QVERIFY2(success, "migration should work!");

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    // migration 'Migration No2' should fail, because table 'testtable1' no longer exists
    QTest::ignoreMessage(QtWarningMsg, "table doesn't exist! ");
    success = migrator.execute("Migration No2", migrationContext);
    QVERIFY2(!success, "migration should fail!");

    tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be recreated during rollback!");
}

void BasicTest::testAlterColumnType()
{
    Commands::CommandPtr command(
                new Commands::CreateTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    .add(Column("col1", SqlType(QVariant::String, 23)))
                    .add(Column("col2", SqlType(QVariant::String, 23)))
                    ));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    //TODO insert some data

    //TODO: handle case sensitivity of types and column names
    Commands::CommandPtr command2(new Commands::AlterColumnType("col1", "testtable1", "varchar(42)"));
    execution.execute(command2, m_context.commandServiceRepository(), serviceContext);

    //check if old column was removed and new column included successfully
    Structure::Table table = m_context.helperRepository().dbReaderService->getTableDefinition("testtable1", m_context.database());
    Structure::Column col1;
    bool success;
    col1 = table.fetchColumnByName("col1", success);
    QVERIFY2(success, "column col1 should exist");
    QVERIFY2(col1.sqlTypeString() == m_context.helperRepository().typeMapperService->map(SqlType(QVariant::String, 42)), "column col1 should be retyped to varchar(42) during migration");

    Commands::CommandPtr command3(new Commands::AlterColumnType("col1", "testtable1", SqlType(QVariant::String, 43)));
    execution.execute(command3, m_context.commandServiceRepository(), serviceContext);

    //check if old column was removed and new column included successfully
    table = m_context.helperRepository().dbReaderService->getTableDefinition("testtable1", m_context.database());
    col1 = table.fetchColumnByName("col1", success);
    QVERIFY2(success, "column col1 should exist");
    QVERIFY2(col1.sqlTypeString() == m_context.helperRepository().typeMapperService->map(SqlType(QVariant::String, 43)), "column col1 should be retyped to varchar(43) during migration");

    //TODO check if test data was copied correctly
}

void BasicTest::testColumnType()
{
    Table testtable =
            Table("testtable")
            .add(Column("bool_",                QVariant::Bool))
            .add(Column("integer_",             QVariant::Int))
            .add(Column("biginteger_",          QVariant::LongLong))
            .add(Column("double_",              QVariant::Double))
            .add(Column("decimal_",    SqlType(QVariant::Double, 10, 5)))
            .add(Column("date_",                QVariant::Date))
            .add(Column("time_",                QVariant::Time))
            .add(Column("datetime_",            QVariant::DateTime))
            .add(Column("char_1",               QVariant::Char))
            .add(Column("char_",       SqlType(QVariant::Char, 5)))
            .add(Column("varchar_",    SqlType(QVariant::String, 5)))
            .add(Column("blob_",                QVariant::ByteArray));
    Commands::CommandPtr command(new Commands::CreateTable(testtable));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains(testtable.name()), "table should be created during migration!");

    Structure::Table table = m_context.helperRepository().dbReaderService->getTableDefinition(testtable.name(), m_context.database());

    foreach(Structure::Column column, testtable.columns()) {
        bool success;
        Structure::Column col = table.fetchColumnByName(column.name(), success);
        QVERIFY2(success, "column should exist");
        QVERIFY2(m_context.helperRepository().typeMapperService->map(column.sqlType()) == col.sqlTypeString(), "wrong type");
    }

}

void BasicTest::testCreateIndex()
{
    Commands::CommandPtr command(
                new Commands::CreateTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    .add(Column("col1", SqlType(QVariant::String, 23)))
                    .add(Column("col2", SqlType(QVariant::String, 23)))
                    ));

    QSharedPointer<Index> index;
    if (m_driverName != "QMYSQL") {
        index.reset(new Index("index1", "testtable1"));
        index->addColumn("name", Index::Ascending);
        index->addColumn("col2", Index::Descending);
        index->addColumn("col1");
    } else {
        index.reset(new Index("index1", "testtable1"));
        index->addColumn("name", Index::Ascending);
        index->addColumn("col2", Index::Ascending); // MySQL doesn't implement Descending order for indexes
        index->addColumn("col1");
    }
    Commands::CommandPtr command2(new Commands::CreateIndex(*index));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);
    execution.execute(command2, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    //check if index was created successfully
    Structure::Index realIndex = m_context.helperRepository().dbReaderService->getIndexDefinition("index1", "testtable1", m_context.database());
    bool indexPresent = realIndex.columns() == index->columns();
    if (!indexPresent) {
            qDebug() << "local scheme index:" << m_context.helperRepository().columnService->generateIndexColumnDefinitionSql(index->columns());
            qDebug() << "real index:" << m_context.helperRepository().columnService->generateIndexColumnDefinitionSql(realIndex.columns());
    }
    QVERIFY2(indexPresent, "real and local scheme index differ");
}

void BasicTest::testDropColumn()
{
    Commands::CommandPtr command(
                new Commands::CreateTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    .add(Column("col1", SqlType(QVariant::String, 23)))
                    .add(Column("col2", SqlType(QVariant::String, 23)))
                    ));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    //TODO insert some data

    Commands::CommandPtr command2(
                new Commands::DropColumn("col1", "testtable1"));
    execution.execute(command2, m_context.commandServiceRepository(), serviceContext);

    //check if column was dropped successfully
    bool columnRemoved;
    m_context.helperRepository().dbReaderService->getTableDefinition("testtable1", m_context.database()).fetchColumnByName("col1", columnRemoved);
    QVERIFY2(!columnRemoved, "col1 should be removed during migration");
}

void BasicTest::testRenameColumn()
{
    Commands::CommandPtr command(
                new Commands::CreateTable(
                    Table("testtable1")
                    .add(Column("ID", QVariant::Int, Column::Primary))
                    .add(Column("name", SqlType(QVariant::String, 23), Column::NotNullable))
                    .add(Column("col1", SqlType(QVariant::String, 23)))
                    .add(Column("col2", SqlType(QVariant::String, 23)))
                    ));

    CommandExecution::CommandExecutionContext serviceContext(m_context.database(), m_context.migrationConfig(), m_context.helperRepository());
    CommandExecution::CommandExecutionService execution;
    execution.execute(command, m_context.commandServiceRepository(), serviceContext);

    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    //TODO insert some data

    Commands::CommandPtr command2(
                new Commands::RenameColumn("col1", "new_column1", "testtable1"));
    execution.execute(command2, m_context.commandServiceRepository(), serviceContext);

    //check if old column was removed and new column included successfully
    Structure::Table table = m_context.helperRepository().dbReaderService->getTableDefinition("testtable1", m_context.database());
    bool columnRenamed;
    Structure::Column column = table.fetchColumnByName("col1", columnRenamed);
    QVERIFY2(!columnRenamed, "col1 should be removed during migration");
    column = table.fetchColumnByName("new_column1", columnRenamed);
    QVERIFY2(columnRenamed, "col1 should be renamed to new_colum1 during migration");

    //TODO check if test data was copied correctly
}

void BasicTest::testLocalSchemeMigration()
{
    // migrations
    Migration m;
    m.add(new Commands::CreateTable(
              Table("testtable1")
              .add(Column("id1", QVariant::Int, Column::Primary | Column::AutoIncrement))
              .add(Column("name1", SqlType(QVariant::String, 23), Column::Unique))
              .add(Column("weight1", QVariant::Double))
              ));

    Migration m2;
    m2.add(new Commands::CreateTable(
               Table("testtable2")
               .add(Column("id2", QVariant::Int, Column::Primary | Column::AutoIncrement))
               .add(Column("name2", SqlType(QVariant::String, 23), Column::Unique))
               .add(Column("weight2", QVariant::Double))
               ));
    m2.add(new Commands::CreateIndex(
               Index("index1", "testtable1")
               .addColumn("name1")
               .addColumn("weight1")
               ));

    const QString migrationNo1 = "Migration No1";
    const QString migrationNo2 = "Migration No2";
    QMap<QString, const Migration*> migrationMap;
    migrationMap[migrationNo1] = &m;
    migrationMap[migrationNo2] = &m2;

    bool success;

    // execute migrations on real database
    MigrationExecutionService migrator;
    QScopedPointer<MigrationExecutionConfig> migrationConfig(new MigrationExecutionConfig);
    MigrationExecutionContext migrationContext(migrationMap, *migrationConfig);
    migrationContext.setDatabase(m_context.database());
    migrationContext.setBaseMigrationTableService(m_context.baseMigrationTableService());
    migrationContext.setCommandServiceRepository(m_context.commandServiceRepository());
    migrationContext.setHelperRepository(m_context.helperRepository());

    success = migrator.execute(migrationNo1, migrationContext);
    QVERIFY2(success, "Migration should work!");
    QStringList tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable1"), "testtable should be created during migration!");

    success = migrator.execute(migrationNo2, migrationContext);
    QVERIFY2(success, "Migration should work!");
    tables = m_context.database().tables(QSql::Tables);
    QVERIFY2(tables.contains("testtable2"), "testtable should be created during migration!");

    // execute migrations on local scheme
    MigrationExecution::LocalSchemeMigrationExecutionService localSchemeMigrator;
    LocalSchemePtr localScheme(new Structure::LocalScheme); // database equivalent
    MigrationExecution::LocalSchemeMigrationExecutionContext localSchemeMigrationContext(migrationMap);
    localSchemeMigrationContext.setLocalScheme(localScheme);
    localSchemeMigrationContext.setLocalSchemeCommandServiceRepository(LocalSchemeMigrator::createCommandServiceRepository());

    success = localSchemeMigrator.execute(migrationNo1, localSchemeMigrationContext);
    QVERIFY2(success, "Migration should work!");
    success = localSchemeMigrator.execute(migrationNo2, localSchemeMigrationContext);
    QVERIFY2(success, "Migration should work!");

    // compare local scheme with database
    LocalSchemeMigrator::LocalSchemeComparisonContext comparisonContext;
    comparisonContext.setDatabase(m_context.database());
    comparisonContext.setHelperRepository(m_context.helperRepository());
    comparisonContext.setLocalScheme(localScheme);
    LocalSchemeMigrator::LocalSchemeComparisonService comparisonService;
    success = comparisonService.compareLocalSchemeWithDatabase(comparisonContext);
    QVERIFY2(success, "local scheme should be identical to actual database scheme");
}
