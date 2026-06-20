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
    uint32_t colors[16] = {
        0xff000000,             // 0: 黑   (R=0, G=0, B=0, A=255)
        0xff000080,             // 1: 红   (R=128, G=0, B=0, A=255)
        0xff008000,             // 2: 绿   (R=0, G=128, B=0, A=255)
        0xff008080,             // 3: 黄   (R=128, G=128, B=0, A=255)
        0xff800000,             // 4: 蓝   (R=0, G=0, B=128, A=255)
        0xff800080,             // 5: 品红 (R=128, G=0, B=128, A=255)
        0xff808000,             // 6: 青   (R=0, G=128, B=128, A=255)
        0xffc0c0c0,             // 7: 白   (R=192, G=192, B=192, A=255)
        0xff808080,             // 8: 亮黑
        0xff0000ff,             // 9: 亮红 (R=255, G=0, B=0, A=255)
        0xff00ff00,             // 10: 亮绿 (R=0, G=255, B=0, A=255)
        0xff00ffff,             // 11: 亮黄 (R=255, G=255, B=0, A=255)
        0xffff0000,             // 12: 亮蓝 (R=0, G=0, B=255, A=255)
        0xffff00ff,             // 13: 亮品红
        0xffffff00,             // 14: 亮青
        0xffffffff,             // 15: 亮白
    };

    /// 默认前景色 (白色不透明)
    uint32_t defaultFg = 0xffffffff;
    /// 默认背景色 (深色背景)
    uint32_t defaultBg = 0xff1e1e2e;

    /// 获取 256 色中的指定颜色
    uint32_t color256(int index) const;

    /// 获取单元格的前景色 (ABGR for OpenGL)
    uint32_t cellFg(const TerminalCell& cell) const;

    /// 获取单元格的背景色 (ABGR for OpenGL)
    uint32_t cellBg(const TerminalCell& cell) const;
};
