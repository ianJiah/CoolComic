import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs as PlatformDialogs
import CoolComic
import "components"
import "pages"

/*
 * 应用主窗口与页面协调入口。
 * 功能：提供首页、本地书库、阅读历史导航，搜索框和目录/CBZ 导入对话框；
 *       根据 ReaderController.hasComic 在书库界面和阅读器之间切换。
 * 数据：library 负责书库操作和列表，reader 负责当前漫画及阅读进度。
 * 约定：QML 仅负责布局和用户交互，不直接扫描目录、执行 SQL 或拼接图片路径。
 * 当前支持图片目录与 CBZ/ZIP 漫画，不包含网络漫画源。
 */
ApplicationWindow {
    id: root
    visible: true
    width: 900
    height: 600
    minimumWidth: 500
    minimumHeight: 420
    title: reader.hasComic ? reader.title + " - CoolComic" : "CoolComic"
    color: "#ffffff"
    font.family: "Microsoft YaHei UI"
    font.pixelSize: 14
    palette.window: "#ffffff"
    palette.windowText: "#25282d"
    palette.text: "#25282d"
    palette.buttonText: "#25282d"
    palette.highlight: "#07548c"

    property string currentView: "home" // 书库视图模式：home、local、history；阅读状态单独管理。
    property url selectedSource // 暂存目录或文件 URL，供标题确认后提交导入。
    property bool importingArchive: false // 决定标题对话框提交到目录还是 CBZ 导入接口。

    // 控制器在窗口生命周期内保持存在，切换页面时保留搜索条件和书库数据。
    LibraryController { id: library }
    ReaderController { id: reader }

    // 正常退出前重试仍未保存的进度。保存失败时保留窗口，用户可在提示中重试。
    onClosing: function(close) {
        if (reader.hasComic && !reader.saveProgress()) {
            close.accepted = false
        }
    }

    // 返回操作先要求控制器保存进度；只有成功退出后才刷新书库中的历史和页码。
    ReaderPage {
        anchors.fill: parent
        visible: reader.hasComic
        controller: reader
        onExitRequested: {
            if (reader.closeComic()) {
                library.reload()
            }
        }
    }

    // 本地和历史共用 LibraryPage，通过筛选模式决定记录范围及排列顺序。
    function navigate(view) {
        currentView = view
        library.historyOnly = view === "history"
    }

    // 进入阅读器时隐藏整个导航/书库区域，但不销毁其控件和当前搜索文本。
    RowLayout {
        anchors.fill: parent
        visible: !reader.hasComic
        spacing: 0

        // 固定宽度导航栏，右侧内容随窗口尺寸变化。
        Rectangle {
            Layout.preferredWidth: 72
            Layout.fillHeight: true
            color: "#ffffff"
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#d3d8e2" }
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "CoolComic"
                    font.pixelSize: 10
                    Layout.bottomMargin: 10
                }
                IconButton {
                    Layout.alignment: Qt.AlignHCenter
                    glyph: "\uE80F"
                    toolTip: "首页"
                    checked: root.currentView === "home"
                    onClicked: root.navigate("home")
                }
                IconButton {
                    Layout.alignment: Qt.AlignHCenter
                    glyph: "\uE8F1"
                    toolTip: "本地"
                    checked: root.currentView === "local"
                    onClicked: root.navigate("local")
                }
                IconButton {
                    Layout.alignment: Qt.AlignHCenter
                    glyph: "\uE81C"
                    toolTip: "历史"
                    checked: root.currentView === "history"
                    onClicked: root.navigate("history")
                }
                Item { Layout.fillHeight: true }
                IconButton {
                    Layout.alignment: Qt.AlignHCenter
                    glyph: "\uE72C"
                    toolTip: "刷新漫画库"
                    onClicked: library.reload()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 16
            spacing: 16

            // 搜索实时交给控制器筛选内存记录，不更改数据库；首页搜索时也展示结果列表。
            TextField {
                id: search
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                placeholderText: "搜索本地漫画"
                leftPadding: 46
                rightPadding: 42
                selectByMouse: true
                onTextChanged: library.searchText = text
                background: Rectangle { radius: 23; color: "#e9e9ee" }
                Label {
                    x: 17
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: "Segoe MDL2 Assets"
                    font.pixelSize: 19
                    text: "\uE721"
                    color: "#25282d"
                }
                IconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 34
                    glyph: "\uE711"
                    toolTip: "清空搜索"
                    visible: search.text.length > 0
                    onClicked: search.clear()
                }
            }

            // 书库操作错误与打开漫画失败共用提示区，阅读器错误优先显示。
            // 关闭提示只清空错误文本，不会自动重试失败操作。
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: errorRow.implicitHeight + 16
                visible: library.errorMessage.length > 0 || reader.errorMessage.length > 0
                color: "#fff1ef"
                radius: 6
                RowLayout {
                    id: errorRow
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        Layout.fillWidth: true
                        text: reader.errorMessage.length > 0 ? reader.errorMessage : library.errorMessage
                        color: "#a4322c"
                        wrapMode: Text.WrapAnywhere
                    }
                    IconButton {
                        glyph: "\uE711"
                        toolTip: "关闭提示"
                        onClicked: {
                            library.clearError()
                            reader.clearError()
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.currentView === "home" && search.text.trim().length === 0
                spacing: 16

                // 首页只保留已接入的分组，不展示未实现的网络功能入口。
                ItemDelegate {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    onClicked: root.navigate("history")
                    background: Rectangle { radius: 6; color: "#ffffff"; border.color: "#d5dbe7" }
                    contentItem: RowLayout {
                        spacing: 8
                        Label { text: "历史"; font.pixelSize: 16 }
                        Rectangle {
                            implicitWidth: Math.max(24, historyCountLabel.implicitWidth + 12)
                            implicitHeight: 22
                            radius: 7
                            color: "#d9e5fc"
                            Label { id: historyCountLabel; anchors.centerIn: parent; text: library.historyCount; font.pixelSize: 12 }
                        }
                        Item { Layout.fillWidth: true }
                        Label { text: "\uE76C"; font.family: "Segoe MDL2 Assets" }
                    }
                    padding: 16
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 106
                    radius: 6
                    color: "#ffffff"
                    border.color: "#d5dbe7"
                    ItemDelegate {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 58
                        padding: 16
                        onClicked: root.navigate("local")
                        background: Item { }
                        contentItem: RowLayout {
                            spacing: 8
                            Label { text: "本地"; font.pixelSize: 16 }
                            Rectangle {
                                implicitWidth: Math.max(24, localCountLabel.implicitWidth + 12)
                                implicitHeight: 22
                                radius: 7
                                color: "#d9e5fc"
                                Label { id: localCountLabel; anchors.centerIn: parent; text: library.totalCount; font.pixelSize: 12 }
                            }
                            Item { Layout.fillWidth: true }
                            Label { text: "\uE76C"; font.family: "Segoe MDL2 Assets" }
                        }
                    }
                    Button {
                        id: homeImport
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        text: "导入"
                        implicitWidth: 78
                        implicitHeight: 34
                        contentItem: Label { text: homeImport.text; color: "#ffffff"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: homeImport.down ? "#073d68" : "#07548c"; radius: 17 }
                        onClicked: importMenu.popup()
                    }
                }
                Item { Layout.fillHeight: true }
            }

            // 子页面只发送请求信号；目录选择和打开阅读器由主窗口统一协调。
            LibraryPage {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.currentView !== "home" || search.text.trim().length > 0
                controller: library
                onImportRequested: importMenu.popup()
                onReadRequested: function(comicId) { reader.openComic(comicId) }
            }
        }
    }

    // 所有导入按钮共用此菜单；选择来源后再复用标题确认对话框。
    Menu {
        id: importMenu
        MenuItem { text: "导入图片目录"; onTriggered: folderDialog.open() }
        MenuItem { text: "导入 CBZ / ZIP"; onTriggered: archiveDialog.open() }
    }

    // 第一步：选择目录或压缩包。这里只暂存 URL，取消时不会写入数据库。
    PlatformDialogs.FolderDialog {
        id: folderDialog
        title: "选择漫画图片目录"
        onAccepted: {
            root.selectedSource = selectedFolder
            root.importingArchive = false
            importTitle.clear()
            importDialog.open()
        }
    }

    PlatformDialogs.FileDialog {
        id: archiveDialog
        title: "选择 CBZ 或 ZIP 漫画"
        fileMode: PlatformDialogs.FileDialog.OpenFile
        nameFilters: ["漫画压缩包 (*.cbz *.zip)"]
        onAccepted: {
            root.selectedSource = selectedFile
            root.importingArchive = true
            importTitle.clear()
            importDialog.open()
        }
    }

    // 第二步：确认标题后导入。空标题使用目录名或压缩包文件名；成功后转到本地并清空搜索。
    // 失败说明由控制器属性传到上方错误区，数据库层负责阻止同一来源重复入库。
    Dialog {
        id: importDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        title: root.importingArchive ? "导入 CBZ 漫画" : "导入本地漫画"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).text = "导入"
            standardButton(Dialog.Cancel).text = "取消"
            importTitle.forceActiveFocus()
        }
        onAccepted: {
            const success = root.importingArchive
                ? library.importArchive(root.selectedSource, importTitle.text)
                : library.importDirectory(root.selectedSource, importTitle.text)
            if (success) {
                root.navigate("local")
                search.clear()
            }
        }
        ColumnLayout {
            width: parent.width
            spacing: 12
            TextField {
                id: importTitle
                Layout.fillWidth: true
                placeholderText: root.importingArchive ? "标题（默认使用文件名）" : "标题（默认使用目录名）"
                selectByMouse: true
            }
            Label {
                Layout.fillWidth: true
                text: root.selectedSource.toString()
                color: "#6c7480"
                wrapMode: Text.WrapAnywhere
            }
        }
    }
}
