#pragma once

#include <string>

namespace nimbus {

struct Settings {
    int version = 1;
    std::string home_name = "New Delhi";
    std::string home_country = "India";
    double home_lat = 28.6139;
    double home_lon = 77.2090;
    int home_utc_offset = 19800;
    bool celsius = true;
    bool wind_kmh = true;
    bool distance_km = true;
    bool clock_24h = true;
    int map_style = 0;          // 0 satellite, 1 atlas, 2 streets
    int weather_layer = -1;     // 0 temperature, 1 humidity, 2 pressure, -1 none
    bool wind_layer = true;
    bool borders = true;
    bool clouds = true;
    bool bloom = true;
    bool auto_rotate = true;
    bool labels = true;
    bool show_orbits = true;
    int ambient_minutes = 3;
    int quality = 1;            // 0 performance, 1 balanced, 2 quality
    float render_scale = 1.0f;
    int data_saver = 0;
    bool first_run = true;
    bool live_time = true;
    int time_offset_minutes = 0;
    bool show_fps = false;
    bool boot_animation = true;

    bool load(const std::string &data_dir);
    bool save(const std::string &data_dir) const;
    std::string path(const std::string &data_dir) const;
};

}  // namespace nimbus
