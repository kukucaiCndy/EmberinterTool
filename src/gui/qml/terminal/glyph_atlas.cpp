#include "glyph_atlas.h"
#include <QPainter>
#include <cstring>

GlyphAtlas::GlyphAtlas() = default;

GlyphAtlas::~GlyphAtlas()
{
    // OpenGL 资源应在 cleanup() 中释放
}

void GlyphAtlas::setupFont(const QFont& font, int cellWidth, int cellHeight)
{
    font_ = font;
    font_.setStyleHint(QFont::Monospace);
    font_.setHintingPreference(QFont::PreferFullHinting);
    font_.setStyleStrategy(QFont::PreferAntialias);
    // 设置 CJK 字体回退, 确保中日韩文字有合适字体
    font_.setFamilies({font.family(), "Microsoft YaHei", "SimHei", "Noto Sans CJK SC", "PingFang SC"});

    boldFont_ = font_;
    boldFont_.setBold(true);
    boldFont_.setHintingPreference(QFont::PreferFullHinting);
    boldFont_.setStyleStrategy(QFont::PreferAntialias);

    italicFont_ = font_;
    italicFont_.setItalic(true);
    italicFont_.setHintingPreference(QFont::PreferFullHinting);
    italicFont_.setStyleStrategy(QFont::PreferAntialias);

    boldItalicFont_ = font_;
    boldItalicFont_.setBold(true);
    boldItalicFont_.setItalic(true);
    boldItalicFont_.setHintingPreference(QFont::PreferFullHinting);
    boldItalicFont_.setStyleStrategy(QFont::PreferAntialias);

    cellWidth_ = cellWidth;
    cellHeight_ = cellHeight;
}

void GlyphAtlas::initGL(QOpenGLFunctions* gl)
{
    // 重置布局
    cursorX_ = 0;
    cursorY_ = 0;
    rowHeight_ = 0;

    // 创建图集纹理
    if (textureId_ != 0) {
        gl->glDeleteTextures(1, &textureId_);
        textureId_ = 0;
    }

    gl->glGenTextures(1, &textureId_);
    gl->glBindTexture(GL_TEXTURE_2D, textureId_);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 分配空纹理 (RGBA)
    std::vector<uint8_t> zeros(atlasWidth_ * atlasHeight_ * 4, 0);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth_, atlasHeight_,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, zeros.data());

    // 创建离屏图像用于光栅化
    atlasImage_ = QImage(atlasWidth_, atlasHeight_, QImage::Format_RGBA8888);
    atlasImage_.fill(Qt::transparent);

    // 清除缓存
    glyphs_.clear();
    pendingUploads_.clear();
    needsRebuild_ = false;
    glInitialized_ = true;
}

void GlyphAtlas::cleanup(QOpenGLFunctions* gl)
{
    if (textureId_ != 0 && gl) {
        gl->glDeleteTextures(1, &textureId_);
        textureId_ = 0;
    }
    glyphs_.clear();
    atlasImage_ = QImage();
    pendingUploads_.clear();
    glInitialized_ = false;
}

const GlyphInfo* GlyphAtlas::getGlyph(const GlyphKey& key)
{
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) {
        return &it->second;
    }

    // 新字形: 光栅化到图集图像, 记录待上传区域
    GlyphInfo info = addGlyphToAtlas(key);
    auto result = glyphs_.emplace(key, info);

    // 记录待上传区域
    pendingUploads_.emplace_back(info.x, info.y, info.width, info.height);

    return &result.first->second;
}

GlyphInfo GlyphAtlas::addGlyphToAtlas(const GlyphKey& key)
{
    // 全角字符需要双倍宽度
    int glyphWidth = key.wide ? cellWidth_ * 2 : cellWidth_;

    // 检查当前行是否放得下
    if (cursorX_ + glyphWidth > atlasWidth_) {
        cursorX_ = 0;
        cursorY_ += rowHeight_;
        rowHeight_ = 0;
    }

    // 检查图集是否已满
    if (cursorY_ + cellHeight_ > atlasHeight_) {
        cursorX_ = 0;
        cursorY_ = 0;
        rowHeight_ = 0;
        glyphs_.clear();
        atlasImage_.fill(Qt::transparent);
        needsRebuild_ = true;
    }

    // 光栅化字形到图集图像 (白色字形, 颜色由着色器着色)
    rasterizeGlyph(key, atlasImage_, cursorX_, cursorY_, glyphWidth);

    // 记录字形信息
    float u0 = static_cast<float>(cursorX_) / atlasWidth_;
    float v0 = static_cast<float>(cursorY_) / atlasHeight_;
    float u1 = static_cast<float>(cursorX_ + glyphWidth) / atlasWidth_;
    float v1 = static_cast<float>(cursorY_ + cellHeight_) / atlasHeight_;

    GlyphInfo info;
    info.u0 = u0;
    info.v0 = v0;
    info.u1 = u1;
    info.v1 = v1;
    info.x = cursorX_;
    info.y = cursorY_;
    info.width = glyphWidth;
    info.height = cellHeight_;
    info.advance = glyphWidth;

    // 前进光标
    cursorX_ += glyphWidth;
    rowHeight_ = std::max(rowHeight_, cellHeight_);

    return info;
}

void GlyphAtlas::rasterizeGlyph(const GlyphKey& key, QImage& image, int x, int y, int maxW) const
{
    // 选择字体
    const QFont* f = &font_;
    if (key.bold && key.italic) f = &boldItalicFont_;
    else if (key.bold) f = &boldFont_;
    else if (key.italic) f = &italicFont_;

    // 在图集图像上绘制白色字形
    // 颜色由着色器在渲染时着色, 图集只存储 alpha 遮罩
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 裁剪到分配区域, 防止字形溢出影响相邻字符
    painter.setClipRect(x, y, maxW, cellHeight_);

    // 白色字形 + alpha 通道
    painter.setPen(Qt::white);
    painter.setFont(*f);

    QString text;
    if (key.ch < 0x10000) {
        text = QChar(static_cast<ushort>(key.ch));
    } else {
        char32_t cp = key.ch;
        text = QString::fromUcs4(&cp, 1);
    }

    // 居中绘制: 使用 QRect + AlignCenter 确保字符在分配区域内居中
    QRect cellRect(x, y, maxW, cellHeight_);
    painter.drawText(cellRect, Qt::AlignCenter, text);
    painter.end();
}

void GlyphAtlas::uploadPending(QOpenGLFunctions* gl)
{
    if (!glInitialized_ || textureId_ == 0) return;

    // 如果需要全量重建, 先上传整个图集
    if (needsRebuild_) {
        uploadFull(gl);
        return;
    }

    // 增量上传待上传区域
    for (const auto& rect : pendingUploads_) {
        uploadRegion(gl, rect.x(), rect.y(), rect.width(), rect.height());
    }
    pendingUploads_.clear();
}

void GlyphAtlas::uploadFull(QOpenGLFunctions* gl)
{
    if (!glInitialized_ || textureId_ == 0 || atlasImage_.isNull()) return;

    QImage img = atlasImage_.convertedTo(QImage::Format_RGBA8888);
    gl->glBindTexture(GL_TEXTURE_2D, textureId_);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, atlasWidth_, atlasHeight_,
                        GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());

    pendingUploads_.clear();
    needsRebuild_ = false;
}

bool GlyphAtlas::uploadRegion(QOpenGLFunctions* gl, int x, int y, int w, int h) const
{
    QImage region = atlasImage_.copy(x, y, w, h).convertedTo(QImage::Format_RGBA8888);

    gl->glBindTexture(GL_TEXTURE_2D, textureId_);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                        GL_RGBA, GL_UNSIGNED_BYTE, region.constBits());
    return true;
}

void GlyphAtlas::clearCache()
{
    glyphs_.clear();
    cursorX_ = 0;
    cursorY_ = 0;
    rowHeight_ = 0;
    pendingUploads_.clear();
    needsRebuild_ = true;
}
