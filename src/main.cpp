#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app/app.hpp"
#include "platform/platform.hpp"

namespace {

nimbus::AppOptions parse_args(int argc, char **argv) {
    nimbus::AppOptions options;
    options.platform.width = 1600;
    options.platform.height = 900;
    options.start_altitude_km = 12000.0;
    bool start_override = false;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (key == "--width") options.platform.width = std::max(320, std::atoi(next().c_str()));
        else if (key == "--height") options.platform.height = std::max(240, std::atoi(next().c_str()));
        else if (key == "--screenshot") {
            options.platform.screenshot = true;
            options.platform.screenshot_path = next();
        } else if (key == "--screen") {
            const std::string screen = next();
            options.screen = screen == "flights" ? 1 : screen == "satellites" ? 2 : screen == "settings" ? 3 : 0;
        } else if (key == "--modal") {
            options.modal = next();
        } else if (key == "--offline") options.offline = true;
        else if (key == "--demo") options.demo = true;
        else if (key == "--hidden") options.platform.hidden = true;
        else if (key == "--fullscreen") options.platform.fullscreen = true;
        else if (key == "--assets") options.platform.assets_dir = next();
        else if (key == "--data") options.platform.data_dir = next();
        else if (key == "--utc") options.utc = next();
        else if (key == "--style") {
            options.style = next();
        } else if (key == "--layer") {
            const std::string layer = next();
            options.layer_override = layer == "temp" ? 0 : layer == "humidity" ? 1
                                    : layer == "pressure" ? 2 : -1;
        } else if (key == "--wind") {
            options.wind_override = next() == "on" ? 1 : 0;
        } else if (key == "--lat") {
            options.start_lat = std::atof(next().c_str());
            start_override = true;
        } else if (key == "--lon") {
            options.start_lon = std::atof(next().c_str());
            start_override = true;
        } else if (key == "--alt") {
            options.start_altitude_km = std::atof(next().c_str());
            start_override = true;
        } else if (key == "--tilt") {
            options.start_tilt_degrees = std::atof(next().c_str());
            start_override = true;
        } else if (key == "--heading") {
            options.start_heading_degrees = std::atof(next().c_str());
            start_override = true;
        } else if (key == "--wait") {
            options.screenshot_wait = std::atof(next().c_str());
        } else if (key == "--skip-boot") options.skip_boot = true;
        else if (key == "--help") {
            std::printf(
                "Nimbus — live Earth for PS5 homebrew\n"
                "  --screenshot PATH   render a PNG and exit\n"
                "  --screen NAME       globe | flights | satellites\n"
                "  --style NAME        satellite | atlas | streets\n"
                "  --layer NAME        temp | humidity | pressure | off\n"
                "  --wind on|off       toggle wind streamlines\n"
                "  --lat / --lon       starting latitude / longitude\n"
                "  --alt KM            starting altitude above the surface\n"
                "  --tilt DEG          camera tilt (0 = top-down, 82 = horizon)\n"
                "  --utc ISO8601       freeze the sky clock\n"
                "  --offline           use only cached tiles and settings\n"
                "  --width / --height  window or screenshot size\n");
            std::exit(0);
        }
    }
    if (options.platform.screenshot) {
        options.platform.hidden = true;
        options.platform.width = std::max(1920, options.platform.width);
        options.platform.height = std::max(1080, options.platform.height);
        options.platform.vsync = false;
        options.skip_boot = true;
    }
    options.start_override = start_override;
    if (start_override) options.start_altitude_km = std::max(0.05, options.start_altitude_km);
    return options;
}

}  // namespace

int main(int argc, char **argv) {
    nimbus::AppOptions options = parse_args(argc, argv);
    std::string error;
    nimbus::platform::Options platform_options = options.platform;
    if (!nimbus::platform::init(platform_options, error)) {
        std::fprintf(stderr, "nimbus: %s\n", error.c_str());
        return 1;
    }
    options.platform.width = platform_options.width;
    options.platform.height = platform_options.height;

    nimbus::App app;
    if (!app.init(options)) {
        nimbus::platform::shutdown();
        return 1;
    }

    double previous = nimbus::platform::monotonic_seconds();
    while (!app.wants_quit()) {
        nimbus::platform::Input input;
        nimbus::platform::poll(input);
        if (input.quit) break;
        const double now = nimbus::platform::monotonic_seconds();
        const double dt = std::min(0.1, std::max(0.0, now - previous));
        previous = now;
        app.update(input, dt);
        app.render();
        nimbus::platform::present();
    }

    app.shutdown();
    nimbus::platform::shutdown();
    return 0;
}
