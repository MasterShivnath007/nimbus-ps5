#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nimbus {

struct WeatherGrid {
    static constexpr int kCols = 24;
    static constexpr int kRows = 12;
    static constexpr int kChunks = 3;

    std::array<std::array<float, kCols>, kRows> temperature{};
    std::array<std::array<float, kCols>, kRows> humidity{};
    std::array<std::array<float, kCols>, kRows> pressure{};
    std::array<std::array<float, kCols>, kRows> cloud{};
    std::array<std::array<float, kCols>, kRows> wind_u{};
    std::array<std::array<float, kCols>, kRows> wind_v{};
    std::array<std::array<float, kCols>, kRows> precipitation{};
    std::array<bool, kChunks> chunk_ready{};
    int chunks_ready = 0;
    bool valid = false;
    double updated_unix = 0;
    float temp_min = 40, temp_max = -40;
    float humidity_min = 100, humidity_max = 0;
    float pressure_min = 1100, pressure_max = 800;

    static constexpr float kTempLo = -60.0f;
    static constexpr float kTempHi = 60.0f;
    static constexpr float kHumidityLo = 0.0f;
    static constexpr float kHumidityHi = 100.0f;
    static constexpr float kPressureLo = 880.0f;
    static constexpr float kPressureHi = 1060.0f;

    void reset();
    void mark_chunk(int chunk);
    bool complete() const { return chunks_ready >= kChunks; }
    void finalize();
    void build_texture(std::vector<uint8_t> &rgba, int width = 720, int height = 360) const;
    bool wind_at(double lat, double lon, float &east, float &north) const;
    float sample(double lat, double lon, const std::array<std::array<float, kCols>, kRows> &field) const;
};

struct WeatherPoint {
    bool valid = false;
    std::string name;
    double latitude = 0;
    double longitude = 0;
    double updated_unix = 0;
    double temperature = 0;
    double apparent = 0;
    double humidity = 0;
    double pressure = 0;
    double wind_speed = 0;
    double wind_direction = 0;
    double wind_gust = 0;
    double precipitation = 0;
    double cloud_cover = 0;
    int weather_code = 0;
    int is_day = 1;
    int utc_offset_seconds = 0;
    std::string timezone;
    double sunrise_unix = 0;
    double sunset_unix = 0;
    bool has_sunrise = false;
    bool has_sunset = false;
    std::array<double, 12> hourly_temp{};
    std::array<int, 12> hourly_code{};
    std::array<double, 12> hourly_precip{};
    std::array<double, 12> hourly_wind{};
    int hourly_count = 0;
};

std::string weather_condition(int code);
std::string weather_icon(int code, bool is_day);

}  // namespace nimbus
