#include "app/settings.hpp"

#include <cstdio>
#include <cstring>

#include "data/json.hpp"
#include "data/text.hpp"

namespace nimbus {

std::string Settings::path(const std::string &data_dir) const {
    return data_dir + "/settings.json";
}

bool Settings::load(const std::string &data_dir) {
    std::string text;
    if (!read_text_file(path(data_dir), text)) return false;
    Json json;
    std::string error;
    if (!parse_json(text, json, error)) return false;
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
    std::string out;
    out.reserve(1400);
    out += "{\n";
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "  \"version\": %d,\n", version);
    out += buffer;
    out += "  \"home_name\": \""; out += json_escape(home_name); out += "\",\n";
    out += "  \"home_country\": \""; out += json_escape(home_country); out += "\",\n";
    std::snprintf(buffer, sizeof(buffer), "  \"home_lat\": %.6f,\n", home_lat); out += buffer;
    std::snprintf(buffer, sizeof(buffer), "  \"home_lon\": %.6f,\n", home_lon); out += buffer;
    std::snprintf(buffer, sizeof(buffer), "  \"home_utc_offset\": %d,\n", home_utc_offset); out += buffer;
    out += "  \"celsius\": "; out += celsius ? "true" : "false"; out += ",\n";
    out += "  \"wind_kmh\": "; out += wind_kmh ? "true" : "false"; out += ",\n";
    out += "  \"distance_km\": "; out += distance_km ? "true" : "false"; out += ",\n";
    out += "  \"clock_24h\": "; out += clock_24h ? "true" : "false"; out += ",\n";
    std::snprintf(buffer, sizeof(buffer), "  \"map_style\": %d,\n", map_style); out += buffer;
    std::snprintf(buffer, sizeof(buffer), "  \"weather_layer\": %d,\n", weather_layer); out += buffer;
    out += "  \"wind_layer\": "; out += wind_layer ? "true" : "false"; out += ",\n";
    out += "  \"borders\": "; out += borders ? "true" : "false"; out += ",\n";
    out += "  \"clouds\": "; out += clouds ? "true" : "false"; out += ",\n";
    out += "  \"bloom\": "; out += bloom ? "true" : "false"; out += ",\n";
    out += "  \"auto_rotate\": "; out += auto_rotate ? "true" : "false"; out += ",\n";
    out += "  \"labels\": "; out += labels ? "true" : "false"; out += ",\n";
    out += "  \"show_orbits\": "; out += show_orbits ? "true" : "false"; out += ",\n";
    std::snprintf(buffer, sizeof(buffer), "  \"ambient_minutes\": %d,\n", ambient_minutes); out += buffer;
    std::snprintf(buffer, sizeof(buffer), "  \"quality\": %d,\n", quality); out += buffer;
    std::snprintf(buffer, sizeof(buffer), "  \"render_scale\": %.6f,\n", render_scale); out += buffer;
    std::snprintf(buffer, sizeof(buffer), "  \"data_saver\": %d,\n", data_saver); out += buffer;
    out += "  \"first_run\": "; out += first_run ? "true" : "false"; out += ",\n";
    out += "  \"live_time\": "; out += live_time ? "true" : "false"; out += ",\n";
    std::snprintf(buffer, sizeof(buffer), "  \"time_offset_minutes\": %d,\n", time_offset_minutes); out += buffer;
    out += "  \"show_fps\": "; out += show_fps ? "true" : "false"; out += ",\n";
    out += "  \"boot_animation\": "; out += boot_animation ? "true" : "false"; out += "\n";
    out += "}\n";
    const std::string final_path = path(data_dir);
    const std::string temporary = final_path + ".tmp";
    std::FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) return false;
    const size_t written = std::fwrite(out.data(), 1, out.size(), file);
    std::fclose(file);
    if (written != out.size()) {
        std::remove(temporary.c_str());
        return false;
    }
    std::remove(final_path.c_str());
    return std::rename(temporary.c_str(), final_path.c_str()) == 0;

}

}  // namespace nimbus
