#pragma once

#include <QOpenGLFunctions>
#include <QImage>
#include <QFont>
#include <QFontMetrics>
#include <unordered_map>
#include <vector>

/// 字形键 - 唯一标识一个字形 (不含颜色, 颜色由着色器着色)
struct GlyphKey {
    uint32_t ch = 0;
    bool bold = false;
    bool italic = false;
    bool wide = false;   // 全角字符 (CJK) 需要双倍宽度

    bool operator==(const GlyphKey& other) const {
        return ch == other.ch && bold == other.bold && italic == other.italic && wide == other.wide;
    }
};

/// 字形键哈希
struct GlyphKeyHash {
    size_t operator()(const GlyphKey& k) const {
        size_t h = k.ch;
        h ^= (k.bold ? 1ULL : 0ULL) << 32;
        h ^= (k.italic ? 1ULL : 0ULL) << 33;
        h ^= (k.wide ? 1ULL : 0ULL) << 34;
        return h;
    }
};

/// 图集中的字形位置
struct GlyphInfo {
    float u0, v0;     // 纹理坐标 (左上)
    float u1, v1;     // 纹理坐标 (右下)
    int x, y;         // 图集中的像素位置
    int width, height; // 字形像素尺寸
    int advance;       // 水平前进量
};

/// 字形纹理图集 - 预渲染字形到纹理, 避免每帧重复光栅化
/// 字形以白色光栅化, 前景色由着色器着色 (参考 xterm.js CharAtlas 设计)
///
/// 线程模型:
/// - setupFont() / getGlyph() / rasterizeGlyph(): 可在任意线程调用 (不需要 GL)
/// - initGL() / uploadPending() / uploadRegion(): 必须在 render() 中调用 (需要 GL 上下文)
class GlyphAtlas {
public:
    GlyphAtlas();
    ~GlyphAtlas();

    /// 设置字体参数 (不需要 GL, 可在 synchronize 中调用)
    void setupFont(const QFont& font, int cellWidth, int cellHeight);

    /// 初始化/重建 OpenGL 纹理 (需要 GL 上下文, 必须在 render 中调用)
    void initGL(QOpenGLFunctions* gl);

    /// 释放 OpenGL 资源 (需要 GL 上下文)
    void cleanup(QOpenGLFunctions* gl);

    /// 获取字形信息, 如果图集中不存在则光栅化并标记待上传
    /// 不需要 GL, 可在 synchronize 中调用
    const GlyphInfo* getGlyph(const GlyphKey& key);

    /// 上传所有待上传的字形到 GPU (需要 GL 上下文, 必须在 render 中调用)
    void uploadPending(QOpenGLFunctions* gl);

    /// 上传整个图集图像到 GPU (需要 GL 上下文, 用于初始化或全量更新)
    void uploadFull(QOpenGLFunctions* gl);

    /// 获取 OpenGL 纹理 ID
    GLuint textureId() const { return textureId_; }

    /// 图集尺寸
    int atlasWidth() const { return atlasWidth_; }
    int atlasHeight() const { return atlasHeight_; }

    /// 单元格尺寸
    int cellWidth() const { return cellWidth_; }
    int cellHeight() const { return cellHeight_; }

    /// 字体变化时需要重建
    bool needsRebuild() const { return needsRebuild_; }
    void markNeedsRebuild() { needsRebuild_ = true; }

    /// 获取字形缓存大小
    size_t glyphCount() const { return glyphs_.size(); }

    /// 是否有待上传的字形
    bool hasPendingUploads() const { return !pendingUploads_.empty(); }

    /// 清除缓存 (配色方案变化时)
    void clearCache();

private:
    GlyphInfo addGlyphToAtlas(const GlyphKey& key);
    void rasterizeGlyph(const GlyphKey& key, QImage& image, int x, int y, int maxW) const;
    bool uploadRegion(QOpenGLFunctions* gl, int x, int y, int w, int h) const;

    // 图集布局
    static constexpr int kAtlasSize = 2048;
    int atlasWidth_ = kAtlasSize;
    int atlasHeight_ = kAtlasSize;
    int cursorX_ = 0;
    int cursorY_ = 0;
    int rowHeight_ = 0;

    // 字体
    QFont font_;
    QFont boldFont_;
    QFont italicFont_;
    QFont boldItalicFont_;
    int cellWidth_ = 0;
    int cellHeight_ = 0;

    // OpenGL 纹理
    GLuint textureId_ = 0;
    bool needsRebuild_ = true;
    bool glInitialized_ = false;

    // 字形缓存
    std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphs_;

    // 离屏绘制
    QImage atlasImage_;

    // 待上传到 GPU 的字形区域列表
    std::vector<QRect> pendingUploads_;
};
