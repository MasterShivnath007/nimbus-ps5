#include "gfx/font.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace nimbus::gfx {

namespace {
constexpr int kPadding = 2;

uint64_t glyph_key(int font, int px, char32_t cp) {
    return (static_cast<uint64_t>(font) << 48) | (static_cast<uint64_t>(px) << 32) |
           static_cast<uint64_t>(cp);
}
}  // namespace

FontAtlas::~FontAtlas() {
    for (auto &font : fonts_) delete static_cast<stbtt_fontinfo *>(font.info);
}

bool FontAtlas::init(int size) {
    atlas_size_ = size;
    std::vector<uint8_t> blank(static_cast<size_t>(size) * size, 0);
    if (!texture_.create_r8(size, size, blank.data(), false)) return false;
    glBindTexture(GL_TEXTURE_2D, texture_.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

int FontAtlas::add_font(const std::string &path) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
        std::fprintf(stderr, "[font] cannot open %s\n", path.c_str());
        return -1;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    FontData data;
    if (size <= 0) {
        std::fclose(file);
        return -1;
    }
    data.bytes.resize(static_cast<size_t>(size));
    const size_t read = std::fread(data.bytes.data(), 1, data.bytes.size(), file);
    std::fclose(file);
    data.bytes.resize(read);

    auto *info = new stbtt_fontinfo();
    if (!stbtt_InitFont(info, data.bytes.data(),
                        stbtt_GetFontOffsetForIndex(data.bytes.data(), 0))) {
        std::fprintf(stderr, "[font] invalid font %s\n", path.c_str());
        delete info;
        return -1;
    }
    data.info = info;
    fonts_.push_back(std::move(data));
    return static_cast<int>(fonts_.size()) - 1;
}

bool FontAtlas::pack(int width, int height, int &out_x, int &out_y) {
    if (width + kPadding * 2 > atlas_size_ || height + kPadding * 2 > atlas_size_) return false;
    if (shelf_x_ + width + kPadding > atlas_size_) {
        shelf_x_ = kPadding;
        shelf_y_ += shelf_height_ + kPadding;
        shelf_height_ = 0;
    }
    if (shelf_y_ + height + kPadding > atlas_size_) return false;
    out_x = shelf_x_;
    out_y = shelf_y_;
    shelf_x_ += width + kPadding;
    if (height > shelf_height_) shelf_height_ = height;
    return true;
}

const Glyph &FontAtlas::bake(int font_index, int pixel_size, char32_t codepoint) {
    static Glyph missing;
    const uint64_t key = glyph_key(font_index, pixel_size, codepoint);
    const auto found = glyphs_.find(key);
    if (found != glyphs_.end()) return found->second;
    if (font_index < 0 || font_index >= static_cast<int>(fonts_.size())) return missing;

    auto *info = static_cast<stbtt_fontinfo *>(fonts_[font_index].info);
    const float scale = stbtt_ScaleForPixelHeight(info, static_cast<float>(pixel_size));
    int advance = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(info, static_cast<int>(codepoint), &advance, &lsb);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(info, static_cast<int>(codepoint), scale, scale, &x0, &y0, &x1, &y1);
    const int width = x1 - x0;
    const int height = y1 - y0;

    Glyph glyph;
    glyph.width = width;
    glyph.height = height;
    glyph.bearing_x = x0;
    glyph.bearing_y = y0;
    glyph.advance = advance * scale;
    glyph.valid = true;

    if (width > 0 && height > 0) {
        std::vector<uint8_t> bitmap(static_cast<size_t>(width) * height, 0);
        stbtt_MakeCodepointBitmap(info, bitmap.data(), width, height, width, scale, scale,
                                  static_cast<int>(codepoint));
        int atlas_x = 0, atlas_y = 0;
        if (!pack(width, height, atlas_x, atlas_y)) {
            std::fprintf(stderr, "[font] atlas full while baking U+%04X at %dpx\n",
                         static_cast<unsigned>(codepoint), pixel_size);
            glyphs_.emplace(key, glyph);
            return glyphs_[key];
        }
        texture_.update_r8(atlas_x, atlas_y, width, height, bitmap.data());
        glyph.u0 = static_cast<float>(atlas_x) / atlas_size_;
        glyph.u1 = static_cast<float>(atlas_x + width) / atlas_size_;
        // The bitmap's first row is uploaded at atlas_y, which is the bottom
        // of the OpenGL texture.  Our quads call v1 the top edge, so the
        // smaller coordinate belongs to v1.
        glyph.v1 = static_cast<float>(atlas_y) / atlas_size_;
        glyph.v0 = static_cast<float>(atlas_y + height) / atlas_size_;
    }
    const auto inserted = glyphs_.emplace(key, glyph);
    return inserted.first->second;
}

Glyph FontAtlas::glyph(int font_index, int pixel_size, char32_t codepoint) {
    return bake(font_index, pixel_size, codepoint);
}

float FontAtlas::measure(int font_index, int pixel_size, std::string_view utf8) {
    std::vector<char32_t> codepoints;
    decode_utf8(utf8, codepoints);
    float width = 0;
    char32_t previous = 0;
    for (char32_t cp : codepoints) {
        if (cp == '\n') {
            width = 0;
            continue;
        }
        if (previous) width += kerning(font_index, pixel_size, previous, cp);
        width += glyph(font_index, pixel_size, cp).advance;
        previous = cp;
    }
    return width;
}

float FontAtlas::line_height(int font_index, int pixel_size) const {
    if (font_index < 0 || font_index >= static_cast<int>(fonts_.size())) return pixel_size;
    auto *info = static_cast<stbtt_fontinfo *>(fonts_[font_index].info);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);
    const float scale = stbtt_ScaleForPixelHeight(info, static_cast<float>(pixel_size));
    return (ascent - descent + line_gap) * scale;
}

float FontAtlas::ascent(int font_index, int pixel_size) const {
    if (font_index < 0 || font_index >= static_cast<int>(fonts_.size())) return pixel_size;
    auto *info = static_cast<stbtt_fontinfo *>(fonts_[font_index].info);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);
    return ascent * stbtt_ScaleForPixelHeight(info, static_cast<float>(pixel_size));
}

float FontAtlas::kerning(int font_index, int pixel_size, char32_t a, char32_t b) {
    if (font_index < 0 || font_index >= static_cast<int>(fonts_.size())) return 0;
    auto *info = static_cast<stbtt_fontinfo *>(fonts_[font_index].info);
    const float scale = stbtt_ScaleForPixelHeight(info, static_cast<float>(pixel_size));
    return stbtt_GetCodepointKernAdvance(info, static_cast<int>(a), static_cast<int>(b)) * scale;
}

void decode_utf8(std::string_view text, std::vector<char32_t> &out) {
    out.clear();
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        char32_t cp = '?';
        size_t length = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
            length = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<unsigned char>(text[i + 2]) & 0x3Fu);
            length = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) {
            cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6) |
                 (static_cast<unsigned char>(text[i + 3]) & 0x3Fu);
            length = 4;
        }
        out.push_back(cp);
        i += length;
    }
}

}  // namespace nimbus::gfx
