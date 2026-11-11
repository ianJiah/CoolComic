#include "archiveimageprovider.h"

#include "archivereader.h"

#include <QBuffer>
#include <QDebug>
#include <QImage>
#include <QImageIOHandler>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>

ArchiveImageProvider::ArchiveImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image, QQuickImageProvider::ForceAsynchronousImageLoading)
{
}

QImage ArchiveImageProvider::requestImage(const QString& id,
                                         QSize* size,
                                         const QSize& requestedSize)
{
    if (size != nullptr)
    {
        *size = QSize();
    }

    // 路径和条目名使用 JSON 加 Base64URL 传输，中文、空格和 # 等字符不会改变 URL 结构。
    const QByteArray json = QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding
                                                                 | QByteArray::AbortOnBase64DecodingErrors);
    const QJsonObject payload = QJsonDocument::fromJson(json).object();
    const QString archivePath = payload.value(QStringLiteral("archive")).toString();
    const QString entry = payload.value(QStringLiteral("entry")).toString();
    if (archivePath.isEmpty() || entry.isEmpty())
    {
        qWarning() << "无效的 CBZ 图片请求。";
        return {};
    }

    QString error;
    QByteArray bytes = ArchiveReader().readEntry(archivePath, entry, &error);
    if (bytes.isEmpty())
    {
        qWarning() << "CBZ 图片读取失败：" << error;
        return {};
    }

    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize originalSize = reader.size();
    if (originalSize.isEmpty())
    {
        qWarning() << "无法识别 CBZ 图片尺寸：" << entry << reader.errorString();
        return {};
    }

    // 正常请求由 QML 提供视口或封面尺寸；未指定尺寸时仍限制解码到 4096 边长。
    const QSize bounds(requestedSize.width() > 0 ? requestedSize.width() : 4096,
                       requestedSize.height() > 0 ? requestedSize.height() : 4096);
    // EXIF 旋转会交换宽高，解码目标与返回的原始显示尺寸也需要对应调整。
    const bool rotated = reader.transformation().testFlag(QImageIOHandler::TransformationRotate90);
    const QSize decodeBounds = rotated ? bounds.transposed() : bounds;
    QSize decodeSize = originalSize.scaled(decodeBounds, Qt::KeepAspectRatio);
    decodeSize.setWidth(qMax(1, decodeSize.width()));
    decodeSize.setHeight(qMax(1, decodeSize.height()));
    if (decodeSize.width() < originalSize.width() || decodeSize.height() < originalSize.height())
    {
        reader.setScaledSize(decodeSize);
    }
    const QImage image = reader.read();
    if (image.isNull())
    {
        qWarning() << "CBZ 图片解码失败：" << entry << reader.errorString();
        return {};
    }
    if (size != nullptr)
    {
        *size = rotated ? originalSize.transposed() : originalSize;
    }
    return image;
}
