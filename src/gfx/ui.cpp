#include "gfx/ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace nimbus::gfx {

Color hex(uint32_t rgb, float alpha) {
    return {static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
            static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
            static_cast<float>(rgb & 0xFF) / 255.0f, alpha};
}

Color mix(Color a, Color b, float t) {
    return {lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t),
            lerpf(a.a, b.a, t)};
}

Color with_alpha(Color c, float alpha) {
    c.a = alpha;
    return c;
}

namespace {
constexpr const char *kVertex = R"GLSL(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(location=3) in vec2 aLocal;
layout(location=4) in vec3 aShape;
layout(location=5) in float aMode;
uniform vec2 uViewport;
out vec2 vUV;
out vec4 vColor;
out vec2 vLocal;
out vec3 vShape;
out float vMode;
void main() {
    vUV = aUV;
    vColor = aColor;
    vLocal = aLocal;
    vShape = aShape;
    vMode = aMode;
    gl_Position = vec4(aPos.x / uViewport.x * 2.0 - 1.0,
                       1.0 - aPos.y / uViewport.y * 2.0, 0.0, 1.0);
}
)GLSL";

constexpr const char *kFragment = R"GLSL(
#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec2 vLocal;
in vec3 vShape;
in float vMode;
uniform sampler2D uTexture;
out vec4 fragColor;

float rounded_box(vec2 p, vec2 half_size, float radius) {
    vec2 q = abs(p) - half_size + vec2(radius);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    vec4 out_color = vColor;
    if (vMode < 0.5) {
        // solid
    } else if (vMode < 1.5) {
        float alpha = texture(uTexture, vUV).r;
        out_color.a *= alpha;
    } else if (vMode < 2.5) {
        float d = rounded_box(vLocal, vShape.xy, vShape.z);
        float aa = 0.9;
        float alpha = 1.0 - smoothstep(-aa, aa, d);
        out_color.a *= alpha;
    } else if (vMode < 3.5) {
        float d = rounded_box(vLocal, vShape.xy, vShape.z);
        float blur = max(1.0, vShape.z * 0.3);
        float alpha = exp(-max(d, 0.0) / blur);
        alpha *= 1.0 - smoothstep(0.0, 2.0, -d);
        out_color.a *= alpha;
    } else {
        out_color *= texture(uTexture, vUV);
    }
    if (out_color.a <= 0.001) discard;
    fragColor = out_color;
}
)GLSL";

GLuint icon_codepoint(const std::string &name) {
    for (const auto &entry : ui::icons::kIcons) {
        if (name == entry.name) return static_cast<GLuint>(entry.codepoint);
    }
    return 0;
}
}  // namespace

Ui::~Ui() = default;

bool Ui::init(int atlas_size) {
    if (!atlas_.init(atlas_size)) return false;
    if (!shader_.build("ui", kVertex, kFragment)) return false;
    const uint8_t white[4] = {255, 255, 255, 255};
    white_.create_rgba(1, 1, white, false);
    ready_ = true;
    return true;
}

bool Ui::load_fonts(const std::string &asset_dir) {
    font_ui_ = atlas_.add_font(asset_dir + "/fonts/Manrope-400.ttf");
    font_medium_ = atlas_.add_font(asset_dir + "/fonts/Manrope-500.ttf");
    font_semibold_ = atlas_.add_font(asset_dir + "/fonts/Manrope-600.ttf");
    font_bold_ = atlas_.add_font(asset_dir + "/fonts/Manrope-700.ttf");
    font_display_ = atlas_.add_font(asset_dir + "/fonts/SpaceGrotesk-500.ttf");
    font_display_bold_ = atlas_.add_font(asset_dir + "/fonts/SpaceGrotesk-700.ttf");
    font_icons_ = atlas_.add_font(asset_dir + "/fonts/lucide.ttf");
    return font_ui_ >= 0 && font_display_ >= 0 && font_icons_ >= 0;
}

void Ui::begin(int physical_width, int physical_height) {
    width_ = physical_width;
    height_ = physical_height;
    scale_ = std::max(0.5f, static_cast<float>(physical_height) / 1080.0f);
    // Keep a 16:9 virtual canvas, but use the actual aspect for ultrawide screens.
    virtual_height_ = 1080;
    virtual_width_ = static_cast<int>(physical_width / scale_ + 0.5f);
    vertices_.clear();
    commands_.clear();
    clip_stack_.clear();
    additive_ = false;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, physical_width, physical_height);
}

void Ui::end() {
    flush();
    glDisable(GL_SCISSOR_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
}

void Ui::set_additive(bool additive) {
    if (additive_ == additive) return;
    flush();
    additive_ = additive;
    glBlendFunc(GL_SRC_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
}

void Ui::push_clip(const Rect &rect) {
    flush();
    clip_stack_.push_back(rect);
    apply_clip();
}

void Ui::pop_clip() {
    flush();
    if (!clip_stack_.empty()) clip_stack_.pop_back();
    apply_clip();
}

void Ui::apply_clip() {
    if (clip_stack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect &r = clip_stack_.back();
    const int x = static_cast<int>(std::floor(r.x * scale_));
    const int w = static_cast<int>(std::ceil(r.w * scale_));
    const int h = static_cast<int>(std::ceil(r.h * scale_));
    const int y = static_cast<int>(std::ceil((virtual_height_ - r.y - r.h) * scale_));
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, std::max(0, w), std::max(0, h));
}

void Ui::flush_cmd() {
    if (vertices_.empty()) return;
    mesh_.upload(vertices_.data(), vertices_.size() * sizeof(Vertex),
                 static_cast<int>(vertices_.size()), GL_TRIANGLES,
                 {2, 2, 4, 2, 3, 1});
    shader_.use();
    shader_.set_vec2("uViewport", static_cast<float>(width_), static_cast<float>(height_));
    shader_.set_int("uTexture", 0);
    GLuint current = 0;
    glBindVertexArray(mesh_.vao);
    for (const auto &cmd : commands_) {
        if (cmd.count <= 0) continue;
        if (cmd.texture != current) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cmd.texture);
            current = cmd.texture;
        }
        glDrawArrays(GL_TRIANGLES, cmd.first, cmd.count);
    }
    glBindVertexArray(0);
    vertices_.clear();
    commands_.clear();
}

void Ui::flush() {
    flush_cmd();
}

void Ui::add_vertex(float x, float y, float u, float v, Color color, float local_x, float local_y,
                    float half_w, float half_h, float radius, float mode, GLuint texture) {
    if (!ready_) return;
    Vertex vx{x, y, u, v, color.r, color.g, color.b, color.a, local_x, local_y, half_w, half_h,
              radius, mode};
    if (commands_.empty() || commands_.back().texture != texture) {
        DrawCmd cmd;
        cmd.texture = texture;
        cmd.first = static_cast<int>(vertices_.size());
        cmd.count = 0;
        commands_.push_back(cmd);
    }
    vertices_.push_back(vx);
    commands_.back().count++;
}

void Ui::add_quad(const Rect &r, float local_x, float local_y, float half_w, float half_h,
                  float radius, Color c0, Color c1, float u0, float v0, float u1, float v1,
                  float mode, GLuint texture) {
    if (r.w <= 0 || r.h <= 0) return;
    add_vertex(r.x, r.y, u0, v1, c0, local_x - half_w, local_y - half_h, half_w, half_h, radius,
               mode, texture);
    add_vertex(r.x + r.w, r.y, u1, v1, c0, local_x + half_w, local_y - half_h, half_w, half_h,
               radius, mode, texture);
    add_vertex(r.x + r.w, r.y + r.h, u1, v0, c1, local_x + half_w, local_y + half_h, half_w,
               half_h, radius, mode, texture);
    add_vertex(r.x, r.y, u0, v1, c0, local_x - half_w, local_y - half_h, half_w, half_h, radius,
               mode, texture);
    add_vertex(r.x + r.w, r.y + r.h, u1, v0, c1, local_x + half_w, local_y + half_h, half_w,
               half_h, radius, mode, texture);
    add_vertex(r.x, r.y + r.h, u0, v0, c1, local_x - half_w, local_y + half_h, half_w, half_h,
               radius, mode, texture);
}

void Ui::rect(const Rect &r, Color color) {
    add_quad({r.x * scale_, r.y * scale_, r.w * scale_, r.h * scale_}, 0, 0, 0, 0, 0, color,
             color, 0, 0, 1, 1, 0.0f, white_.id);
}

void Ui::rounded_rect(const Rect &r, float radius, Color color) {
    const float s = scale_;
    add_quad({r.x * s, r.y * s, r.w * s, r.h * s}, 0, 0, r.w * s * 0.5f, r.h * s * 0.5f,
             radius * s, color, color, 0, 0, 1, 1, 2.0f, white_.id);
}

void Ui::gradient_rect(const Rect &r, Color top, Color bottom) {
    // A plain quad: mode 0 keeps the interpolated alpha without the SDF edge
    // coverage that would otherwise leave a seam where two scrims meet.
    const float s = scale_;
    add_quad({r.x * s, r.y * s, r.w * s, r.h * s}, 0, 0, 0, 0, 0, top, bottom, 0, 0, 1, 1,
             0.0f, white_.id);
}

void Ui::rounded_rect_gradient(const Rect &r, float radius, Color top, Color bottom) {
    const float s = scale_;
    add_quad({r.x * s, r.y * s, r.w * s, r.h * s}, 0, 0, r.w * s * 0.5f, r.h * s * 0.5f,
             radius * s, top, bottom, 0, 0, 1, 1, 2.0f, white_.id);
}

void Ui::glow(const Rect &r, float radius, float blur, Color color) {
    const float s = scale_;
    add_quad({r.x * s, r.y * s, r.w * s, r.h * s}, 0, 0, r.w * s * 0.5f, r.h * s * 0.5f,
             blur * s, color, color, 0, 0, 1, 1, 3.0f, white_.id);
}

void Ui::line(float x0, float y0, float x1, float y1, float thickness, Color color) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f) return;
    const float nx = -dy / length * thickness * 0.5f;
    const float ny = dx / length * thickness * 0.5f;
    const float s = scale_;
    const float ax = (x0 + nx), ay = (y0 + ny);
    const float bx = (x0 - nx), by = (y0 - ny);
    const float cx = (x1 - nx), cy = (y1 - ny);
    const float ex = (x1 + nx), ey = (y1 + ny);
    add_vertex(ax * s, ay * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(bx * s, by * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(cx * s, cy * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(ax * s, ay * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(cx * s, cy * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(ex * s, ey * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
}

void Ui::circle(float cx, float cy, float radius, Color color) {
    rounded_rect({cx - radius, cy - radius, radius * 2, radius * 2}, radius, color);
}

void Ui::ring(float cx, float cy, float radius, float thickness, Color color) {
    const int segments = std::max(12, static_cast<int>(radius * 1.4f));
    for (int i = 0; i < segments; ++i) {
        const float a0 = static_cast<float>(i) / segments * 6.2831853f;
        const float a1 = static_cast<float>(i + 1) / segments * 6.2831853f;
        line(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
             cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, thickness, color);
    }
}

void Ui::triangle(float x0, float y0, float x1, float y1, float x2, float y2, Color color) {
    const float s = scale_;
    add_vertex(x0 * s, y0 * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(x1 * s, y1 * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
    add_vertex(x2 * s, y2 * s, 0, 0, color, 0, 0, 0, 0, 0, 0, white_.id);
}

void Ui::arc(float cx, float cy, float radius, float start_angle, float end_angle,
             float thickness, Color color, int segments) {
    for (int i = 0; i < segments; ++i) {
        const float t0 = static_cast<float>(i) / segments;
        const float t1 = static_cast<float>(i + 1) / segments;
        const float a0 = lerpf(start_angle, end_angle, t0);
        const float a1 = lerpf(start_angle, end_angle, t1);
        line(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
             cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, thickness, color);
    }
}

float Ui::text_width(int font, float size, std::string_view utf8) const {
    const int px = std::max(1, static_cast<int>(std::round(size * scale_)));
    return const_cast<FontAtlas &>(atlas_).measure(font, px, utf8) / scale_;
}

float Ui::text_height(int font, float size) const {
    const int px = std::max(1, static_cast<int>(std::round(size * scale_)));
    return atlas_.line_height(font, px) / scale_;
}

void Ui::text(const TextStyle &style, const std::string &utf8, float x, float y) {
    const int px = std::max(1, static_cast<int>(std::round(style.size * scale_)));
    const float s = scale_;
    float pen = x;
    float baseline = y + atlas_.ascent(style.font, px) / s;
    float start_x = x;
    char32_t previous = 0;
    std::vector<char32_t> codepoints;
    decode_utf8(utf8, codepoints);

    auto draw_line = [&](size_t begin, size_t end) {
        if (style.align == 1 || style.align == 2) {
            float line_width = 0;
            for (size_t i = begin; i < end; ++i) {
                line_width += atlas_.glyph(style.font, px, codepoints[i]).advance / s;
                line_width += style.letter_spacing;
            }
            if (!codepoints.empty()) line_width -= style.letter_spacing;
            if (style.align == 1) pen = start_x + (style.max_width > 0
                                                       ? (style.max_width - line_width) * 0.5f
                                                       : -line_width * 0.5f);
            else if (style.max_width > 0) pen = start_x + style.max_width - line_width;
            else pen = start_x - line_width;
        } else {
            pen = start_x;
        }
        previous = 0;
        for (size_t i = begin; i < end; ++i) {
            const char32_t cp = codepoints[i];
            if (previous) pen += atlas_.kerning(style.font, px, previous, cp) / s;
            const Glyph g = atlas_.glyph(style.font, px, cp);
            if (g.valid && g.width > 0) {
                const float gx = pen + g.bearing_x / s;
                const float gy = baseline + g.bearing_y / s;
                add_quad({gx * s, gy * s, static_cast<float>(g.width), static_cast<float>(g.height)},
                         0, 0, 0, 0, 0, style.color, style.color, g.u0, g.v0, g.u1, g.v1, 1.0f,
                         font_texture());
            }
            pen += g.advance / s + style.letter_spacing;
            previous = cp;
        }
    };

    if (style.max_width <= 0) {
        size_t start = 0;
        for (size_t i = 0; i <= codepoints.size(); ++i) {
            if (i == codepoints.size() || codepoints[i] == '\n') {
                draw_line(start, i);
                if (i < codepoints.size()) baseline += style.size * style.line_spacing;
                start = i + 1;
            }
        }
        return;
    }

    // Greedy word wrapping.
    std::vector<std::pair<size_t, size_t>> lines;
    size_t line_start = 0;
    size_t last_space = std::string::npos;
    float width = 0;
    for (size_t i = 0; i < codepoints.size(); ++i) {
        if (codepoints[i] == '\n') {
            lines.emplace_back(line_start, i);
            line_start = i + 1;
            width = 0;
            last_space = std::string::npos;
            continue;
        }
        const Glyph g = atlas_.glyph(style.font, px, codepoints[i]);
        width += g.advance / s + style.letter_spacing;
        if (codepoints[i] == ' ') last_space = i;
        if (width > style.max_width && i > line_start) {
            if (last_space != std::string::npos && last_space > line_start) {
                lines.emplace_back(line_start, last_space);
                line_start = last_space + 1;
                i = last_space;
            } else {
                lines.emplace_back(line_start, i);
                line_start = i;
            }
            width = 0;
            last_space = std::string::npos;
        }
    }
    if (line_start < codepoints.size()) lines.emplace_back(line_start, codepoints.size());
    for (size_t index = 0; index < lines.size(); ++index) {
        draw_line(lines[index].first, lines[index].second);
        if (index + 1 < lines.size()) baseline += style.size * style.line_spacing;
    }
}

void Ui::text(const TextStyle &style, const std::string &utf8, const Rect &box) {
    TextStyle local = style;
    if (local.max_width <= 0) local.max_width = box.w;
    float y = box.y;
    if (box.h > 0) {
        const float line = text_height(local.font, local.size);
        y = box.y + (box.h - line) * 0.5f;
    }
    if (local.align == 1) text(local, utf8, box.cx() - local.max_width * 0.5f, y);
    else if (local.align == 2) text(local, utf8, box.x + box.w - local.max_width, y);
    else text(local, utf8, box.x, y);
}

void Ui::text_shadowed(const TextStyle &style, const std::string &utf8, float x, float y,
                       Color shadow) {
    TextStyle s = style;
    s.color = shadow;
    text(s, utf8, x + 1.0f, y + 1.5f);
    s.color = style.color;
    text(s, utf8, x, y);
}

void Ui::icon(const std::string &name, float center_x, float center_y, float size, Color color) {
    if (font_icons_ < 0) return;
    const char32_t cp = static_cast<char32_t>(icon_codepoint(name));
    if (!cp) return;
    const int px = std::max(1, static_cast<int>(std::round(size * scale_)));
    const Glyph g = atlas_.glyph(font_icons_, px, cp);
    if (!g.valid) return;
    const float s = scale_;
    const float top = center_y * s - g.height * 0.5f;
    const float left = center_x * s - g.width * 0.5f;
    const float baseline = top - g.bearing_y;
    (void)baseline;
    add_quad({left, top, static_cast<float>(g.width), static_cast<float>(g.height)}, 0, 0, 0, 0, 0,
             color, color, g.u0, g.v0, g.u1, g.v1, 1.0f, font_texture());
}

float Ui::icon_width(float size) const { return size; }

void Ui::panel(const Rect &r, float radius, Color fill, Color border) {
    glow({r.x + 2, r.y + 10, r.w, r.h}, radius + 4, 18, {0, 0, 0, 0.45f});
    rounded_rect(r, radius, border);
    rounded_rect(r.inset(1.0f), std::max(0.0f, radius - 1), fill);
    // Hairline highlight along the top gives glass an edge.
    rounded_rect({r.x + radius, r.y + 0.5f, r.w - radius * 2, 1.0f}, 0.5f,
                 {1, 1, 1, 0.06f});
}

void Ui::button(const Rect &r, const std::string &label, bool focused, bool accent,
                Color accent_color) {
    const Color fill = accent ? accent_color : Color{1, 1, 1, 0.07f};
    const Color text_color = accent ? Color{0.02f, 0.05f, 0.06f, 1} : Color{0.92f, 1, 1, 1};
    if (focused) glow({r.x - 4, r.y - 4, r.w + 8, r.h + 8}, r.h * 0.5f + 4, 12,
                      with_alpha(accent_color, 0.45f));
    rounded_rect(r, r.h * 0.5f, focused ? mix(fill, accent_color, accent ? 0.2f : 0.13f) : fill);
    if (!accent)
        rounded_rect({r.x, r.y, r.w, 1}, 0.5f, {1, 1, 1, focused ? 0.16f : 0.08f});
    TextStyle style;
    style.font = font_semibold_;
    style.size = 14;
    style.color = text_color;
    style.align = 1;
    style.max_width = r.w;
    text(style, label, r.x, r.y + (r.h - text_height(font_semibold_, 14)) * 0.5f - 1);
}

void Ui::pill(const Rect &r, const std::string &label, Color fill, Color text_color) {
    rounded_rect(r, r.h * 0.5f, fill);
    TextStyle style;
    style.font = font_medium_;
    style.size = 12;
    style.color = text_color;
    style.align = 1;
    style.max_width = r.w;
    text(style, label, r.x, r.y + (r.h - text_height(font_medium_, 12)) * 0.5f - 1);
}

void Ui::ring_focus(const Rect &r, float radius, Color color) {
    rounded_rect({r.x - 2, r.y - 2, r.w + 4, r.h + 4}, radius + 2, with_alpha(color, 0.9f));
    rounded_rect({r.x - 1, r.y - 1, r.w + 2, r.h + 2}, radius + 1, {0, 0, 0, 0.0f});
    // Draw a hollow ring using four thin rounded strips.
    rounded_rect({r.x - 2, r.y - 2, r.w + 4, 1.5f}, 0.75f, color);
    rounded_rect({r.x - 2, r.y + r.h + 0.5f, r.w + 4, 1.5f}, 0.75f, color);
    rounded_rect({r.x - 2, r.y - 2, 1.5f, r.h + 4}, 0.75f, color);
    rounded_rect({r.x + r.w + 0.5f, r.y - 2, 1.5f, r.h + 4}, 0.75f, color);
}

void Ui::spinner(float cx, float cy, float radius, float phase, Color color) {
    for (int i = 0; i < 18; ++i) {
        const float t = static_cast<float>(i) / 18.0f;
        const float a = phase + t * 6.2831853f;
        const float alpha = 0.15f + 0.85f * t;
        const float r0 = radius * 0.75f;
        line(cx + std::cos(a) * r0, cy + std::sin(a) * r0, cx + std::cos(a) * radius,
             cy + std::sin(a) * radius, 1.6f, with_alpha(color, alpha));
    }
}

GLuint Ui::font_texture() const { return atlas_.texture(); }

}  // namespace nimbus::gfx
