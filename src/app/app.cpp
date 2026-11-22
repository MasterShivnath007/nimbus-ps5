#include "app/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "app/astro.hpp"
#include "app/timeutil.hpp"
#include "data/json.hpp"
#include "gfx/math.hpp"

namespace nimbus {

namespace {
double parse_utc(const std::string &text) {
    if (text.empty()) return 0.0;
    std::tm tm{};
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 3)
        return 0.0;
    return static_cast<double>(unix_from_utc(tm.tm_year, static_cast<unsigned>(tm.tm_mon),
                                             static_cast<unsigned>(tm.tm_mday),
                                             static_cast<unsigned>(tm.tm_hour),
                                             static_cast<unsigned>(tm.tm_min),
                                             static_cast<unsigned>(tm.tm_sec)));
}

const char *kCuratedCities[] = {
    "New Delhi", "Mumbai", "Bengaluru", "Tokyo", "London", "New York", "Paris", "Singapore",
    "Dubai", "Sydney", "Cape Town", "Los Angeles", "Sao Paulo", "Cairo", "Bangkok", "Seoul",
    "Istanbul", "Mexico City", "Toronto", "Berlin", "Madrid", "Rome", "Amsterdam", "Hong Kong",
};
}  // namespace

bool App::init(const AppOptions &options) {
    options_ = options;
    online_ = !options.offline;
    settings_.load(platform::data_dir());
    if (options.platform.screenshot) boot_active_ = false;
    if (options.skip_boot || !settings_.boot_animation) boot_active_ = false;
    boot_elapsed_ = 0;

    if (!sky_.init(platform::asset_dir())) {
        std::fprintf(stderr, "nimbus: sky init failed\n");
        return false;
    }
    if (!earth_.init(platform::asset_dir(), platform::data_dir())) {
        std::fprintf(stderr, "nimbus: earth init failed\n");
        return false;
    }
    if (!post_.init(platform::asset_dir())) {
        std::fprintf(stderr, "nimbus: post init failed\n");
        return false;
    }
    if (!ui_.init(4096) || !ui_.load_fonts(platform::asset_dir())) {
        std::fprintf(stderr, "nimbus: ui init failed\n");
        return false;
    }
    const std::string ca = platform::asset_dir() + "/cacert.pem";
    if (!hub_.init(platform::asset_dir(), platform::data_dir(), ca, &settings_)) {
        std::fprintf(stderr, "nimbus: data init failed\n");
        return false;
    }
    hub_.set_online(online_);

    if (!options.style.empty()) {
        if (options.style == "atlas") settings_.map_style = 1;
        else if (options.style == "streets") settings_.map_style = 2;
        else settings_.map_style = 0;
    }
    if (options.layer_override != -2) settings_.weather_layer = options.layer_override;
    if (options.wind_override >= 0) settings_.wind_layer = options.wind_override != 0;
    apply_settings();

    const double start_lat = options.start_override ? options.start_lat : settings_.home_lat;
    const double start_lon = options.start_override ? options.start_lon : settings_.home_lon;
    const double altitude = options.start_altitude_km > 0 ? options.start_altitude_km * 1000.0
                                                           : 12000000.0;
    camera_.set_focus(start_lat, start_lon, altitude);
    camera_.tilt = options.start_tilt_degrees * kDeg;
    camera_.heading = options.start_heading_degrees * kDeg;
    camera_.update(1920, 1080);
    if (settings_.first_run && !options.platform.screenshot) {
        modal_ = ModalLocation;
        location_for_home_ = true;
        location_keyboard_ = false;
        location_query_.clear();
        refresh_location_results();
    }
    screen_ = static_cast<Screen>(std::max(0, std::min(3, options.screen)));
    if (screen_ == ScreenSettings) {
        settings_return_ = ScreenGlobe;
    } else {
        settings_return_ = ScreenGlobe;
    }
    if (screen_ == ScreenFlights) {
        camera_.set_focus(start_lat, start_lon, 900000.0);
        satellite_filter_ = 1;
    } else if (screen_ == ScreenSatellites) {
        camera_.set_focus(start_lat, start_lon, 12500000.0);
        satellite_filter_ = 1;
    }

    if (options.start_override) {
        double distance = 0;
        const City *city = hub_.places().nearest(start_lat, start_lon, 2.0, &distance);
        hub_.request_point(start_lat, start_lon, city ? city->name : "Selected location");
    } else {
        hub_.request_point(settings_.home_lat, settings_.home_lon, settings_.home_name);
    }
    if (options.modal == "location") {
        modal_ = ModalLocation;
        location_for_home_ = true;
        location_query_.clear();
        refresh_location_results();
    } else if (options.modal == "discover") {
        modal_ = ModalDiscover;
    } else if (options.modal == "help") {
        modal_ = ModalHelp;
    } else if (options.modal == "credits") {
        modal_ = ModalCredits;
    }
    hub_.request_weather_now();
    hub_.request_cyclones_now();
    hub_.request_flights(settings_.home_lat, settings_.home_lon, false);

    wall_time_ = options.utc.empty() ? platform::wall_seconds() : parse_utc(options.utc);
    if (wall_time_ <= 0) wall_time_ = platform::wall_seconds();
    sky_time_ = wall_time_;
    last_frame_ = platform::monotonic_seconds();
    screenshot_start_ = last_frame_;
    initialized_ = true;
    std::fprintf(stderr, "[app] ready at %.3f, %.3f\n", settings_.home_lat, settings_.home_lon);
    return true;
}

void App::shutdown() {
    if (settings_dirty_) settings_.save(platform::data_dir());
    hub_.shutdown();
    earth_.release();
    sky_.release();
    post_.release();
    initialized_ = false;
}

void App::apply_settings() {
    settings_.map_style = std::max(0, std::min(2, settings_.map_style));
    settings_.weather_layer = std::max(-1, std::min(2, settings_.weather_layer));
    settings_.quality = std::max(0, std::min(2, settings_.quality));
    settings_.ambient_minutes = std::max(0, std::min(30, settings_.ambient_minutes));
    switch (settings_.quality) {
        case 0: settings_.render_scale = 0.85f; break;
        case 2: settings_.render_scale = 1.25f; break;
        default: settings_.render_scale = 1.0f; break;
    }
    online_ = !options_.offline;
    hub_.set_online(online_);
    earth_.set_style(static_cast<gfx::MapStyle>(settings_.map_style));
    earth_.set_weather_layer(settings_.weather_layer);
    earth_.set_wind_visible(settings_.wind_layer);
    earth_.set_borders_visible(settings_.borders);
    earth_.set_clouds_visible(settings_.clouds);
}

void App::update(const platform::Input &input, double dt) {
    if (!initialized_) return;
    dt = std::max(0.0, std::min(0.1, dt));
    window_w_ = std::max(1, input.view_w);
    window_h_ = std::max(1, input.view_h);
    const float ui_scale = std::max(0.35f, static_cast<float>(window_h_) / 1080.0f);
    const int virtual_w = static_cast<int>(window_w_ / ui_scale + 0.5f);
    camera_.update(virtual_w, 1080);

    wall_time_ = options_.utc.empty() ? platform::wall_seconds() : parse_utc(options_.utc);
    if (wall_time_ <= 0) wall_time_ = platform::wall_seconds();
    const double offset = scrubbing_ ? scrub_offset_minutes_
                                     : (settings_.live_time ? 0.0 : settings_.time_offset_minutes);
    sky_time_ = wall_time_ + offset * 60.0;
    frame_phase_ += dt;
    screen_fade_ = std::min(1.0f, screen_fade_ + static_cast<float>(dt) * 3.2f);

    if (boot_active_) {
        boot_elapsed_ += dt;
        if (boot_elapsed_ > 2.2 || input.any_input || options_.platform.screenshot) boot_active_ = false;
        if (boot_active_) {
            idle_seconds_ = 0;
            return;
        }
    }

    const bool had_input = input.any_input || input.pressed || input.mouse_pressed ||
                           std::fabs(input.lx) > 0.12f || std::fabs(input.ly) > 0.12f;
    if (had_input) {
        idle_seconds_ = 0;
        if (ambient_) {
            ambient_ = false;
            hud_visible_ = true;
        }
    } else {
        idle_seconds_ += dt;
    }

    process_mouse(input);
    if (modal_ != ModalNone) {
        if (input.pressed & platform::BtnOptions) {
            modal_ = ModalNone;
        } else if (input.pressed & platform::BtnCircle) {
            if (modal_ == ModalLocation && location_keyboard_) location_keyboard_ = false;
            else if (modal_ == ModalLocation && settings_.first_run) {
                // Keep the first-run picker available rather than trapping the
                // user in a modal they cannot dismiss.
                modal_ = ModalNone;
            } else {
                modal_ = ModalNone;
            }
        } else if (modal_ == ModalLocation) {
            handle_location(input);
        } else if (modal_ == ModalDiscover) {
            handle_discover(input);
        } else {
            handle_help(input);
        }
        return;
    }

    handle_global(input);
    if (quit_) return;
    if (screen_ == ScreenSettings) {
        handle_settings(input);
    } else if (screen_ == ScreenFlights) {
        handle_flights(input, dt);
    } else if (screen_ == ScreenSatellites) {
        handle_satellites(input, dt);
    } else {
        handle_globe(input, dt);
    }
    update_ambient(dt);

    // Camera must be valid before data projection and rendering.
    camera_.update(virtual_w, 1080);
    hub_.update(platform::monotonic_seconds(), camera_, static_cast<int>(screen_), online_, earth_);

    if (hub_.point().valid) {
        // Keep the point card's local time in sync with the selected location.
        if (settings_.home_utc_offset == 0 || std::fabs(hub_.point().latitude - settings_.home_lat) < 0.35)
            settings_.home_utc_offset = hub_.point().utc_offset_seconds;
    }

    if (settings_dirty_ && platform::monotonic_seconds() - last_save_ > 1.5) {
        settings_.save(platform::data_dir());
        settings_dirty_ = false;
        last_save_ = platform::monotonic_seconds();
    }
}

void App::handle_global(const platform::Input &input) {
    if (input.pressed & platform::BtnOptions) {
        if (screen_ == ScreenSettings) {
            screen_ = settings_return_;
        } else {
            settings_return_ = screen_;
            screen_ = ScreenSettings;
            settings_focus_ = 0;
        }
        screen_fade_ = 0;
        return;
    }
    if (input.pressed & platform::BtnTouchpad) reset_view(true);
    if ((input.pressed & platform::BtnTriangle) && screen_ == ScreenGlobe) {
        if (ambient_) {
            ambient_ = false;
            hud_visible_ = true;
        } else {
            hud_visible_ = !hud_visible_;
            if (hud_visible_) toast("Interface on");
        }
    }
    if (screen_ != ScreenSettings && modal_ == ModalNone) {
        if (input.pressed & platform::BtnL1) switch_screen((static_cast<int>(screen_) + 2) % 3);
        if (input.pressed & platform::BtnR1) switch_screen((static_cast<int>(screen_) + 1) % 3);
    }
    if (input.pressed & platform::BtnR3) {
        const Destination *destination = hub_.places().random_destination(random_seed_++);
        if (destination) {
            screen_ = ScreenGlobe;
            fly_to(destination->lat, destination->lon, 2.2, 58.0, 5.2);
            hub_.request_point(destination->lat, destination->lon, destination->name);
            toast(std::string("Flying to ") + destination->name);
        }
    }
}

void App::switch_screen(int screen) {
    if (screen < 0 || screen > 3) return;
    if (screen == static_cast<int>(screen_)) return;
    screen_ = static_cast<Screen>(screen);
    screen_fade_ = 0;
    label_rects_.clear();
    if (screen_ == ScreenFlights) {
        flight_focus_ = 0;
        flight_scroll_ = 0;
        flight_world_ = false;
        const double altitude = 900000.0;
        camera_.set_focus(settings_.home_lat, settings_.home_lon, altitude);
        camera_.tilt = 0;
        hub_.request_flights(settings_.home_lat, settings_.home_lon, false);
        toast("Live airspace around home");
    } else if (screen_ == ScreenSatellites) {
        satellite_focus_ = 0;
        satellite_scroll_ = 0;
        satellite_filter_ = 1;
        camera_.set_focus(settings_.home_lat, settings_.home_lon, 12500000.0);
        camera_.tilt = 0;
        toast("Orbital traffic — D-pad selects");
    } else if (screen_ == ScreenGlobe) {
        toast("Live Earth");
    } else if (screen_ == ScreenSettings) {
        settings_return_ = ScreenGlobe;
        settings_focus_ = 0;
    }
}

void App::update_ambient(double dt) {
    (void)dt;
    if (settings_.ambient_minutes <= 0) return;
    if (modal_ != ModalNone || screen_ == ScreenSettings) return;
    if (idle_seconds_ > settings_.ambient_minutes * 60.0) {
        ambient_ = true;
        hud_visible_ = false;
    }
}

void App::reset_view(bool home) {
    const double lat = home ? settings_.home_lat : camera_.lat;
    const double lon = home ? settings_.home_lon : camera_.lon;
    camera_.fly_to(lat, lon, 12000000.0, 0.0, 2.8);
    if (home) {
        screen_ = ScreenGlobe;
        hub_.request_point(settings_.home_lat, settings_.home_lon, settings_.home_name);
    }
    toast(home ? "Returning home" : "Orbit restored");
}

void App::fly_to(double lat, double lon, double altitude_km, double tilt_degrees, double duration) {
    camera_.fly_to(lat, lon, std::max(0.03, altitude_km) * 1000.0, tilt_degrees * kDeg, duration);
}

void App::focus_city(const City &city, bool inspect) {
    const bool satellite = settings_.map_style == 0;
    const double altitude = inspect ? (satellite ? 18.0 : 2.4) : 7.0;
    const double tilt = 0.0;
    fly_to(city.lat, city.lon, altitude, tilt, inspect ? 4.4 : 3.2);
    hub_.request_point(city.lat, city.lon, city.name);
    screen_ = ScreenGlobe;
    toast(city.name + ", " + city.country);
}

void App::select_home(const City &city) {
    settings_.home_name = city.name;
    settings_.home_country = city.country;
    settings_.home_lat = city.lat;
    settings_.home_lon = city.lon;
    settings_.first_run = false;
    settings_dirty_ = true;
    hub_.set_home(city.lat, city.lon);
    hub_.request_point(city.lat, city.lon, city.name);
    modal_ = ModalNone;
    screen_ = ScreenGlobe;
    fly_to(city.lat, city.lon, 9000.0, 0.0, 3.6);
    toast("Home set to " + city.name);
    apply_settings();
}

void App::save_settings() {
    settings_.save(platform::data_dir());
    settings_dirty_ = false;
}

void App::toast(const std::string &message, double seconds) {
    toast_text_ = message;
    toast_until_ = platform::monotonic_seconds() + seconds;
}

void App::handle_globe(const platform::Input &input, double dt) {
    if (input.any_input) {
        // no-op: idle bookkeeping already happened
    }
    const bool surface = camera_.height < 260000.0;
    if (std::fabs(input.lx) > 0.1f || std::fabs(input.ly) > 0.1f) {
        camera_.pan_pixels(-input.lx * 680.0f * dt, -input.ly * 680.0f * dt);
    }
    if (surface) {
        if (std::fabs(input.rx) > 0.1f) {
            camera_.heading += input.rx * dt * 0.9;
            camera_.heading = std::fmod(camera_.heading, 6.2831853f);
        }
        if (std::fabs(input.ry) > 0.1f) {
            // Surface views stay north-up; the orbital view owns the drama.
        }
    } else if (std::fabs(input.rx) > 0.1f || std::fabs(input.ry) > 0.1f) {
        camera_.pan_pixels(-input.rx * 1100.0f * dt, -input.ry * 1100.0f * dt);
    }
    const float zoom = std::max(0.0f, input.r2) - std::max(0.0f, input.l2);
    if (std::fabs(zoom) > 0.04f) {
        camera_.zoom_by(std::exp(-static_cast<double>(zoom) * dt * 2.4));
        if (input.has_gamepad) platform::set_rumble(0.0f, 0.06f + std::fabs(zoom) * 0.10f);
    }
    if (input.wheel != 0) camera_.zoom_by(std::pow(0.84, input.wheel));
    if (input.mouse_down) {
        if (!mouse_drag_started_) {
            mouse_drag_started_ = true;
            drag_x_ = input.mouse_x;
            drag_y_ = input.mouse_y;
        }
        camera_.pan_pixels((input.mouse_x - drag_x_) * 1.6f,
                           (input.mouse_y - drag_y_) * 1.6f);
        drag_x_ = input.mouse_x;
        drag_y_ = input.mouse_y;
    } else {
        mouse_drag_started_ = false;
    }
    if (input.key_left) camera_.pan_pixels(-620.0f * dt, 0);
    if (input.key_right) camera_.pan_pixels(620.0f * dt, 0);
    if (input.key_up) camera_.pan_pixels(0, -620.0f * dt);
    if (input.key_down) camera_.pan_pixels(0, 620.0f * dt);

    if (input.pressed & platform::BtnLeft) {
        settings_.weather_layer -= 1;
        if (settings_.weather_layer < -1) settings_.weather_layer = 2;
        settings_dirty_ = true;
        apply_settings();
        toast(layer_name(settings_.weather_layer));
    }
    if (input.pressed & platform::BtnRight) {
        settings_.weather_layer += 1;
        if (settings_.weather_layer > 2) settings_.weather_layer = -1;
        settings_dirty_ = true;
        apply_settings();
        toast(layer_name(settings_.weather_layer));
    }
    if (input.pressed & platform::BtnUp) {
        settings_.map_style = (settings_.map_style + 2) % 3;
        settings_dirty_ = true;
        apply_settings();
        toast(map_style_name(settings_.map_style));
    }
    if (input.pressed & platform::BtnDown) {
        settings_.map_style = (settings_.map_style + 1) % 3;
        settings_dirty_ = true;
        apply_settings();
        toast(map_style_name(settings_.map_style));
    }
    if (input.pressed & platform::BtnSquare) {
        settings_.wind_layer = !settings_.wind_layer;
        settings_dirty_ = true;
        apply_settings();
        toast(settings_.wind_layer ? "Wind streamlines on" : "Wind streamlines off");
    }
    if (input.pressed & platform::BtnCross) {
        const City *city = nearest_city_to_center(camera_.height > 1500000.0 ? 8.0 : 2.2);
        if (city) {
            focus_city(*city, true);
            platform::set_rumble(0.25f, 0.35f);
        } else {
            toast("No city under the reticle — drag the globe");
        }
    }
    if (input.pressed & platform::BtnCircle) {
        if (camera_.height < 2500000.0) {
            fly_to(camera_.lat, camera_.lon, 12000.0, 0.0, 2.2);
        } else {
            const City *city = nearest_city_to_center(10.0);
            if (city) focus_city(*city, false);
        }
    }
    if (input.buttons & platform::BtnL3) {
        if (!scrubbing_) {
            scrubbing_ = true;
            scrub_offset_minutes_ = 0;
        }
        scrub_offset_minutes_ += input.rx * dt * 240.0f;
        scrub_offset_minutes_ = clamp(scrub_offset_minutes_, -720.0, 720.0);
    } else if (scrubbing_) {
        scrubbing_ = false;
        scrub_offset_minutes_ = 0;
        toast("Live time restored");
    }

    if (settings_.auto_rotate && idle_seconds_ > 6.0 && !camera_.flying && !ambient_ &&
        camera_.orbit_mode) {
        camera_.lon = wrap_longitude(camera_.lon + dt * 0.45);
    } else if (settings_.auto_rotate && ambient_ && camera_.orbit_mode) {
        camera_.lon = wrap_longitude(camera_.lon + dt * 0.22);
    }
    camera_.fly_update(dt);
}

const City *App::nearest_city_to_center(double max_degrees, double *distance) const {
    return hub_.places().nearest(camera_.lat, camera_.lon, max_degrees, distance);
}

const City *App::nearest_city_to_screen(float x, float y, double max_pixels) const {
    const double half_lat = std::max(0.4, camera_.height / kEarthRadius / kDeg * 3.0);
    const double half_lon = half_lat / std::max(0.2, std::cos(camera_.lat * kDeg)) *
                            static_cast<double>(camera_.viewport_w) / camera_.viewport_h;
    const auto candidates = hub_.places().label_cities(camera_.lat, camera_.lon, half_lat, half_lon,
                                                       30, 0);
    const City *best = nullptr;
    double best_distance = max_pixels * max_pixels;
    for (const City *city : candidates) {
        const Vec3 point = latlon_to_unit(city->lat, city->lon) * static_cast<float>(kEarthRadius);
        if (!camera_.visible(point)) continue;
        float sx = 0, sy = 0;
        if (!camera_.project(point, sx, sy)) continue;
        const double dx = sx - x, dy = sy - y;
        const double distance = dx * dx + dy * dy;
        if (distance < best_distance) {
            best_distance = distance;
            best = city;
        }
    }
    return best;
}

std::string App::temperature(double value, bool with_unit) const {
    char buffer[32];
    if (settings_.celsius) {
        std::snprintf(buffer, sizeof(buffer), "%.0f%s", value, with_unit ? "°C" : "");
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.0f%s", value * 9.0 / 5.0 + 32.0,
                      with_unit ? "°F" : "");
    }
    return buffer;
}

std::string App::wind_speed(double meters_per_second) const {
    char buffer[40];
    if (settings_.wind_kmh) {
        std::snprintf(buffer, sizeof(buffer), "%.0f km/h", meters_per_second * 3.6);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.0f mph", meters_per_second * 2.237);
    }
    return buffer;
}

std::string App::distance_string(double km) const {
    char buffer[40];
    if (settings_.distance_km) {
        if (km < 10.0) std::snprintf(buffer, sizeof(buffer), "%.1f km", km);
        else std::snprintf(buffer, sizeof(buffer), "%.0f km", km);
    } else {
        if (km < 10.0) std::snprintf(buffer, sizeof(buffer), "%.1f mi", km * 0.621371);
        else std::snprintf(buffer, sizeof(buffer), "%.0f mi", km * 0.621371);
    }
    return buffer;
}

std::string App::pressure_string(double hpa) const {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f hPa", hpa);
    return buffer;
}

std::string App::time_string(double unix_time, bool with_utc_offset) const {
    return format_local(unix_time, with_utc_offset ? settings_.home_utc_offset : 0,
                        settings_.clock_24h);
}

std::string App::number(double value, int decimals) const {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
}

std::string App::ellipsize(int font, float size, const std::string &text, float width) const {
    if (ui_.text_width(font, size, text) <= width) return text;
    std::string result = text;
    while (!result.empty() && ui_.text_width(font, size, result + "…") > width) result.pop_back();
    return result + "…";
}

std::string App::latlon_string(double lat, double lon) const {
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.2f°%c  %.2f°%c", std::fabs(lat), lat >= 0 ? 'N' : 'S',
                  std::fabs(lon), lon >= 0 ? 'E' : 'W');
    return buffer;
}

std::string App::layer_name(int layer) const {
    switch (layer) {
        case 0: return "Temperature";
        case 1: return "Humidity";
        case 2: return "Pressure";
        default: return "Weather off";
    }
}

std::string App::map_style_name(int style) const {
    switch (style) {
        case 0: return "Satellite";
        case 1: return "Atlas";
        case 2: return "Streets";
        default: return "Satellite";
    }
}

std::string App::current_clock() const {
    return time_string(wall_time_, true);
}

void App::handle_flights(const platform::Input &input, double dt) {
    if (std::fabs(input.lx) > 0.1f || std::fabs(input.ly) > 0.1f)
        camera_.pan_pixels(-input.lx * 680.0f * static_cast<float>(dt),
                           -input.ly * 680.0f * static_cast<float>(dt));
    const float zoom = std::max(0.0f, input.r2) - std::max(0.0f, input.l2);
    if (std::fabs(zoom) > 0.04f)
        camera_.zoom_by(std::exp(-static_cast<double>(zoom) * dt * 2.4));
    if (input.wheel != 0) camera_.zoom_by(std::pow(0.84, input.wheel));
    if (input.mouse_down) {
        if (!mouse_drag_started_) {
            mouse_drag_started_ = true;
            drag_x_ = input.mouse_x;
            drag_y_ = input.mouse_y;
        }
        camera_.pan_pixels((input.mouse_x - drag_x_) * 1.6f, (input.mouse_y - drag_y_) * 1.6f);
        drag_x_ = input.mouse_x;
        drag_y_ = input.mouse_y;
    } else {
        mouse_drag_started_ = false;
    }
    if (input.pressed & platform::BtnUp) {
        --flight_focus_;
        if (flight_focus_ < 0) flight_focus_ = std::max(0, static_cast<int>(hub_.flights().size()) - 1);
    }
    if (input.pressed & platform::BtnDown) {
        ++flight_focus_;
        if (flight_focus_ >= static_cast<int>(hub_.flights().size())) flight_focus_ = 0;
    }
    if (input.pressed & platform::BtnSquare) {
        flight_world_ = !flight_world_;
        flight_focus_ = 0;
        camera_.set_focus(settings_.home_lat, settings_.home_lon,
                          flight_world_ ? 12000000.0 : 900000.0);
        hub_.request_flights(settings_.home_lat, settings_.home_lon, flight_world_);
        toast(flight_world_ ? "World snapshot — every 10 minutes" : "Local radar — every 30 seconds");
    }
    if (input.pressed & platform::BtnR1) {
        hub_.request_flights(camera_.lat, camera_.lon, flight_world_);
        toast("Scanning airspace");
    }
    if (input.pressed & platform::BtnCross && !hub_.flights().empty()) {
        const Aircraft &aircraft = hub_.flights()[flight_focus_ % hub_.flights().size()];
        fly_to(aircraft.lat, aircraft.lon, 160.0, 35.0, 3.2);
        toast(aircraft.callsign.empty() ? "Aircraft" : aircraft.callsign);
    }
    if (input.pressed & platform::BtnCircle) switch_screen(ScreenGlobe);
    if (input.pressed & platform::BtnTriangle) {
        settings_.show_orbits = !settings_.show_orbits;
        settings_dirty_ = true;
        toast(settings_.show_orbits ? "Trails shown" : "Trails hidden");
    }
    camera_.fly_update(dt);
}

std::vector<int> App::filtered_satellites() const {
    std::vector<int> indices;
    const auto &all = hub_.satellites();
    const std::string filter = active_satellite_category();
    for (size_t i = 0; i < all.size(); ++i) {
        if (!all[i].has_tle) continue;
        if (filter == "Overhead" && !all[i].overhead) continue;
        if (filter != "All" && filter != "Overhead" && all[i].category != filter) continue;
        indices.push_back(static_cast<int>(i));
    }
    return indices;
}

std::string App::active_satellite_category() const {
    switch (satellite_filter_) {
        case 0: return "Overhead";
        case 1: return "Station";
        case 2: return "Weather";
        case 3: return "Navigation";
        case 4: return "Science";
        default: return "All";
    }
}

const Satellite *App::selected_satellite() const {
    const auto indices = filtered_satellites();
    if (indices.empty()) return nullptr;
    const int index = indices[std::max(0, std::min(static_cast<int>(indices.size()) - 1, satellite_focus_))];
    return &hub_.satellites()[index];
}

void App::handle_satellites(const platform::Input &input, double dt) {
    if (std::fabs(input.lx) > 0.1f) camera_.pan_pixels(-input.lx * 680.0f * static_cast<float>(dt), 0);
    if (std::fabs(input.ly) > 0.1f)
        camera_.pan_pixels(0, -input.ly * 680.0f * static_cast<float>(dt));
    const float zoom = std::max(0.0f, input.r2) - std::max(0.0f, input.l2);
    if (std::fabs(zoom) > 0.04f)
        camera_.zoom_by(std::exp(-static_cast<double>(zoom) * dt * 2.1));
    if (input.wheel != 0) camera_.zoom_by(std::pow(0.84, input.wheel));
    const auto indices = filtered_satellites();
    if (input.pressed & platform::BtnUp) {
        --satellite_focus_;
        if (satellite_focus_ < 0) satellite_focus_ = std::max(0, static_cast<int>(indices.size()) - 1);
    }
    if (input.pressed & platform::BtnDown) {
        ++satellite_focus_;
        if (satellite_focus_ >= static_cast<int>(indices.size())) satellite_focus_ = 0;
    }
    if (input.pressed & platform::BtnLeft) {
        satellite_filter_ = (satellite_filter_ + 5) % 6;
        satellite_focus_ = 0;
        toast("Filter: " + active_satellite_category());
    }
    if (input.pressed & platform::BtnRight) {
        satellite_filter_ = (satellite_filter_ + 1) % 6;
        satellite_focus_ = 0;
        toast("Filter: " + active_satellite_category());
    }
    if (!indices.empty()) hub_.select_satellite(indices[std::min<int>(satellite_focus_, indices.size() - 1)]);
    if (input.pressed & platform::BtnTriangle) {
        settings_.show_orbits = !settings_.show_orbits;
        settings_dirty_ = true;
        toast(settings_.show_orbits ? "Orbit shown" : "Orbit hidden");
    }
    if (input.pressed & platform::BtnCross) {
        const Satellite *sat = selected_satellite();
        if (sat) {
            const double altitude = std::max(8000.0, std::min(45000.0, sat->altitude_km * 2.2));
            fly_to(sat->lat, sat->lon, altitude, 12.0, 3.0);
            toast(sat->name);
        }
    }
    if (input.pressed & platform::BtnSquare) {
        hub_.refresh_satellites(platform::monotonic_seconds());
        toast("Orbital elements refreshed");
    }
    if (input.pressed & platform::BtnCircle) switch_screen(ScreenGlobe);
    camera_.fly_update(dt);
}

void App::handle_help(const platform::Input &input) {
    if (input.pressed & (platform::BtnCircle | platform::BtnCross | platform::BtnOptions)) {
        modal_ = ModalNone;
    }
}

void App::process_mouse(const platform::Input &input) {
    if (!input.mouse_pressed) return;
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
        if (it->rect.contains(input.mouse_x, input.mouse_y)) {
            if (it->action) it->action();
            return;
        }
    }
}

void App::add_hit(const gfx::Rect &rect, std::function<void()> action) {
    hits_.push_back({rect, std::move(action)});
}

void App::refresh_location_results() {
    location_results_.clear();
    if (location_query_.empty()) {
        for (const char *name : kCuratedCities) {
            auto found = hub_.places().search(name, 1);
            if (!found.empty()) {
                bool duplicate = false;
                for (const City *city : location_results_) {
                    if (city == found[0]) duplicate = true;
                }
                if (!duplicate) location_results_.push_back(found[0]);
            }
        }
    } else {
        location_results_ = hub_.places().search(location_query_, 12);
    }
    location_focus_ = std::max(0, std::min(location_focus_,
                                          static_cast<int>(location_results_.size()) - 1));
}

void App::on_keyboard_character(char32_t codepoint) {
    if (codepoint < 32 || codepoint > 126) return;
    if (location_query_.size() >= 40) return;
    location_query_.push_back(static_cast<char>(codepoint));
    refresh_location_results();
}

void App::handle_location(const platform::Input &input) {
    if (input.text_input) {
        on_keyboard_character(static_cast<char32_t>(input.text.empty() ? 0 : input.text[0]));
        for (size_t i = 1; i < input.text.size(); ++i)
            on_keyboard_character(static_cast<unsigned char>(input.text[i]));
    }
    if (input.backspace && !location_query_.empty()) {
        location_query_.pop_back();
        refresh_location_results();
    }
    if (input.enter && !location_results_.empty()) {
        const City *city = location_results_[std::min<int>(location_focus_, location_results_.size() - 1)];
        if (location_for_home_) select_home(*city);
        else focus_city(*city, true);
        return;
    }
    if (location_keyboard_) {
        const char *keys[] = {
            "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM<>_",
        };
        const int rows = 4;
        const int cols = 10;
        if (input.pressed & platform::BtnLeft) keyboard_x_ = (keyboard_x_ + cols - 1) % cols;
        if (input.pressed & platform::BtnRight) keyboard_x_ = (keyboard_x_ + 1) % cols;
        if (input.pressed & platform::BtnUp) keyboard_y_ = (keyboard_y_ + rows - 1) % rows;
        if (input.pressed & platform::BtnDown) keyboard_y_ = (keyboard_y_ + 1) % rows;
        if (input.pressed & platform::BtnCross) {
            const char key = keys[std::min(keyboard_y_, rows - 1)][std::min(keyboard_x_, cols - 1)];
            if (key == '<') {
                if (!location_query_.empty()) location_query_.pop_back();
                refresh_location_results();
            } else if (key == '>') {
                location_keyboard_ = false;
            } else if (key == '_') {
                location_query_.push_back(' ');
                refresh_location_results();
            } else if (key != ' ') {
                on_keyboard_character(static_cast<unsigned char>(key));
            }
        }
        if (input.pressed & platform::BtnSquare) {
            if (!location_query_.empty()) {
                location_query_.pop_back();
                refresh_location_results();
            }
        }
        if (input.pressed & platform::BtnTriangle) location_keyboard_ = false;
        return;
    }
    if (input.pressed & platform::BtnUp) {
        location_focus_ = std::max(0, location_focus_ - 1);
    }
    if (input.pressed & platform::BtnDown) {
        location_focus_ = std::min(static_cast<int>(location_results_.size()) - 1, location_focus_ + 1);
    }
    if (input.pressed & platform::BtnCross && !location_results_.empty()) {
        const City *city = location_results_[location_focus_];
        if (location_for_home_) select_home(*city);
        else {
            focus_city(*city, true);
            modal_ = ModalNone;
        }
        return;
    }
    if (input.pressed & platform::BtnSquare) {
        location_keyboard_ = true;
        return;
    }
    if (input.pressed & platform::BtnTriangle) {
        modal_ = ModalNone;
        settings_.first_run = false;
        settings_dirty_ = true;
        return;
    }
}

void App::handle_discover(const platform::Input &input) {
    const int columns = 3;
    const auto &destinations = hub_.places().destinations();
    if (input.pressed & platform::BtnLeft) discover_focus_ = std::max(0, discover_focus_ - 1);
    if (input.pressed & platform::BtnRight)
        discover_focus_ = std::min(static_cast<int>(destinations.size()) - 1, discover_focus_ + 1);
    if (input.pressed & platform::BtnUp)
        discover_focus_ = std::max(0, discover_focus_ - columns);
    if (input.pressed & platform::BtnDown)
        discover_focus_ = std::min(static_cast<int>(destinations.size()) - 1, discover_focus_ + columns);
    if (input.pressed & platform::BtnL1) {
        discover_focus_ = std::max(0, discover_focus_ - 6);
    }
    if (input.pressed & platform::BtnR1) {
        discover_focus_ = std::min(static_cast<int>(destinations.size()) - 1, discover_focus_ + 6);
    }
    if (input.pressed & platform::BtnSquare) {
        discover_focus_ = static_cast<int>(random_seed_++ % destinations.size());
    }
    if (input.pressed & platform::BtnCross && !destinations.empty()) {
        const Destination &destination = destinations[discover_focus_];
        modal_ = ModalNone;
        screen_ = ScreenGlobe;
        fly_to(destination.lat, destination.lon, 2.2, 58.0, 5.0);
        hub_.request_point(destination.lat, destination.lon, destination.name);
        toast(std::string("Diving into ") + destination.name);
    }
    if (input.pressed & platform::BtnTriangle) modal_ = ModalNone;
}

void App::handle_settings(const platform::Input &input) {
    if (input.pressed & platform::BtnUp) {
        settings_focus_ = (settings_focus_ + SetCount - 1) % SetCount;
    }
    if (input.pressed & platform::BtnDown) {
        settings_focus_ = (settings_focus_ + 1) % SetCount;
    }
    if (input.pressed & platform::BtnLeft) cycle_setting(static_cast<SettingId>(settings_focus_), -1);
    if (input.pressed & platform::BtnRight) cycle_setting(static_cast<SettingId>(settings_focus_), 1);
    if (input.pressed & platform::BtnCross) activate_setting(static_cast<SettingId>(settings_focus_));
    if (input.pressed & platform::BtnCircle) screen_ = settings_return_;
    if (input.pressed & platform::BtnTriangle) {
        modal_ = ModalHelp;
    }
}

void App::cycle_setting(SettingId id, int direction) {
    switch (id) {
        case SetStyle:
            settings_.map_style = (settings_.map_style + direction + 3) % 3;
            break;
        case SetTemp: settings_.celsius = !settings_.celsius; break;
        case SetWindUnit: settings_.wind_kmh = !settings_.wind_kmh; break;
        case SetDistance: settings_.distance_km = !settings_.distance_km; break;
        case SetClock: settings_.clock_24h = !settings_.clock_24h; break;
        case SetLayerDefault:
            settings_.weather_layer = std::max(-1, std::min(2, settings_.weather_layer + direction));
            break;
        case SetWindOverlay: settings_.wind_layer = !settings_.wind_layer; break;
        case SetBorders: settings_.borders = !settings_.borders; break;
        case SetClouds: settings_.clouds = !settings_.clouds; break;
        case SetLabels: settings_.labels = !settings_.labels; break;
        case SetAutoRotate: settings_.auto_rotate = !settings_.auto_rotate; break;
        case SetAmbient: {
            static const int values[] = {0, 1, 3, 5, 10, 20};
            int index = 0;
            for (int i = 0; i < 6; ++i) if (values[i] == settings_.ambient_minutes) index = i;
            index = (index + direction + 6) % 6;
            settings_.ambient_minutes = values[index];
            break;
        }
        case SetBloom: settings_.bloom = !settings_.bloom; break;
        case SetQuality: settings_.quality = std::max(0, std::min(2, settings_.quality + direction)); break;
        case SetFps: settings_.show_fps = !settings_.show_fps; break;
        case SetDataSaver: settings_.data_saver = settings_.data_saver ? 0 : 1; break;
        default: break;
    }
    settings_dirty_ = true;
    apply_settings();
}

void App::activate_setting(SettingId id) {
    switch (id) {
        case SetHome:
            location_for_home_ = true;
            location_keyboard_ = false;
            location_query_.clear();
            refresh_location_results();
            modal_ = ModalLocation;
            break;
        case SetTemp:
        case SetWindUnit:
        case SetDistance:
        case SetClock:
        case SetStyle:
        case SetLayerDefault:
        case SetWindOverlay:
        case SetBorders:
        case SetClouds:
        case SetLabels:
        case SetAutoRotate:
        case SetAmbient:
        case SetBloom:
        case SetQuality:
        case SetFps:
        case SetDataSaver:
            cycle_setting(id, 1);
            break;
        case SetRefresh:
            hub_.request_weather_now();
            hub_.request_cyclones_now();
            toast("Refreshing weather and tropical systems");
            break;
        case SetHelp: modal_ = ModalHelp; break;
        case SetCredits: modal_ = ModalCredits; break;
        default: break;
    }
}

std::string App::setting_value(SettingId id) const {
    switch (id) {
        case SetHome: return settings_.home_name;
        case SetStyle: return map_style_name(settings_.map_style);
        case SetTemp: return settings_.celsius ? "Celsius" : "Fahrenheit";
        case SetWindUnit: return settings_.wind_kmh ? "km/h" : "mph";
        case SetDistance: return settings_.distance_km ? "Kilometres" : "Miles";
        case SetClock: return settings_.clock_24h ? "24-hour" : "12-hour";
        case SetLayerDefault: return layer_name(settings_.weather_layer);
        case SetWindOverlay: return settings_.wind_layer ? "On" : "Off";
        case SetBorders: return settings_.borders ? "On" : "Off";
        case SetClouds: return settings_.clouds ? "On" : "Off";
        case SetLabels: return settings_.labels ? "On" : "Off";
        case SetAutoRotate: return settings_.auto_rotate ? "On" : "Off";
        case SetAmbient: {
            if (settings_.ambient_minutes <= 0) return "Never";
            return std::to_string(settings_.ambient_minutes) + " min";
        }
        case SetBloom: return settings_.bloom ? "On" : "Off";
        case SetQuality:
            return settings_.quality == 0 ? "Performance" : settings_.quality == 2 ? "Cinematic" : "Balanced";
        case SetFps: return settings_.show_fps ? "Show" : "Hide";
        case SetDataSaver: return settings_.data_saver ? "On" : "Off";
        case SetRefresh: return "Now";
        case SetHelp: return "Controls & data";
        case SetCredits: return "Sources";
        default: return {};
    }
}

void App::request_current_location_weather() {
    hub_.request_point(camera_.lat, camera_.lon, "Selected location");
}

void App::render() {
    if (!initialized_) return;
    const int window_w = window_w_;
    const int window_h = window_h_;
    const float ui_scale = std::max(0.35f, static_cast<float>(window_h) / 1080.0f);
    const int virtual_w = static_cast<int>(window_w / ui_scale + 0.5f);
    const int virtual_h = 1080;
    camera_.update(virtual_w, virtual_h);

    const SkyState sky = compute_sky(sky_time_);
    const double sun_elevation = sun_elevation_degrees(sky, camera_.lat, camera_.lon);
    const float surface = static_cast<float>(1.0 - smoothstep(80000.0, 2500000.0, camera_.height));
    float star_alpha = static_cast<float>(1.0 - smoothstep(-12.0, 5.0, sun_elevation));
    if (camera_.height > 1500000.0) star_alpha = 1.0f;
    if (surface > 0.05f) star_alpha *= surface;

    const float render_scale = options_.platform.screenshot ? 1.0f : settings_.render_scale;
    post_.begin(window_w, window_h, render_scale);
    sky_.render(camera_, sky, star_alpha, surface, sky_time_);
    earth_.render(camera_, sky, sky_time_, 1.0f / 60.0f);

    if (screen_ == ScreenSatellites && settings_.show_orbits) {
        const auto filtered = filtered_satellites();
        if (!filtered.empty() && selected_satellite()) {
            const int index = filtered[std::min<int>(satellite_focus_, filtered.size() - 1)];
            if (orbit_satellite_ != index || orbit_valid_until_ < platform::monotonic_seconds()) {
                orbit_points_ = hub_.orbit_path(index, sky_time_, 140);
                orbit_satellite_ = index;
                orbit_valid_until_ = platform::monotonic_seconds() + 20.0;
            }
            earth_.draw_orbit_path(camera_, orbit_points_, 0.32f, 0.95f, 0.88f, 0.55f);
        }
    }

    post_.end(settings_.bloom, 1.18f, 0.55f);
    ui_.begin(window_w, window_h);
    hits_.clear();
    label_rects_.clear();

    if (boot_active_) {
        draw_boot();
        ui_.end();
        return;
    }

    if (modal_ != ModalNone) {
        draw_toast();
        if (modal_ == ModalLocation) draw_location_modal();
        else if (modal_ == ModalDiscover) draw_discover_modal();
        else draw_help_modal();
    } else if (ambient_ || (!hud_visible_ && screen_ == ScreenGlobe)) {
        draw_ambient();
    } else {
        if (screen_ == ScreenFlights) {
            // A night-radar scrim over the crisp street map; it sits under the
            // HUD and keeps the cyan traffic legible at any hour.
            ui_.rect({0, 0, static_cast<float>(virtual_w), 1080}, gfx::hex(0x02060B, 0.52f));
        }
        if (screen_ != ScreenSettings) draw_top_bar();
        if (screen_ == ScreenGlobe) draw_globe_hud();
        else if (screen_ == ScreenFlights) draw_flights_hud();
        else if (screen_ == ScreenSatellites) draw_satellites_hud();
        else if (screen_ == ScreenSettings) draw_settings_hud();
        draw_toast();
        if (settings_.show_fps) {
            gfx::TextStyle fps;
            fps.font = ui_.font_ui();
            fps.size = 11;
            fps.color = gfx::hex(0x8FA6B8, 0.9f);
            ui_.text(fps, std::to_string(static_cast<int>(1.0 / std::max(0.001, 1.0 / 60.0))) + " fps",
                     virtual_w - 86, 96);
        }
    }
    ui_.end();

    if (options_.platform.screenshot) {
        const double elapsed = platform::monotonic_seconds() - screenshot_start_;
        const bool data_ready = hub_.status().weather_live || elapsed > options_.screenshot_wait + 8.0;
        const bool screen_ready = screen_ != ScreenFlights || hub_.status().flights_updated > 0.0 ||
                                  elapsed > options_.screenshot_wait + 7.0;
        const bool tiles_ready = camera_.height > 2500000.0 ||
                                earth_.tile_stats().loaded >= std::max(0, earth_.tile_stats().visible - 2) ||
                                elapsed > options_.screenshot_wait + 14.0;
        if (!has_captured_ && elapsed > options_.screenshot_wait && data_ready && tiles_ready && screen_ready) {
            std::vector<uint8_t> pixels(static_cast<size_t>(window_w) * window_h * 4);
            glReadPixels(0, 0, window_w, window_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (gfx::write_png(options_.platform.screenshot_path, window_w, window_h, pixels.data())) {
                std::fprintf(stderr, "[app] screenshot %s\n", options_.platform.screenshot_path.c_str());
            }
            has_captured_ = true;
            capture_requested_ = true;
            quit_ = true;
        }
    }
}

}  // namespace nimbus
