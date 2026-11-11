#ifndef COMICLISTMODEL_H
#define COMICLISTMODEL_H

#include "dbmanager.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

/** 漫画列表的展示模型：只向 QML 提供角色数据，不负责扫描或数据库写入。 */
class ComicListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role
    {
        ComicIdRole = Qt::UserRole + 1,
        TitleRole,
        CoverUrlRole,
        PageCountRole,
        LastReadPageRole,
        SourcePathRole,
        HasReadRole
    };

    explicit ComicListModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;

    /** 用查询和筛选后的记录替换整个列表，通知 QML 重新显示。 */
    void setComics(const QList<ComicRecord>& comics);

signals:
    void countChanged();

private:
    QList<ComicRecord> m_comics;
};

#endif // COMICLISTMODEL_H
