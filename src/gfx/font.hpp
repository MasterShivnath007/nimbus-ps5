#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gfx/gl.hpp"
#include "gfx/math.hpp"
#include "gfx/texture.hpp"

namespace nimbus::gfx {

struct Glyph {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // v0 = bottom, v1 = top
    int width = 0, height = 0;
    int bearing_x = 0;
    int bearing_y = 0;  // from baseline to top, negative above
    float advance = 0;
    bool valid = false;
};

class FontAtlas {
public:
    FontAtlas() = default;
    ~FontAtlas();
    bool init(int size = 4096);
    int add_font(const std::string &path);
    Glyph glyph(int font_index, int pixel_size, char32_t codepoint);
    float measure(int font_index, int pixel_size, std::string_view utf8);
    float line_height(int font_index, int pixel_size) const;
    float ascent(int font_index, int pixel_size) const;
    float kerning(int font_index, int pixel_size, char32_t a, char32_t b);
    GLuint texture() const { return texture_.id; }
    bool ready() const { return texture_.id != 0; }

private:
    struct FontData {
        std::vector<uint8_t> bytes;
        void *info = nullptr;  // stbtt_fontinfo*
        float scale_for_pixel_height = 0;
    };
    bool pack(int width, int height, int &out_x, int &out_y);
    const Glyph &bake(int font_index, int pixel_size, char32_t codepoint);

    Texture2D texture_;
    std::vector<FontData> fonts_;
    std::unordered_map<uint64_t, Glyph> glyphs_;
    int atlas_size_ = 0;
    int shelf_x_ = 1;
    int shelf_y_ = 1;
    int shelf_height_ = 0;
};

void decode_utf8(std::string_view text, std::vector<char32_t> &out);

}  // namespace nimbus::gfx
