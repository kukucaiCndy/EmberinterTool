pragma Singleton
import QtQuick

/// 尘智 Design System — 单一事实来源的设计令牌
/// 所有组件通过 DesignSystem.xxx 引用，确保全局一致性
/// 支持暗黑/明亮主题动态切换
QtObject {
    // ═══════════════════════════════════════════════
    // 主题控制
    // ═══════════════════════════════════════════════
    // 0=暗黑, 1=明亮
    property int theme: 0
    readonly property bool isDark: theme === 0

    function setTheme(t) {
        theme = (t === 1) ? 1 : 0
    }

    // ═══════════════════════════════════════════════
    // 色彩系统 (根据主题动态切换)
    // ═══════════════════════════════════════════════

    // 背景层级 (由深到浅)
    readonly property color bgPrimary:   isDark ? "#0D1117" : "#FFFFFF"
    readonly property color bgSecondary: isDark ? "#161B22" : "#F6F8FA"
    readonly property color bgTertiary:  isDark ? "#21262D" : "#EAEEF2"

    // 语义色 (明亮主题用更深的蓝色保证对比度)
    readonly property color accent:       isDark ? "#2F81F7" : "#0969DA"
    readonly property color accentHover:  isDark ? "#58A6FF" : "#2188FF"
    readonly property color accentMuted:  isDark ? "#1F6FEB" : "#0969DA"

    // 文本色
    readonly property color textPrimary:   isDark ? "#E6EDF3" : "#1F2328"
    readonly property color textSecondary: isDark ? "#8B949E" : "#656D76"
    readonly property color textInverse:   isDark ? "#0D1117" : "#FFFFFF"

    // 边框
    readonly property color border: isDark ? "#30363D" : "#D0D7DE"
    readonly property color borderFocus: accent  // 聚焦边框

    // 状态色
    readonly property color success: isDark ? "#3FB950" : "#1A7F37"
    readonly property color error:   isDark ? "#F85149" : "#CF222E"
    readonly property color warning: isDark ? "#D29922" : "#9A6700"
    readonly property color info:    isDark ? "#58A6FF" : "#0969DA"

    // 交互态叠加色
    readonly property color hover:   isDark ? "#FFFFFF08" : "#00000008"
    readonly property color selected: isDark ? "#FFFFFF10" : "#00000010"
    readonly property color active:  isDark ? "#FFFFFF14" : "#00000014"

    // 半透明变体 (用于微交互)
    readonly property color accent10: isDark ? "#2F81F71A" : "#0969DA1A"
    readonly property color accent15: isDark ? "#2F81F726" : "#0969DA26"
    readonly property color error15:  isDark ? "#F8514926" : "#CF222E26"
    readonly property color success15: isDark ? "#3FB95026" : "#1A7F3726"

    // ═══════════════════════════════════════════════
    // 圆角
    // ═══════════════════════════════════════════════
    readonly property int radiusLg: 8
    readonly property int radiusMd: 6
    readonly property int radiusSm: 4
    readonly property int radiusFull: 99

    // ═══════════════════════════════════════════════
    // 间距系统 (4px 基准)
    // ═══════════════════════════════════════════════
    readonly property int spaceXs: 4
    readonly property int spaceSm: 8
    readonly property int spaceMd: 12
    readonly property int spaceLg: 16
    readonly property int spaceXl: 24
    readonly property int space2xl: 32

    // ═══════════════════════════════════════════════
    // 字体
    // ═══════════════════════════════════════════════
    readonly property string fontBody:  "Inter, system-ui, sans-serif"
    readonly property string fontHead:  "Geist, system-ui, sans-serif"
    readonly property string fontMono:  "Cascadia Code, Consolas, monospace"

    // 可调字体大小 (由设置对话框控制)
    property int fontScale: 0  // 0=默认, 正数=放大, 负数=缩小
    readonly property int fontSizeXs:  10 + fontScale
    readonly property int fontSizeSm:  11 + fontScale
    readonly property int fontSizeMd:  12 + fontScale
    readonly property int fontSizeLg:  13 + fontScale
    readonly property int fontSizeXl:  14 + fontScale
    readonly property int fontSize2xl: 16 + fontScale
    readonly property int fontSize3xl: 18 + fontScale
    readonly property int fontSize4xl: 22 + fontScale
    readonly property int fontSize5xl: 28 + fontScale

    // 字重
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightMedium: Font.DemiBold
    readonly property int fontWeightBold:   Font.Bold

    // ═══════════════════════════════════════════════
    // 布局尺寸
    // ═══════════════════════════════════════════════
    readonly property int sidebarWidth:     220
    readonly property int sidebarCollapsed: 48
    readonly property int tabBarHeight:      36
    readonly property int statusBarHeight:   24
    readonly property int sidebarLogoHeight: 48
    readonly property int toolbarHeight:     40
    readonly property int sendPanelHeight:   72
    readonly property int termStatusHeight:  32
    readonly property int termCmdBarHeight:  36
    readonly property int logLineHeight:     22
    readonly property int sessionItemHeight: 36

    // ═══════════════════════════════════════════════
    // 动画时长 (ms)
    // ═══════════════════════════════════════════════
    readonly property int animFast:    100
    readonly property int animNormal:  150
    readonly property int animSlow:    200
    readonly property int animSlower:  250
    readonly property int animSlowest: 300

    // ═══════════════════════════════════════════════
    // 终端配色方案
    // ═══════════════════════════════════════════════
    readonly property var terminalSchemes: [
        { name: "MSYS2 Dark", fg: "#CCCCCC", bg: "#000000" },
        { name: "VS Code Dark+", fg: "#D4D4D4", bg: "#1E1E1E" },
        { name: "VS Code Dark", fg: "#CCCCCC", bg: "#181818" },
        { name: "Light", fg: "#1E1E2E", bg: "#EFF1F5" },
        { name: "Green Phosphor", fg: "#00FF41", bg: "#0D0D0D" },
        { name: "Amber", fg: "#FFB000", bg: "#1A1A1A" },
        { name: "Nord", fg: "#D8DEE9", bg: "#2E3440" }
    ]

    // ═══════════════════════════════════════════════
    // 日志级别颜色映射
    // ═══════════════════════════════════════════════
    readonly property color logInfo:    textPrimary
    readonly property color logDebug:   textSecondary
    readonly property color logWarning: warning
    readonly property color logError:   error
    readonly property color logTX:      isDark ? "#7EE787" : "#1A7F37"
    readonly property color logRX:      isDark ? "#79C0FF" : "#0969DA"
    readonly property color logHex:     isDark ? "#A5D6FF" : "#0969DA"

    // ═══════════════════════════════════════════════
    // 图标 Unicode 码位
    // ═══════════════════════════════════════════════
    readonly property string iconPlus:      "\u002B"
    readonly property string iconClose:     "\u2715"
    readonly property string iconExport:    "\u21E9"
    readonly property string iconPause:     "\u23F8"
    readonly property string iconPlay:      "\u25B6"
    readonly property string iconSettings:  "\u2699"
    readonly property string iconInfo:      "\u2139"
    readonly property string iconSearch:    "\u2315"
    readonly property string iconChevron:   "\u276F"
    readonly property string iconWarning:   "\u26A0"
    readonly property string iconCheck:     "\u2713"
    readonly property string iconArrowDown: "\u2193"
    readonly property string iconExpand:    "\u00AB"
    readonly property string iconCollapse:  "\u00BB"
}
