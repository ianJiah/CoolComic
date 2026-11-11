#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include "database.h"
#include "comicimport.h"
#include "archiveimageprovider.h"

// 手动导入测试：通过 --test-import <目录路径> 指定样例，避免普通启动重复导入。
bool test01(const QString& path)
{
    ComicImport ci;
    ComicRecord comic;
    QString errorMessage;
    if (!ci.importComicFromImages(path, comic, QString(), &errorMessage))
    {
        qWarning() << "test01 导入失败：" << errorMessage;
        return false;
    }

    // 新导入记录的 lastReadAt 为 0，表示尚未阅读，并不代表导入失败。
    qInfo() << "test01 导入成功："
            << "标题：" << comic.title
            << "ID：" << comic.id
            << "来源：" << comic.sourcePath
            << "封面：" << comic.coverRef
            << "页数：" << comic.pageCount
            << "导入时间：" << comic.addedAt;
    return true;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    if (!DataBase::instance().init())
    {
        return -1;
    }

    const QStringList arguments = app.arguments();
    const int testArgumentIndex = arguments.indexOf(QStringLiteral("--test-import"));
    if (testArgumentIndex >= 0)
    {
        if (testArgumentIndex + 1 >= arguments.size())
        {
            qWarning() << "--test-import 后需要指定漫画目录路径。";
            return -1;
        }
        if (!test01(arguments.at(testArgumentIndex + 1)))
        {
            return -1;
        }
    }

    QQmlApplicationEngine engine;
    // 图片提供器归 QML 引擎管理，后台请求不访问数据库连接。
    engine.addImageProvider(QStringLiteral("cbz"), new ArchiveImageProvider());
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("CoolComic", "Main");

    return app.exec();
}
