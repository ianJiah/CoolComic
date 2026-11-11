#include "comiclistmodel.h"

#include "archivereader.h"

#include <QDir>
#include <QUrl>

ComicListModel::ComicListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ComicListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant ComicListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.column() != 0
        || index.row() < 0 || index.row() >= count())
    {
        return {};
    }

    const ComicRecord& comic = m_comics.at(index.row());
    switch (role)
    {
    case ComicIdRole:
        return comic.id;
    case TitleRole:
        return comic.title;
    case CoverUrlRole:
        // QML Image 接受 URL，不能直接把 Windows 文件路径当作网络地址使用。
        if (comic.sourceType == QStringLiteral("directory"))
        {
            return QUrl::fromLocalFile(QDir(comic.sourcePath).filePath(comic.coverRef));
        }
        if (comic.sourceType == QStringLiteral("cbz"))
        {
            return ArchiveReader::imageUrl(comic.sourcePath, comic.coverRef);
        }
        return QUrl();
    case PageCountRole:
        return comic.pageCount;
    case LastReadPageRole:
        return comic.lastReadPage;
    case SourcePathRole:
        return comic.sourcePath;
    case HasReadRole:
        return comic.lastReadAt > 0;
    default:
        return {};
    }
}

QHash<int, QByteArray> ComicListModel::roleNames() const
{
    return {
        {ComicIdRole, "comicId"},
        {TitleRole, "comicTitle"},
        {CoverUrlRole, "coverUrl"},
        {PageCountRole, "pageCount"},
        {LastReadPageRole, "lastReadPage"},
        {SourcePathRole, "sourcePath"},
        {HasReadRole, "hasRead"}
    };
}

int ComicListModel::count() const
{
    return static_cast<int>(m_comics.size());
}

void ComicListModel::setComics(const QList<ComicRecord>& comics)
{
    beginResetModel();
    m_comics = comics;
    endResetModel();
    emit countChanged();
}
