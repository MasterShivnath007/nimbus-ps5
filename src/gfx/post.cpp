#include "gfx/post.hpp"

#include <cmath>
#include <cstdio>

#include "gfx/math.hpp"

namespace nimbus::gfx {

bool PostProcessor::init(const std::string &asset_dir) {
    if (!bright_.build_from_files("bright", asset_dir + "/shaders/post.vert",
                                  asset_dir + "/shaders/bright.frag"))
        return false;
    if (!blur_.build_from_files("blur", asset_dir + "/shaders/post.vert",
                                asset_dir + "/shaders/blur.frag"))
        return false;
    if (!composite_.build_from_files("composite", asset_dir + "/shaders/post.vert",
                                     asset_dir + "/shaders/composite.frag"))
        return false;
    quad_ = make_quad();
    const uint8_t black[4] = {0, 0, 0, 255};
    black_.create_rgba(1, 1, black, false);
    ready_ = true;
    return true;
}

void PostProcessor::begin(int viewport_width, int viewport_height, float render_scale) {
    screen_w_ = std::max(1, viewport_width);
    screen_h_ = std::max(1, viewport_height);
    const float scale = clampf(render_scale, 0.6f, 2.0f);
    const int target_w = std::max(1, static_cast<int>(screen_w_ * scale));
    const int target_h = std::max(1, static_cast<int>(screen_h_ * scale));
    if (target_w != scene_w_ || target_h != scene_h_ || !scene_.fbo) {
        scene_.create(target_w, target_h, true);
        bloom_a_.create(std::max(1, target_w / 2), std::max(1, target_h / 2), true);
        bloom_b_.create(std::max(1, target_w / 2), std::max(1, target_h / 2), true);
        scene_w_ = target_w;
        scene_h_ = target_h;
    }
    scene_.bind();
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessor::end(bool bloom, float exposure, float bloom_strength) {
    bloom_a_.bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    bright_.use();
    bright_.set_int("uScene", 0);
    bright_.set_vec2("uTexel", 1.0f / scene_w_, 1.0f / scene_h_);
    bright_.set_float("uThreshold", bloom ? 0.9f : 100.0f);
    bright_.set_float("uSoftKnee", 0.55f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_.color.id);
    quad_.draw();

    if (bloom) {
        glDisable(GL_DEPTH_TEST);
        blur_.use();
        blur_.set_int("uImage", 0);
        blur_.set_vec2("uTexel", 1.0f / bloom_a_.width, 1.0f / bloom_a_.height);
        // Horizontal.
        bloom_b_.bind();
        blur_.set_vec2("uDirection", 1.0f, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloom_a_.color.id);
        quad_.draw();
        // Vertical, back into A.
        bloom_a_.bind();
        blur_.set_vec2("uDirection", 0.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, bloom_b_.color.id);
        quad_.draw();
        // A second, wider pass gives the glow a cinematic falloff.
        bloom_b_.bind();
        blur_.set_vec2("uDirection", 1.0f, 0.0f);
        blur_.set_vec2("uTexel", 2.0f / bloom_a_.width, 2.0f / bloom_a_.height);
        glBindTexture(GL_TEXTURE_2D, bloom_a_.color.id);
        quad_.draw();
        bloom_a_.bind();
        blur_.set_vec2("uDirection", 0.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, bloom_b_.color.id);
        quad_.draw();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_w_, screen_h_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    composite_.use();
    composite_.set_int("uScene", 0);
    composite_.set_int("uBloom", 1);
    composite_.set_float("uBloomStrength", bloom ? bloom_strength : 0.0f);
    composite_.set_float("uExposure", exposure);
    composite_.set_vec2("uResolution", static_cast<float>(screen_w_),
                        static_cast<float>(screen_h_));
    composite_.set_float("uVignette", 0.26f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_.color.id);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, bloom ? bloom_a_.color.id : black_.id);
    quad_.draw();
}

void PostProcessor::release() {
    scene_.release();
    bloom_a_.release();
    bloom_b_.release();
    ready_ = false;
}

}  // namespace nimbus::gfx
