# CoolComic

使用 Qt Quick/QML 与 C++20 开发的 Windows 本地漫画阅读器。

当前代码支持图片目录、CBZ/ZIP 导入、封面书库、标题搜索、阅读历史、单页阅读及进度恢复。
支持本地图片阅读，不支持 PDF、MOBI、网络漫画源或 ZIP 内嵌的 PDF。

## 开发环境

- CMake（工程最低声明 3.16，建议使用 Qt Kit 随附版本）。
- Qt 6.10 或更新版本，安装 Quick、QuickControls2、QuickDialogs2、Sql 及对应图片格式插件。
- 与 Qt Kit 匹配的 C/C++ 编译器，开发环境使用 Qt 6.12.0 MinGW 64-bit。
- 本地 `third_party/miniz/miniz.c` 和 `miniz.h`：开发者手动下载的 miniz 3.1.2 合并版源码。

## 构建

在 Qt Creator 中选择匹配的 Kit，打开 CMakeLists.txt，重新配置后构建即可。

miniz 源码已随仓库放在 `third_party/miniz`，CMake 不联网下载依赖。
缺少 miniz.c 或 miniz.h 时配置会提示路径。
当前为完整合并版，这两个文件已包含 ZIP 和解压实现，不需要再下载分拆的 miniz_zip.c 等文件。
CMake 将 miniz.c 作为 C99 静态库编译，应用只包含 miniz.h；Qt Kit 需同时配置 C 和 C++ 编译器。
读取使用 QFile 回调，支持中文文件路径，避免把整本漫画读入内存。

可执行文件生成到项目根目录的 `bin/appCoolComic.exe`。首次正常运行会创建 `bin/coolcomic.db`。
数据库不会由 CMake 覆盖；备份时退出程序再备份该文件，漫画源文件需另外保留。
如需从资源管理器独立启动或分发，使用 Kit 中的 `windeployqt` 部署 Qt DLL、QML 和 SQLite/
图片插件。保留 Qt 和 miniz 源码中的上游许可证与分发要求，不能只复制 exe。

## 使用

1. 点击“导入”，选择图片目录或 CBZ/ZIP。目录只扫描直接图片文件；ZIP 可以包含子目录。
2. 输入标题或留空使用默认名称。导入登记原始来源，不复制、不合并图片、不整本解压 ZIP。
3. 在本地列表按标题搜索，点击封面进入单页阅读，信息按钮可查看来源和页数。
4. 左右方向键或底部按钮翻页，输入合法页码后按回车跳转，Esc 返回书库。
5. 图片成功显示后保存阅读进度；再次打开时恢复位置。移除操作只删记录，不删原文件。

CBZ 是包含图片的 ZIP 文件。当前支持 JPG/JPEG、PNG、WebP，普通存储/Deflate ZIP，
条目名称需为 UTF-8 或 ASCII；加密压缩包和旧编码名称不支持。
每次只解压一张请求图片，不把整本漫画留在内存或展开到磁盘。单张解压字节上限为 128 MiB，
压缩包条目数上限为 100000。新版 miniz 支持 ZIP64，已移除旧版小于 4 GiB 的包大小限制，
但包内单张图片仍受上述大小限制。
部分非封面图片的损坏只有读到该页时才会被发现。

## 手动导入样例

默认启动不自动写入样例记录。需要时可通过启动参数调用 main.cpp 中的 test01：

```powershell
.\bin\appCoolComic.exe --test-import "D:/Comics/MyComic"
```

该参数目前只测试目录导入，重复来源会报错。

## 本地文件

构建输出、SQLite 数据库、本地开发文档和测试漫画由 `.gitignore` 排除。
仓库只包含源代码、构建配置、README 和构建所需的 miniz 源码，不包含漫画样例。
