#pragma once

#include <string>

#include "gfx/gl.hpp"
#include "gfx/mesh.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"

namespace nimbus::gfx {

class PostProcessor {
public:
    bool init(const std::string &asset_dir);
    void begin(int viewport_width, int viewport_height, float render_scale);
    void end(bool bloom, float exposure, float bloom_strength);
    void release();
    int scene_width() const { return scene_w_; }
    int scene_height() const { return scene_h_; }
    bool ready() const { return ready_; }

private:
    Shader bright_;
    Shader blur_;
    Shader composite_;
    Mesh quad_;
    Texture2D black_;
    Framebuffer scene_;
    Framebuffer bloom_a_;
    Framebuffer bloom_b_;
    int screen_w_ = 0;
    int screen_h_ = 0;
    int scene_w_ = 0;
    int scene_h_ = 0;
    bool ready_ = false;
};

}  // namespace nimbus::gfx
