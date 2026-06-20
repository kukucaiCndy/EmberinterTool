#include "terminal_buffer.h"
#include <QString>
#include <algorithm>
#include <cstring>

TerminalBuffer::TerminalBuffer(int cols, int rows, int scrollback)
    : cols_(cols), rows_(rows), scrollbackMax_(scrollback)
{
    screen_.resize(rows_);
    for (auto& line : screen_) {
        line.resize(cols_);
    }
    scrollBottom_ = rows_ - 1;
    tabStops_.resize(cols_, false);
    for (int i = 0; i < cols_; i += 8) tabStops_[i] = true;
    dirtyRows_.resize(rows_, true);
}

void TerminalBuffer::resize(int cols, int rows)
{
    if (cols == cols_ && rows == rows_) return;

    // 保存旧数据
    auto oldScreen = std::move(screen_);
    int oldCols = cols_;
    int oldRows = rows_;

    cols_ = cols;
    rows_ = rows;

    // 重建屏幕
    screen_.resize(rows_);
    for (auto& line : screen_) {
        line.resize(cols_);
    }

    // 复制旧数据
    int copyRows = std::min(oldRows, rows_);
    int copyCols = std::min(oldCols, cols);
    for (int r = 0; r < copyRows; r++) {
        for (int c = 0; c < copyCols; c++) {
            screen_[r][c] = oldScreen[r][c];
        }
    }

    // 重建 tab stops
    tabStops_.resize(cols, false);
    for (int i = 0; i < cols; i += 8) tabStops_[i] = true;

    // 重建脏标记
    dirtyRows_.resize(rows_, true);

    // 更新滚动区域
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;

    // 调整光标位置
    cursor_.col = std::min(cursor_.col, cols_ - 1);
    cursor_.row = std::min(cursor_.row, rows_ - 1);
}

void TerminalBuffer::useAlternateBuffer()
{
    if (usingAlternate_) return;

    // 保存 normal buffer 状态
    altScreen_ = std::move(screen_);
    altScrollback_ = std::move(scrollback_);
    savedCursorAlt_ = cursor_;

    // 创建干净的 alternate buffer
    usingAlternate_ = true;
    screen_.resize(rows_);
    for (auto& line : screen_) {
        line.assign(cols_, TerminalCell{});
    }
    scrollback_.clear();
    cursor_.col = 0;
    cursor_.row = 0;
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    pendingWrap_ = false;
    markAllDirty();
}

void TerminalBuffer::useNormalBuffer()
{
    if (!usingAlternate_) return;

    // 恢复 normal buffer
    usingAlternate_ = false;
    screen_ = std::move(altScreen_);
    scrollback_ = std::move(altScrollback_);
    cursor_ = savedCursorAlt_;

    // 确保屏幕大小正确
    if (static_cast<int>(screen_.size()) != rows_) {
        screen_.resize(rows_);
        for (auto& line : screen_) {
            line.resize(cols_);
        }
    }

    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
    pendingWrap_ = false;
    markAllDirty();
}

// --- 光标操作 ---

void TerminalBuffer::setCursorCol(int col)
{
    cursor_.col = std::clamp(col, 0, cols_ - 1);
    pendingWrap_ = false;
}

void TerminalBuffer::setCursorRow(int row)
{
    int minRow = cursor_.originMode ? scrollTop_ : 0;
    int maxRow = cursor_.originMode ? scrollBottom_ : (rows_ - 1);
    cursor_.row = std::clamp(row, minRow, maxRow);
    pendingWrap_ = false;
}

void TerminalBuffer::setCursorPos(int col, int row)
{
    setCursorRow(row);
    setCursorCol(col);
}

void TerminalBuffer::moveCursor(int dCol, int dRow)
{
    setCursorPos(cursor_.col + dCol, cursor_.row + dRow);
}

void TerminalBuffer::cursorNextLine()
{
    setCursorPos(0, cursor_.row + 1);
}

void TerminalBuffer::cursorPrevLine()
{
    setCursorPos(0, cursor_.row - 1);
}

void TerminalBuffer::saveCursor()
{
    savedCursor_ = cursor_;
}

void TerminalBuffer::restoreCursor()
{
    auto saved = savedCursor_;
    cursor_ = saved;
    cursor_.col = std::min(cursor_.col, cols_ - 1);
    cursor_.row = std::min(cursor_.row, rows_ - 1);
    pendingWrap_ = false;
}

// --- 滚动区域 ---

void TerminalBuffer::setScrollRegion(int top, int bottom)
{
    scrollTop_ = std::clamp(top, 0, rows_ - 1);
    scrollBottom_ = std::clamp(bottom, 0, rows_ - 1);
    if (scrollTop_ >= scrollBottom_) {
        scrollTop_ = 0;
        scrollBottom_ = rows_ - 1;
    }
}

void TerminalBuffer::resetScrollRegion()
{
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
}

// --- 字符写入 ---

void TerminalBuffer::writeChar(uint32_t ch, CellWidth width)
{
    if (pendingWrap_) {
        cursor_.col = 0;
        if (cursor_.row == scrollBottom_) {
            scrollUp(1);
        } else if (cursor_.row < rows_ - 1) {
            cursor_.row++;
        }
        pendingWrap_ = false;
    }

    int charWidth = static_cast<int>(width);

    // 检查是否需要折行 (宽字符在行尾放不下)
    if (cursor_.col + charWidth > cols_) {
        if (autoWrap_) {
            cursor_.col = 0;
            if (cursor_.row == scrollBottom_) {
                scrollUp(1);
            } else if (cursor_.row < rows_ - 1) {
                cursor_.row++;
            }
        } else {
            cursor_.col = cols_ - charWidth;
        }
    }

    ensureRow(cursor_.row);

    // 写入主字符
    TerminalCell& cell = screen_[cursor_.row][cursor_.col];
    cell.ch = ch;
    cell.width = width;
    applyCurrentStyle(cell);
    cell.dirty = true;
    markDirty(cursor_.row);

    // 宽字符: 标记后续格为 Cont
    if (charWidth == 2) {
        if (cursor_.col + 1 < cols_) {
            TerminalCell& cont = screen_[cursor_.row][cursor_.col + 1];
            cont.width = CellWidth::Cont;
            cont.ch = 0;
            cont.dirty = true;
        }
    }

    // 移动光标
    cursor_.col += charWidth;
    if (cursor_.col >= cols_) {
        if (autoWrap_) {
            pendingWrap_ = true;
            cursor_.col = cols_ - 1;  // 光标停在行末
        } else {
            cursor_.col = cols_ - 1;
        }
    }
}

void TerminalBuffer::lineFeed()
{
    pendingWrap_ = false;
    if (cursor_.row == scrollBottom_) {
        scrollUp(1);
    } else if (cursor_.row < rows_ - 1) {
        cursor_.row++;
    }
    // LNM 模式: LF 同时执行 CR (回到列 0)
    if (lineFeedNewLineMode_) {
        cursor_.col = 0;
    }
}

void TerminalBuffer::carriageReturn()
{
    cursor_.col = 0;
    pendingWrap_ = false;
}

void TerminalBuffer::reverseLineFeed()
{
    pendingWrap_ = false;
    if (cursor_.row == scrollTop_) {
        scrollDown(1);
    } else if (cursor_.row > 0) {
        cursor_.row--;
    }
}

// --- 擦除操作 ---

void TerminalBuffer::eraseInDisplay(int mode)
{
    switch (mode) {
    case 0: { // 光标到末尾
        // 当前行: 光标位置到行尾
        for (int c = cursor_.col; c < cols_; c++) {
            screen_[cursor_.row][c].reset();
        }
        markDirty(cursor_.row);
        // 后续行
        for (int r = cursor_.row + 1; r < rows_; r++) {
            for (auto& cell : screen_[r]) cell.reset();
            markDirty(r);
        }
        break;
    }
    case 1: { // 开头到光标
        // 前面的行
        for (int r = 0; r < cursor_.row; r++) {
            for (auto& cell : screen_[r]) cell.reset();
            markDirty(r);
        }
        // 当前行: 开头到光标
        for (int c = 0; c <= cursor_.col; c++) {
            screen_[cursor_.row][c].reset();
        }
        markDirty(cursor_.row);
        break;
    }
    case 2: { // 全部
        for (int r = 0; r < rows_; r++) {
            for (auto& cell : screen_[r]) cell.reset();
            markDirty(r);
        }
        break;
    }
    case 3: // 清除回滚缓冲区
        scrollback_.clear();
        break;
    }
}

void TerminalBuffer::eraseInLine(int mode)
{
    ensureRow(cursor_.row);
    switch (mode) {
    case 0: // 光标到行尾
        for (int c = cursor_.col; c < cols_; c++) {
            screen_[cursor_.row][c].reset();
        }
        break;
    case 1: // 开头到光标
        for (int c = 0; c <= cursor_.col; c++) {
            screen_[cursor_.row][c].reset();
        }
        break;
    case 2: // 整行
        for (auto& cell : screen_[cursor_.row]) cell.reset();
        break;
    }
    markDirty(cursor_.row);
}

void TerminalBuffer::eraseCharacters(int n)
{
    ensureRow(cursor_.row);
    for (int i = 0; i < n && (cursor_.col + i) < cols_; i++) {
        screen_[cursor_.row][cursor_.col + i].reset();
    }
    markDirty(cursor_.row);
}

// --- 插入/删除 ---

void TerminalBuffer::insertLines(int n)
{
    if (cursor_.row < scrollTop_ || cursor_.row > scrollBottom_) return;

    // 在滚动区域内, 从底部删除 n 行, 在光标行插入 n 行空行
    scrollDownInRange(cursor_.row, scrollBottom_, n);
    markAllDirty();
}

void TerminalBuffer::deleteLines(int n)
{
    if (cursor_.row < scrollTop_ || cursor_.row > scrollBottom_) return;

    scrollUpInRange(cursor_.row, scrollBottom_, n);
    markAllDirty();
}

void TerminalBuffer::insertCharacters(int n)
{
    ensureRow(cursor_.row);
    auto& row = screen_[cursor_.row];

    // 右移 n 个字符
    for (int c = cols_ - 1; c >= cursor_.col + n; c--) {
        row[c] = row[c - n];
    }
    // 插入空格
    for (int c = cursor_.col; c < cursor_.col + n && c < cols_; c++) {
        row[c].reset();
    }
    markDirty(cursor_.row);
}

void TerminalBuffer::deleteCharacters(int n)
{
    ensureRow(cursor_.row);
    auto& row = screen_[cursor_.row];

    // 左移 n 个字符
    for (int c = cursor_.col; c < cols_ - n; c++) {
        row[c] = row[c + n];
    }
    // 末尾填充空格
    for (int c = std::max(0, cols_ - n); c < cols_; c++) {
        row[c].reset();
    }
    markDirty(cursor_.row);
}

// --- 滚动 ---

void TerminalBuffer::scrollUp(int n)
{
    scrollUpInRange(scrollTop_, scrollBottom_, n);
}

void TerminalBuffer::scrollDown(int n)
{
    scrollDownInRange(scrollTop_, scrollBottom_, n);
}

void TerminalBuffer::scrollUpInRange(int top, int bottom, int n)
{
    if (n <= 0 || top > bottom) return;
    n = std::min(n, bottom - top + 1);

    // 将滚出的行移入回滚缓冲区 (仅当 top == scrollTop_ == 0 时)
    if (top == 0 && !usingAlternate_) {
        for (int i = 0; i < n; i++) {
            scrollback_.push_back(std::move(screen_[i]));
            if (static_cast<int>(scrollback_.size()) > scrollbackMax_) {
                scrollback_.pop_front();
            }
        }
    }

    // 上移行
    for (int r = top; r <= bottom - n; r++) {
        screen_[r] = std::move(screen_[r + n]);
    }

    // 底部填充空行
    for (int r = bottom - n + 1; r <= bottom; r++) {
        screen_[r].assign(cols_, TerminalCell{});
    }

    for (int r = top; r <= bottom; r++) markDirty(r);
}

void TerminalBuffer::scrollDownInRange(int top, int bottom, int n)
{
    if (n <= 0 || top > bottom) return;
    n = std::min(n, bottom - top + 1);

    // 下移行
    for (int r = bottom; r >= top + n; r--) {
        screen_[r] = std::move(screen_[r - n]);
    }

    // 顶部填充空行
    for (int r = top; r < top + n; r++) {
        screen_[r].assign(cols_, TerminalCell{});
    }

    for (int r = top; r <= bottom; r++) markDirty(r);
}

// --- Tab ---

void TerminalBuffer::setTabStop()
{
    if (cursor_.col >= 0 && cursor_.col < static_cast<int>(tabStops_.size())) {
        tabStops_[cursor_.col] = true;
    }
}

void TerminalBuffer::clearTabStop()
{
    if (cursor_.col >= 0 && cursor_.col < static_cast<int>(tabStops_.size())) {
        tabStops_[cursor_.col] = false;
    }
}

void TerminalBuffer::clearAllTabStops()
{
    std::fill(tabStops_.begin(), tabStops_.end(), false);
}

int TerminalBuffer::nextTabStop(int col) const
{
    for (int c = col + 1; c < cols_; c++) {
        if (c < static_cast<int>(tabStops_.size()) && tabStops_[c]) return c;
    }
    return cols_ - 1;
}

int TerminalBuffer::prevTabStop(int col) const
{
    for (int c = col - 1; c >= 0; c--) {
        if (c < static_cast<int>(tabStops_.size()) && tabStops_[c]) return c;
    }
    return 0;
}

// --- 行操作 ---

std::vector<TerminalCell>& TerminalBuffer::line(int row)
{
    ensureRow(row);
    return screen_[row];
}

const std::vector<TerminalCell>& TerminalBuffer::line(int row) const
{
    return screen_[row];
}

int TerminalBuffer::scrollbackSize() const
{
    return static_cast<int>(scrollback_.size());
}

// --- 标记 ---

void TerminalBuffer::markDirty(int row)
{
    if (row >= 0 && row < static_cast<int>(dirtyRows_.size())) {
        dirtyRows_[row] = true;
    }
}

void TerminalBuffer::markAllDirty()
{
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), true);
}

bool TerminalBuffer::isRowDirty(int row) const
{
    if (row >= 0 && row < static_cast<int>(dirtyRows_.size())) {
        return dirtyRows_[row];
    }
    return false;
}

void TerminalBuffer::clearDirty(int row)
{
    if (row >= 0 && row < static_cast<int>(dirtyRows_.size())) {
        dirtyRows_[row] = false;
    }
}

void TerminalBuffer::clearAllDirty()
{
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), false);
}

// --- 选区 ---

QString TerminalBuffer::selectedText() const
{
    if (!selection_.active) return {};

    int startRow = std::min(selection_.startRow, selection_.endRow);
    int endRow = std::max(selection_.startRow, selection_.endRow);
    int startCol = (startRow == selection_.startRow) ? selection_.startCol : selection_.endCol;
    int endCol = (endRow == selection_.endRow) ? selection_.endCol : selection_.startCol;

    // 确保 startCol <= endCol 在同一行
    if (startRow == endRow && startCol > endCol) std::swap(startCol, endCol);

    QString text;
    for (int r = startRow; r <= endRow; r++) {
        if (r < 0 || r >= rows_) continue;
        int from = (r == startRow) ? startCol : 0;
        int to = (r == endRow) ? endCol : cols_ - 1;

        for (int c = from; c <= to; c++) {
            if (c < 0 || c >= cols_) continue;
            const auto& cell = screen_[r][c];
            if (cell.width != CellWidth::Cont && cell.ch != ' ') {
                text += QChar(cell.ch);
            }
        }
        if (r < endRow) text += '\n';
    }
    return text;
}

// --- 私有方法 ---

void TerminalBuffer::ensureRow(int row)
{
    // screen_ 已在 resize 中分配, 这里只做边界检查
    (void)row;
}

void TerminalBuffer::applyCurrentStyle(TerminalCell& cell)
{
    cell.fg = cursor_.fg;
    cell.bg = cursor_.bg;
    cell.fgIsDefault = cursor_.fgIsDefault;
    cell.bgIsDefault = cursor_.bgIsDefault;
    cell.fgIsRgb = cursor_.fgIsRgb;
    cell.bgIsRgb = cursor_.bgIsRgb;
    std::memcpy(cell.rgbFg, cursor_.rgbFg, 3);
    std::memcpy(cell.rgbBg, cursor_.rgbBg, 3);
    cell.bold = cursor_.bold;
    cell.italic = cursor_.italic;
    cell.underline = cursor_.underline;
    cell.strikethrough = cursor_.strikethrough;
    cell.blink = cursor_.blink;
    cell.inverse = cursor_.inverse;
    cell.invisible = cursor_.invisible;
    cell.dim = cursor_.dim;
}
