#include "database.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>

namespace {

// Qt SQL 使用连接名称管理数据库连接；固定名称让应用复用同一个 SQLite 连接。
const QString kConnectionName = QStringLiteral("CoolComicSQLite");

// 当前程序支持的数据库结构版本。结构变化时增加版本号并补充迁移逻辑。
constexpr int kSchemaVersion = 1;

} // namespace

/**
 * @brief 初始化 SQLite 驱动、应用数据目录、数据库文件和表结构。
 *
 * 初始化按顺序检查驱动、确定可执行文件目录、打开数据库并准备表结构。数据库文件
 * 与可执行程序位于同一目录，方便开发阶段直接查看和备份。任一步骤失败都会记录中文
 * 错误信息并返回 false，调用方不得在初始化失败后继续执行数据库操作。
 */
bool DataBase::init()
{
    if (m_isInitialized && m_database.isOpen())
    {
        return true;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE")))
    {
        qCritical() << "SQLite 驱动（QSQLITE）不可用。";
        return false;
    }

    // 本项目约定数据库与可执行程序同处 bin 目录。
    // 必须在 QCoreApplication 创建后调用，Qt 才能正确返回应用程序所在目录。
    const QString dataDirectory = QCoreApplication::applicationDirPath();
    if (dataDirectory.isEmpty() || !QDir().mkpath(dataDirectory))
    {
        qCritical() << "无法创建应用数据目录：" << dataDirectory;
        return false;
    }

    // 如果连接曾被关闭，则重新取得已注册的连接；首次初始化时才创建新连接。
    if (QSqlDatabase::contains(kConnectionName))
    {
        m_database = QSqlDatabase::database(kConnectionName, false);
    }
    else
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                               kConnectionName);
    }

    // 使用应用程序所在目录而非当前工作目录，避免启动位置改变时生成多份数据库。
    m_database.setDatabaseName(
        QDir(dataDirectory).filePath(QStringLiteral("coolcomic.db")));

    if (!m_database.open())
    {
        qCritical() << "无法打开 SQLite 数据库：" << m_database.lastError().text();
        return false;
    }

    if (!createTable())
    {
        m_database.close();
        return false;
    }

    m_isInitialized = true;
    qInfo() << "数据库初始化成功，文件位置：" << m_database.databaseName();
    return true;
}

/**
 * @brief 关闭连接并清除初始化状态。
 *
 * 本函数只关闭数据库连接，不会删除数据库文件或 Qt 中注册的连接名称；后续仍可调用
 * init() 重新打开同一数据库。调用前应结束使用此连接的 SQL 查询。
 */
void DataBase::close()
{
    if (m_database.isValid())
    {
        m_database.close();
    }

    m_isInitialized = false;
}

/**
 * @brief 根据 PRAGMA user_version 创建或检查数据库结构。
 *
 * 新数据库版本为 0。本函数会在同一事务中创建 comics 表并将版本改为 1，确保建表和
 * 版本记录要么一起成功，要么一起回滚。当前版本高于程序支持版本时拒绝继续使用。
 */
bool DataBase::createTable()
{
    QSqlQuery versionQuery(m_database);
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version"))
        || !versionQuery.next())
    {
        qCritical() << "无法读取数据库结构版本："
                    << versionQuery.lastError().text();
        return false;
    }

    const int currentVersion = versionQuery.value(0).toInt();
    versionQuery.finish();

    if (currentVersion > kSchemaVersion)
    {
        qCritical() << "数据库结构版本高于当前程序支持的版本：" << currentVersion;
        return false;
    }

    if (currentVersion == kSchemaVersion)
    {
        return true;
    }

    if (!m_database.transaction())
    {
        qCritical() << "无法开启数据库结构迁移事务："
                    << m_database.lastError().text();
        return false;
    }

    QSqlQuery query(m_database);

    // comics 表只保存漫画元数据和阅读进度；图片文件本身仍保留在目录或 CBZ 中。
    const QString createSql = QStringLiteral(R"SQL(
        CREATE TABLE IF NOT EXISTS comics (
            id TEXT PRIMARY KEY,
            source_type TEXT NOT NULL
                CHECK (source_type IN ('directory', 'cbz')),
            source_path TEXT NOT NULL COLLATE NOCASE,
            title TEXT NOT NULL,
            cover_ref TEXT NOT NULL,
            page_count INTEGER NOT NULL DEFAULT 0
                CHECK (page_count >= 0),
            last_read_page INTEGER NOT NULL DEFAULT 1
                CHECK (last_read_page >= 1),
            added_at INTEGER NOT NULL,
            last_read_at INTEGER,
            UNIQUE (source_type, source_path)
        )
    )SQL");

    if (!query.exec(createSql))
    {
        qCritical() << "创建 comics 表失败：" << query.lastError().text();
        m_database.rollback();
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version = 1")))
    {
        qCritical() << "更新数据库结构版本失败："
                    << query.lastError().text();
        m_database.rollback();
        return false;
    }

    if (!m_database.commit())
    {
        qCritical() << "提交数据库结构迁移事务失败："
                    << m_database.lastError().text();
        m_database.rollback();
        return false;
    }

    return true;
}

/** 构造单例对象；连接延迟到 init() 中创建，避免应用对象尚未创建时查询路径。 */
DataBase::DataBase()
{
}

/** 单例销毁时关闭数据库连接；数据库文件会保留在应用数据目录中。 */
DataBase::~DataBase()
{
    close();
}

/** 返回共享的 Qt SQL 连接句柄，供 DBManager 创建查询对象。 */
QSqlDatabase DataBase::connection() const
{
    return m_database;
}
