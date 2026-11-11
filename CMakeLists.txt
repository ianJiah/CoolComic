cmake_minimum_required(VERSION 3.16)
project(CoolComic VERSION 0.1 LANGUAGES C CXX)

# 设置c++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt模块
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 Sql)

# 第三方库由开发者手动下载到本地，配置过程中不联网获取任何依赖。
set(COOLCOMIC_MINIZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz")
foreach(miniz_file miniz.c miniz.h)
    if(NOT EXISTS "${COOLCOMIC_MINIZ_DIR}/${miniz_file}")
        message(FATAL_ERROR "缺少 ${COOLCOMIC_MINIZ_DIR}/${miniz_file}，请手动准备 miniz 合并版源码。")
    endif()
endforeach()

# 当前使用本地 miniz 3.1.2 合并版。实现作为 C 静态库单独编译，应用只包含声明头。
add_library(coolcomic_miniz STATIC "${COOLCOMIC_MINIZ_DIR}/miniz.c")
target_include_directories(coolcomic_miniz SYSTEM PUBLIC "${COOLCOMIC_MINIZ_DIR}")
set_target_properties(coolcomic_miniz PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
# 应用只需要解压，禁用写入和 zlib 兼容接口。文件读取统一经由 QFile 回调完成。
# PUBLIC 确保库和使用它的 C++ 文件看到相同的头文件配置。
target_compile_definitions(coolcomic_miniz PUBLIC
    MINIZ_NO_STDIO
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_ARCHIVE_WRITING_APIS
    MINIZ_NO_ZLIB_APIS
    MINIZ_NO_ZLIB_COMPATIBLE_NAMES
)

# 启用Qt标准项目配置
qt_standard_project_setup(REQUIRES 6.10)

# 创建Qt应用程序
qt_add_executable(appCoolComic src/main.cpp)

# 自动生成的 QML 类型注册代码需要能找到 src 中声明的 C++ 类型头文件。
target_include_directories(appCoolComic PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# 注册Qml模块
qt_add_qml_module(appCoolComic
    URI CoolComic
    QML_FILES
        qml/Main.qml
        qml/components/IconButton.qml
        qml/pages/LibraryPage.qml
        qml/pages/ReaderPage.qml
        SOURCES src/database.cpp src/database.h
        SOURCES src/dbmanager.h src/dbmanager.cpp
        SOURCES src/page.h
        SOURCES src/directoryscanner.h src/directoryscanner.cpp
        SOURCES src/comicimport.h src/comicimport.cpp
        SOURCES src/comiclistmodel.h src/comiclistmodel.cpp
        SOURCES src/librarycontroller.h src/librarycontroller.cpp
        SOURCES src/readercontroller.h src/readercontroller.cpp
        SOURCES src/archivereader.h src/archivereader.cpp
        SOURCES src/archiveimageprovider.h src/archiveimageprovider.cpp
)

# 将不同构建配置生成的可执行文件统一放到项目根目录的 bin 文件夹。
set_target_properties(appCoolComic PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

# 链接库
target_link_libraries(appCoolComic
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Sql
        coolcomic_miniz
)
