#include "librarycontroller.h"

#include "comicimport.h"

#include <algorithm>

LibraryController::LibraryController(QObject* parent)
    : QObject(parent)
{
    // QML 创建控制器时立即加载持久化记录，重启应用后无需重新导入漫画。
    reload();
}

ComicListModel* LibraryController::comics()
{
    return &m_model;
}

int LibraryController::totalCount() const
{
    return static_cast<int>(m_records.size());
}

int LibraryController::historyCount() const
{
    // 使用完整记录计数，避免搜索后首页历史徽标随搜索结果缩小。
    return static_cast<int>(std::count_if(m_records.cbegin(), m_records.cend(),
                                        [](const ComicRecord& comic)
    {
        return comic.lastReadAt > 0;
    }));
}

QString LibraryController::searchText() const
{
    return m_searchText;
}

void LibraryController::setSearchText(const QString& text)
{
    if (m_searchText == text)
    {
        return;
    }
    m_searchText = text;
    applyFilter();
    emit searchTextChanged();
}

bool LibraryController::historyOnly() const
{
    return m_historyOnly;
}

void LibraryController::setHistoryOnly(bool enabled)
{
    if (m_historyOnly == enabled)
    {
        return;
    }
    m_historyOnly = enabled;
    applyFilter();
    emit historyOnlyChanged();
}

QString LibraryController::errorMessage() const
{
    return m_errorMessage;
}

bool LibraryController::reload()
{
    QString error;
    const QList<ComicRecord> records = DBManager().getAllComics(&error);
    setErrorMessage(error);
    if (!error.isEmpty())
    {
        return false;
    }
    // 只在查询成功后替换快照，防止查询失败把已有列表误显示为空书库。
    m_records = records;
    applyFilter();
    emit countsChanged();
    return true;
}

bool LibraryController::importDirectory(const QUrl& directory, const QString& title)
{
    if (!directory.isLocalFile())
    {
        setErrorMessage(QStringLiteral("请选择本地漫画目录。"));
        return false;
    }

    // FolderDialog 使用 URL，扫描器使用文件路径。toLocalFile() 能正确处理空格和中文。
    ComicRecord comic;
    QString error;
    if (!ComicImport().importComicFromImages(directory.toLocalFile(), comic, title, &error))
    {
        setErrorMessage(error);
        return false;
    }
    // 重新读取数据库，保持列表顺序、数量徽标和当前搜索条件一致。
    return reload();
}

bool LibraryController::importArchive(const QUrl& file, const QString& title)
{
    if (!file.isLocalFile())
    {
        setErrorMessage(QStringLiteral("请选择本地 CBZ 或 ZIP 文件。"));
        return false;
    }
    ComicRecord comic;
    QString error;
    if (!ComicImport().importComicFromArchive(file.toLocalFile(), comic, title, &error))
    {
        setErrorMessage(error);
        return false;
    }
    return reload();
}

bool LibraryController::removeComic(const QString& id)
{
    // 数据访问层只删除数据库行，不修改漫画源文件。
    QString error;
    if (!DBManager().removeComic(id, &error))
    {
        setErrorMessage(error);
        return false;
    }
    return reload();
}

void LibraryController::clearError()
{
    setErrorMessage(QString());
}

void LibraryController::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message)
    {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void LibraryController::applyFilter()
{
    // 每次从完整快照重新筛选，清空搜索或退出历史模式时即可恢复全部记录。
    const QString keyword = m_searchText.trimmed();
    QList<ComicRecord> filtered;
    for (const ComicRecord& comic : m_records)
    {
        if (m_historyOnly && comic.lastReadAt <= 0)
        {
            continue;
        }
        if (!comic.title.contains(keyword, Qt::CaseInsensitive))
        {
            continue;
        }
        filtered.append(comic);
    }

    if (m_historyOnly)
    {
        // 相同阅读时间的记录保持原有顺序，减少刷新时卡片位置无意义地交换。
        std::stable_sort(filtered.begin(), filtered.end(),
                         [](const ComicRecord& left, const ComicRecord& right)
        {
            return left.lastReadAt > right.lastReadAt;
        });
    }
    m_model.setComics(filtered);
}
