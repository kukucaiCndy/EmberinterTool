#pragma once

#include "terminal_cell.h"
#include <vector>
#include <deque>
#include <cstdint>

/// 终端缓冲区 - 支持 Normal/Alternate 双缓冲 + 滚动区域
class TerminalBuffer {
public:
    explicit TerminalBuffer(int cols = 80, int rows = 24, int scrollback = 5000);

    /// 调整缓冲区大小
    void resize(int cols, int rows);

    /// 切换到 Alternate Buffer (全屏应用模式)
    void useAlternateBuffer();
    /// 切换回 Normal Buffer
    void useNormalBuffer();
    /// 当前是否在 Alternate Buffer
    bool isAlternateBuffer() const { return usingAlternate_; }

    // --- 光标操作 ---
    struct CursorState {
        int col = 0;
        int row = 0;
        uint16_t fg = 0;
        uint16_t bg = 0;
        bool fgIsDefault = true;
        bool bgIsDefault = true;
        bool fgIsRgb = false;
        bool bgIsRgb = false;
        uint8_t rgbFg[3] = {};
        uint8_t rgbBg[3] = {};
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikethrough = false;
        bool blink = false;
        bool inverse = false;
        bool invisible = false;
        bool dim = false;
        bool originMode = false;  // DECOM
    };

    CursorState& cursor() { return cursor_; }
    const CursorState& cursor() const { return cursor_; }

    int cursorCol() const { return cursor_.col; }
    int cursorRow() const { return cursor_.row; }

    void setCursorCol(int col);
    void setCursorRow(int row);
    void setCursorPos(int col, int row);
    void moveCursor(int dCol, int dRow);
    void cursorNextLine();
    void cursorPrevLine();

    /// 保存/恢复光标 (DECSC/DECRC)
    void saveCursor();
    void restoreCursor();

    // --- 滚动区域 ---
    void setScrollRegion(int top, int bottom);
    void resetScrollRegion();
    int scrollRegionTop() const { return scrollTop_; }
    int scrollRegionBottom() const { return scrollBottom_; }

    // --- 字符写入 ---
    /// 在光标位置写入字符, 自动处理换行和滚动
    void writeChar(uint32_t ch, CellWidth width);

    /// 换行 (LF): LNM 开启时执行 CR+LF, 否则仅下移光标
    void lineFeed();
    /// 回车 (CR)
    void carriageReturn();
    /// 反向换行 (RI)
    void reverseLineFeed();

    /// LNM 模式设置 (ESC [ 20 h/l)
    void setLineFeedNewLineMode(bool enabled) { lineFeedNewLineMode_ = enabled; }
    bool lineFeedNewLineMode() const { return lineFeedNewLineMode_; }

    // --- 擦除操作 ---
    void eraseInDisplay(int mode);     // CSI J: 0=光标到末尾, 1=开头到光标, 2=全部
    void eraseInLine(int mode);        // CSI K: 0=光标到末尾, 1=开头到光标, 2=整行
    void eraseCharacters(int n);       // CSI X: 删除n个字符(用空格替换)

    // --- 插入/删除 ---
    void insertLines(int n);           // CSI L
    void deleteLines(int n);           // CSI M
    void insertCharacters(int n);      // CSI @
    void deleteCharacters(int n);      // CSI P

    // --- 滚动 ---
    void scrollUp(int n = 1);          // 向上滚动n行
    void scrollDown(int n = 1);        // 向下滚动n行

    // --- Tab ---
    void setTabStop();
    void clearTabStop();
    void clearAllTabStops();
    int nextTabStop(int col) const;
    int prevTabStop(int col) const;

    // --- 行操作 ---
    /// 获取指定行 (row 为视口行号 0-based)
    std::vector<TerminalCell>& line(int row);
    const std::vector<TerminalCell>& line(int row) const;

    /// 获取回滚缓冲区大小
    int scrollbackSize() const;
    /// 获取回滚缓冲区中的行
    const std::deque<std::vector<TerminalCell>>& scrollback() const { return scrollback_; }

    // --- 尺寸信息 ---
    int columns() const { return cols_; }
    int rows() const { return rows_; }
    int scrollbackMax() const { return scrollbackMax_; }

    // --- 标记 ---
    void markDirty(int row);
    void markAllDirty();
    bool isRowDirty(int row) const;
    void clearDirty(int row);
    void clearAllDirty();

    // --- 选区 ---
    struct Selection {
        int startCol = -1, startRow = -1;
        int endCol = -1, endRow = -1;
        bool active = false;
    };

    Selection& selection() { return selection_; }
    const Selection& selection() const { return selection_; }

    /// 获取选区文本
    QString selectedText() const;

private:
    void ensureRow(int row);
    void applyCurrentStyle(TerminalCell& cell);
    void wrapCursor();
    void scrollUpInRange(int top, int bottom, int n);
    void scrollDownInRange(int top, int bottom, int n);

    /// LNM (Line Feed/New Line Mode): true=LF执行CR+LF, false=仅LF
    bool lineFeedNewLineMode_ = true;

    int cols_;
    int rows_;
    int scrollbackMax_;

    // 主缓冲区 (rows_ 行, 每行 cols_ 个 cell)
    std::vector<std::vector<TerminalCell>> screen_;

    // 回滚缓冲区
    std::deque<std::vector<TerminalCell>> scrollback_;

    // Alternate buffer
    std::vector<std::vector<TerminalCell>> altScreen_;
    std::deque<std::vector<TerminalCell>> altScrollback_;
    bool usingAlternate_ = false;

    // 光标
    CursorState cursor_;
    CursorState savedCursor_;
    CursorState savedCursorAlt_;  // alternate buffer 保存的光标

    // 滚动区域
    int scrollTop_ = 0;
    int scrollBottom_ = 0;  // 0 表示 rows_-1

    // Tab stops
    std::vector<bool> tabStops_;

    // 脏行标记
    std::vector<bool> dirtyRows_;

    // 选区
    Selection selection_;

    // 自动换行模式 (DECAWM)
    bool autoWrap_ = true;
    // 待换行标志 (光标在行末, 下一个字符应先换行)
    bool pendingWrap_ = false;
};
