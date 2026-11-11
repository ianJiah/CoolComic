#ifndef PAGE_H
#define PAGE_H

#include <QString>

/**
 * @brief 表示漫画中一页图片的运行时数据，不单独存入 SQLite。
 *
 * 页码由目录扫描器排序后分配，与漫画阅读进度一样从 1 开始。
 */
struct Page
{
    /** 图片在漫画中的页码；扫描成功后保证连续且从 1 开始。 */
    int m_index{1};

    /** 目录漫画保存图片绝对路径；CBZ 保存包内条目名。由阅读控制器转换为对应 URL。 */
    QString m_path;
};

#endif // PAGE_H
