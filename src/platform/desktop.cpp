#if !defined(NIMBUS_PS5)
#include "platform/platform.hpp"

#include <SDL.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "gfx/gl.hpp"
#include "gfx/math.hpp"

namespace nimbus::platform {
namespace {

SDL_Window *g_window = nullptr;
SDL_GLContext g_context = nullptr;
SDL_GameController *g_controller = nullptr;
Options g_options;
std::string g_asset_dir = "assets";
std::string g_data_dir;
uint32_t g_previous_buttons = 0;
float g_last_lx = 0, g_last_ly = 0, g_last_rx = 0, g_last_ry = 0;
float g_last_l2 = 0, g_last_r2 = 0;
float g_rumble_low = 0, g_rumble_high = 0;

float axis_value(Sint16 value, bool trigger = false) {
    if (trigger) return std::max(0.0f, static_cast<float>(value) / 32767.0f);
    float v = static_cast<float>(value) / 32767.0f;
    if (std::fabs(v) < 0.14f) return 0.0f;
    const float sign = v < 0 ? -1.0f : 1.0f;
    return sign * (std::fabs(v) - 0.14f) / 0.86f;
}

uint32_t current_buttons() {
    if (!g_controller) return 0;
    uint32_t b = 0;
    auto held = [&](SDL_GameControllerButton button) {
        return SDL_GameControllerGetButton(g_controller, button) != 0;
    };
    if (held(SDL_CONTROLLER_BUTTON_A)) b |= BtnCross;
    if (held(SDL_CONTROLLER_BUTTON_B)) b |= BtnCircle;
    if (held(SDL_CONTROLLER_BUTTON_X)) b |= BtnSquare;
    if (held(SDL_CONTROLLER_BUTTON_Y)) b |= BtnTriangle;
    if (held(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) b |= BtnL1;
    if (held(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) b |= BtnR1;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_UP)) b |= BtnUp;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) b |= BtnDown;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) b |= BtnLeft;
    if (held(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) b |= BtnRight;
    if (held(SDL_CONTROLLER_BUTTON_START)) b |= BtnOptions;
    if (held(SDL_CONTROLLER_BUTTON_BACK)) b |= BtnTouchpad;
    if (held(SDL_CONTROLLER_BUTTON_LEFTSTICK)) b |= BtnL3;
    if (held(SDL_CONTROLLER_BUTTON_RIGHTSTICK)) b |= BtnR3;
    const float l2 = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT), true);
    const float r2 = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT), true);
    if (l2 > 0.28f) b |= BtnL2;
    if (r2 > 0.28f) b |= BtnR2;
    return b;
}

void open_controller(int device_index) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = nullptr;
    }
    if (device_index >= 0 && SDL_IsGameController(device_index)) {
        g_controller = SDL_GameControllerOpen(device_index);
        if (g_controller) {
            std::fprintf(stderr, "[input] controller: %s\n",
                         SDL_GameControllerName(g_controller));
        }
    }
}

bool make_directory(const std::string &path) {
    if (path.empty()) return false;
    std::string current;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!current.empty()) ::mkdir(current.c_str(), 0755);
        }
        if (i < path.size()) current.push_back(path[i]);
    }
    return true;
}

std::string default_data_dir() {
    const char *xdg = std::getenv("XDG_DATA_HOME");
#ifdef __APPLE__
    const char *mac_home = std::getenv("HOME");
    if (mac_home && *mac_home) return std::string(mac_home) + "/Library/Application Support/Nimbus";
#endif
    if (xdg && *xdg) return std::string(xdg) + "/nimbus";
    const char *home = std::getenv("HOME");
    if (home && *home) return std::string(home) + "/.local/share/nimbus";
    return "/tmp/nimbus";
}

}  // namespace

bool init(Options &options, std::string &error) {
    g_options = options;
    if (!options.data_dir.empty()) g_data_dir = options.data_dir;
    else g_data_dir = default_data_dir();
    make_directory(g_data_dir);
    make_directory(g_data_dir + "/cache");
    make_directory(g_data_dir + "/tiles");
    g_asset_dir = options.assets_dir;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        error = std::string("SDL_Init: ") + SDL_GetError();
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);

    uint32_t flags = SDL_WINDOW_OPENGL;
    if (options.hidden) flags |= SDL_WINDOW_HIDDEN;
    if (options.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    g_window = SDL_CreateWindow("Nimbus", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                options.width, options.height, flags);
    if (!g_window) {
        error = std::string("SDL_CreateWindow: ") + SDL_GetError();
        return false;
    }
    g_context = SDL_GL_CreateContext(g_window);
    if (!g_context) {
        error = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
        return false;
    }
    SDL_GL_MakeCurrent(g_window, g_context);
    SDL_GL_SetSwapInterval(options.vsync && !options.screenshot ? 1 : 0);
    SDL_StartTextInput();

    const int joystick_count = SDL_NumJoysticks();
    for (int i = 0; i < joystick_count; ++i) {
        if (SDL_IsGameController(i)) {
            open_controller(i);
            break;
        }
    }

    std::fprintf(stderr, "[gl] %s | %s\n", glGetString(GL_VERSION),
                 glGetString(GL_RENDERER));
    options.width = options.width;
    options.height = options.height;
    return true;
}

void shutdown() {
    if (g_controller) SDL_GameControllerClose(g_controller);
    g_controller = nullptr;
    if (g_context) SDL_GL_DeleteContext(g_context);
    g_context = nullptr;
    if (g_window) SDL_DestroyWindow(g_window);
    g_window = nullptr;
    SDL_Quit();
}

void poll(Input &input) {
    input = Input{};
    int w = 0, h = 0;
    SDL_GetWindowSize(g_window, &w, &h);
    input.mouse_x = 0;
    input.mouse_y = 0;
    SDL_Event event;
    bool first_motion = true;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                input.quit = true;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) input.quit = true;
                break;
            case SDL_KEYDOWN:
                if (!event.key.repeat) {
                    input.any_input = true;
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: input.escape = true; break;
                        case SDLK_BACKSPACE: input.backspace = true; break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER: input.enter = true; break;
                        case SDLK_LEFT: input.key_left = true; break;
                        case SDLK_RIGHT: input.key_right = true; break;
                        case SDLK_UP: input.key_up = true; break;
                        case SDLK_DOWN: input.key_down = true; break;
                        default: break;
                    }
                }
                break;
            case SDL_TEXTINPUT:
                input.text += event.text.text;
                input.text_input = true;
                input.any_input = true;
                break;
            case SDL_MOUSEMOTION:
                input.mouse_x = static_cast<float>(event.motion.x) * (1920.0f / std::max(1, w));
                input.mouse_y = static_cast<float>(event.motion.y) * (1080.0f / std::max(1, h));
                (void)first_motion;
                input.any_input = true;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    input.mouse_down = true;
                    input.mouse_pressed = true;
                    input.any_input = true;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    input.mouse_down = false;
                    input.mouse_released = true;
                }
                break;
            case SDL_MOUSEWHEEL:
                input.wheel = static_cast<float>(event.wheel.y);
                input.any_input = true;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (!g_controller) open_controller(event.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (g_controller) {
                    SDL_GameControllerClose(g_controller);
                    g_controller = nullptr;
                    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                        if (SDL_IsGameController(i)) {
                            open_controller(i);
                            break;
                        }
                    }
                }
                break;
            default: break;
        }
    }
    input.view_w = w;
    input.view_h = h;
    if (g_controller) {
        input.has_gamepad = true;
        input.lx = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX));
        input.ly = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY));
        input.rx = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX));
        input.ry = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY));
        input.l2 = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT), true);
        input.r2 = axis_value(SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT), true);
        input.buttons = current_buttons();
        input.pressed = input.buttons & ~g_previous_buttons;
        g_previous_buttons = input.buttons;
        if (std::fabs(input.lx - g_last_lx) > 0.05f || std::fabs(input.ly - g_last_ly) > 0.05f ||
            std::fabs(input.rx - g_last_rx) > 0.05f || std::fabs(input.ry - g_last_ry) > 0.05f ||
            std::fabs(input.l2 - g_last_l2) > 0.03f || std::fabs(input.r2 - g_last_r2) > 0.03f) {
            input.any_input = true;
            g_last_lx = input.lx;
            g_last_ly = input.ly;
            g_last_rx = input.rx;
            g_last_ry = input.ry;
            g_last_l2 = input.l2;
            g_last_r2 = input.r2;
        }
        if (input.pressed) input.any_input = true;
    } else {
        g_previous_buttons = 0;
    }
    if (input.key_left) input.mouse_x = 0;
}

void present() {
    if (g_options.screenshot) return;
    SDL_GL_SwapWindow(g_window);
}

double monotonic_seconds() {
    return static_cast<double>(SDL_GetPerformanceCounter()) /
           static_cast<double>(SDL_GetPerformanceFrequency());
}

double wall_seconds() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

void sleep_ms(int milliseconds) { SDL_Delay(static_cast<Uint32>(std::max(0, milliseconds))); }

std::string asset_dir() { return g_asset_dir; }
std::string data_dir() { return g_data_dir; }
std::string platform_name() { return "macOS / SDL2"; }
bool is_ps5() { return false; }

void set_rumble(float low, float high) {
    if (!g_controller) return;
    if (std::fabs(low - g_rumble_low) < 0.02f && std::fabs(high - g_rumble_high) < 0.02f) return;
    g_rumble_low = low;
    g_rumble_high = high;
    SDL_GameControllerRumble(g_controller, static_cast<Uint16>(clampf(low, 0, 1) * 65535),
                             static_cast<Uint16>(clampf(high, 0, 1) * 65535), 120);
}

void set_trigger_effect(float left, float right) { (void)left; (void)right; }
void notify(const std::string &text) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Nimbus", text.c_str(), g_window); }

}  // namespace nimbus::platform

#endif  // !NIMBUS_PS5
