#include "readercontroller.h"

#include "dbmanager.h"
#include "directoryscanner.h"
#include "archivereader.h"

#include <QFileInfo>
#include <algorithm>

ReaderController::ReaderController(QObject* parent)
    : QObject(parent)
{
}

bool ReaderController::hasComic() const
{
    return !m_pages.isEmpty();
}

QString ReaderController::title() const
{
    return m_title;
}

int ReaderController::pageCount() const
{
    return static_cast<int>(m_pages.size());
}

int ReaderController::currentPage() const
{
    return m_currentPage;
}

QUrl ReaderController::imageSource() const
{
    if (m_currentPage < 1 || m_currentPage > pageCount())
    {
        return {};
    }
    const QString path = m_pages.at(m_currentPage - 1).m_path;
    if (!m_archivePath.isEmpty())
    {
        return ArchiveReader::imageUrl(m_archivePath, path);
    }
    return QUrl::fromLocalFile(path);
}

QString ReaderController::pageLabel() const
{
    if (m_currentPage < 1 || m_currentPage > pageCount())
    {
        return {};
    }
    const QString path = m_pages.at(m_currentPage - 1).m_path;
    return m_archivePath.isEmpty() ? path : QFileInfo(m_archivePath).fileName() + QStringLiteral(" / ") + path;
}

QString ReaderController::errorMessage() const
{
    return m_errorMessage;
}

QString ReaderController::noticeMessage() const
{
    return m_noticeMessage;
}

bool ReaderController::openComic(const QString& id)
{
    // 切换漫画前保存上次已成功显示的页面；失败时停留在原漫画，避免丢失进度。
    if (!saveProgress())
    {
        return false;
    }
    resetComic();
    clearError();

    ComicRecord comic;
    QString error;
    if (!DBManager().getComic(id, comic, &error))
    {
        setErrorMessage(error);
        return false;
    }
    QList<Page> pages;
    if (comic.sourceType == QStringLiteral("directory"))
    {
        if (!DirectoryScanner::instance().scan(comic.sourcePath, pages, &error))
        {
            setErrorMessage(error);
            return false;
        }
    }
    else if (comic.sourceType == QStringLiteral("cbz"))
    {
        QStringList entries;
        if (!ArchiveReader().listImages(comic.sourcePath, entries, &error))
        {
            setErrorMessage(error);
            return false;
        }
        for (const QString& entry : entries)
        {
            Page page;
            page.m_index = static_cast<int>(pages.size()) + 1;
            page.m_path = entry;
            pages.append(page);
        }
        m_archivePath = comic.sourcePath;
    }
    else
    {
        setErrorMessage(QStringLiteral("不支持的漫画来源类型：%1").arg(comic.sourceType));
        return false;
    }

    m_comicId = comic.id;
    m_title = comic.title;
    m_pages = pages;
    // 图片减少后旧页码可能越界，恢复时按最新页数修正。
    m_currentPage = std::clamp(comic.lastReadPage, 1, pageCount());
    if (comic.pageCount != pageCount() || comic.lastReadPage != m_currentPage)
    {
        m_noticeMessage = QStringLiteral("漫画页数已更新为 %1 页，已定位到第 %2 页。")
                              .arg(pageCount()).arg(m_currentPage);
    }
    emit comicChanged();
    emit pageChanged();
    return true;
}

bool ReaderController::closeComic()
{
    if (!saveProgress())
    {
        return false;
    }
    resetComic();
    clearError();
    return true;
}

bool ReaderController::goToPage(int page)
{
    if (!hasComic() || page < 1 || page > pageCount())
    {
        return false;
    }
    if (m_currentPage == page)
    {
        return true;
    }
    m_currentPage = page;
    emit pageChanged();
    return true;
}

bool ReaderController::nextPage()
{
    return goToPage(m_currentPage + 1);
}

bool ReaderController::previousPage()
{
    return goToPage(m_currentPage - 1);
}

void ReaderController::confirmPageLoaded(int page, const QUrl& source)
{
    if (!hasComic() || page != m_currentPage || source != imageSource())
    {
        return;
    }
    // 同一张图片可能因窗口缩放重新解码，避免重复更新阅读时间。
    if (m_lastDisplayedPage == page && !m_progressDirty)
    {
        return;
    }
    m_lastDisplayedPage = page;
    m_progressDirty = true;
    saveProgress();
}

bool ReaderController::saveProgress()
{
    if (!m_progressDirty || !hasComic())
    {
        return true;
    }

    QString error;
    if (!DBManager().updateReadingProgress(m_comicId, m_lastDisplayedPage, pageCount(), &error))
    {
        setErrorMessage(error);
        return false;
    }
    m_progressDirty = false;
    clearError();
    return true;
}

void ReaderController::clearError()
{
    setErrorMessage(QString());
}

void ReaderController::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message)
    {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void ReaderController::resetComic()
{
    m_comicId.clear();
    m_title.clear();
    m_archivePath.clear();
    m_noticeMessage.clear();
    m_pages.clear();
    m_currentPage = 0;
    m_lastDisplayedPage = 0;
    m_progressDirty = false;
    emit comicChanged();
    emit pageChanged();
}
