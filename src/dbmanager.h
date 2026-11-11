#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QList>
#include <QString>

/**
 * @brief 漫画记录在应用层使用的数据结构。
 *
 * 该结构对应 comics 表中的一行，但不持有 SQL 查询或数据库连接。
 * 时间字段使用 Unix 毫秒时间戳；lastReadAt 为 0 表示漫画尚未阅读。
 * coverRef 对目录漫画保存图片相对路径，对 CBZ 漫画保存压缩包内条目路径。
 */
struct ComicRecord
{
    /** 漫画记录的稳定唯一标识，建议由 QUuid 生成。 */
    QString id;

    /** 漫画来源类型，只允许 directory 或 cbz。 */
    QString sourceType;

    /** 漫画来源的绝对路径，保存前会规范化并用于来源去重。 */
    QString sourcePath;

    /** 漫画标题，通常默认为目录名或 CBZ 文件名。 */
    QString title;

    /** 封面引用：目录漫画为图片相对路径，CBZ 漫画为压缩包内条目路径。 */
    QString coverRef;

    /** 当前扫描到的图片总页数，尚未扫描时为 0。 */
    int pageCount{0};

    /** 上次阅读页码，从 1 开始；从未阅读时默认为第 1 页。 */
    int lastReadPage{1};

    /** 首次导入时间，使用 Unix 毫秒时间戳；新增时为 0 则自动填充当前时间。 */
    qint64 addedAt{0};

    /** 最近阅读时间，使用 Unix 毫秒时间戳；0 表示尚未阅读。 */
    qint64 lastReadAt{0};
};

/**
 * @brief 漫画数据库仓库，封装 comics 表的增删查改操作。
 *
 * DBManager 不拥有数据库连接，而是使用 DataBase 单例已经打开的连接。
 * 因此应先成功调用 DataBase::instance().init()，再调用本类的数据库操作。
 * 本类方法应在创建数据库连接的同一个线程中调用。
 */
class DBManager
{
public:
    DBManager() = default;

    /**
     * @brief 新增一条漫画记录。
     * @param comic 待写入的漫画数据。addedAt 为 0 时自动填入当前时间。
     * @param errorMessage 可选错误输出；成功时清空，失败时写入原因。
     * @return 写入成功返回 true；违反字段约束、来源重复或 SQL 执行失败返回 false。
     */
    bool addComic(const ComicRecord& comic, QString* errorMessage = nullptr) const;

    /**
     * @brief 按漫画 ID 查询一条记录。
     * @param id 漫画的稳定唯一标识。
     * @param comic 查询成功时用于接收结果。
     * @param errorMessage 可选错误输出；未找到记录时也会写入说明。
     * @return 找到记录并成功读取时返回 true。
     */
    bool getComic(const QString& id,
                  ComicRecord& comic,
                  QString* errorMessage = nullptr) const;

    /**
     * @brief 查询漫画库中的全部记录，按导入时间倒序排列。
     * @param errorMessage 可选错误输出；成功时清空，失败时写入原因。
     * @return 查询结果。失败时返回空列表，并通过 errorMessage 提供错误原因。
     */
    QList<ComicRecord> getAllComics(QString* errorMessage = nullptr) const;

    /**
     * @brief 查询已阅读过的漫画，按最近阅读时间倒序排列。
     * @param limit 最多返回的记录数，必须大于 0。
     * @param errorMessage 可选错误输出；成功时清空，失败时写入原因。
     * @return 阅读历史列表。失败时返回空列表。
     */
    QList<ComicRecord> getReadingHistory(int limit = 100,
                                         QString* errorMessage = nullptr) const;

    /**
     * @brief 更新漫画标题、来源、封面及页数等元数据。
     * @param comic 使用 comic.id 定位记录，并写入其余元数据字段。
     * 阅读进度字段不会被本操作覆盖。
     * @param errorMessage 可选错误输出；成功时清空，失败时写入原因。
     * @return 找到记录并更新成功时返回 true。
     */
    bool updateComic(const ComicRecord& comic,
                     QString* errorMessage = nullptr) const;

    /**
     * @brief 更新阅读页码、扫描页数和最近阅读时间。
     * @param id 漫画的稳定唯一标识。
     * @param page 当前页码，从 1 开始。
     * @param pageCount 当前扫描得到的总页数。
     * @param errorMessage 可选错误输出；成功时清空，失败时写入原因。
     * @return 找到记录并更新成功时返回 true。
     */
    bool updateReadingProgress(const QString& id,
                               int page,
                               int pageCount,
                               QString* errorMessage = nullptr) const;

    /**
     * @brief 从书库中删除指定漫画记录。
     * @note 本操作只删除数据库记录，不会删除漫画目录或 CBZ 原文件。
     * @param id 漫画的稳定唯一标识。
     * @param errorMessage 可选错误输出；成功时清空，失败时写入原因。
     * @return 找到记录并删除成功时返回 true。
     */
    bool removeComic(const QString& id,
                     QString* errorMessage = nullptr) const;
};

#endif // DBMANAGER_H
