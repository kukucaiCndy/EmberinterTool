#include "vt_parser.h"
#include <cstring>
#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

VtParser::VtParser()
    : state_(State::Normal)
{
}

void VtParser::parse(const char* data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        processByte(static_cast<uint8_t>(data[i]));
    }
}

void VtParser::parse(const std::string& data)
{
    parse(data.data(), data.size());
}

void VtParser::reset()
{
    state_ = State::Normal;
    utf8Char_ = 0;
    utf8Remaining_ = 0;
    utf8BytesProcessed_ = 0;
    utf8MinCodePoint_ = 0;
    csiParams_.clear();
    csiIntermediates_.clear();
    csiPrivate_ = false;
    csiCurrentParam_ = 0;
    csiParamStarted_ = false;
    oscBuffer_.clear();
    oscCommand_ = -1;
    oscPendingDispatch_ = false;
    dcsParams_.clear();
    dcsIntermediates_.clear();
    dcsPrivate_ = false;
    dcsBuffer_.clear();
    dcsFinal_ = 0;
    escIntermediates_.clear();
}

void VtParser::transition(State newState)
{
    state_ = newState;
}

void VtParser::emitEvent(const Event& ev)
{
    if (ev.type == Event::CsiDispatch) {
        std::string params;
        for (size_t i = 0; i < ev.csiParams.size(); i++) {
            if (i) params += ";";
            params += std::to_string(ev.csiParams[i]);
        }
        SPDLOG_DEBUG("[VT-CSI] final='{}' private={} params={}", ev.csiFinal, ev.csiPrivate, params);
    } else if (ev.type == Event::Print) {
        if (ev.ch < 0x20 || ev.ch == 0x7F) {
            SPDLOG_DEBUG("[VT-PRINT] ch=<{:02X}> width={}", ev.ch, ev.width);
        } else if (ev.ch == '[' || ev.ch == ']' || ev.ch == 'm' || ev.ch == 'H') {
            SPDLOG_DEBUG("[VT-PRINT] ch='{}' width={}", static_cast<char>(ev.ch), ev.width);
        }
    } else if (ev.type == Event::Execute) {
        SPDLOG_DEBUG("[VT-EXEC] ctrl=<{:02X}>", ev.ctrl);
    } else if (ev.type == Event::OscDispatch) {
        SPDLOG_DEBUG("[VT-OSC] cmd={} data_len={}", ev.oscCommand, ev.oscData.size());
    } else if (ev.type == Event::EscDispatch) {
        SPDLOG_DEBUG("[VT-ESC] final='{}'", ev.escFinal);
    }
    if (handler_) handler_(ev);
}

void VtParser::processByte(uint8_t byte)
{
    while (true) {
    // C0 控制字符在任何状态下都要优先处理 (包括中断不完整的 UTF-8 序列)
    if (byte < 0x20) {
        // C0 控制字符会终止当前未完成的 UTF-8 多字节序列
        utf8Remaining_ = 0;

        switch (byte) {
        case 0x00: // NUL - 忽略
            return;
        case 0x07: // BEL
            if (state_ == State::Osc) {
                oscDispatch();
                transition(State::Normal);
                return;
            }
            {
                Event ev;
                ev.type = Event::Execute;
                ev.ctrl = byte;
                emitEvent(ev);
            }
            return;
        case 0x08: // BS
        case 0x09: // HT
        case 0x0A: // LF
        case 0x0B: // VT
        case 0x0C: // FF
        case 0x0D: // CR
        {
            Event ev;
            ev.type = Event::Execute;
            ev.ctrl = byte;
            emitEvent(ev);
            return;
        }
        case 0x1B: // ESC
            // 终止当前序列
            if (state_ == State::Osc) {
                // 可能是 OSC 结束 (ESC \)
                oscPendingDispatch_ = true;
                transition(State::OscEnd);
                return;
            }
            if (state_ == State::Dcs || state_ == State::DcsData) {
                // DCS 由 ST (ESC \) 结束，先进入 OscEnd 等待 '\'
                // 但需先触发 unhook 事件
                if (state_ == State::DcsData) {
                    dcsUnhook();
                }
                oscPendingDispatch_ = false;
                transition(State::OscEnd);
                return;
            }
            transition(State::Esc);
            return;
        case 0x18: // CAN - 中止序列
        case 0x1A: // SUB - 中止序列
            if (state_ != State::Normal) {
                transition(State::Normal);
            }
            return;
        default:
            // 其他 C0 控制字符在序列中忽略, 在 Normal 状态执行
            if (state_ == State::Normal) {
                Event ev;
                ev.type = Event::Execute;
                ev.ctrl = byte;
                emitEvent(ev);
            }
            return;
        }
    }

    // DEL (0x7F) - 在大多数状态下忽略
    if (byte == 0x7F) {
        return;
    }

    // UTF-8 多字节序列的续字节 (0x80-0xBF) 优先处理，
    // 不能被 C1 控制字符拦截，否则含 0x80-0x9F 续字节的中文会被截断
    // (如 "一"=E4 B8 80, 0x80 会被误认为 C1 IND)
    if (utf8Remaining_ > 0 && byte >= 0x80 && byte <= 0xBF) {
        processUtf8(byte);
        return;
    }

    // C1 控制字符 (0x80-0x9F) - 作为 8-bit 等价 ESC 序列处理
    // 仅在非 Normal 状态或无待完成 UTF-8 序列时处理
    if (byte >= 0x80 && byte <= 0x9F && state_ != State::Normal) {
        switch (byte) {
        case 0x84: // IND - 索引 (等价 ESC D)
        {
            Event ev;
            ev.type = Event::EscDispatch;
            ev.escFinal = 'D';
            emitEvent(ev);
            return;
        }
        case 0x85: // NEL - 下一行 (等价 ESC E)
        {
            Event ev;
            ev.type = Event::EscDispatch;
            ev.escFinal = 'E';
            emitEvent(ev);
            return;
        }
        case 0x88: // HTS - 水平制表设置 (等价 ESC H)
        {
            Event ev;
            ev.type = Event::EscDispatch;
            ev.escFinal = 'H';
            emitEvent(ev);
            return;
        }
        case 0x8D: // RI - 反向索引 (等价 ESC M)
        {
            Event ev;
            ev.type = Event::EscDispatch;
            ev.escFinal = 'M';
            emitEvent(ev);
            return;
        }
        case 0x8E: // SS2
        case 0x8F: // SS3
            return; // 忽略
        case 0x90: // DCS
            transition(State::Dcs);
            dcsParams_.clear();
            dcsIntermediates_.clear();
            dcsPrivate_ = false;
            dcsBuffer_.clear();
            dcsCurrentParam_ = 0;
            dcsParamStarted_ = false;
            return;
        case 0x9B: // CSI
            transition(State::Csi);
            csiParams_.clear();
            csiIntermediates_.clear();
            csiPrivate_ = false;
            csiCurrentParam_ = 0;
            csiParamStarted_ = false;
            return;
        case 0x9D: // OSC
            transition(State::Osc);
            oscBuffer_.clear();
            oscCommand_ = -1;
            return;
        case 0x98: // SOS
            transition(State::Sos);
            return;
        case 0x9E: // PM
            transition(State::Pm);
            return;
        case 0x9F: // APC
            transition(State::Apc);
            return;
        default:
            return; // 忽略其他 C1
        }
    }

    // 状态机处理
    switch (state_) {
    case State::Normal:
        processUtf8(byte);
        break;

    case State::Esc:
        if (byte == '[') {
            transition(State::Csi);
            csiParams_.clear();
            csiIntermediates_.clear();
            csiPrivate_ = false;
            csiCurrentParam_ = 0;
            csiParamStarted_ = false;
        } else if (byte == ']') {
            transition(State::Osc);
            oscBuffer_.clear();
            oscCommand_ = -1;
        } else if (byte == 'P') {
            transition(State::Dcs);
            dcsParams_.clear();
            dcsIntermediates_.clear();
            dcsPrivate_ = false;
            dcsBuffer_.clear();
            dcsCurrentParam_ = 0;
            dcsParamStarted_ = false;
        } else if (byte == 'X') {
            transition(State::Sos);
        } else if (byte == '^') {
            transition(State::Pm);
        } else if (byte == '_') {
            transition(State::Apc);
        } else if (byte >= '0' && byte <= '~') {
            // ESC + 最终字节 (可能带中间字节)
            Event ev;
            ev.type = Event::EscDispatch;
            ev.escFinal = static_cast<char>(byte);
            ev.escIntermediates = escIntermediates_;
            emitEvent(ev);
            escIntermediates_.clear();
            transition(State::Normal);
        } else if (byte >= 0x20 && byte <= 0x2F) {
            // 中间字节
            escIntermediates_ += static_cast<char>(byte);
        } else {
            // 无效, 回到 Normal
            escIntermediates_.clear();
            transition(State::Normal);
        }
        break;

    case State::Csi:
    case State::CsiPrivate:
        if (byte >= 0x30 && byte <= 0x39) {
            // 数字 (参数)
            csiCurrentParam_ = csiCurrentParam_ * 10 + (byte - 0x30);
            csiParamStarted_ = true;
        } else if (byte == 0x3B) {
            // 分号 (参数分隔符)
            csiParams_.push_back(csiCurrentParam_);
            csiCurrentParam_ = 0;
            csiParamStarted_ = false;
        } else if (byte == 0x3A) {
            // 冒号 (子参数分隔符, 如 SGR 38:2::r:g:b)
            // 作为分号处理简化
            csiParams_.push_back(csiCurrentParam_);
            csiCurrentParam_ = 0;
            csiParamStarted_ = false;
        } else if (byte == 0x3F) {
            // ? 前缀 (DEC 私有模式)
            if (csiParams_.empty() && !csiParamStarted_) {
                csiPrivate_ = true;
                transition(State::CsiPrivate);
            }
        } else if (byte >= 0x20 && byte <= 0x2F) {
            // 中间字节
            csiIntermediates_ += static_cast<char>(byte);
        } else if (byte >= 0x40 && byte <= 0x7E) {
            // 最终字节
            csiParams_.push_back(csiCurrentParam_);
            Event ev;
            ev.type = Event::CsiDispatch;
            ev.csiFinal = static_cast<char>(byte);
            ev.csiParams = csiParams_;
            ev.csiIntermediates = csiIntermediates_;
            ev.csiPrivate = csiPrivate_;
            emitEvent(ev);
            transition(State::Normal);
        } else {
            // 无效字节, 中止
            transition(State::Normal);
        }
        break;

    case State::Osc:
        oscCollect(byte);
        break;

    case State::OscEnd:
        if (byte == '\\') {
            if (oscPendingDispatch_) {
                oscDispatch();
            }
            oscPendingDispatch_ = false;
            transition(State::Normal);
        } else {
            // ESC 后不是 \, 作为新 ESC 序列处理 (用循环代替递归)
            oscPendingDispatch_ = false;
            transition(State::Esc);
            continue;
        }
        break;

    case State::Dcs:
        if (byte >= 0x30 && byte <= 0x39) {
            dcsCurrentParam_ = dcsCurrentParam_ * 10 + (byte - 0x30);
            dcsParamStarted_ = true;
        } else if (byte == 0x3B) {
            dcsParams_.push_back(dcsCurrentParam_);
            dcsCurrentParam_ = 0;
            dcsParamStarted_ = false;
        } else if (byte == 0x3F) {
            if (dcsParams_.empty() && !dcsParamStarted_) {
                dcsPrivate_ = true;
            }
        } else if (byte >= 0x20 && byte <= 0x2F) {
            dcsIntermediates_ += static_cast<char>(byte);
        } else if (byte >= 0x40 && byte <= 0x7E) {
            // 最终字节：触发 hook，进入数据收集状态
            dcsParams_.push_back(dcsCurrentParam_);
            dcsFinal_ = static_cast<char>(byte);
            dcsHook();
            transition(State::DcsData);
        } else {
            // 无效
            transition(State::Normal);
        }
        break;

    case State::DcsData:
        // 收集 DCS 数据字节 (0x20-0x7E)
        // C0 控制字符 (包括 ESC/BEL) 已在 processByte 开头处理
        if (byte >= 0x20 && byte <= 0x7E) {
            dcsCollect(byte);
        }
        // 其他字节 (如 0x80-0x9F C1, 0x7F DEL) 在 DCS 数据中忽略
        break;

    case State::DcsIgnore:
        // 忽略所有数据直到 ESC
        break;

    case State::Sos:
    case State::Pm:
    case State::Apc:
        // 忽略直到 ST (ESC \) 或 BEL
        if (byte == 0x1B) {
            oscPendingDispatch_ = false;
            transition(State::OscEnd);  // 复用 OscEnd 状态
        }
        break;
    }
    break;
    }
}

void VtParser::processUtf8(uint8_t byte)
{
    if (byte < 0x80) {
        // ASCII
        Event ev;
        ev.type = Event::Print;
        ev.ch = byte;
        ev.width = charWidth(byte);
        emitEvent(ev);
        return;
    }

    if (utf8Remaining_ == 0) {
        // 开始新的 UTF-8 序列
        if ((byte & 0xE0) == 0xC0) {
            utf8Char_ = byte & 0x1F;
            utf8Remaining_ = 1;
            utf8MinCodePoint_ = 0x80;
        } else if ((byte & 0xF0) == 0xE0) {
            utf8Char_ = byte & 0x0F;
            utf8Remaining_ = 2;
            utf8MinCodePoint_ = 0x800;
        } else if ((byte & 0xF8) == 0xF0) {
            utf8Char_ = byte & 0x07;
            utf8Remaining_ = 3;
            utf8MinCodePoint_ = 0x10000;
        } else {
            // 无效 UTF-8 起始字节, 忽略
            return;
        }
        utf8BytesProcessed_ = 1;
    } else {
        // 续字节
        if ((byte & 0xC0) != 0x80) {
            // 无效续字节, 重置
            utf8Remaining_ = 0;
            return;
        }
        utf8Char_ = (utf8Char_ << 6) | (byte & 0x3F);
        utf8BytesProcessed_++;
        utf8Remaining_--;
    }

    if (utf8Remaining_ == 0) {
        // 校验 overlong 编码和无效 Unicode 码点
        if (utf8Char_ < utf8MinCodePoint_ ||
            utf8Char_ > 0x10FFFF ||
            (utf8Char_ >= 0xD800 && utf8Char_ <= 0xDFFF)) {
            // 无效码点, 忽略
            return;
        }
        Event ev;
        ev.type = Event::Print;
        ev.ch = utf8Char_;
        ev.width = charWidth(utf8Char_);
        emitEvent(ev);
    }
}

// csiCollectParam 和 csiDispatch 已内联到 processByte 中

void VtParser::oscCollect(uint8_t byte)
{
    if (byte == 0x07) {  // BEL 结束 OSC
        oscDispatch();
        transition(State::Normal);
        return;
    }

    if (byte == 0x1B) {
        transition(State::OscEnd);
        return;
    }

    // 解析命令号: OSC 格式为 <command> ; <data>
    // 在第一个分号之前的所有数字字符构成命令号
    if (oscCommand_ == -1) {
        if (byte >= '0' && byte <= '9') {
            oscCommand_ = oscCommand_ == -1 ? 0 : oscCommand_;
            oscCommand_ = oscCommand_ * 10 + (byte - '0');
        } else if (byte == ';') {
            // 分号表示命令号结束, 数据开始
            // oscCommand_ 已为 0 或解析出的数字
            oscCommand_ = std::max(0, oscCommand_);
        } else {
            // 遇到非数字非分号字符, 将整个缓冲视为数据 (命令号为 0)
            oscCommand_ = 0;
        }
    }

    oscBuffer_ += static_cast<char>(byte);
}

void VtParser::oscDispatch()
{
    Event ev;
    ev.type = Event::OscDispatch;
    ev.oscCommand = oscCommand_ >= 0 ? oscCommand_ : 0;

    // 提取数据部分 (第一个分号之后)
    size_t semicolonPos = oscBuffer_.find(';');
    if (semicolonPos != std::string::npos) {
        ev.oscData = oscBuffer_.substr(semicolonPos + 1);
    } else {
        ev.oscData = oscBuffer_;
    }

    emitEvent(ev);
}

void VtParser::dcsHook()
{
    Event ev;
    ev.type = Event::DcsHook;
    ev.dcsParams = dcsParams_;
    ev.dcsIntermediates = dcsIntermediates_;
    ev.dcsPrivate = dcsPrivate_;
    ev.dcsFinal = dcsFinal_;
    emitEvent(ev);
}

void VtParser::dcsCollect(uint8_t byte)
{
    dcsBuffer_ += static_cast<char>(byte);
}

void VtParser::dcsUnhook()
{
    Event ev;
    ev.type = Event::DcsUnhook;
    ev.dcsData = dcsBuffer_;
    emitEvent(ev);
}

void VtParser::escDispatch()
{
    Event ev;
    ev.type = Event::EscDispatch;
    ev.escIntermediates = escIntermediates_;
    emitEvent(ev);
    escIntermediates_.clear();
}

int VtParser::charWidth(uint32_t ch)
{
    // ASCII 控制字符
    if (ch < 0x20) return 0;
    // ASCII 可打印
    if (ch < 0x7F) return 1;
    // DEL
    if (ch == 0x7F) return 0;

    // CJK 统一汉字 (基本区)
    if (ch >= 0x4E00 && ch <= 0x9FFF) return 2;
    // CJK 兼容汉字
    if (ch >= 0xF900 && ch <= 0xFAFF) return 2;
    // CJK 统一汉字扩展 A
    if (ch >= 0x3400 && ch <= 0x4DBF) return 2;
    // CJK 统一汉字扩展 B-I
    if (ch >= 0x20000 && ch <= 0x323AF) return 2;
    // CJK 部首补充 / 康熙部首
    if (ch >= 0x2E80 && ch <= 0x2FDF) return 2;
    // CJK 笔画
    if (ch >= 0x31C0 && ch <= 0x31EF) return 2;
    // 带圈字母数字补充 / CJK 兼容字符
    if (ch >= 0x3200 && ch <= 0x33FF) return 2;

    // 平假名
    if (ch >= 0x3040 && ch <= 0x309F) return 2;
    // 片假名
    if (ch >= 0x30A0 && ch <= 0x30FF) return 2;
    // 韩文
    if (ch >= 0xAC00 && ch <= 0xD7AF) return 2;
    if (ch >= 0x1100 && ch <= 0x11FF) return 2;
    if (ch >= 0x3130 && ch <= 0x318F) return 2;

    // 彝文
    if (ch >= 0xA000 && ch <= 0xA4CF) return 2;

    // 全角符号 / 半角全角形式
    if (ch >= 0xFF01 && ch <= 0xFF60) return 2;
    if (ch >= 0xFFE0 && ch <= 0xFFE6) return 2;
    if (ch >= 0xFF00 && ch <= 0xFFEF) return 2;

    // CJK 兼容形式
    if (ch >= 0xFE30 && ch <= 0xFE4F) return 2;

    // 中文标点
    if (ch >= 0x3000 && ch <= 0x303F) return 2;

    // 箭头和几何形状
    if (ch >= 0x2190 && ch <= 0x21FF) return 2;
    if (ch >= 0x2200 && ch <= 0x22FF) return 2;
    if (ch >= 0x2300 && ch <= 0x23FF) return 2;
    if (ch >= 0x2500 && ch <= 0x257F) return 2;  // 制表符
    if (ch >= 0x2580 && ch <= 0x259F) return 2;  // 方块元素
    if (ch >= 0x25A0 && ch <= 0x25FF) return 2;
    if (ch >= 0x2600 && ch <= 0x26FF) return 2;  // 杂项符号
    if (ch >= 0x2700 && ch <= 0x27BF) return 2;  // Dingbats

    // Emoji 和杂项符号及象形文字
    if (ch >= 0x1F300 && ch <= 0x1F5FF) return 2;  // 杂项符号和象形文字
    if (ch >= 0x1F600 && ch <= 0x1F64F) return 2;  // 表情符号
    if (ch >= 0x1F680 && ch <= 0x1F6FF) return 2;  // 交通和地图符号
    if (ch >= 0x1F700 && ch <= 0x1F77F) return 2;  // 炼金术符号
    if (ch >= 0x1F780 && ch <= 0x1F7FF) return 2;  // 几何图形扩展
    if (ch >= 0x1F800 && ch <= 0x1F8FF) return 2;  // 补充箭头
    if (ch >= 0x1F900 && ch <= 0x1F9FF) return 2;  // 补充符号和象形文字
    if (ch >= 0x1FA00 && ch <= 0x1FA6F) return 2;  // 国际象棋符号
    if (ch >= 0x1FA70 && ch <= 0x1FAFF) return 2;  // 符号和象形文字扩展-A

    // 默认窄字符
    return 1;
}
