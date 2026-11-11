#ifndef COMICIMPORT_H
#define COMICIMPORT_H

#include <QString>
#include "dbmanager.h"

/**
 * @brief 协调图片目录/CBZ 检查和漫画记录入库的导入服务。
 *
 * 一本漫画对应一条 ComicRecord。导入只是登记原目录或压缩包，不合并图片，
 * 不复制图片、不整本解压，也不修改原文件。数据库连接必须先由 DataBase 初始化成功。
 */
class ComicImport
{
public:
    ComicImport() = default;

    /**
     * @brief 将指定目录中的图片作为一本漫画导入书库。
     * @param path 本地目录路径，不接收 file:// URL。
     * @param comic 导入成功后接收完整记录；失败时保留原值。
     * @param title 可选标题；为空或全为空白时，默认使用目录名。
     * @param errorMessage 可选错误说明，成功时清空。
     * @return 扫描并保存记录都成功时返回 true，重复来源等入库错误返回 false。
     *
     * 该接口同步执行，且入库必须发生在数据库连接所属线程。
     * 后续异步扫描时应将扫描与入库分开，不能在工作线程复用主线程 SQL 连接。
     */
    bool importComicFromImages(const QString& path,
                               ComicRecord& comic,
                               const QString& title = QString(),
                               QString* errorMessage = nullptr) const;

    /**
     * @brief 登记一个 CBZ/ZIP 文件，不复制或整本解压原文件。
     *
     * 枚举图片条目并排序，检查首张封面可识别后保存记录。
     * sourcePath 保存压缩包绝对路径，coverRef 保存包内封面条目名称。
     * 失败时不覆盖 comic，具体原因通过可选 errorMessage 返回。
     */
    bool importComicFromArchive(const QString& path,
                                ComicRecord& comic,
                                const QString& title = QString(),
                                QString* errorMessage = nullptr) const;
};

#endif // COMICIMPORT_H
