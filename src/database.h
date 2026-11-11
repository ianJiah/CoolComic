#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>

/**
 * @brief SQLite 数据库连接和结构初始化的单例管理类。
 *
 * 本类负责创建应用数据目录、打开唯一的 SQLite 连接、创建数据库表，
 * 并通过 PRAGMA user_version 记录数据库结构版本。漫画数据的具体增删查改
 * 由 DBManager 完成，避免连接管理类同时承担业务数据访问职责。
 *
 * Qt SQL 连接具有线程归属。当前应用应在主线程初始化数据库，并在同一线程
 * 使用 connection() 返回的连接句柄。
 */
class DataBase
{
public:
    /**
     * @brief 获取数据库单例。
     *
     * 函数内静态对象保证程序中只有一个 DataBase 实例，并由 C++ 在程序退出时
     * 自动销毁。拷贝和移动操作已禁用，避免连接状态被意外复制。
     */
    static DataBase& instance()
    {
        static DataBase db;
        return db;
    }

    /**
     * @brief 初始化并打开 SQLite 数据库。
     *
     * 调用时需要已有 QCoreApplication 或 QGuiApplication 实例。数据库默认放在
     * 可执行程序所在目录；函数可重复调用，已成功初始化时直接返回 true。
     * @return 驱动、目录、连接和表结构均准备成功时返回 true，否则返回 false。
     */
    bool init();

    /**
     * @brief 关闭当前数据库连接。
     *
     * 关闭连接但保留 Qt 注册的命名连接，以便稍后再次调用 init() 时重新打开。
     * 调用前应确保没有仍在使用该连接的查询对象。
     */
    void close();

    /**
     * @brief 获取当前 SQLite 连接句柄。
     *
     * 返回 QSqlDatabase 的轻量共享句柄。调用方应先确认 init() 成功，且只能在
     * 创建连接的线程中使用该句柄。
     */
    QSqlDatabase connection() const;

    /** 禁止复制数据库管理器，避免复制单例状态。 */
    DataBase(const DataBase&) = delete;
    DataBase& operator=(const DataBase&) = delete;

    /** 禁止移动数据库管理器，保持连接生命周期固定。 */
    DataBase(DataBase&&) = delete;
    DataBase& operator=(DataBase&&) = delete;

private:
    /**
     * @brief 创建或检查当前程序版本所需的数据表。
     *
     * 首次运行时创建 comics 表并设置 schema 版本；若数据库版本高于当前程序支持的
     * 版本，则拒绝继续初始化，避免旧程序修改新版本数据库。
     */
    bool createTable();

    /** 构造函数私有化，外部只能通过 instance() 获取唯一实例。 */
    DataBase();

    /** 析构时关闭数据库连接；私有化以维持单例生命周期。 */
    ~DataBase();

    /** Qt 注册的 SQLite 数据库连接句柄。 */
    QSqlDatabase m_database;

    /** 仅在连接打开且数据库结构初始化成功后置为 true。 */
    bool m_isInitialized{false};
};

#endif // DATABASE_H
