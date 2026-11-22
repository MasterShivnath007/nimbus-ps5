#include <cstdio>
#include <cstdlib>
#include <string>

#include "app/astro.hpp"
#include "app/camera.hpp"
#include "app/timeutil.hpp"
#include "data/json.hpp"
#include "data/weather.hpp"

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL: %s (%s:%d)\n", #condition, __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    const nimbus::SkyState sky = nimbus::compute_sky(1764000000.0);
    CHECK(sky.sun_ecef.length() > 0.99f && sky.sun_ecef.length() < 1.01f);
    CHECK(sky.subsolar_lat > -24.0 && sky.subsolar_lat < 24.0);
    CHECK(nimbus::great_circle_degrees(0, 0, 0, 90) > 89.9);
    CHECK(nimbus::great_circle_degrees(0, 0, 90, 0) > 89.9);
    const nimbus::Vec3 a = nimbus::latlon_to_unit(0, 0);
    const nimbus::Vec3 b = nimbus::latlon_to_unit(0, 90);
    const nimbus::Vec3 mid = nimbus::slerp_unit(a, b, 0.5);
    double lat = 0, lon = 0;
    nimbus::unit_to_latlon(mid, lat, lon);
    CHECK(lon > 44.9 && lon < 45.1);

    CHECK(nimbus::unix_from_utc(2026, 11, 22, 16, 0, 0) == 1795363200LL);
    const nimbus::UtcTime utc = nimbus::utc_from_unix(1795363200LL);
    CHECK(utc.year == 2026 && utc.month == 11 && utc.day == 22 && utc.hour == 16);
    CHECK(nimbus::unix_from_utc(2024, 2, 29, 0, 0, 0) == 1709164800LL);
    CHECK(nimbus::unix_from_utc(1970, 1, 1, 0, 0, 0) == 0);

    nimbus::Json json;
    std::string error;
    CHECK(nimbus::parse_json(R"({"a":[1,2,3],"b":"x\n\u00e9","c":true})", json, error));
    CHECK(json.get("a").size() == 3);
    CHECK(json.get("a")[1].as_int() == 2);
    CHECK(json.get("b").as_string() == "x\n\xc3\xa9");
    CHECK(json.get("c").as_bool());
    CHECK(!nimbus::parse_json("{bad", json, error));

    nimbus::WeatherGrid grid;
    grid.reset();
    CHECK(!grid.valid);
    for (int row = 0; row < nimbus::WeatherGrid::kRows; ++row) {
        for (int col = 0; col < nimbus::WeatherGrid::kCols; ++col) {
            grid.temperature[row][col] = 10.0f + row;
            grid.humidity[row][col] = 50.0f;
            grid.pressure[row][col] = 1010.0f;
            grid.cloud[row][col] = 20.0f;
            grid.wind_u[row][col] = 4.0f;
            grid.wind_v[row][col] = -2.0f;
        }
    }
    for (int i = 0; i < nimbus::WeatherGrid::kChunks; ++i) grid.mark_chunk(i);
    grid.finalize();
    CHECK(grid.valid);
    float east = 0, north = 0;
    CHECK(grid.wind_at(0, 0, east, north));
    CHECK(east > 3.5f && east < 4.5f);
    CHECK(north < -1.5f && north > -2.5f);
    std::vector<uint8_t> pixels;
    grid.build_texture(pixels, 72, 36);
    CHECK(pixels.size() == 72u * 36u * 4u);

    std::printf("nimbus tests passed\n");
    return 0;
}
