#ifndef ARCHIVEREADER_H
#define ARCHIVEREADER_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

/**
 * @brief CBZ（ZIP）只读服务，使用本地 miniz 3.1.2 枚举和读取图片。
 *
 * 每次操作单独打开文件，不共享解压状态，因此可以由图片提供器的后台线程调用。
 * 不查询数据库，不解压文件到磁盘；图片只在需要显示时读入内存。
 * 支持普通存储/Deflate ZIP 及 ZIP64、UTF-8 或 ASCII 条目名称，不支持加密压缩包。
 * 应用层最多接受 100000 个条目，单张图片解压字节不超过 128 MiB。
 */
class ArchiveReader
{
public:
    /** 枚举图片条目并自然排序；失败时清空输出，拒绝同名图片以保持封面引用唯一。 */
    bool listImages(const QString& archivePath,
                    QStringList& entries,
                    QString* errorMessage = nullptr) const;

    /** 按条目原始名称读取一张图片，检查 CRC 和单张解压大小；失败返回空数据。 */
    QByteArray readEntry(const QString& archivePath,
                         const QString& entry,
                         QString* errorMessage = nullptr) const;

    /** 把压缩包路径和条目名编码为 image://cbz URL，不暴露为普通文件路径。 */
    static QUrl imageUrl(const QString& archivePath, const QString& entry);
};

#endif // ARCHIVEREADER_H
