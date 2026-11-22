#pragma once

#include <string>

#include "app/astro.hpp"
#include "app/camera.hpp"
#include "gfx/gl.hpp"
#include "gfx/mesh.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"

namespace nimbus::gfx {

class SkyRenderer {
public:
    bool init(const std::string &asset_dir);
    void render(const Camera &camera, const SkyState &sky, float star_alpha, float surface_mix,
                double wall_time);
    void release();

private:
    Shader sky_shader_;
    Shader star_shader_;
    Mesh quad_;
    Mesh stars_;
    Texture2D moon_;
    Texture2D white_;
    bool ready_ = false;
};

}  // namespace nimbus::gfx
