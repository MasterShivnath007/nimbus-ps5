#include "app/settings.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "data/json.hpp"

namespace nimbus {

std::string Settings::path(const std::string &data_dir) const {
    return data_dir + "/settings.json";
}

bool Settings::load(const std::string &data_dir) {
    std::ifstream file(path(data_dir));
    if (!file) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    Json json;
    std::string error;
    if (!parse_json(buffer.str(), json, error)) return false;
    home_name = json.get("home_name").string_or(home_name);
    home_country = json.get("home_country").string_or(home_country);
    home_lat = json.get("home_lat").as_number(home_lat);
    home_lon = json.get("home_lon").as_number(home_lon);
    home_utc_offset = json.get("home_utc_offset").as_int(home_utc_offset);
    celsius = json.get("celsius").as_bool(celsius);
    wind_kmh = json.get("wind_kmh").as_bool(wind_kmh);
    distance_km = json.get("distance_km").as_bool(distance_km);
    clock_24h = json.get("clock_24h").as_bool(clock_24h);
    map_style = json.get("map_style").as_int(map_style);
    weather_layer = json.get("weather_layer").as_int(weather_layer);
    wind_layer = json.get("wind_layer").as_bool(wind_layer);
    borders = json.get("borders").as_bool(borders);
    clouds = json.get("clouds").as_bool(clouds);
    bloom = json.get("bloom").as_bool(bloom);
    auto_rotate = json.get("auto_rotate").as_bool(auto_rotate);
    labels = json.get("labels").as_bool(labels);
    show_orbits = json.get("show_orbits").as_bool(show_orbits);
    ambient_minutes = json.get("ambient_minutes").as_int(ambient_minutes);
    quality = json.get("quality").as_int(quality);
    render_scale = static_cast<float>(json.get("render_scale").as_number(render_scale));
    data_saver = json.get("data_saver").as_int(data_saver);
    first_run = json.get("first_run").as_bool(true);
    live_time = json.get("live_time").as_bool(live_time);
    time_offset_minutes = json.get("time_offset_minutes").as_int(time_offset_minutes);
    show_fps = json.get("show_fps").as_bool(show_fps);
    boot_animation = json.get("boot_animation").as_bool(boot_animation);
    return true;
}

bool Settings::save(const std::string &data_dir) const {
    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": " << version << ",\n";
    out << "  \"home_name\": \"" << json_escape(home_name) << "\",\n";
    out << "  \"home_country\": \"" << json_escape(home_country) << "\",\n";
    out << "  \"home_lat\": " << home_lat << ",\n";
    out << "  \"home_lon\": " << home_lon << ",\n";
    out << "  \"home_utc_offset\": " << home_utc_offset << ",\n";
    out << "  \"celsius\": " << (celsius ? "true" : "false") << ",\n";
    out << "  \"wind_kmh\": " << (wind_kmh ? "true" : "false") << ",\n";
    out << "  \"distance_km\": " << (distance_km ? "true" : "false") << ",\n";
    out << "  \"clock_24h\": " << (clock_24h ? "true" : "false") << ",\n";
    out << "  \"map_style\": " << map_style << ",\n";
    out << "  \"weather_layer\": " << weather_layer << ",\n";
    out << "  \"wind_layer\": " << (wind_layer ? "true" : "false") << ",\n";
    out << "  \"borders\": " << (borders ? "true" : "false") << ",\n";
    out << "  \"clouds\": " << (clouds ? "true" : "false") << ",\n";
    out << "  \"bloom\": " << (bloom ? "true" : "false") << ",\n";
    out << "  \"auto_rotate\": " << (auto_rotate ? "true" : "false") << ",\n";
    out << "  \"labels\": " << (labels ? "true" : "false") << ",\n";
    out << "  \"show_orbits\": " << (show_orbits ? "true" : "false") << ",\n";
    out << "  \"ambient_minutes\": " << ambient_minutes << ",\n";
    out << "  \"quality\": " << quality << ",\n";
    out << "  \"render_scale\": " << render_scale << ",\n";
    out << "  \"data_saver\": " << data_saver << ",\n";
    out << "  \"first_run\": " << (first_run ? "true" : "false") << ",\n";
    out << "  \"live_time\": " << (live_time ? "true" : "false") << ",\n";
    out << "  \"time_offset_minutes\": " << time_offset_minutes << ",\n";
    out << "  \"show_fps\": " << (show_fps ? "true" : "false") << ",\n";
    out << "  \"boot_animation\": " << (boot_animation ? "true" : "false") << "\n";
    out << "}\n";
    const std::string final_path = path(data_dir);
    const std::string temporary = final_path + ".tmp";
    std::FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) return false;
    const std::string text = out.str();
    const size_t written = std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
    if (written != text.size()) {
        std::remove(temporary.c_str());
        return false;
    }
    std::remove(final_path.c_str());
    return std::rename(temporary.c_str(), final_path.c_str()) == 0;
}

}  // namespace nimbus
