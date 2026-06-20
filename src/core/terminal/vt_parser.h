#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

/// VT 解析器 - 完整的 ANSI/VT100/VT220/XTerm 转义序列解析器
/// 参考 xterm.js Parser 和 VT500 系列规范
class VtParser {
public:
    /// 解析状态
    enum class State : uint8_t {
        Normal,     // 普通文本
        Esc,        // ESC 已接收
        Csi,        // CSI (ESC [)
        CsiPrivate, // CSI ? (DEC 私有模式)
        Osc,        // OSC (ESC ])
        OscEnd,     // OSC 结束 (ESC \ 或 BEL)
        Dcs,        // DCS (ESC P)
        DcsIgnore,  // DCS 忽略
        Sos,        // SOS (ESC X) - 忽略
        Pm,         // PM (ESC ^) - 忽略
        Apc,        // APC (ESC _) - 忽略
    };

    /// 解析产生的事件
    struct Event {
        enum Type : uint8_t {
            Print,          // 打印字符
            Execute,        // 执行控制字符 (C0)
            CsiDispatch,    // CSI 序列执行
            OscDispatch,    // OSC 序列执行
            EscDispatch,    // ESC 序列执行
            DcsHook,        // DCS 开始
            DcsPut,         // DCS 数据
            DcsUnhook,      // DCS 结束
        } type;

        // Print
        uint32_t ch = 0;           // 字符码点
        int width = 1;             // 字符宽度 (1 or 2)

        // Execute (C0 控制字符)
        uint8_t ctrl = 0;          // 控制字符值

        // CSI
        char csiFinal = 0;         // CSI 最终字节
        bool csiPrivate = false;   // 是否 DEC 私有模式 (? 前缀)
        std::vector<int> csiParams; // CSI 参数
        std::string csiIntermediates; // 中间字节

        // OSC
        int oscCommand = -1;       // OSC 命令号
        std::string oscData;       // OSC 数据

        // ESC
        char escFinal = 0;         // ESC 最终字节
        std::string escIntermediates; // 中间字节

        // DCS
        char dcsFinal = 0;
        bool dcsPrivate = false;
        std::vector<int> dcsParams;
        std::string dcsIntermediates;
        std::string dcsData;
    };

    using EventHandler = std::function<void(const Event&)>;

    explicit VtParser();

    /// 处理原始字节数据
    void parse(const char* data, size_t len);

    /// 处理 UTF-8 字符串
    void parse(const std::string& data);

    /// 设置事件处理器
    void setHandler(EventHandler handler) { handler_ = std::move(handler); }

    /// 重置解析器状态
    void reset();

    /// 当前状态 (调试用)
    State state() const { return state_; }

    /// 判断字符宽度
    static int charWidth(uint32_t ch);

private:
    void transition(State newState);
    void emitEvent(const Event& ev);
    void processByte(uint8_t byte);
    void processUtf8(uint8_t byte);

    // OSC 数据收集
    void oscCollect(uint8_t byte);
    void oscDispatch();

    // DCS 处理
    void dcsHook();
    void dcsCollect(uint8_t byte);
    void dcsUnhook();

    // ESC 处理
    void escDispatch();

    // UTF-8 解码
    uint32_t utf8Char_ = 0;
    int utf8Remaining_ = 0;
    int utf8BytesProcessed_ = 0;

    State state_;
    EventHandler handler_;

    // CSI 收集
    std::vector<int> csiParams_;
    std::string csiIntermediates_;
    bool csiPrivate_ = false;
    int csiCurrentParam_ = 0;
    bool csiParamStarted_ = false;

    // OSC 收集
    std::string oscBuffer_;
    int oscCommand_ = -1;

    // DCS 收集
    std::vector<int> dcsParams_;
    std::string dcsIntermediates_;
    bool dcsPrivate_ = false;
    std::string dcsBuffer_;
    int dcsCurrentParam_ = 0;
    bool dcsParamStarted_ = false;

    // ESC 收集
    std::string escIntermediates_;
};
