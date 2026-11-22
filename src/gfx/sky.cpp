#include "gfx/sky.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

#include "gfx/math.hpp"

namespace nimbus::gfx {

namespace {
struct StarVertex {
    float dir[3];
    float size;
    float color[3];
};

uint32_t lcg(uint32_t &state) {
    state = state * 1664525u + 1013904223u;
    return state;
}
float rnd(uint32_t &state) { return (lcg(state) & 0xFFFFFF) / 16777216.0f; }
}  // namespace

bool SkyRenderer::init(const std::string &asset_dir) {
    if (!sky_shader_.build_from_files("sky", asset_dir + "/shaders/sky.vert",
                                      asset_dir + "/shaders/sky.frag"))
        return false;
    if (!star_shader_.build_from_files("stars", asset_dir + "/shaders/stars.vert",
                                       asset_dir + "/shaders/stars.frag"))
        return false;
    quad_ = make_quad();
    moon_ = load_texture(asset_dir + "/moon_disc.png", true, false);
    const uint8_t white[4] = {255, 255, 255, 255};
    white_.create_rgba(1, 1, white, false);

    std::vector<StarVertex> vertices;
    vertices.reserve(6000);
    uint32_t seed = 0x3f12a9c1u;
    for (int i = 0; i < 5800; ++i) {
        const float z = 2.0f * rnd(seed) - 1.0f;
        const float phi = rnd(seed) * 6.2831853f;
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        StarVertex v;
        v.dir[0] = r * std::cos(phi);
        v.dir[1] = r * std::sin(phi);
        v.dir[2] = z;
        const float magnitude = std::pow(rnd(seed), 3.7f);
        v.size = 1.05f + magnitude * 3.2f;
        const float tint = rnd(seed);
        if (tint < 0.16f) {
            v.color[0] = 0.70f; v.color[1] = 0.82f; v.color[2] = 1.0f;
        } else if (tint < 0.36f) {
            v.color[0] = 1.0f; v.color[1] = 0.90f; v.color[2] = 0.72f;
        } else {
            v.color[0] = 0.92f; v.color[1] = 0.94f; v.color[2] = 1.0f;
        }
        const float brightness = 0.18f + 0.82f * magnitude;
        v.color[0] *= brightness;
        v.color[1] *= brightness;
        v.color[2] *= brightness;
        vertices.push_back(v);
    }
    stars_.upload(vertices.data(), vertices.size() * sizeof(StarVertex),
                  static_cast<int>(vertices.size()), GL_POINTS, {3, 1, 3});
    ready_ = true;
    return true;
}

void SkyRenderer::render(const Camera &camera, const SkyState &sky, float star_alpha,
                         float surface_mix, double wall_time) {
    if (!ready_) return;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    const Vec3 focus = latlon_to_unit(camera.lat, camera.lon);
    sky_shader_.use();
    sky_shader_.set_vec3("uCameraPos", camera.eye.x, camera.eye.y, camera.eye.z);
    sky_shader_.set_vec3("uLocalUp", focus.x, focus.y, focus.z);
    sky_shader_.set_vec3("uForward", camera.forward.x, camera.forward.y, camera.forward.z);
    sky_shader_.set_vec3("uRight", camera.right.x, camera.right.y, camera.right.z);
    sky_shader_.set_vec3("uUp", camera.true_up.x, camera.true_up.y, camera.true_up.z);
    sky_shader_.set_float("uTanHalfFov", camera.tan_half_fov);
    sky_shader_.set_float("uAspect", camera.aspect);
    sky_shader_.set_vec3("uSunEci", sky.sun_eci.x, sky.sun_eci.y, sky.sun_eci.z);
    sky_shader_.set_vec3("uMoonEci", sky.moon_eci.x, sky.moon_eci.y, sky.moon_eci.z);
    sky_shader_.set_vec3("uSunEcef", sky.sun_ecef.x, sky.sun_ecef.y, sky.sun_ecef.z);
    sky_shader_.set_float("uGmst", static_cast<float>(sky.gmst));
    sky_shader_.set_float("uSurface", surface_mix);
    sky_shader_.set_float("uStarAlpha", star_alpha);
    sky_shader_.set_float("uTime", static_cast<float>(wall_time));
    sky_shader_.set_int("uMoonTex", 1);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, moon_.id);
    quad_.draw();

    // Stars live at infinity.  Additive blending keeps their faint dust soft.
    if (star_alpha > 0.01f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        star_shader_.use();
        star_shader_.set_mat4("uViewProj", camera.view_proj.m);
        star_shader_.set_vec3("uCameraPos", camera.eye.x, camera.eye.y, camera.eye.z);
        star_shader_.set_float("uPixelScale", std::max(1.0f, camera.viewport_h / 1080.0f));
        star_shader_.set_float("uAlpha", star_alpha);
        stars_.draw();
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_BLEND);
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void SkyRenderer::release() { ready_ = false; }

}  // namespace nimbus::gfx
