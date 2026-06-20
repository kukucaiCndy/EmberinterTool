#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QColor>
#include <vector>

#include "terminal_cell.h"

class TerminalBuffer;

/// 终端渲染快照 - 从 GUI 线程复制到渲染线程的数据
struct TerminalSnapshot {
    double devicePixelRatio = 1.0; // 逻辑像素 -> 物理像素比例
    int viewportWidth = 0;
    int viewportHeight = 0;
    float scrollOffset = 0.0f;   // 像素滚动偏移 (物理像素)
    int scrollbackLines = 0;     // scrollback 总行数
    int firstLineIdx = 0;        // rows[0] 对应的全局行索引
    bool cursorVisible = true;
    int cursorStyle = 0;       // 0=Block, 1=Underline, 2=Bar
    uint32_t cursorColor = 0;  // ABGR
    uint32_t defaultBg = 0;    // ABGR
    int cursorRow = 0;         // active screen 中的行
    int cursorCol = 0;
    float cellWidth = 8.0f;    // 物理像素
    float cellHeight = 16.0f;  // 物理像素

    // 选区 (全局行索引, -1 表示无选区)
    int selStartLine = -1, selStartCol = -1;
    int selEndLine = -1, selEndCol = -1;
    bool hasSelection = false;

    /// 行数据快照
    struct CellSnapshot {
        uint32_t ch;
        CellWidth width;
        uint32_t fgColor;  // 已解析的 ABGR 前景色
        uint32_t bgColor;  // 已解析的 ABGR 背景色
        bool invisible;
        bool bold;
        bool italic;
        bool inverse;
        // 字形纹理坐标 (在 synchronize 中预计算)
        float u0, v0, u1, v1;
    };

    struct RowSnapshot {
        std::vector<CellSnapshot> cells;
    };

    std::vector<RowSnapshot> rows;
};

/// OpenGL 终端渲染器 - 使用字形图集纹理批量绘制
class TerminalRenderer {
public:
    TerminalRenderer();
    ~TerminalRenderer();

    /// 初始化 OpenGL 资源 (在 OpenGL 上下文中调用)
    void init(QOpenGLFunctions* gl);

    /// 释放 OpenGL 资源
    void cleanup(QOpenGLFunctions* gl);

    /// 渲染终端内容 (使用快照数据, 线程安全)
    void render(QOpenGLFunctions* gl,
                const TerminalSnapshot& snapshot,
                GLuint atlasTextureId);

    /// 设置字体参数
    void setFontMetrics(int cellWidth, int cellHeight);

private:
    struct Vertex {
        float x, y;       // 屏幕坐标
        float u, v;       // 纹理坐标
        uint32_t color;   // 颜色 (ABGR)
        uint32_t flags;   // 标志位
    };

    void buildBackgroundVertices(const TerminalSnapshot& snapshot);
    void buildTextVertices(const TerminalSnapshot& snapshot);
    void buildCursorVertices(const TerminalSnapshot& snapshot);
    void drawBackgroundBatch(QOpenGLFunctions* gl, int viewportWidth, int viewportHeight);
    void drawTextBatch(QOpenGLFunctions* gl, int viewportWidth, int viewportHeight,
                       GLuint atlasTextureId);
    void drawCursorBatch(QOpenGLFunctions* gl, int viewportWidth, int viewportHeight);

    // 着色器程序
    QOpenGLShaderProgram* bgProgram_ = nullptr;
    QOpenGLShaderProgram* textProgram_ = nullptr;

    // 顶点缓冲
    QOpenGLBuffer vbo_;

    // 顶点数据
    std::vector<Vertex> bgVertices_;
    std::vector<Vertex> textVertices_;
    std::vector<Vertex> cursorVertices_;

    // 字体参数
    int cellWidth_ = 8;
    int cellHeight_ = 16;

    // 是否已初始化
    bool initialized_ = false;
};
