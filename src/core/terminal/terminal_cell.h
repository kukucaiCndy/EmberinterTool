#pragma once

#include <QChar>
#include <cstdint>

/// 终端单元格宽度类型
enum class CellWidth : uint8_t {
    Narrow = 1,   // 半角字符 (ASCII, 拉丁等)
    Wide   = 2,   // 全角字符 (CJK, emoji等)
    Cont   = 0,   // 宽字符的后续占位格
};

/// 终端字符单元 - 存储单个单元格的完整状态
struct TerminalCell {
    // 字符 (BMP 平面)
    uint32_t ch = ' ';

    // 颜色 (使用紧凑存储)
    // fg/bg: 0-255 = 256色索引 (0=黑, 1=红, ... 15=亮白, 16-231=6x6x6立方体, 232-255=灰度)
    uint16_t fg = 0;  // 前景色索引
    uint16_t bg = 0;  // 背景色索引
    bool fgIsDefault = true;  // true = 使用 defaultFg (fg 字段无效)
    bool bgIsDefault = true;  // true = 使用 defaultBg (bg 字段无效)
    bool fgIsRgb = false;  // true = fg 是 RGB 真彩色 (存储在 rgbFg 中)
    bool bgIsRgb = false;  // true = bg 是 RGB 真彩色 (存储在 rgbBg 中)

    // 真彩色 (仅当 fgIsRgb/bgIsRgb 为 true 时有效)
    uint8_t rgbFg[3] = {0, 0, 0};
    uint8_t rgbBg[3] = {0, 0, 0};

    // 样式标志
    bool bold      = false;
    bool italic    = false;
    bool underline = false;
    bool strikethrough = false;
    bool blink     = false;
    bool inverse   = false;  // 反色显示
    bool invisible = false;  // 隐藏
    bool dim       = false;  // 暗淡

    // 宽度
    CellWidth width = CellWidth::Narrow;

    // 脏标记 (渲染用)
    bool dirty = true;

    void reset() { *this = TerminalCell{}; }

    bool isDefault() const {
        return ch == ' ' && fgIsDefault && bgIsDefault &&
               !fgIsRgb && !bgIsRgb &&
               !bold && !italic && !underline && !strikethrough &&
               !blink && !inverse && !invisible && !dim &&
               width == CellWidth::Narrow;
    }
};

/// 颜色调色板 - 16色基础 + 256色扩展
/// 颜色格式: uint32_t ABGR (bits31-24=A, 23-16=B, 15-8=G, 7-0=R)
/// 在小端系统上内存布局为 [R, G, B, A]，与 GL_UNSIGNED_BYTE 直接兼容
struct ColorPalette {
    /// 标准 16 色 (0-7: 标准, 8-15: 亮色) — ABGR 格式
    /// 默认使用 VGA/Windows Console 配色, 与 MSYS2 mintty 暗色主题接近
    uint32_t colors[16] = {
        0xff000000,             // 0: 黑
        0xff0000aa,             // 1: 红   #AA0000
        0xff00aa00,             // 2: 绿   #00AA00
        0xff0055aa,             // 3: 黄   #AA5500
        0xffaa0000,             // 4: 蓝   #0000AA
        0xffaa00aa,             // 5: 品红 #AA00AA
        0xffaaaa00,             // 6: 青   #00AAAA
        0xffaaaaaa,             // 7: 白   #AAAAAA
        0xff555555,             // 8: 亮黑 #555555
        0xff5555ff,             // 9: 亮红 #FF5555
        0xff55ff55,             // 10: 亮绿 #55FF55
        0xff55ffff,             // 11: 亮黄 #FFFF55
        0xffff5555,             // 12: 亮蓝 #5555FF
        0xffff55ff,             // 13: 亮品红 #FF55FF
        0xffffff55,             // 14: 亮青 #55FFFF
        0xffffffff,             // 15: 亮白 #FFFFFF
    };

    /// 默认前景色 (浅灰)
    uint32_t defaultFg = 0xffaaaaaa;
    /// 默认背景色 (纯黑, MSYS2 暗色终端风格)
    uint32_t defaultBg = 0xff000000;

    /// 获取 256 色中的指定颜色
    uint32_t color256(int index) const;

    /// 获取单元格的前景色 (ABGR for OpenGL)
    uint32_t cellFg(const TerminalCell& cell) const;

    /// 获取单元格的背景色 (ABGR for OpenGL)
    uint32_t cellBg(const TerminalCell& cell) const;
};
