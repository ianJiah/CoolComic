#include "archivereader.h"

// miniz 实现在独立 C 静态库中编译；读取/解压相关宏由 CMake 统一设置。
#include <miniz.h>

#include <QCollator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>
#include <limits>

namespace
{

// 单张图片最多解压 128 MiB；限制的是压缩条目字节，不是整本漫画的总大小。
constexpr mz_uint64 kMaxImageBytes = 128ULL * 1024 * 1024;
// ZIP64 可以包含更多条目，应用层仍限制数量，避免枚举过大的漫画包。
constexpr mz_uint kMaxEntries = 100000;

void setError(QString* output, const QString& message)
{
    if (output != nullptr)
    {
        *output = message;
    }
}

/** 局部 ZIP 句柄负责 QFile 和 miniz 生命周期，任何失败返回都会正确释放资源。 */
class ZipHandle
{
public:
    explicit ZipHandle(const QString& path)
        : m_file(path)
    {
    }

    ~ZipHandle()
    {
        if (m_open)
        {
            mz_zip_reader_end(&m_archive);
        }
    }

    ZipHandle(const ZipHandle&) = delete;
    ZipHandle& operator=(const ZipHandle&) = delete;

    bool open(QString* errorMessage)
    {
        if (!m_file.open(QIODevice::ReadOnly))
        {
            setError(errorMessage, QStringLiteral("无法读取 CBZ：%1").arg(m_file.errorString()));
            return false;
        }
        m_archive.m_pIO_opaque = &m_file;
        m_archive.m_pRead = [](void* opaque, mz_uint64 offset, void* buffer, size_t size) -> size_t
        {
            QFile* file = static_cast<QFile*>(opaque);
            if (offset > static_cast<mz_uint64>(std::numeric_limits<qint64>::max())
                || size > static_cast<size_t>(std::numeric_limits<qint64>::max())
                || !file->seek(static_cast<qint64>(offset)))
            {
                return 0;
            }
            const qint64 bytesRead = file->read(static_cast<char*>(buffer), static_cast<qint64>(size));
            return bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0;
        };
        if (!mz_zip_reader_init(&m_archive, static_cast<mz_uint64>(m_file.size()), 0))
        {
            setError(errorMessage, QStringLiteral("无法读取 CBZ/ZIP：%1").arg(zipError()));
            return false;
        }
        m_open = true;
        if (mz_zip_reader_get_num_files(&m_archive) > kMaxEntries)
        {
            setError(errorMessage, QStringLiteral("压缩包条目过多，当前最多支持 100000 个条目。"));
            return false;
        }
        return true;
    }

    /** 返回该局部句柄的错误，不与其他线程共享 miniz 状态。 */
    QString zipError()
    {
        return QString::fromUtf8(mz_zip_get_error_string(mz_zip_peek_last_error(&m_archive)));
    }

    /** 读取完整名称，不使用长度有限的 stat.m_filename，以免截断长路径。 */
    bool entryName(mz_uint index, QString& name, QString* errorMessage)
    {
        const mz_uint length = mz_zip_reader_get_filename(&m_archive, index, nullptr, 0);
        if (length == 0)
        {
            setError(errorMessage, QStringLiteral("无法读取压缩包条目名称：%1").arg(zipError()));
            return false;
        }
        QByteArray bytes(static_cast<qsizetype>(length), '\0');
        if (mz_zip_reader_get_filename(&m_archive, index, bytes.data(), length) != length)
        {
            setError(errorMessage, QStringLiteral("读取压缩包条目名称失败。"));
            return false;
        }
        bytes.chop(1);
        QStringDecoder decoder(QStringDecoder::Utf8);
        name = decoder(bytes);
        if (decoder.hasError() || name.contains(QChar::Null))
        {
            setError(errorMessage, QStringLiteral("压缩包条目名称不是有效 UTF-8，请重新压缩并使用 UTF-8 文件名。"));
            return false;
        }
        return true;
    }

    mz_zip_archive m_archive{};

private:
    QFile m_file;
    bool m_open{false};
};

bool isImageEntry(const QString& name)
{
    if (name.startsWith(QStringLiteral("__MACOSX/")))
    {
        return false;
    }
    const QString suffix = QFileInfo(name).suffix().toLower();
    return suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")
        || suffix == QStringLiteral("png") || suffix == QStringLiteral("webp");
}

} // namespace

bool ArchiveReader::listImages(const QString& archivePath,
                               QStringList& entries,
                               QString* errorMessage) const
{
    entries.clear();
    setError(errorMessage, QString());
    ZipHandle zip(archivePath);
    if (!zip.open(errorMessage))
    {
        return false;
    }

    QStringList images;
    QSet<QString> names;
    const mz_uint count = mz_zip_reader_get_num_files(&zip.m_archive);
    for (mz_uint index = 0; index < count; ++index)
    {
        if (mz_zip_reader_is_file_a_directory(&zip.m_archive, index))
        {
            continue;
        }
        QString name;
        if (!zip.entryName(index, name, errorMessage))
        {
            return false;
        }
        if (!isImageEntry(name))
        {
            continue;
        }
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip.m_archive, index, &stat))
        {
            setError(errorMessage, QStringLiteral("无法读取图片条目信息：%1").arg(zip.zipError()));
            return false;
        }
        if (!mz_zip_reader_is_file_supported(&zip.m_archive, index))
        {
            setError(errorMessage, QStringLiteral("图片使用了不支持的压缩方式或加密：%1").arg(name));
            return false;
        }
        if (stat.m_uncomp_size == 0 || stat.m_uncomp_size > kMaxImageBytes)
        {
            setError(errorMessage, QStringLiteral("图片为空或解压后超过 128 MiB：%1").arg(name));
            return false;
        }
        if (names.contains(name))
        {
            setError(errorMessage, QStringLiteral("压缩包内存在同名图片，无法确定页面：%1").arg(name));
            return false;
        }
        names.insert(name);
        images.append(name);
    }
    if (images.isEmpty())
    {
        setError(errorMessage, QStringLiteral("CBZ 内没有 JPG、JPEG、PNG 或 WebP 图片。"));
        return false;
    }

    // 与目录扫描器保持相同的数字排序规则，子目录按完整条目路径排序。
    QCollator collator(QLocale(QLocale::English, QLocale::UnitedStates));
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(images.begin(), images.end(), [&collator](const QString& left, const QString& right)
    {
        const int result = collator.compare(left, right);
        return result == 0 ? left < right : result < 0;
    });
    entries = images;
    return true;
}

QByteArray ArchiveReader::readEntry(const QString& archivePath,
                                    const QString& entry,
                                    QString* errorMessage) const
{
    setError(errorMessage, QString());
    ZipHandle zip(archivePath);
    if (!zip.open(errorMessage))
    {
        return {};
    }
    const mz_uint count = mz_zip_reader_get_num_files(&zip.m_archive);
    for (mz_uint index = 0; index < count; ++index)
    {
        QString name;
        if (!zip.entryName(index, name, errorMessage))
        {
            return {};
        }
        if (name != entry)
        {
            continue;
        }
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip.m_archive, index, &stat)
            || stat.m_uncomp_size == 0 || stat.m_uncomp_size > kMaxImageBytes
            || !mz_zip_reader_is_file_supported(&zip.m_archive, index))
        {
            setError(errorMessage, QStringLiteral("条目不可读取、过大或使用不支持的压缩方式：%1").arg(entry));
            return {};
        }
        QByteArray bytes(static_cast<qsizetype>(stat.m_uncomp_size), '\0');
        // 解压接口会验证 CRC，损坏的数据不会作为有效图片返回。
        if (!mz_zip_reader_extract_to_mem(&zip.m_archive, index, bytes.data(),
                                          static_cast<size_t>(bytes.size()), 0))
        {
            setError(errorMessage, QStringLiteral("读取 CBZ 图片失败：%1").arg(zip.zipError()));
            return {};
        }
        return bytes;
    }
    setError(errorMessage, QStringLiteral("CBZ 内找不到图片条目：%1").arg(entry));
    return {};
}

QUrl ArchiveReader::imageUrl(const QString& archivePath, const QString& entry)
{
    const QJsonObject payload{{QStringLiteral("archive"), archivePath},
                              {QStringLiteral("entry"), entry}};
    const QByteArray token = QJsonDocument(payload).toJson(QJsonDocument::Compact)
                                 .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QUrl(QStringLiteral("image://cbz/") + QString::fromLatin1(token));
}
