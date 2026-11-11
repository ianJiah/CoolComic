#ifndef DIRECTORYSCANNER_H
#define DIRECTORYSCANNER_H

#include <QList>
#include "page.h"

/**
 * @brief 单例目录扫描器，只负责图片枚举、格式检查、排序和页码生成。
 *
 * 第一阶段只扫描指定目录的直接文件，不递归扫描章节子目录。
 * 扫描过程不访问数据库，不创建、修改或删除用户文件，也不保存上次扫描状态。
 */
class DirectoryScanner
{
public:
    /** 获取唯一实例；函数内静态对象由 C++ 负责初始化和销毁。 */
    static DirectoryScanner& instance()
    {
        static DirectoryScanner ds;
        return ds;
    }

    /**
     * @brief 扫描 JPG/JPEG、PNG、WebP 图片并按自然数字顺序生成页列表。
     * @param path 本地目录路径，允许绝对路径或相对路径，不接收 file:// URL。
     * @param pages 输出页列表；调用开始时清空，只有完全扫描成功才填入结果。
     * @param errorMessage 可选错误说明，成功时清空。
     * @return 目录有效、含图片且所有候选图片可识别时返回 true。
     *
     * 扫描会检查图片头部是否可读取，不会把整本漫画解码到内存中。
     * 图片像素数据损坏仍可能在阅读时才被发现，需要由阅读器报告加载失败。
     */
    bool scan(const QString& path,
              QList<Page>& pages,
              QString* errorMessage = nullptr) const;

    // 禁止复制和移动，确保调用方只能使用单例实例。
    DirectoryScanner(const DirectoryScanner&) = delete;
    DirectoryScanner& operator=(const DirectoryScanner&) = delete;
    DirectoryScanner(DirectoryScanner&&) = delete;
    DirectoryScanner& operator=(DirectoryScanner&&) = delete;

private:
    DirectoryScanner() = default;
    ~DirectoryScanner() = default;
};

#endif // DIRECTORYSCANNER_H
