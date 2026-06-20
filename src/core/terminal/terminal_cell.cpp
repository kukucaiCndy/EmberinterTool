#include "terminal_cell.h"

uint32_t ColorPalette::color256(int index) const
{
    if (index < 0) return defaultFg;
    if (index < 16) return colors[index];

    // 216 色 (16-231): 6x6x6 RGB 色彩立方体
    // 返回 ABGR 格式: (A<<24)|(B<<16)|(G<<8)|R
    if (index < 232) {
        int i = index - 16;
        int r = (i % 6);       // 0-5
        int g = ((i / 6) % 6); // 0-5
        int b = (i / 36);      // 0-5
        // 0-5 映射: 0->0, 1->95, 2->135, 3->175, 4->215, 5->255
        auto v = [](int c) -> uint8_t {
            if (c == 0) return 0;
            return static_cast<uint8_t>(55 + c * 40);
        };
        uint8_t rv = v(r), gv = v(g), bv = v(b);
        return (255u << 24) | (static_cast<uint32_t>(bv) << 16) |
               (static_cast<uint32_t>(gv) << 8) | rv;
    }

    // 24 级灰度 (232-255)
    if (index < 256) {
        uint8_t gray = static_cast<uint8_t>(8 + (index - 232) * 10);
        return (255u << 24) | (static_cast<uint32_t>(gray) << 16) |
               (static_cast<uint32_t>(gray) << 8) | gray;
    }

    return defaultFg;
}

uint32_t ColorPalette::cellFg(const TerminalCell& cell) const
{
    if (cell.inverse) return cellBg(cell);
    if (cell.fgIsRgb) {
        // RGB → ABGR: (A<<24)|(B<<16)|(G<<8)|R
        return (255u << 24) |
               (static_cast<uint32_t>(cell.rgbFg[2]) << 16) |
               (static_cast<uint32_t>(cell.rgbFg[1]) << 8) |
               cell.rgbFg[0];
    }
    if (cell.fgIsDefault) return defaultFg;
    return color256(cell.fg);
}

uint32_t ColorPalette::cellBg(const TerminalCell& cell) const
{
    if (cell.inverse) return cellFg(cell);
    if (cell.bgIsRgb) {
        // RGB → ABGR
        return (255u << 24) |
               (static_cast<uint32_t>(cell.rgbBg[2]) << 16) |
               (static_cast<uint32_t>(cell.rgbBg[1]) << 8) |
               cell.rgbBg[0];
    }
    if (cell.bgIsDefault) return defaultBg;
    return color256(cell.bg);
}
