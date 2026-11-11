#include "comicimport.h"

#include "directoryscanner.h"
#include "archivereader.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QUuid>

bool ComicImport::importComicFromImages(const QString& path,
                                        ComicRecord& comic,
                                        const QString& title,
                                        QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    QList<Page> comicPages;
    if (!DirectoryScanner::instance().scan(path, comicPages, errorMessage))
    {
        return false;
    }

    // canonicalPath 统一相对路径、. 和 .. 等写法，与 DBManager 的路径规范化一致。
    // 若扫描期间目录已被移走，应停止导入，避免保存无效的来源路径。
    const QString sourcePath = QDir(path).canonicalPath();
    if (sourcePath.isEmpty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("漫画目录已不存在，无法完成导入：%1").arg(path);
        }
        return false;
    }

    const QDir sourceDirectory(sourcePath);
    ComicRecord importedComic;
    importedComic.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    importedComic.sourceType = QStringLiteral("directory");
    importedComic.sourcePath = sourcePath;
    importedComic.title = title.trimmed();
    if (importedComic.title.isEmpty())
    {
        importedComic.title = sourceDirectory.dirName();
        if (importedComic.title.isEmpty())
        {
            // 根目录可能没有普通目录名，此时使用来源路径作为默认标题。
            importedComic.title = sourcePath;
        }
    }

    // 封面只保存相对引用；阅读器或漫画库可用 sourcePath 与 coverRef 拼接路径。
    importedComic.coverRef = sourceDirectory.relativeFilePath(comicPages.first().m_path);
    importedComic.pageCount = static_cast<int>(comicPages.size());
    importedComic.addedAt = QDateTime::currentMSecsSinceEpoch();

    // 数据库唯一约束负责拦截同一目录的重复导入；失败信息原样传回调用方。
    const DBManager manager;
    if (!manager.addComic(importedComic, errorMessage))
    {
        return false;
    }

    // 仅在数据库写入成功后发布结果，避免调用方将未保存的记录当成已导入漫画。
    comic = importedComic;
    return true;
}

bool ComicImport::importComicFromArchive(const QString& path,
                                         ComicRecord& comic,
                                         const QString& title,
                                         QString* errorMessage) const
{
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    const auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    };

    const QFileInfo fileInfo(path);
    if (path.trimmed().isEmpty() || !fileInfo.isFile() || !fileInfo.isReadable())
    {
        return fail(QStringLiteral("CBZ 文件不存在或不可读：%1").arg(path));
    }
    const QString suffix = fileInfo.suffix().toLower();
    if (suffix != QStringLiteral("cbz") && suffix != QStringLiteral("zip"))
    {
        return fail(QStringLiteral("请选择 CBZ 或 ZIP 漫画文件。"));
    }
    const QString sourcePath = fileInfo.canonicalFilePath();
    if (sourcePath.isEmpty())
    {
        return fail(QStringLiteral("无法解析 CBZ 文件的实际路径：%1").arg(path));
    }

    const ArchiveReader archive;
    QStringList entries;
    if (!archive.listImages(sourcePath, entries, errorMessage))
    {
        return false;
    }

    // 导入时只读取封面检查格式；其余页到阅读时才解压，避免整本漫画进入内存。
    QByteArray coverBytes = archive.readEntry(sourcePath, entries.first(), errorMessage);
    if (coverBytes.isEmpty())
    {
        return false;
    }
    QBuffer coverBuffer(&coverBytes);
    coverBuffer.open(QIODevice::ReadOnly);
    QImageReader coverReader(&coverBuffer);
    if (!coverReader.canRead())
    {
        return fail(QStringLiteral("无法识别 CBZ 封面图片：%1").arg(coverReader.errorString()));
    }

    ComicRecord importedComic;
    importedComic.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    importedComic.sourceType = QStringLiteral("cbz");
    importedComic.sourcePath = sourcePath;
    importedComic.title = title.trimmed().isEmpty() ? fileInfo.completeBaseName() : title.trimmed();
    importedComic.coverRef = entries.first();
    importedComic.pageCount = static_cast<int>(entries.size());
    importedComic.addedAt = QDateTime::currentMSecsSinceEpoch();

    if (!DBManager().addComic(importedComic, errorMessage))
    {
        return false;
    }
    comic = importedComic;
    return true;
}
