#include "dbmanager.h"

#include "database.h"

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

/**
 * @brief 将数据库错误写入调用方提供的错误字符串。
 *
 * errorMessage 允许为空，便于只关心成功/失败的调用者使用接口。
 */
void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage != nullptr)
    {
        *errorMessage = message;
    }
}

/**
 * @brief 将当前查询行转换为 ComicRecord。
 *
 * 查询语句必须包含 comics 表的全部列。SQLite 中 NULL 的 last_read_at
 * 转换为 0，以匹配 ComicRecord 对“尚未阅读”的约定。
 */
ComicRecord readComicRecord(const QSqlQuery& query)
{
    ComicRecord comic;
    comic.id = query.value(QStringLiteral("id")).toString();
    comic.sourceType = query.value(QStringLiteral("source_type")).toString();
    comic.sourcePath = query.value(QStringLiteral("source_path")).toString();
    comic.title = query.value(QStringLiteral("title")).toString();
    comic.coverRef = query.value(QStringLiteral("cover_ref")).toString();
    comic.pageCount = query.value(QStringLiteral("page_count")).toInt();
    comic.lastReadPage = query.value(QStringLiteral("last_read_page")).toInt();
    comic.addedAt = query.value(QStringLiteral("added_at")).toLongLong();
    comic.lastReadAt = query.value(QStringLiteral("last_read_at")).isNull()
                           ? 0
                           : query.value(QStringLiteral("last_read_at")).toLongLong();
    return comic;
}

/**
 * @brief 获取已经初始化并打开的数据库连接。
 *
 * 所有 DBManager 操作都通过 DataBase 单例取得同一命名连接，避免各自
 * 创建连接导致连接名冲突、连接生命周期不一致或操作不同数据库文件。
 */
QSqlDatabase getOpenDatabase(QString* errorMessage)
{
    QSqlDatabase database = DataBase::instance().connection();
    if (!database.isValid() || !database.isOpen())
    {
        setError(errorMessage, QStringLiteral("数据库尚未初始化或连接已关闭。"));
        return {};
    }

    return database;
}

/**
 * @brief 为 SQL 语句绑定可空的最近阅读时间。
 *
 * 0 表示从未阅读，应写入 SQL NULL，这样历史查询可以直接使用
 * last_read_at IS NOT NULL 过滤。
 */
QVariant lastReadAtValue(qint64 lastReadAt)
{
    if (lastReadAt <= 0)
    {
        return QVariant();
    }

    return QVariant::fromValue(lastReadAt);
}

/**
 * @brief 将漫画来源路径转换为稳定的绝对路径。
 *
 * 已存在的目录或文件优先使用 canonicalFilePath 消除相对路径和 .、.. 等差异；
 * 尚不存在的路径退回到绝对路径并清理冗余分隔符。数据库字段使用 NOCASE 排序规则，
 * 因此 Windows 路径大小写差异也不会绕过来源唯一约束。
 */
QString normalizedSourcePath(const QString& sourcePath)
{
    if (sourcePath.trimmed().isEmpty())
    {
        return {};
    }

    const QFileInfo fileInfo(sourcePath);
    QString path = fileInfo.canonicalFilePath();
    if (path.isEmpty())
    {
        path = fileInfo.absoluteFilePath();
    }

    return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

} // namespace

bool DBManager::addComic(const ComicRecord& comic, QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return false;
    }

    const QString sourcePath = normalizedSourcePath(comic.sourcePath);
    if (sourcePath.isEmpty())
    {
        setError(errorMessage, QStringLiteral("漫画来源路径不能为空。"));
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO comics ("
        "id, source_type, source_path, title, cover_ref, page_count, "
        "last_read_page, added_at, last_read_at"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(comic.id);
    query.addBindValue(comic.sourceType);
    query.addBindValue(sourcePath);
    query.addBindValue(comic.title);
    query.addBindValue(comic.coverRef);
    query.addBindValue(comic.pageCount);
    query.addBindValue(comic.lastReadPage);
    query.addBindValue(comic.addedAt > 0
                           ? comic.addedAt
                           : QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(lastReadAtValue(comic.lastReadAt));

    if (!query.exec())
    {
        setError(errorMessage,
                 QStringLiteral("新增漫画记录失败：%1").arg(query.lastError().text()));
        return false;
    }

    return true;
}

bool DBManager::getComic(const QString& id,
                         ComicRecord& comic,
                         QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT * FROM comics WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec())
    {
        setError(errorMessage,
                 QStringLiteral("查询漫画记录失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (!query.next())
    {
        setError(errorMessage, QStringLiteral("没有找到 ID 为 %1 的漫画记录。").arg(id));
        return false;
    }

    comic = readComicRecord(query);
    return true;
}

QList<ComicRecord> DBManager::getAllComics(QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return {};
    }

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT * FROM comics ORDER BY added_at DESC")))
    {
        setError(errorMessage,
                 QStringLiteral("查询漫画列表失败：%1").arg(query.lastError().text()));
        return {};
    }

    QList<ComicRecord> comics;
    while (query.next())
    {
        comics.append(readComicRecord(query));
    }

    return comics;
}

QList<ComicRecord> DBManager::getReadingHistory(int limit,
                                                QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (limit <= 0)
    {
        setError(errorMessage, QStringLiteral("历史记录数量必须大于 0。"));
        return {};
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return {};
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT * FROM comics "
        "WHERE last_read_at IS NOT NULL "
        "ORDER BY last_read_at DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec())
    {
        setError(errorMessage,
                 QStringLiteral("查询阅读历史失败：%1").arg(query.lastError().text()));
        return {};
    }

    QList<ComicRecord> comics;
    while (query.next())
    {
        comics.append(readComicRecord(query));
    }

    return comics;
}

bool DBManager::updateComic(const ComicRecord& comic, QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return false;
    }

    const QString sourcePath = normalizedSourcePath(comic.sourcePath);
    if (sourcePath.isEmpty())
    {
        setError(errorMessage, QStringLiteral("漫画来源路径不能为空。"));
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE comics SET "
        "source_type = ?, source_path = ?, title = ?, cover_ref = ?, page_count = ? "
        "WHERE id = ?"));
    query.addBindValue(comic.sourceType);
    query.addBindValue(sourcePath);
    query.addBindValue(comic.title);
    query.addBindValue(comic.coverRef);
    query.addBindValue(comic.pageCount);
    query.addBindValue(comic.id);

    if (!query.exec())
    {
        setError(errorMessage,
                 QStringLiteral("更新漫画信息失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0)
    {
        setError(errorMessage,
                 QStringLiteral("没有找到 ID 为 %1 的漫画记录。更新未执行。").arg(comic.id));
        return false;
    }

    return true;
}

bool DBManager::updateReadingProgress(const QString& id,
                                      int page,
                                      int pageCount,
                                      QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    if (pageCount <= 0 || page < 1 || page > pageCount)
    {
        setError(errorMessage, QStringLiteral("阅读页码必须在 1 到总页数之间。"));
        return false;
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE comics SET last_read_page = ?, page_count = ?, last_read_at = ? "
        "WHERE id = ?"));
    query.addBindValue(page);
    query.addBindValue(pageCount);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(id);

    if (!query.exec())
    {
        setError(errorMessage,
                 QStringLiteral("保存阅读进度失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0)
    {
        setError(errorMessage,
                 QStringLiteral("没有找到 ID 为 %1 的漫画记录。进度未保存。").arg(id));
        return false;
    }

    return true;
}

bool DBManager::removeComic(const QString& id, QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QSqlDatabase database = getOpenDatabase(errorMessage);
    if (!database.isValid())
    {
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("DELETE FROM comics WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec())
    {
        setError(errorMessage,
                 QStringLiteral("删除漫画记录失败：%1").arg(query.lastError().text()));
        return false;
    }

    if (query.numRowsAffected() == 0)
    {
        setError(errorMessage,
                 QStringLiteral("没有找到 ID 为 %1 的漫画记录。删除未执行。").arg(id));
        return false;
    }

    return true;
}
