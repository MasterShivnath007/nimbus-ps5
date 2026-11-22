#pragma once

#include <cstdint>
#include <string>

namespace nimbus::platform {

enum Button : uint32_t {
    BtnCross = 1u << 0,
    BtnCircle = 1u << 1,
    BtnSquare = 1u << 2,
    BtnTriangle = 1u << 3,
    BtnL1 = 1u << 4,
    BtnR1 = 1u << 5,
    BtnL2 = 1u << 6,
    BtnR2 = 1u << 7,
    BtnUp = 1u << 8,
    BtnDown = 1u << 9,
    BtnLeft = 1u << 10,
    BtnRight = 1u << 11,
    BtnOptions = 1u << 12,
    BtnTouchpad = 1u << 13,
    BtnL3 = 1u << 14,
    BtnR3 = 1u << 15,
};

struct Input {
    uint32_t buttons = 0;
    uint32_t pressed = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0;
    float l2 = 0, r2 = 0;
    float mouse_x = 0, mouse_y = 0;
    float wheel = 0;
    bool mouse_down = false;
    bool mouse_pressed = false;
    bool mouse_released = false;
    bool has_gamepad = false;
    int view_w = 0;
    int view_h = 0;
    bool any_input = false;
    bool text_input = false;
    std::string text;
    bool backspace = false;
    bool enter = false;
    bool escape = false;
    bool key_left = false, key_right = false, key_up = false, key_down = false;
    bool quit = false;
};

struct Options {
    int width = 1600;
    int height = 900;
    bool fullscreen = false;
    bool hidden = false;
    bool vsync = true;
    bool screenshot = false;
    std::string screenshot_path;
    std::string assets_dir = "assets";
    std::string data_dir;
    std::string screen = "globe";
    std::string utc;
    bool offline = false;
    bool demo = false;
    std::string env = "desktop";
};

bool init(Options &options, std::string &error);
void shutdown();
void poll(Input &input);
void present();
double monotonic_seconds();
double wall_seconds();
void sleep_ms(int milliseconds);
std::string asset_dir();
std::string data_dir();
std::string platform_name();
bool is_ps5();
void set_rumble(float low, float high);
void set_trigger_effect(float left, float right);
void notify(const std::string &text);

}  // namespace nimbus::platform
