// Nimbus - native PS5 platform layer (EGL/OpenGL + DualSense).
//
// Built only for the PS5 title; the desktop build uses platform/desktop.cpp.
#include "platform/platform.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

#include "gfx/math.hpp"

// Imports supplied by the public ps5-payload-sdk stubs.
extern "C" {
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetInitialUser(int *user_id);
int scePadInit(void);
int scePadOpen(int user_id, int type, int index, const void *params);
int scePadReadState(int handle, void *data);
int scePadClose(int handle);
int scePadSetVibration(int handle, const void *vibration);
int sceSystemServiceHideSplashScreen(void);
int sceKernelSendNotificationRequest(int device, void *request, size_t size, int blocking);
}

namespace nimbus::platform {
namespace {

constexpr uint32_t kBtnL3 = 0x0002;
constexpr uint32_t kBtnR3 = 0x0004;
constexpr uint32_t kBtnOptions = 0x0008;
constexpr uint32_t kBtnUp = 0x0010;
constexpr uint32_t kBtnRight = 0x0020;
constexpr uint32_t kBtnDown = 0x0040;
constexpr uint32_t kBtnLeft = 0x0080;
constexpr uint32_t kBtnL2 = 0x0100;
constexpr uint32_t kBtnR2 = 0x0200;
constexpr uint32_t kBtnL1 = 0x0400;
constexpr uint32_t kBtnR1 = 0x0800;
constexpr uint32_t kBtnTriangle = 0x1000;
constexpr uint32_t kBtnCircle = 0x2000;
constexpr uint32_t kBtnCross = 0x4000;
constexpr uint32_t kBtnSquare = 0x8000;
constexpr uint32_t kBtnTouchpad = 0x100000;

struct PadData {
    uint32_t buttons;
    struct { uint8_t x, y; } left_stick;
    struct { uint8_t x, y; } right_stick;
    struct { uint8_t l2, r2; } analog;
    uint16_t padding;
    struct { float x, y, z, w; } quat;
    struct { float x, y, z; } velocity;
    struct { float x, y, z; } acceleration;
    struct {
        uint8_t fingers;
        uint8_t pad1[3];
        uint32_t pad2;
        struct { uint16_t x, y; uint8_t finger; uint8_t pad[3]; } touch[2];
    } touch;
    uint8_t connected;
    uint64_t timestamp;
    uint8_t ext[16];
    uint8_t count;
    uint8_t unknown[15];
};
static_assert(sizeof(PadData) == 120, "ScePad sample layout");

struct PadVibration {
    uint8_t large;
    uint8_t small;
};

EGLDisplay g_display = EGL_NO_DISPLAY;
EGLSurface g_surface = EGL_NO_SURFACE;
EGLContext g_context = EGL_NO_CONTEXT;
int g_pad = -1;
int g_user = -1;
uint32_t g_previous_buttons = 0;
Options g_options;
std::string g_asset_dir = "/app0/assets";
std::string g_data_dir = "/data/nimbus";
float g_rumble_low = 0, g_rumble_high = 0;

float stick(uint8_t value) {
    const float v = (static_cast<float>(value) - 128.0f) / 127.0f;
    if (std::fabs(v) < 0.14f) return 0.0f;
    const float sign = v < 0 ? -1.0f : 1.0f;
    return sign * (std::fabs(v) - 0.14f) / 0.86f;
}

void make_dirs(const std::string &path) {
    std::string current;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!current.empty()) ::mkdir(current.c_str(), 0755);
        }
        if (i < path.size()) current.push_back(path[i]);
    }
}

void open_pad() {
    if (g_pad >= 0) return;
    if (g_user < 0) {
        if (sceUserServiceGetInitialUser(&g_user) != 0) return;
    }
    g_pad = scePadOpen(g_user, 0, 0, nullptr);
    if (g_pad < 0) g_pad = scePadOpen(0xff, 0, 0, nullptr);
    if (g_pad >= 0) std::fprintf(stderr, "[ps5] pad opened for user %d\n", g_user);
}

void send_notification(const std::string &text) {
    struct {
        char unused[45];
        char message[3075];
    } request{};
    std::snprintf(request.message, sizeof(request.message), "%s", text.c_str());
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
}

}  // namespace

bool init(Options &options, std::string &error) {
    g_options = options;
    g_asset_dir = (options.assets_dir.empty() || options.assets_dir == "assets") ? "/app0/assets"
                                                                         : options.assets_dir;
    g_data_dir = options.data_dir.empty() ? "/data/nimbus" : options.data_dir;
    make_dirs(g_data_dir);
    make_dirs(g_data_dir + "/cache");
    make_dirs(g_data_dir + "/tiles");
    sceSystemServiceHideSplashScreen();

    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (g_display == EGL_NO_DISPLAY || !eglInitialize(g_display, &major, &minor)) {
        error = "EGL display initialisation failed";
        return false;
    }
    eglBindAPI(EGL_OPENGL_API);
    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
        EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfig(g_display, config_attributes, &config, 1, &count) || count < 1) {
        error = "no EGL config";
        return false;
    }
    g_surface = eglCreateWindowSurface(g_display, config, (EGLNativeWindowType)0, nullptr);
    const EGLint context_attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
        EGL_CONTEXT_MINOR_VERSION_KHR, 6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_NONE,
    };
    g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, context_attributes);
    if (g_surface == EGL_NO_SURFACE || g_context == EGL_NO_CONTEXT ||
        !eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
        error = "EGL surface/context creation failed";
        return false;
    }
    EGLint width = 0, height = 0;
    eglQuerySurface(g_display, g_surface, EGL_WIDTH, &width);
    eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &height);
    eglSwapInterval(g_display, 1);
    options.width = width > 0 ? width : 1920;
    options.height = height > 0 ? height : 1080;
    std::fprintf(stderr, "[ps5] EGL %d.%d, %dx%d, GL %s\n", major, minor, options.width,
                 options.height, glGetString(GL_VERSION));

    sceUserServiceInitialize(nullptr);
    scePadInit();
    open_pad();
    return true;
}

void shutdown() {
    if (g_pad >= 0) scePadClose(g_pad);
    g_pad = -1;
    if (g_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
        if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
        eglTerminate(g_display);
    }
    g_display = EGL_NO_DISPLAY;
    g_surface = EGL_NO_SURFACE;
    g_context = EGL_NO_CONTEXT;
}

void poll(Input &input) {
    input = Input{};
    if (g_pad < 0) open_pad();
    if (g_pad < 0) {
        input.view_w = g_options.width;
        input.view_h = g_options.height;
        return;
    }
    PadData pad{};
    const int read = scePadReadState(g_pad, &pad);
    if (read < 0) return;
    input.has_gamepad = true;
    input.lx = stick(pad.left_stick.x);
    input.ly = stick(pad.left_stick.y);
    input.rx = stick(pad.right_stick.x);
    input.ry = stick(pad.right_stick.y);
    input.l2 = pad.analog.l2 / 255.0f;
    input.r2 = pad.analog.r2 / 255.0f;
    uint32_t buttons = 0;
    const uint32_t b = pad.buttons;
    if (b & kBtnCross) buttons |= BtnCross;
    if (b & kBtnCircle) buttons |= BtnCircle;
    if (b & kBtnSquare) buttons |= BtnSquare;
    if (b & kBtnTriangle) buttons |= BtnTriangle;
    if (b & kBtnL1) buttons |= BtnL1;
    if (b & kBtnR1) buttons |= BtnR1;
    if (b & kBtnL2) buttons |= BtnL2;
    if (b & kBtnR2) buttons |= BtnR2;
    if (b & kBtnUp) buttons |= BtnUp;
    if (b & kBtnDown) buttons |= BtnDown;
    if (b & kBtnLeft) buttons |= BtnLeft;
    if (b & kBtnRight) buttons |= BtnRight;
    if (b & kBtnOptions) buttons |= BtnOptions;
    if (b & kBtnTouchpad) buttons |= BtnTouchpad;
    if (b & kBtnL3) buttons |= BtnL3;
    if (b & kBtnR3) buttons |= BtnR3;
    input.buttons = buttons;
    input.pressed = buttons & ~g_previous_buttons;
    g_previous_buttons = buttons;
    input.any_input = input.pressed != 0 || std::fabs(input.lx) > 0.1f ||
                      std::fabs(input.ly) > 0.1f || std::fabs(input.rx) > 0.1f ||
                      std::fabs(input.ry) > 0.1f || input.l2 > 0.1f || input.r2 > 0.1f;
    input.view_w = g_options.width;
    input.view_h = g_options.height;
}

void present() {
    if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE) {
        if (!eglSwapBuffers(g_display, g_surface)) {
            std::fprintf(stderr, "[ps5] eglSwapBuffers failed\n");
        }
    }
}

double monotonic_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1e9;
}

double wall_seconds() { return static_cast<double>(std::time(nullptr)); }
void sleep_ms(int milliseconds) { usleep(static_cast<useconds_t>(std::max(0, milliseconds)) * 1000u); }
std::string asset_dir() { return g_asset_dir; }
std::string data_dir() { return g_data_dir; }
std::string platform_name() { return "PlayStation 5"; }
bool is_ps5() { return true; }

void set_rumble(float low, float high) {
    if (g_pad < 0) return;
    if (std::fabs(low - g_rumble_low) < 0.02f && std::fabs(high - g_rumble_high) < 0.02f) return;
    g_rumble_low = clampf(low, 0, 1);
    g_rumble_high = clampf(high, 0, 1);
    PadVibration vibration{
        static_cast<uint8_t>(clampf(low, 0, 1) * 255.0f),
        static_cast<uint8_t>(clampf(high, 0, 1) * 255.0f),
    };
    scePadSetVibration(g_pad, &vibration);
}

void set_trigger_effect(float left, float right) { (void)left; (void)right; }
void notify(const std::string &text) { send_notification(text); }

}  // namespace nimbus::platform
