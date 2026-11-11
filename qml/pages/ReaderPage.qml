import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import CoolComic
import "../components"

/*
 * 本地目录与 CBZ 漫画共用的单页阅读界面，所有页码统一从 1 开始。
 * 功能：按视口等比例显示当前图片、上一页/下一页、左右方向键翻页、回车跳页和返回书库。
 * 数据：ReaderController 提供当前页、真实总页数、图片 URL 和保存错误。
 * 进度：仅 Image.Ready 时通知控制器确认已读；加载失败不记进度，返回/关闭由主窗口协调保存。
 * 内存：只加载当前页，限制解码尺寸且关闭图片缓存；尚无邻页预加载、缩放拖动和连续滚动。
 */
FocusScope {
    id: page
    required property ReaderController controller
    signal exitRequested() // 请求退出，实际保存、关闭和书库刷新由 Main.qml 完成。
    // 页面重新可见时取回焦点，避免沿用书库搜索框或上次页码编辑时的输入焦点。
    onVisibleChanged: {
        if (visible) {
            forceActiveFocus()
        }
    }

    // 使用窗口级快捷键，不依赖翻页后焦点停留在哪一个按钮。
    // 编辑页码时停用左右键翻页，避免干扰输入框内移动光标。
    Shortcut {
        sequence: "Left"
        enabled: page.visible && !pageNumber.activeFocus
        onActivated: page.controller.previousPage()
    }
    Shortcut {
        sequence: "Right"
        enabled: page.visible && !pageNumber.activeFocus
        onActivated: page.controller.nextPage()
    }
    Shortcut {
        sequence: "Escape"
        enabled: page.visible
        onActivated: page.exitRequested()
    }

    // 编辑 TextField 会打断其初始 text 绑定，用控制器通知保持按钮翻页与输入框页码同步。
    Connections {
        target: page.controller
        function onPageChanged() {
            pageNumber.text = String(page.controller.currentPage)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部工具栏显示当前漫画、页码，并提供返回入口。
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 12
            IconButton {
                glyph: "\uE72B"
                toolTip: "返回书库"
                onClicked: page.exitRequested()
            }
            Label {
                Layout.fillWidth: true
                text: page.controller.title
                font.pixelSize: 16
                elide: Text.ElideRight
            }
            Label {
                text: page.controller.currentPage + " / " + page.controller.pageCount
                color: "#657082"
            }
        }

        // 与保存错误分开显示：原目录或压缩包发生变化时，说明恢复后的实际位置。
        Label {
            Layout.fillWidth: true
            visible: page.controller.noticeMessage.length > 0
            text: page.controller.noticeMessage
            padding: 8
            color: "#365474"
            wrapMode: Text.Wrap
            background: Rectangle { color: "#edf3fb" }
        }

        // 此处报告数据库保存错误，与画布上的图片加载失败分别处理。
        // 保存失败时控制器保留待写进度，“重试保存”会重新尝试写入。
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: progressError.implicitHeight + 16
            visible: page.controller.errorMessage.length > 0
            color: "#fff1ef"
            RowLayout {
                id: progressError
                anchors.fill: parent
                anchors.margins: 8
                Label {
                    Layout.fillWidth: true
                    text: page.controller.errorMessage
                    color: "#a4322c"
                    wrapMode: Text.WrapAnywhere
                }
                Button {
                    text: "重试保存"
                    onClicked: page.controller.saveProgress()
                }
            }
        }

        // 图片画布只接收控制器提供的 URL；目录扫描、排序和边界校验都留在 C++ 层。
        Rectangle {
            id: canvas
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#f0f1f3"
            clip: true

            Image {
                id: pageImage
                anchors.fill: parent
                anchors.margins: 8
                source: page.controller.imageSource
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                retainWhileLoading: false
                // 只解码当前页到接近视口的尺寸，不把整本漫画或超大原图留在缓存中。
                sourceSize.width: Math.max(1, Math.ceil(width * Screen.devicePixelRatio))
                sourceSize.height: Math.max(1, Math.ceil(height * Screen.devicePixelRatio))
                cache: false
                onStatusChanged: {
                    if (status === Image.Ready) {
                        // 控制器再次核对页码和 URL，排除快速翻页时过期的加载结果。
                        page.controller.confirmPageLoaded(page.controller.currentPage, source)
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: pageImage.status === Image.Loading
                visible: running
            }

            // 本地文件被移走或损坏时允许重试，也可以继续翻页；失败页不会覆盖已保存位置。
            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 400)
                spacing: 12
                visible: pageImage.status === Image.Error
                Label {
                    width: parent.width
                    text: "图片加载失败"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 16
                }
                Label {
                    width: parent.width
                    text: page.controller.pageLabel
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAnywhere
                    color: "#6c7480"
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "重试"
                    onClicked: {
                        // 清空后重新绑定当前页 URL，使修复后的本地文件可以重新读取。
                        pageImage.source = ""
                        Qt.callLater(function() {
                            pageImage.source = Qt.binding(function() {
                                return page.controller.imageSource
                            })
                        })
                    }
                }
            }
        }

        // 底部页码导航：按钮在首尾禁用，C++ 层同时校验边界，避免只依赖界面限制。
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 12
            Item { Layout.fillWidth: true }
            IconButton {
                glyph: "\uE76B"
                toolTip: "上一页"
                enabled: page.controller.currentPage > 1
                onClicked: page.controller.previousPage()
            }
            TextField {
                id: pageNumber
                // 合法页码按回车才跳转；结束编辑后恢复显示控制器实际页码。
                Layout.preferredWidth: 74
                implicitHeight: 36
                text: String(page.controller.currentPage)
                horizontalAlignment: Text.AlignHCenter
                selectByMouse: true
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: "阅读页码"
                validator: IntValidator { bottom: 1; top: Math.max(1, page.controller.pageCount) }
                onAccepted: {
                    if (acceptableInput) {
                        page.controller.goToPage(Number(text))
                        page.forceActiveFocus()
                    }
                }
                onEditingFinished: text = String(page.controller.currentPage)
            }
            Label { text: "/ " + page.controller.pageCount; color: "#657082" }
            IconButton {
                glyph: "\uE76C"
                toolTip: "下一页"
                enabled: page.controller.currentPage < page.controller.pageCount
                onClicked: page.controller.nextPage()
            }
            Item { Layout.fillWidth: true }
        }
    }
}
