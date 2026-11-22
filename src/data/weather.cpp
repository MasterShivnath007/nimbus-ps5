#include "data/weather.hpp"

#include <algorithm>
#include <cmath>

#include "gfx/math.hpp"

namespace nimbus {

void WeatherGrid::reset() {
    *this = WeatherGrid{};
}

void WeatherGrid::mark_chunk(int chunk) {
    if (chunk < 0 || chunk >= kChunks) return;
    if (!chunk_ready[chunk]) {
        chunk_ready[chunk] = true;
        ++chunks_ready;
    }
}

void WeatherGrid::finalize() {
    temp_min = 100;
    temp_max = -100;
    humidity_min = 100;
    humidity_max = 0;
    pressure_min = 1100;
    pressure_max = 800;
    bool any = false;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            if (!std::isfinite(temperature[row][col])) continue;
            any = true;
            temp_min = std::min(temp_min, temperature[row][col]);
            temp_max = std::max(temp_max, temperature[row][col]);
            humidity_min = std::min(humidity_min, humidity[row][col]);
            humidity_max = std::max(humidity_max, humidity[row][col]);
            pressure_min = std::min(pressure_min, pressure[row][col]);
            pressure_max = std::max(pressure_max, pressure[row][col]);
        }
    }
    if (!any) {
        temp_min = -20;
        temp_max = 35;
        humidity_min = 10;
        humidity_max = 100;
        pressure_min = 980;
        pressure_max = 1035;
    }
    valid = complete();
}

float WeatherGrid::sample(double lat, double lon,
                          const std::array<std::array<float, kCols>, kRows> &field) const {
    lat = clamp(lat, -89.9, 89.9);
    lon = wrap_longitude(lon);
    const double fy = (lat + 90.0) / 180.0 * kRows - 0.5;
    const double fx = (lon + 180.0) / 360.0 * kCols - 0.5;
    const int y0 = static_cast<int>(std::floor(fy));
    const int x0 = static_cast<int>(std::floor(fx));
    const float ty = static_cast<float>(fy - y0);
    const float tx = static_cast<float>(fx - x0);
    auto get = [&](int y, int x) {
        y = std::clamp(y, 0, kRows - 1);
        x = ((x % kCols) + kCols) % kCols;
        return field[y][x];
    };
    const float a = lerpf(get(y0, x0), get(y0, x0 + 1), tx);
    const float b = lerpf(get(y0 + 1, x0), get(y0 + 1, x0 + 1), tx);
    return lerpf(a, b, ty);
}

void WeatherGrid::build_texture(std::vector<uint8_t> &rgba, int width, int height) const {
    rgba.resize(static_cast<size_t>(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        const double lat = 90.0 - (y + 0.5) / height * 180.0;
        for (int x = 0; x < width; ++x) {
            const double lon = -180.0 + (x + 0.5) / width * 360.0;
            const float t = sample(lat, lon, temperature);
            const float h = sample(lat, lon, humidity);
            const float p = sample(lat, lon, pressure);
            const float c = sample(lat, lon, cloud);
            const size_t index = (static_cast<size_t>(y) * width + x) * 4;
            rgba[index + 0] = static_cast<uint8_t>(clamp((t - kTempLo) / (kTempHi - kTempLo), 0.0, 1.0) * 255);
            rgba[index + 1] = static_cast<uint8_t>(clamp((h - kHumidityLo) / (kHumidityHi - kHumidityLo), 0.0, 1.0) * 255);
            rgba[index + 2] = static_cast<uint8_t>(clamp((p - kPressureLo) / (kPressureHi - kPressureLo), 0.0, 1.0) * 255);
            rgba[index + 3] = static_cast<uint8_t>(clamp(c / 100.0, 0.0, 1.0) * 255);
        }
    }
}

bool WeatherGrid::wind_at(double lat, double lon, float &east, float &north) const {
    if (!valid) return false;
    east = sample(lat, lon, wind_u);
    north = sample(lat, lon, wind_v);
    return true;
}

std::string weather_condition(int code) {
    switch (code) {
        case 0: return "Clear sky";
        case 1: return "Mainly clear";
        case 2: return "Partly cloudy";
        case 3: return "Overcast";
        case 45: return "Fog";
        case 48: return "Rime fog";
        case 51: return "Light drizzle";
        case 53: return "Drizzle";
        case 55: return "Heavy drizzle";
        case 56: case 57: return "Freezing drizzle";
        case 61: return "Light rain";
        case 63: return "Rain";
        case 65: return "Heavy rain";
        case 66: case 67: return "Freezing rain";
        case 71: return "Light snow";
        case 73: return "Snow";
        case 75: return "Heavy snow";
        case 77: return "Snow grains";
        case 80: return "Rain showers";
        case 81: return "Rain showers";
        case 82: return "Violent showers";
        case 85: case 86: return "Snow showers";
        case 95: return "Thunderstorm";
        case 96: case 99: return "Thunderstorm with hail";
        default: return "—";
    }
}

std::string weather_icon(int code, bool is_day) {
    if (code == 0) return is_day ? "sun" : "moon";
    if (code <= 2) return is_day ? "cloud-sun" : "cloud-moon";
    if (code == 3) return "cloud";
    if (code == 45 || code == 48) return "cloud";
    if (code >= 51 && code <= 67) return "cloud-rain";
    if (code >= 71 && code <= 77) return "snowflake";
    if (code >= 80 && code <= 82) return "cloud-rain";
    if (code >= 85 && code <= 86) return "snowflake";
    if (code >= 95) return "cloud-lightning";
    return "cloud";
}

}  // namespace nimbus
