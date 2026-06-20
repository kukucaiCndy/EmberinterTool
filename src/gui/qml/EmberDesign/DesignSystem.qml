pragma Singleton
import QtQuick

/// 尘智 Design System — 单一事实来源的设计令牌
/// 所有组件通过 DesignSystem.xxx 引用，确保全局一致性
QtObject {
    // ═══════════════════════════════════════════════
    // 色彩系统
    // ═══════════════════════════════════════════════

    // 背景层级 (由深到浅)
    readonly property color bgPrimary:   "#0D1117"  // 主背景 (内容区)
    readonly property color bgSecondary: "#161B22"  // 次背景 (侧边栏、TabBar)
    readonly property color bgTertiary:  "#21262D"  // 第三级背景 (输入框、卡片)

    // 语义色
    readonly property color accent:       "#2F81F7"  // 主题蓝
    readonly property color accentHover:  "#58A6FF"  // 主题蓝悬停
    readonly property color accentMuted:  "#1F6FEB"  // 主题蓝深色

    // 文本色
    readonly property color textPrimary:   "#E6EDF3"  // 主文本
    readonly property color textSecondary: "#8B949E"  // 次要文本
    readonly property color textInverse:   "#0D1117"  // 反色文本 (深色背景上)

    // 边框
    readonly property color border: "#30363D"  // 默认边框
    readonly property color borderFocus: accent  // 聚焦边框

    // 状态色
    readonly property color success: "#3FB950"  // 成功/已连接
    readonly property color error:   "#F85149"  // 错误/危险
    readonly property color warning: "#D29922"  // 警告/调试
    readonly property color info:    "#58A6FF"  // 信息

    // 交互态叠加色
    readonly property color hover:   "#FFFFFF08"   // 悬停叠加 (白8%)
    readonly property color selected: "#FFFFFF10"  // 选中叠加 (白10%)
    readonly property color active:  "#FFFFFF14"   // 按下叠加 (白14%)

    // 半透明变体 (用于微交互)
    readonly property color accent10: "#2F81F71A"  // accent 10% opacity
    readonly property color accent15: "#2F81F726"  // accent 15% opacity
    readonly property color error15:  "#F8514926"  // error 15% opacity
    readonly property color success15:"#3FB95026"  // success 15% opacity

    // ═══════════════════════════════════════════════
    // 圆角
    // ═══════════════════════════════════════════════
    readonly property int radiusLg: 8   // 大圆角 (卡片、弹窗)
    readonly property int radiusMd: 6   // 中圆角 (输入框、按钮)
    readonly property int radiusSm: 4   // 小圆角 (标签、徽章)
    readonly property int radiusFull: 99 // 全圆角 (药丸形状)

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

    readonly property int fontSizeXs:  10
    readonly property int fontSizeSm:  11
    readonly property int fontSizeMd:  12
    readonly property int fontSizeLg:  13
    readonly property int fontSizeXl:  14
    readonly property int fontSize2xl: 16
    readonly property int fontSize3xl: 18
    readonly property int fontSize4xl: 22
    readonly property int fontSize5xl: 28

    // 字重
    readonly property int fontWeightNormal: Font.Normal
    readonly property int fontWeightMedium: Font.DemiBold
    readonly property int fontWeightBold:   Font.Bold

    // ═══════════════════════════════════════════════
    // 布局尺寸
    // ═══════════════════════════════════════════════
    readonly property int sidebarWidth:     220  // 侧边栏展开宽度
    readonly property int sidebarCollapsed:  48  // 侧边栏折叠宽度
    readonly property int tabBarHeight:      36  // TabBar 高度
    readonly property int statusBarHeight:   24  // 状态栏高度
    readonly property int sidebarLogoHeight: 48  // 侧边栏 Logo 区高度
    readonly property int toolbarHeight:     40  // 串口 Tab 工具栏高度
    readonly property int sendPanelHeight:   72  // 发送面板高度
    readonly property int termStatusHeight:  32  // 终端状态栏高度
    readonly property int termCmdBarHeight:  36  // 终端命令栏高度
    readonly property int logLineHeight:     22  // 日志行高
    readonly property int sessionItemHeight: 36  // 侧边栏会话条目高度

    // ═══════════════════════════════════════════════
    // 阴影 (用于 Popup/弹窗)
    // ═══════════════════════════════════════════════
    readonly property string shadowPopup: "0 4px 24px rgba(0,0,0,0.4)"
    readonly property string shadowCard:  "0 2px 8px rgba(0,0,0,0.25)"

    // ═══════════════════════════════════════════════
    // 动画时长 (ms)
    // ═══════════════════════════════════════════════
    readonly property int animFast:    100  // 微交互
    readonly property int animNormal:  150  // 悬停色变
    readonly property int animSlow:    200  // 切换过渡
    readonly property int animSlower:  250  // 页面滑动
    readonly property int animSlowest: 300  // Toast

    // ═══════════════════════════════════════════════
    // 终端配色方案
    // ═══════════════════════════════════════════════
    readonly property var terminalSchemes: [
        {
            name: "Catppuccin Dark",
            fg: "#CDD6F4",
            bg: "#1E1E2E"
        },
        {
            name: "Light",
            fg: "#1E1E2E",
            bg: "#EFF1F5"
        },
        {
            name: "Green Phosphor",
            fg: "#00FF41",
            bg: "#0D0D0D"
        },
        {
            name: "Amber",
            fg: "#FFB000",
            bg: "#1A1A1A"
        },
        {
            name: "Nord",
            fg: "#D8DEE9",
            bg: "#2E3440"
        }
    ]

    // ═══════════════════════════════════════════════
    // 日志级别颜色映射
    // ═══════════════════════════════════════════════
    readonly property color logInfo:    textPrimary
    readonly property color logDebug:   "#8B949E"
    readonly property color logWarning: warning
    readonly property color logError:   error
    readonly property color logTX:      "#7EE787"  // 发送 (TX) 绿色
    readonly property color logRX:      "#79C0FF"  // 接收 (RX) 蓝色
    readonly property color logHex:     "#A5D6FF"  // HEX 模式

    // ═══════════════════════════════════════════════
    // 图标 Unicode 码位
    // ═══════════════════════════════════════════════
    readonly property string iconPlus:      "\u002B"   // +
    readonly property string iconClose:     "\u2715"   // ✕
    readonly property string iconExport:    "\u21E9"   // ⇩
    readonly property string iconPause:     "\u23F8"   // ⏸
    readonly property string iconPlay:      "\u25B6"   // ▶
    readonly property string iconSettings:  "\u2699"   // ⚙
    readonly property string iconInfo:      "\u2139"   // ℹ
    readonly property string iconSearch:    "\u2315"   // ⌕
    readonly property string iconChevron:   "\u276F"   // ❯
    readonly property string iconWarning:   "\u26A0"   // ⚠
    readonly property string iconCheck:     "\u2713"   // ✓
    readonly property string iconArrowDown: "\u2193"   // ↓
    readonly property string iconExpand:    "\u00AB"   // «
    readonly property string iconCollapse:  "\u00BB"   // »
}