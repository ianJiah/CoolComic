#include "directoryscanner.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>

#include <algorithm>

bool DirectoryScanner::scan(const QString& path,
                            QList<Page>& pages,
                            QString* errorMessage) const
{
    pages.clear();
    if (errorMessage != nullptr)
    {
        errorMessage->clear();
    }

    // 统一错误出口，保证每一种失败场景都能向上层说明原因。
    const auto fail = [errorMessage](const QString& message)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = message;
        }
        return false;
    };

    if (path.trimmed().isEmpty())
    {
        return fail(QStringLiteral("漫画目录路径不能为空。"));
    }

    const QFileInfo directoryInfo(path);
    if (!directoryInfo.exists() || !directoryInfo.isDir())
    {
        return fail(QStringLiteral("漫画目录不存在或路径不是目录：%1").arg(path));
    }

    if (!directoryInfo.isReadable())
    {
        return fail(QStringLiteral("没有读取漫画目录的权限：%1").arg(path));
    }

    // 先解析目录链接并规范化路径，保证页路径与入库的来源路径使用同一个基准。
    const QString directoryPath = directoryInfo.canonicalFilePath();
    if (directoryPath.isEmpty())
    {
        return fail(QStringLiteral("无法解析漫画目录的实际路径：%1").arg(path));
    }

    const QDir directory(directoryPath);
    const QFileInfoList files = directory.entryInfoList(QDir::Files, QDir::NoSort);
    QFileInfoList imageFiles;
    for (const QFileInfo& fileInfo : files)
    {
        // 不区分扩展名大小写；非图片文件直接忽略，子目录不会出现在 files 中。
        const QString suffix = fileInfo.suffix().toLower();
        if (suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg")
            && suffix != QStringLiteral("png") && suffix != QStringLiteral("webp"))
        {
            continue;
        }

        if (!fileInfo.isReadable())
        {
            return fail(QStringLiteral("图片文件不可读：%1").arg(fileInfo.absoluteFilePath()));
        }

        // canRead() 检查格式和头部；避免仅凭扩展名把普通文本文件当作图片导入。
        QImageReader reader(fileInfo.absoluteFilePath());
        if (!reader.canRead())
        {
            return fail(QStringLiteral("无法识别图片：%1。原因：%2")
                            .arg(fileInfo.absoluteFilePath(), reader.errorString()));
        }

        imageFiles.append(fileInfo);
    }

    if (imageFiles.isEmpty())
    {
        return fail(QStringLiteral("目录中没有可导入的 JPG、JPEG、PNG 或 WebP 图片：%1")
                        .arg(path));
    }

    // 使用固定语言环境和数字排序，使 2.jpg 排在 10.jpg 前面。
    // 排序键相同（例如 1.jpg 与 01.jpg）时再按文件名比较，保证多次扫描顺序一致。
    QCollator collator(QLocale(QLocale::English, QLocale::UnitedStates));
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(imageFiles.begin(), imageFiles.end(),
              [&collator](const QFileInfo& left, const QFileInfo& right)
    {
        const int result = collator.compare(left.fileName(), right.fileName());
        if (result == 0)
        {
            return left.fileName() < right.fileName();
        }
        return result < 0;
    });

    QList<Page> scannedPages;
    scannedPages.reserve(imageFiles.size());
    for (const QFileInfo& fileInfo : imageFiles)
    {
        Page page;
        page.m_index = static_cast<int>(scannedPages.size()) + 1;
        page.m_path = fileInfo.absoluteFilePath();
        scannedPages.append(page);
    }

    pages = scannedPages;
    return true;
}
