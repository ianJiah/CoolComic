#ifndef READERCONTROLLER_H
#define READERCONTROLLER_H

#include "page.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

/**
 * @brief 本地目录和 CBZ 漫画的单页阅读状态与进度控制器。
 *
 * 打开时通过 DBManager 获取来源，用 DirectoryScanner 或 ArchiveReader 生成页列表，
 * 将文件 URL 或 image://cbz URL 交给 QML 异步解码。当前页是正在请求显示的页，已读页只有在
 * QML 确认图片加载成功后才更新，避免加载失败或快速翻页产生错误进度。
 * 数据库操作始终在主线程执行；本类不保存整章解码图片，也不负责绘制图片。
 */
class ReaderController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool hasComic READ hasComic NOTIFY comicChanged)
    Q_PROPERTY(QString title READ title NOTIFY comicChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY comicChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY pageChanged)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY pageChanged)
    Q_PROPERTY(QString pageLabel READ pageLabel NOTIFY pageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString noticeMessage READ noticeMessage NOTIFY comicChanged)

public:
    explicit ReaderController(QObject* parent = nullptr);

    /** 是否已有成功扫描并打开的漫画。 */
    bool hasComic() const;
    /** 当前漫画标题。 */
    QString title() const;
    /** 当前扫描到的真实页数，而非数据库中可能过期的页数。 */
    int pageCount() const;
    /** 正在显示或加载的页码；没有漫画时为 0。 */
    int currentPage() const;
    /** 当前页的本地图片 URL；无有效页时为空。 */
    QUrl imageSource() const;
    /** 当前图片的可读位置说明，避免在出错界面展示编码后的 image:// URL。 */
    QString pageLabel() const;
    /** 打开漫画、翻页或保存进度失败的原因。 */
    QString errorMessage() const;
    /** 图片数量变化时的恢复提示，与数据库或加载错误分开显示。 */
    QString noticeMessage() const;

    /** 打开目录或 CBZ 漫画，恢复并限制已存页码；失败时不显示上一部漫画的残留页面。 */
    Q_INVOKABLE bool openComic(const QString& id);
    /** 保存待写进度后清理阅读状态；保存失败返回 false，保留当前漫画以便重试。 */
    Q_INVOKABLE bool closeComic();
    /** 跳到合法页码，越界时返回 false，保持当前页不变。 */
    Q_INVOKABLE bool goToPage(int page);
    Q_INVOKABLE bool nextPage();
    Q_INVOKABLE bool previousPage();
    /** 由 QML Image.Ready 调用；核对页码与 URL，排除过期图片的加载通知。 */
    Q_INVOKABLE void confirmPageLoaded(int page, const QUrl& source);
    /** 将最近成功显示的页码保存到数据库；没有待写进度时直接成功。 */
    Q_INVOKABLE bool saveProgress();
    /** 关闭错误提示，不清除尚未保存的进度。 */
    Q_INVOKABLE void clearError();

signals:
    void comicChanged();
    void pageChanged();
    void errorMessageChanged();

private:
    void setErrorMessage(const QString& message);
    void resetComic();

    QString m_comicId;               // 当前漫画的数据库 ID。
    QString m_title;                 // 当前漫画标题。
    QString m_archivePath;           // CBZ 的绝对路径；目录漫画时为空。
    QList<Page> m_pages;             // 只保存路径和页码，不持有解码图片。
    int m_currentPage{0};            // 正在显示或加载的页码。
    int m_lastDisplayedPage{0};      // 最近一次经 QML 确认显示成功的页码。
    bool m_progressDirty{false};     // 保存失败时保留，供关闭或重试时再次写入。
    QString m_errorMessage;          // 供界面显示的业务错误。
    QString m_noticeMessage;         // 重新扫描后图片数量变化的提示。
};

#endif // READERCONTROLLER_H
