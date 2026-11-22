#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app/astro.hpp"
#include "app/camera.hpp"
#include "data/weather.hpp"
#include "gfx/gl.hpp"
#include "gfx/mesh.hpp"
#include "gfx/shader.hpp"
#include "gfx/texture.hpp"

namespace nimbus::gfx {

enum class MapStyle : int { Satellite = 0, Atlas = 1, Streets = 2 };

struct TileRequest {
    uint64_t token = 0;
    std::string url;
    std::string cache_key;
};

struct TileStats {
    int loaded = 0;
    int pending = 0;
    int visible = 0;
    int zoom = 0;
};

class EarthRenderer {
public:
    bool init(const std::string &asset_dir, const std::string &data_dir);
    void release();

    void set_style(MapStyle style) { style_ = style; }
    MapStyle style() const { return style_; }
    void set_weather_layer(int layer) { weather_layer_ = layer; }
    int weather_layer() const { return weather_layer_; }
    void set_weather_opacity(float opacity) { weather_opacity_ = opacity; }
    void set_wind_visible(bool visible) { wind_visible_ = visible; }
    void set_borders_visible(bool visible) { borders_visible_ = visible; }
    void set_coastlines_visible(bool visible) { coastlines_visible_ = visible; }
    void set_clouds_visible(bool visible) { clouds_visible_ = visible; }

    void update_weather(const WeatherGrid &grid);
    bool has_weather() const { return weather_valid_; }
    void update_wind(const WeatherGrid &grid);

    // Tile streaming.  The app fetches these URLs and hands the decoded pixels
    // back (or calls fail_tile).  Requests are already deduplicated.
    std::vector<TileRequest> prepare_tiles(const Camera &camera, bool online, int max_new,
                                            double now);
    void deliver_tile(uint64_t token, const uint8_t *rgba, int width, int height);
    void fail_tile(uint64_t token);
    TileStats tile_stats() const { return stats_; }
    void trim_cache(double now);

    void render(const Camera &camera, const SkyState &sky, double now, float frame_dt,
                bool draw_earth = true);
    void draw_orbit_path(const Camera &camera, const std::vector<Vec3> &points, float r, float g,
                         float b, float a);

private:
    bool line_mesh_ready() const;
    struct TileSlot {
        Texture2D texture;
        double last_used = 0;
    };
    struct TileNode {
        uint64_t key = 0;
        int layer = 0;
        int z = 0;
        int x = 0;
        int y = 0;
        float priority = 0;
    };
    struct Pending {
        double started = 0;
        bool overlay = false;
    };

    void draw_base(const Camera &camera, const SkyState &sky, double now);
    void draw_tiles(const Camera &camera, const SkyState &sky, double now);
    void draw_weather(const Camera &camera, const SkyState &sky);
    void draw_atmosphere(const Camera &camera, const SkyState &sky);
    void draw_lines(const Camera &camera, double now);
    void draw_wind(const Camera &camera, float dt);
    void update_particles(float dt);
    void build_line_layer(const std::string &path, float radius_ratio, float r, float g, float b,
                          float a, std::vector<float> &out, int &count);
    std::string tile_url(int layer, int z, int x, int y) const;
    std::string tile_cache(int layer, int z, int x, int y) const;
    static uint64_t tile_key(int layer, int z, int x, int y);
    bool tile_loaded(uint64_t key) const;

    Shader earth_shader_;
    Shader tile_shader_;
    Shader weather_shader_;
    Shader atmosphere_shader_;
    Shader line_shader_;

    Mesh sphere_;
    Mesh tile_grid_;
    Mesh quad_;
    Texture2D day_;
    Texture2D night_;
    Texture2D clouds_;
    Texture2D ocean_;
    Texture2D weather_tex_;
    Texture2D empty_overlay_;

    std::unordered_map<uint64_t, TileSlot> tiles_;
    std::unordered_map<uint64_t, Pending> pending_;
    std::unordered_map<uint64_t, double> failed_;
    std::vector<TileNode> visible_nodes_;
    std::unordered_set<uint64_t> visible_set_;
    TileStats stats_{};

    Mesh border_mesh_;
    int border_vertices_ = 0;
    Mesh coastline_mesh_;
    int coastline_vertices_ = 0;

    struct Particle {
        float lat;
        float lon;
        float prev_lat;
        float prev_lon;
        float age;
        float life;
    };
    std::vector<Particle> particles_;
    LineBatch wind_lines_;
    std::array<std::array<float, WeatherGrid::kCols>, WeatherGrid::kRows> wind_u_{};
    std::array<std::array<float, WeatherGrid::kCols>, WeatherGrid::kRows> wind_v_{};
    bool wind_valid_ = false;
    WeatherGrid weather_{};
    bool weather_valid_ = false;
    int weather_layer_ = 0;
    float weather_opacity_ = 0.58f;
    bool wind_visible_ = true;
    bool borders_visible_ = true;
    bool coastlines_visible_ = true;
    bool clouds_visible_ = true;
    MapStyle style_ = MapStyle::Satellite;
    std::string data_dir_;
    std::string asset_dir_;
    double start_time_ = 0.0;
    bool ready_ = false;
};

}  // namespace nimbus::gfx
