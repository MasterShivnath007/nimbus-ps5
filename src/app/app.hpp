#pragma once

#include <functional>
#include <string>
#include <vector>

#include "app/camera.hpp"
#include "app/settings.hpp"
#include "data/hub.hpp"
#include "data/weather.hpp"
#include "gfx/earth.hpp"
#include "gfx/post.hpp"
#include "gfx/sky.hpp"
#include "gfx/ui.hpp"
#include "platform/platform.hpp"

namespace nimbus {

struct AppOptions {
    platform::Options platform;
    int screen = 0;
    std::string modal;
    double start_lat = 28.6139;
    double start_lon = 77.2090;
    double start_altitude_km = 14500.0;
    double start_tilt_degrees = 0.0;
    double start_heading_degrees = 0.0;
    std::string utc;
    std::string style;
    bool offline = false;
    bool demo = false;
    bool skip_boot = false;
    bool start_override = false;
    int layer_override = -2;
    int wind_override = -1;
    double screenshot_wait = 2.0;
};

class App {
public:
    bool init(const AppOptions &options);
    void update(const platform::Input &input, double dt);
    void render();
    void shutdown();
    bool wants_quit() const { return quit_; }
    bool capture_requested() const { return capture_requested_; }
    const std::string &capture_path() const { return options_.platform.screenshot_path; }

private:
    enum Screen { ScreenGlobe = 0, ScreenFlights = 1, ScreenSatellites = 2, ScreenSettings = 3 };
    enum Modal { ModalNone = 0, ModalLocation, ModalDiscover, ModalHelp, ModalCredits };
    enum SettingId {
        SetHome = 0, SetStyle, SetTemp, SetWindUnit, SetDistance, SetClock,
        SetLayerDefault, SetWindOverlay, SetBorders, SetClouds, SetLabels,
        SetAutoRotate, SetAmbient, SetBloom, SetQuality, SetFps, SetDataSaver,
        SetRefresh, SetHelp, SetCredits, SetCount
    };

    struct Hit {
        gfx::Rect rect;
        std::function<void()> action;
    };

    void process_input(const platform::Input &input, double dt);
    void handle_global(const platform::Input &input);
    void handle_globe(const platform::Input &input, double dt);
    void handle_flights(const platform::Input &input, double dt);
    void handle_satellites(const platform::Input &input, double dt);
    void handle_settings(const platform::Input &input);
    void handle_location(const platform::Input &input);
    void handle_discover(const platform::Input &input);
    void handle_help(const platform::Input &input);
    void process_mouse(const platform::Input &input);
    void add_hit(const gfx::Rect &rect, std::function<void()> action);

    void switch_screen(int screen);
    void reset_view(bool home = true);
    void fly_to(double lat, double lon, double altitude_km, double tilt_degrees, double duration);
    void focus_city(const City &city, bool inspect);
    void select_home(const City &city);
    void refresh_location_results();
    void on_keyboard_character(char32_t codepoint);
    void save_settings();
    void toast(const std::string &message, double seconds = 3.2);
    void update_ambient(double dt);

    const City *nearest_city_to_center(double max_degrees, double *distance = nullptr) const;
    const City *nearest_city_to_screen(float x, float y, double max_pixels) const;
    std::string temperature(double celsius_value, bool with_unit = true) const;
    std::string wind_speed(double meters_per_second) const;
    std::string distance_string(double km) const;
    std::string pressure_string(double hpa) const;
    std::string time_string(double unix_time, bool with_utc_offset) const;
    std::string number(double value, int decimals = 0) const;
    std::string ellipsize(int font, float size, const std::string &text, float width) const;
    std::string latlon_string(double lat, double lon) const;
    std::string layer_name(int layer) const;
    std::string map_style_name(int style) const;
    std::string setting_value(SettingId id) const;
    std::string current_clock() const;
    void cycle_setting(SettingId id, int direction);
    void activate_setting(SettingId id);
    void apply_settings();
    void request_current_location_weather();

    void draw_top_bar();
    void draw_globe_hud();
    void draw_flights_hud();
    void draw_satellites_hud();
    void draw_settings_hud();
    void draw_boot();
    void draw_location_modal();
    void draw_discover_modal();
    void draw_help_modal();
    void draw_toast();
    void draw_ambient();
    void draw_weather_card(const gfx::Rect &rect);
    void draw_layer_rail();
    void draw_city_labels();
    void draw_center_reticle();
    void draw_cyclones();
    void draw_satellite_markers();
    void draw_controller_prompts();
    void draw_map_style_pill();
    void draw_satellite_detail(const gfx::Rect &rect);
    void draw_attribution();
    void draw_brand(float x, float y, float scale);
    void draw_prompt(float x, float y, const char *glyph, const std::string &label);
    void draw_screen_title(const std::string &eyebrow, const std::string &title,
                           const std::string &subtitle, float x, float y);
    gfx::Color category_color(const std::string &category) const;
    std::string active_satellite_category() const;
    std::vector<int> filtered_satellites() const;
    const Satellite *selected_satellite() const;

    AppOptions options_;
    Settings settings_;
    DataHub hub_;
    Camera camera_;
    gfx::SkyRenderer sky_;
    gfx::EarthRenderer earth_;
    gfx::PostProcessor post_;
    gfx::Ui ui_;

    Screen screen_ = ScreenGlobe;
    Screen settings_return_ = ScreenGlobe;
    Modal modal_ = ModalNone;
    bool quit_ = false;
    bool capture_requested_ = false;
    bool initialized_ = false;
    bool online_ = true;
    bool ambient_ = false;
    bool hud_visible_ = true;
    bool settings_dirty_ = false;
    bool location_for_home_ = true;
    bool flight_world_ = false;
    int settings_focus_ = 0;
    int flight_focus_ = 0;
    int flight_scroll_ = 0;
    int satellite_focus_ = 0;
    int satellite_scroll_ = 0;
    int satellite_filter_ = 1;
    int storm_focus_ = 0;
    int discover_focus_ = 0;
    int location_focus_ = 0;
    int keyboard_x_ = 0;
    int keyboard_y_ = 0;
    bool location_keyboard_ = false;
    bool dragging_ = false;
    bool mouse_drag_started_ = false;
    float drag_x_ = 0;
    float drag_y_ = 0;
    float screen_fade_ = 1.0f;
    double boot_elapsed_ = 0.0;
    double idle_seconds_ = 0.0;
    double last_save_ = 0.0;
    double toast_until_ = 0.0;
    double screenshot_start_ = 0.0;
    double last_frame_ = 0.0;
    double sky_time_ = 0.0;
    double wall_time_ = 0.0;
    double scrub_offset_minutes_ = 0.0;
    bool scrubbing_ = false;
    std::string toast_text_;
    std::string location_query_;
    std::vector<const City *> location_results_;
    std::vector<Vec3> orbit_points_;
    double orbit_valid_until_ = 0.0;
    int orbit_satellite_ = -1;
    std::vector<Hit> hits_;
    std::vector<gfx::Rect> label_rects_;
    bool boot_active_ = true;
    bool has_captured_ = false;
    float frame_phase_ = 0.0f;
    int window_w_ = 1920;
    int window_h_ = 1080;
    unsigned random_seed_ = 0x51A7B00Bu;
};

}  // namespace nimbus
