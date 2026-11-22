#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "gfx/font.hpp"
#include "gfx/gl.hpp"
#include "gfx/mesh.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"
#include "ui/icon_codes.hpp"

namespace nimbus::gfx {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
};

Color hex(uint32_t rgb, float alpha = 1.0f);
Color mix(Color a, Color b, float t);
Color with_alpha(Color c, float alpha);

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
    Rect inset(float amount) const {
        return {x + amount, y + amount, w - amount * 2, h - amount * 2};
    }
};

struct TextStyle {
    int font = 0;
    float size = 16;
    Color color{1, 1, 1, 1};
    int align = 0;  // 0 left, 1 center, 2 right
    float letter_spacing = 0;
    float max_width = 0;  // 0 = no wrapping
    float line_spacing = 1.2f;
    bool shadow = false;
};

class Ui {
public:
    Ui() = default;
    ~Ui();
    bool init(int atlas_size = 4096);
    bool load_fonts(const std::string &asset_dir);
    void begin(int physical_width, int physical_height);
    void end();  // flush and restore state

    float scale() const { return scale_; }
    int width() const { return width_; }
    int height() const { return height_; }
    float virtual_width() const { return virtual_width_; }
    float virtual_height() const { return virtual_height_; }
    float px(float virtual_pixels) const { return virtual_pixels * scale_; }

    void push_clip(const Rect &rect);
    void pop_clip();
    void set_additive(bool additive);

    void rect(const Rect &rect, Color color);
    void rounded_rect(const Rect &rect, float radius, Color color);
    void rounded_rect_gradient(const Rect &rect, float radius, Color top, Color bottom);
    void glow(const Rect &rect, float radius, float blur, Color color);
    void line(float x0, float y0, float x1, float y1, float thickness, Color color);
    void circle(float cx, float cy, float radius, Color color);
    void ring(float cx, float cy, float radius, float thickness, Color color);
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color color);
    void arc(float cx, float cy, float radius, float start_angle, float end_angle, float thickness,
             Color color, int segments = 48);

    void text(const TextStyle &style, const std::string &utf8, float x, float y);
    void text(const TextStyle &style, const std::string &utf8, const Rect &box);
    void text_shadowed(const TextStyle &style, const std::string &utf8, float x, float y,
                       Color shadow = {0, 0, 0, 0.5f});
    float text_width(int font, float size, std::string_view utf8) const;
    float text_height(int font, float size) const;
    void icon(const std::string &name, float center_x, float center_y, float size, Color color);
    float icon_width(float size) const;

    void panel(const Rect &rect, float radius, Color fill, Color border);
    void button(const Rect &rect, const std::string &label, bool focused, bool accent,
                Color accent_color);
    void pill(const Rect &rect, const std::string &label, Color fill, Color text_color);
    void ring_focus(const Rect &rect, float radius, Color color);
    void spinner(float cx, float cy, float radius, float phase, Color color);

    int font_ui() const { return font_ui_; }
    int font_medium() const { return font_medium_; }
    int font_semibold() const { return font_semibold_; }
    int font_bold() const { return font_bold_; }
    int font_display() const { return font_display_; }
    int font_display_bold() const { return font_display_bold_; }
    int font_icons() const { return font_icons_; }

    void flush();
    GLuint font_texture() const;

private:
    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
        float local_x, local_y;
        float half_w, half_h;
        float radius;
        float mode;
    };
    struct DrawCmd {
        GLuint texture = 0;
        int first = 0;
        int count = 0;
    };

    void add_quad(const Rect &r, float local_x, float local_y, float half_w, float half_h,
                  float radius, Color c0, Color c1, float u0, float v0, float u1, float v1,
                  float mode, GLuint texture);
    void add_vertex(float x, float y, float u, float v, Color color, float local_x, float local_y,
                    float half_w, float half_h, float radius, float mode, GLuint texture);
    void apply_clip();
    void flush_cmd();

    FontAtlas atlas_;
    Shader shader_;
    Mesh mesh_;
    Texture2D white_;
    int width_ = 0;
    int height_ = 0;
    int virtual_width_ = 1920;
    int virtual_height_ = 1080;
    float scale_ = 1.0f;
    int font_ui_ = -1;
    int font_medium_ = -1;
    int font_semibold_ = -1;
    int font_bold_ = -1;
    int font_display_ = -1;
    int font_display_bold_ = -1;
    int font_icons_ = -1;
    std::vector<Vertex> vertices_;
    std::vector<DrawCmd> commands_;
    std::vector<Rect> clip_stack_;
    bool additive_ = false;
    bool ready_ = false;
};

}  // namespace nimbus::gfx
