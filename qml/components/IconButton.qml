import QtQuick
import QtQuick.Controls.Basic

/*
 * 通用图标工具按钮，供导航栏、漫画卡片和阅读工具栏复用。
 * glyph 是 Segoe MDL2 Assets 字库中的图标码，toolTip 同时提供悬停提示和无障碍名称。
 * 使用 ToolButton 自带的 clicked、enabled、checked 状态，不在组件内部执行业务操作。
 * 尺寸固定，颜色覆盖选中、悬停、按下和禁用状态；当前字库面向 Windows，跨平台时需替换。
 */
ToolButton {
    id: control
    property string glyph: "" // 调用方传入对应图标的 Unicode 码。
    property string toolTip: "" // 操作名称，供提示气泡和辅助技术读取。

    implicitWidth: 46
    implicitHeight: 38
    hoverEnabled: true
    Accessible.name: toolTip
    ToolTip.visible: hovered && toolTip.length > 0
    ToolTip.text: toolTip

    contentItem: Label {
        text: control.glyph
        font.family: "Segoe MDL2 Assets"
        font.pixelSize: 20
        color: control.enabled ? "#25282d" : "#969aa1"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 8
        color: control.checked ? "#d5e3ff"
                               : control.down ? "#e1e5eb"
                                              : control.hovered ? "#edf0f5" : "transparent"
    }
}
