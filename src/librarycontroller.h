#ifndef LIBRARYCONTROLLER_H
#define LIBRARYCONTROLLER_H

#include "comiclistmodel.h"

#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

/**
 * @brief QML 漫画库的操作入口。
 *
 * 负责加载、搜索、导入与移除记录，再刷新展示模型。QML 调用本类公开的属性和
 * Q_INVOKABLE 方法；本类协调 ComicImport、DBManager 和 ComicListModel，
 * 不负责图片绘制、目录扫描算法或具体 SQL 的实现。
 *
 * m_records 保存完整书库，m_model 仅保存当前筛选结果。因此搜索不会改变数据库，
 * 首页总数量也不会随搜索改变。当前操作同步执行，所有数据库访问均在 GUI 主线程。
 */
class LibraryController : public QObject
{
    Q_OBJECT
    // 注册到 CoolComic QML 模块，界面可直接声明 LibraryController 对象。
    QML_ELEMENT
    /** 列表模型地址固定，但模型内的记录可以通过模型通知更新。 */
    Q_PROPERTY(ComicListModel* comics READ comics CONSTANT)
    /** 完整书库数量，不受搜索条件和历史筛选影响。 */
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    /** 完整书库中 lastReadAt 大于 0 的记录数，不受搜索影响。 */
    Q_PROPERTY(int historyCount READ historyCount NOTIFY countsChanged)
    /** 按标题进行不区分大小写的包含搜索，空字符串表示不限制标题。 */
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    /** 是否仅显示已阅读漫画；启用时按最近阅读时间倒序排列。 */
    Q_PROPERTY(bool historyOnly READ historyOnly WRITE setHistoryOnly NOTIFY historyOnlyChanged)
    /** 最近一次操作的错误说明；空字符串表示没有错误。 */
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    /** 创建控制器并读取数据库；调用前 DataBase 必须已初始化成功。 */
    explicit LibraryController(QObject* parent = nullptr);
    /** 返回控制器持有的列表模型；调用方不负责删除该对象。 */
    ComicListModel* comics();
    /** 返回完整书库的记录数，用于首页本地数量徽标。 */
    int totalCount() const;
    /** 返回已阅读记录数，用于首页历史数量徽标。 */
    int historyCount() const;
    /** 获取当前搜索文本。 */
    QString searchText() const;
    /** 保存搜索文本并重新筛选内存列表，不写数据库。 */
    void setSearchText(const QString& text);
    /** 获取当前是否处于历史筛选模式。 */
    bool historyOnly() const;
    /** 切换历史筛选模式，并重新计算列表顺序。 */
    void setHistoryOnly(bool enabled);
    /** 获取可供 QML 显示的错误说明。 */
    QString errorMessage() const;

    /**
     * @brief 重新从 DBManager 查询完整书库，并保留现有搜索和历史筛选条件。
     * @return 查询成功返回 true；失败时保留原列表，并更新 errorMessage。
     */
    Q_INVOKABLE bool reload();
    /**
     * @brief 将 FolderDialog 返回的 URL 转为本地路径，交给 ComicImport 导入。
     * @param directory 必须是本地文件 URL，不支持网络地址。
     * @param title 用户输入的标题；为空时由导入服务使用目录名。
     * @return 导入和列表刷新均成功时返回 true；失败原因保存到 errorMessage。
     * @note 若入库成功但刷新失败，记录已经存在，不应再次重复导入，应重试刷新。
     */
    Q_INVOKABLE bool importDirectory(const QUrl& directory, const QString& title);
    /** 接收 FileDialog 的本地 CBZ/ZIP URL，检查并入库，成功后刷新书库。 */
    Q_INVOKABLE bool importArchive(const QUrl& file, const QString& title);
    /**
     * @brief 按 ID 移除漫画记录并刷新列表，不删除来源目录和图片。
     * @return 删除和刷新均成功时返回 true；失败时更新 errorMessage。
     * @note 确认对话框由 QML 显示，本方法不负责交互确认。
     */
    Q_INVOKABLE bool removeComic(const QString& id);
    /** 仅清除界面错误提示，不重试失败操作，也不修改书库数据。 */
    Q_INVOKABLE void clearError();

signals:
    /** 完整书库重新加载完成，通知数量徽标重新读取。 */
    void countsChanged();
    /** 搜索文本改变，通知依赖该属性的 QML 表达式。 */
    void searchTextChanged();
    /** 历史筛选模式改变。 */
    void historyOnlyChanged();
    /** 错误说明改变，用于显示或隐藏错误提示。 */
    void errorMessageChanged();

private:
    /** 从完整书库生成筛选结果，按模式排序后提交给 ComicListModel。 */
    void applyFilter();
    /** 只在错误内容变化时更新属性并发送通知，避免重复通知。 */
    void setErrorMessage(const QString& message);

    /** 提供给 QML 的展示模型；作为成员对象随控制器一起销毁。 */
    ComicListModel m_model;
    /** 最近一次成功从数据库读取的完整书库快照。 */
    QList<ComicRecord> m_records;
    /** 用户输入的搜索原文；筛选时忽略首尾空白。 */
    QString m_searchText;
    /** 最近的界面操作错误，成功刷新后清空。 */
    QString m_errorMessage;
    /** false 表示本地书库模式，true 表示阅读历史模式。 */
    bool m_historyOnly{false};
};

#endif // LIBRARYCONTROLLER_H
