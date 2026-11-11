#ifndef ARCHIVEIMAGEPROVIDER_H
#define ARCHIVEIMAGEPROVIDER_H

#include <QQuickImageProvider>

/**
 * @brief 为 QML 的 image://cbz URL 提供按需解压的封面和阅读图片。
 *
 * 使用 Qt 图片提供器的异步线程执行，不访问主线程数据库。
 * 每次请求独立打开压缩包，读取并解码一张图片，完成后释放压缩数据。
 */
class ArchiveImageProvider : public QQuickImageProvider
{
public:
    ArchiveImageProvider();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

#endif // ARCHIVEIMAGEPROVIDER_H
