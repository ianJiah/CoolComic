import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import CoolComic
import "../components"

/*
 * 本地书库与阅读历史共用的列表页面。
 * controller.comics 提供已经筛选、排序的 ComicListModel；本页不重复筛选数据。
 * 功能：自适应封面网格、页数/进度展示、空列表状态、漫画信息和移除确认。
 * 对外请求：importRequested 由主窗口打开导入菜单；readRequested 将漫画 ID 交给阅读器。
 * 移除仅删除书库记录，不删除用户的图片目录或 CBZ；实际数据库操作由控制器执行。
 */
Item {
    id: page
    // 主窗口传入同一个控制器，使导航切换与搜索操作使用一致的数据源。
    required property LibraryController controller
    signal importRequested()
    signal readRequested(string comicId)

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: page.controller.historyOnly ? "历史" : "本地"
                font.pixelSize: 18
            }
            Label {
                text: page.controller.comics.count
                color: "#657082"
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "导入"
                onClicked: page.importRequested()
            }
        }

        // 网格列数随宽度变化，单元高度固定，避免图片加载或标题长度引起布局跳动。
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: page.controller.comics
            cellWidth: width / Math.max(1, Math.floor(width / 172))
            cellHeight: 290
            ScrollBar.vertical: ScrollBar { }

            delegate: Item {
                id: tile
                // 这些属性名对应 ComicListModel.roleNames()，由模型为当前行自动填充。
                required property string comicId
                required property string comicTitle
                required property url coverUrl
                required property int pageCount
                required property int lastReadPage
                required property bool hasRead
                required property string sourcePath

                width: grid.cellWidth
                height: grid.cellHeight

                ColumnLayout {
                    anchors.fill: parent
                    anchors.rightMargin: 14
                    anchors.bottomMargin: 16
                    spacing: 5

                    // 点击封面把数据库 ID 交给阅读器，页面本身不负责读取图片目录。
                    Button {
                        id: coverButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 208
                        padding: 0
                        Accessible.name: tile.comicTitle
                        onClicked: page.readRequested(tile.comicId)
                        background: Rectangle {
                            radius: 6
                            color: "#f0f2f5"
                            border.color: coverButton.hovered ? "#8ca9d2" : "#dce0e7"
                        }
                        contentItem: Item {
                            Image {
                                id: cover
                                anchors.fill: parent
                                anchors.margins: 1
                                source: tile.coverUrl
                                // 列表使用缩略尺寸异步解码，避免每张封面都持有原图大小的像素数据。
                                sourceSize.width: 320
                                sourceSize.height: 440
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: cover.status !== Image.Ready
                                text: cover.status === Image.Loading ? "加载中" : "暂无封面"
                                color: "#6c7480"
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: tile.comicTitle
                        elide: Text.ElideRight
                        ToolTip.visible: titleHover.hovered
                        ToolTip.text: text
                        HoverHandler { id: titleHover }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: tile.hasRead ? tile.lastReadPage + " / " + tile.pageCount + " 页"
                                              : tile.pageCount + " 页"
                            color: "#6c7480"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        // 信息操作独立于封面点击，打开弹窗不会进入阅读器或更新阅读进度。
                        IconButton {
                            implicitWidth: 30
                            implicitHeight: 30
                            glyph: "\uE946"
                            toolTip: "漫画信息"
                            onClicked: {
                                details.comicTitle = tile.comicTitle
                                details.sourcePath = tile.sourcePath
                                details.pageCount = tile.pageCount
                                details.open()
                            }
                        }
                        IconButton {
                            implicitWidth: 30
                            implicitHeight: 30
                            glyph: "\uE74D"
                            toolTip: "从书库移除"
                            onClicked: {
                                removeDialog.comicId = tile.comicId
                                removeDialog.comicTitle = tile.comicTitle
                                removeDialog.open()
                            }
                        }
                    }
                }
            }

            // 分别说明书库为空、历史为空或搜索无结果，避免把筛选空列表误当成数据丢失。
            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 320)
                visible: grid.count === 0
                spacing: 16
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: page.controller.searchText.trim().length > 0 ? "未找到匹配的漫画"
                          : page.controller.historyOnly ? "暂无阅读历史" : "暂无本地漫画"
                    color: "#6c7480"
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !page.controller.historyOnly
                    text: "导入"
                    onClicked: page.importRequested()
                }
            }
        }
    }

    // 先在弹窗保存待移除记录的 ID，用户确认后才调用控制器，取消时不产生副作用。
    Dialog {
        id: removeDialog
        property string comicId: ""
        property string comicTitle: ""
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 32)
        title: "从书库移除"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).text = "移除"
            standardButton(Dialog.Cancel).text = "取消"
        }
        onAccepted: page.controller.removeComic(comicId)
        Label {
            width: parent.width
            text: "移除“" + removeDialog.comicTitle + "”？原目录、图片或压缩包将保留。"
            wrapMode: Text.Wrap
        }
    }

    // 只读信息弹窗：展示被点击记录的标题、页数和原始来源路径。
    Dialog {
        id: details
        property string comicTitle: ""
        property string sourcePath: ""
        property int pageCount: 0
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 32)
        title: "漫画信息"
        modal: true
        standardButtons: Dialog.Close
        onOpened: standardButton(Dialog.Close).text = "关闭"
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label { Layout.fillWidth: true; text: details.comicTitle; wrapMode: Text.Wrap }
            Label { text: details.pageCount + " 页"; color: "#6c7480" }
            Label { Layout.fillWidth: true; text: details.sourcePath; wrapMode: Text.WrapAnywhere }
        }
    }
}
