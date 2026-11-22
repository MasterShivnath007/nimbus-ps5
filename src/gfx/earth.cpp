#include "gfx/earth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "platform/platform.hpp"
#include "stb_image.h"

namespace nimbus::gfx {

namespace {


std::string layer_name(int layer) {
    switch (layer) {
        case 0: return "s2cloudless-2025_3857";
        case 1: return "terrain-light_3857";
        case 2: return "osm_3857";
        case 3: return "overlay_3857";
        case 4: return "esri-street";
        case 5: return "esri-imagery";
        case 6: return "esri-topo";
        default: return "esri-labels";
    }
}

const char *layer_ext(int layer) {
    return layer == 2 || layer == 3 || layer == 7 ? "png" : "jpg";
}

int base_layer(MapStyle style, int z) {
    if (style == MapStyle::Satellite) return z <= 14 ? 0 : 5;
    if (style == MapStyle::Atlas) return z <= 14 ? 1 : 6;
    return z <= 14 ? 2 : 4;
}

int overlay_layer(MapStyle style, int z) {
    if (style == MapStyle::Streets) return -1;
    if (style == MapStyle::Atlas && z >= 15) return -1;  // Esri topo has labels
    return z <= 14 ? 3 : 7;
}

double mercator_y(double lat_degrees) {
    const double lat = clamp(lat_degrees, -85.05112878, 85.05112878) * kDeg;
    return std::log(std::tan(kPi * 0.25 + lat * 0.5));
}

double inverse_mercator(double y) {
    return std::atan(std::sinh(y)) / kDeg;
}

struct TileBounds {
    double lon0 = 0, lon1 = 0, merc0 = 0, merc1 = 0, lat0 = 0, lat1 = 0;
};

TileBounds tile_bounds(int z, int x, int y) {
    const int n = 1 << z;
    TileBounds b;
    b.lon0 = static_cast<double>(x) / n * 360.0 - 180.0;
    b.lon1 = static_cast<double>(x + 1) / n * 360.0 - 180.0;
    b.merc0 = kPi * (1.0 - 2.0 * y / n);
    b.merc1 = kPi * (1.0 - 2.0 * (y + 1) / n);
    b.lat0 = inverse_mercator(b.merc0);
    b.lat1 = inverse_mercator(b.merc1);
    return b;
}

struct GroundBox {
    double lat_n = 85;
    double lat_s = -85;
    double lon_w = -180;
    double lon_e = 180;
};

GroundBox screen_ground_box(const Camera &camera) {
    GroundBox box;
    const Vec3 focus = latlon_to_unit(camera.lat, camera.lon);
    const double horizon = std::acos(clamp(static_cast<double>(kEarthRadius) /
                                                std::max(static_cast<double>(camera.eye.length()),
                                                         kEarthRadius),
                                            -1.0, 1.0));
    const double ox = camera.eye.x, oy = camera.eye.y, oz = camera.eye.z;
    double min_lat = 90, max_lat = -90, min_lon = 180, max_lon = -180;
    bool any = false;
    const float margins[] = {-1.1f, 0.0f, 1.1f};
    for (float ny : margins) {
        for (float nx : margins) {
            const Vec3 d = camera.ray_direction(nx, ny);
            const double b = ox * d.x + oy * d.y + oz * d.z;
            const double c = ox * ox + oy * oy + oz * oz - kEarthRadius * kEarthRadius;
            const double disc = b * b - c;
            Vec3 hit;
            if (disc >= 0.0) {
                double t = -b - std::sqrt(std::max(0.0, disc));
                if (t <= 0) t = -b + std::sqrt(std::max(0.0, disc));
                if (t <= 0) continue;
                hit = Vec3(ox + d.x * t, oy + d.y * t, oz + d.z * t);
            } else {
                Vec3 tangent = d - focus * d.dot(focus);
                if (tangent.length() < 1e-5f) continue;
                hit = slerp_unit(focus, tangent.normalized(), horizon) *
                      static_cast<float>(kEarthRadius);
            }
            double lat = 0, lon = 0;
            unit_to_latlon(hit, lat, lon);
            const double rel = wrap_longitude(lon - camera.lon);
            min_lat = std::min(min_lat, lat);
            max_lat = std::max(max_lat, lat);
            min_lon = std::min(min_lon, rel);
            max_lon = std::max(max_lon, rel);
            any = true;
        }
    }
    if (!any) return box;
    const double pad_lat = std::max(0.005, (max_lat - min_lat) * 0.18);
    const double pad_lon = std::max(0.005, (max_lon - min_lon) * 0.18);
    box.lat_n = std::min(85.0, max_lat + pad_lat);
    box.lat_s = std::max(-85.0, min_lat - pad_lat);
    box.lon_w = camera.lon + min_lon - pad_lon;
    box.lon_e = camera.lon + max_lon + pad_lon;
    if (box.lon_e - box.lon_w > 350.0) {
        box.lon_w = -180.0;
        box.lon_e = 180.0;
    }
    return box;
}

Texture2D load_r8(const std::string &path) {
    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load(path.c_str(), &width, &height, &channels, 1);
    Texture2D texture;
    if (!pixels) {
        std::fprintf(stderr, "[texture] cannot load %s\n", path.c_str());
        return texture;
    }
    texture.create_r8(width, height, pixels, true);
    stbi_image_free(pixels);
    return texture;
}

}  // namespace

uint64_t EarthRenderer::tile_key(int layer, int z, int x, int y) {
    return (static_cast<uint64_t>(layer & 0xF) << 60) |
           (static_cast<uint64_t>(z & 0x3F) << 54) |
           (static_cast<uint64_t>(x & 0x3FFFFFF) << 27) |
           static_cast<uint64_t>(y & 0x3FFFFFF);
}

std::string EarthRenderer::tile_url(int layer, int z, int x, int y) const {
    if (layer >= 4) {
        const char *service = layer == 4 ? "World_Street_Map"
                              : layer == 5 ? "World_Imagery"
                              : layer == 6 ? "World_Topo_Map"
                                           : "Reference/World_Boundaries_and_Places";
        return "https://server.arcgisonline.com/ArcGIS/rest/services/" + std::string(service) +
               "/MapServer/tile/" + std::to_string(z) + "/" + std::to_string(y) + "/" +
               std::to_string(x);
    }
    return "https://tiles.maps.eox.at/wmts/1.0.0/" + layer_name(layer) +
           "/default/g/" + std::to_string(z) + "/" + std::to_string(y) + "/" +
           std::to_string(x) + "." + layer_ext(layer);
}

std::string EarthRenderer::tile_cache(int layer, int z, int x, int y) const {
    return layer_name(layer) + "/" + std::to_string(z) + "/" + std::to_string(x) + "_" +
           std::to_string(y) + "." + layer_ext(layer);
}

bool EarthRenderer::init(const std::string &asset_dir, const std::string &data_dir) {
    asset_dir_ = asset_dir;
    data_dir_ = data_dir;
    start_time_ = platform::monotonic_seconds();

    if (!earth_shader_.build_from_files("earth", asset_dir + "/shaders/earth.vert",
                                        asset_dir + "/shaders/earth.frag"))
        return false;
    if (!tile_shader_.build_from_files("tile", asset_dir + "/shaders/tile.vert",
                                       asset_dir + "/shaders/tile.frag"))
        return false;
    if (!weather_shader_.build_from_files("weather", asset_dir + "/shaders/earth.vert",
                                          asset_dir + "/shaders/weather.frag"))
        return false;
    if (!atmosphere_shader_.build_from_files("atmosphere", asset_dir + "/shaders/earth.vert",
                                             asset_dir + "/shaders/atmosphere.frag"))
        return false;
    if (!line_shader_.build_from_files("line", asset_dir + "/shaders/line.vert",
                                       asset_dir + "/shaders/line.frag"))
        return false;

    sphere_ = make_sphere(1.0f, 256, 128);
    tile_grid_ = make_grid(4);
    quad_ = make_quad();
    day_ = load_texture(asset_dir + "/earth_day.jpg", true, true);
    night_ = load_texture(asset_dir + "/earth_night.jpg", true, true);
    clouds_ = load_r8(asset_dir + "/earth_clouds.jpg");
    ocean_ = load_r8(asset_dir + "/earth_ocean.png");
    if (!day_.id || !night_.id || !clouds_.id || !ocean_.id) {
        std::fprintf(stderr, "[earth] missing core texture\n");
        return false;
    }
    for (Texture2D *texture : {&day_, &night_}) {
        glBindTexture(GL_TEXTURE_2D, texture->id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    const uint8_t transparent[4] = {0, 0, 0, 0};
    empty_overlay_.create_rgba(1, 1, transparent, false);

    std::vector<float> border_vertices;
    build_line_layer(asset_dir + "/borders.bin", 1.0f, 0.30f, 0.80f, 0.82f, 0.24f,
                     border_vertices, border_vertices_);
    if (!border_vertices.empty()) {
        border_mesh_.upload(border_vertices.data(), border_vertices.size() * sizeof(float),
                            border_vertices_ / 7, GL_LINES, {3, 4});
    }
    std::vector<float> coast_vertices;
    build_line_layer(asset_dir + "/coastlines.bin", 1.0f, 0.36f, 0.74f, 1.0f, 0.22f,
                     coast_vertices, coastline_vertices_);
    if (!coast_vertices.empty()) {
        coastline_mesh_.upload(coast_vertices.data(), coast_vertices.size() * sizeof(float),
                               coastline_vertices_ / 7, GL_LINES, {3, 4});
    }

    // A small, deterministic initial particle field.  The globe is the star of
    // the show, so particles are deliberately sparse and fine.
    std::mt19937 rng(0x51a7b00bu);
    std::uniform_real_distribution<float> random(0.0f, 1.0f);
    particles_.resize(7000);
    for (auto &p : particles_) {
        p.lat = std::asin(2.0f * random(rng) - 1.0f) / static_cast<float>(kDeg);
        p.lon = random(rng) * 360.0f - 180.0f;
        p.prev_lat = p.lat;
        p.prev_lon = p.lon;
        p.age = random(rng) * 4.0f;
        p.life = 1.4f + random(rng) * 3.6f;
    }
    ready_ = true;
    return true;
}

void EarthRenderer::release() { ready_ = false; }

void EarthRenderer::build_line_layer(const std::string &path, float radius_ratio, float r, float g,
                                     float b, float a, std::vector<float> &out, int &count) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return;
    char magic[4];
    uint32_t version = 0, polylines = 0;
    if (std::fread(magic, 1, 4, file) != 4 || std::memcmp(magic, "NIML", 4) != 0 ||
        std::fread(&version, 4, 1, file) != 1 || std::fread(&polylines, 4, 1, file) != 1) {
        std::fclose(file);
        return;
    }
    out.reserve(static_cast<size_t>(polylines) * 6 * 7);
    count = 0;
    for (uint32_t i = 0; i < polylines; ++i) {
        uint32_t points = 0;
        if (std::fread(&points, 4, 1, file) != 1) break;
        Vec3 previous;
        bool have_previous = false;
        for (uint32_t j = 0; j < points; ++j) {
            float lon = 0, lat = 0;
            if (std::fread(&lon, 4, 1, file) != 1 || std::fread(&lat, 4, 1, file) != 1) break;
            const Vec3 point = latlon_to_unit(lat, lon) * radius_ratio;
            if (have_previous) {
                const float values[] = {previous.x, previous.y, previous.z, r, g, b, a,
                                        point.x, point.y, point.z, r, g, b, a};
                out.insert(out.end(), values, values + 14);
                count += 2;
            }
            previous = point;
            have_previous = true;
        }
    }
    std::fclose(file);
    (void)version;
}

void EarthRenderer::update_weather(const WeatherGrid &grid) {
    weather_ = grid;
    weather_valid_ = grid.valid;
    if (!weather_valid_) return;
    std::vector<uint8_t> pixels;
    grid.build_texture(pixels, 720, 360);
    if (weather_tex_.width == 0) {
        weather_tex_.create_rgba(720, 360, pixels.data(), false);
    } else {
        weather_tex_.update_rgba(720, 360, pixels.data());
    }
    glBindTexture(GL_TEXTURE_2D, weather_tex_.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void EarthRenderer::update_wind(const WeatherGrid &grid) {
    wind_u_ = grid.wind_u;
    wind_v_ = grid.wind_v;
    wind_valid_ = grid.valid;
}

void EarthRenderer::draw_base(const Camera &camera, const SkyState &sky, double now) {
    earth_shader_.use();
    earth_shader_.set_mat4("uViewProj", camera.view_proj.m);
    const float base_radius = static_cast<float>(
        kEarthRadius - (camera.height < 250000.0 ? std::max(80.0, camera.height * 0.06) : 0.0));
    earth_shader_.set_float("uRadius", base_radius);
    earth_shader_.set_int("uDayTex", 0);
    earth_shader_.set_int("uNightTex", 1);
    earth_shader_.set_int("uOceanTex", 2);
    earth_shader_.set_int("uCloudTex", 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, day_.id);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, night_.id);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, ocean_.id);
    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, clouds_.id);
    earth_shader_.set_vec3("uSunDir", sky.sun_ecef.x, sky.sun_ecef.y, sky.sun_ecef.z);
    earth_shader_.set_vec3("uCamPos", camera.eye.x, camera.eye.y, camera.eye.z);
    earth_shader_.set_float("uTime", static_cast<float>(std::fmod(now, 100000.0)));
    earth_shader_.set_float("uClouds",
                           clouds_visible_ ? (weather_layer_ >= 0 ? 0.30f : 0.72f) : 0.0f);
    earth_shader_.set_float("uNightBoost", 1.3f);
    earth_shader_.set_float("uOceanGloss", 0.48f);
    earth_shader_.set_float("uCityGlow", 1.0f);
    earth_shader_.set_float("uBaseFlat",
                           static_cast<float>(1.0 - smoothstep(250000.0, 2500000.0, camera.height)));
    const bool atlas = style_ == MapStyle::Atlas;
    const bool streets = style_ == MapStyle::Streets;
    earth_shader_.set_vec3("uUnderlay", streets ? 0.48f : atlas ? 0.36f : 0.035f,
                          streets ? 0.46f : atlas ? 0.34f : 0.050f,
                          streets ? 0.42f : atlas ? 0.30f : 0.062f);
    sphere_.draw();
}


std::vector<TileRequest> EarthRenderer::prepare_tiles(const Camera &camera, bool online,
                                                      int max_new, double now) {
    std::vector<TileRequest> requests;
    visible_nodes_.clear();
    visible_set_.clear();
    stats_ = TileStats{};
    if (camera.height > 2.5e6) return requests;

    const GroundBox box = screen_ground_box(camera);
    const double mpp = std::max(0.05, camera.meters_per_pixel());
    const double cos_lat = std::max(0.08, std::cos(camera.lat * kDeg));
    int z = static_cast<int>(std::lround(std::log2(156543.03392 * cos_lat / mpp)));
    z = std::clamp(z, 3, 18);
    int x0 = 0, x1 = -1, y0 = 0, y1 = -1;
    for (;;) {
        const int n = 1 << z;
        const int a0 = static_cast<int>(std::floor((box.lon_w + 180.0) / 360.0 * n)) - 1;
        const int a1 = static_cast<int>(std::floor((box.lon_e + 180.0) / 360.0 * n)) + 1;
        const int b0 = static_cast<int>(std::floor((1.0 - mercator_y(box.lat_n) / kPi) * 0.5 * n)) - 1;
        const int b1 = static_cast<int>(std::floor((1.0 - mercator_y(box.lat_s) / kPi) * 0.5 * n)) + 1;
        int width = a1 - a0 + 1;
        int height = b1 - b0 + 1;
        if (width > n) width = n;
        if (height > n) height = n;
        if (width * height <= 260 || z <= 3) {
            x0 = a0;
            x1 = a0 + width - 1;
            y0 = std::clamp(b0, 0, n - 1);
            y1 = std::clamp(b0 + height - 1, 0, n - 1);
            break;
        }
        --z;
    }
    const int n = 1 << z;
    stats_.zoom = z;
    stats_.visible = 0;
    int loaded = 0;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const int wrapped = ((x % n) + n) % n;
            const int layer = base_layer(style_, z);
            TileNode node;
            node.layer = layer;
            node.z = z;
            node.x = wrapped;
            node.y = y;
            node.key = tile_key(layer, z, wrapped, y);
            node.priority = z * 1000.0f;
            visible_nodes_.push_back(node);
            visible_set_.insert(node.key);
            ++stats_.visible;
            if (tile_loaded(node.key)) ++loaded;
        }
    }
    std::sort(visible_nodes_.begin(), visible_nodes_.end(),
              [&](const TileNode &a, const TileNode &b) {
                  const double ax = (static_cast<double>(a.x) + 0.5) / (1 << a.z) * 360.0 - 180.0;
                  const double ay = inverse_mercator(kPi * (1.0 - 2.0 * (a.y + 0.5) / (1 << a.z)));
                  const double bx = (static_cast<double>(b.x) + 0.5) / (1 << b.z) * 360.0 - 180.0;
                  const double by = inverse_mercator(kPi * (1.0 - 2.0 * (b.y + 0.5) / (1 << b.z)));
                  return great_circle_degrees(camera.lat, camera.lon, ay, ax) <
                         great_circle_degrees(camera.lat, camera.lon, by, bx);
              });
    int budget = max_new;
    for (const TileNode &node : visible_nodes_) {
        if (budget <= 0 || !online) break;
        if (tile_loaded(node.key)) continue;
        const auto failed = failed_.find(node.key);
        if (failed != failed_.end() && failed->second > now) continue;
        if (pending_.find(node.key) != pending_.end()) continue;
        pending_[node.key] = {now, false};
        TileRequest request;
        request.token = node.key;
        request.url = tile_url(node.layer, node.z, node.x, node.y);
        request.cache_key = tile_cache(node.layer, node.z, node.x, node.y);
        requests.push_back(std::move(request));
        --budget;
    }
    stats_.loaded = loaded;
    stats_.pending = static_cast<int>(pending_.size());
    // Overlay requests are only meaningful once the base tile exists.
    if (style_ != MapStyle::Streets) {
        int overlay_budget = std::min(2, max_new);
        for (const TileNode &node : visible_nodes_) {
            if (overlay_budget <= 0) break;
            if (node.z < 10 || node.z > 14) continue;
            if (!tile_loaded(node.key)) continue;
            const int layer = overlay_layer(style_, node.z);
            if (layer < 0) continue;
            const uint64_t key = tile_key(layer, node.z, node.x, node.y);
            if (tile_loaded(key) || pending_.find(key) != pending_.end()) continue;
            const auto failed = failed_.find(key);
            if (failed != failed_.end() && failed->second > now) continue;
            pending_[key] = {now, true};
            TileRequest request;
            request.token = key;
            request.url = tile_url(layer, node.z, node.x, node.y);
            request.cache_key = tile_cache(layer, node.z, node.x, node.y);
            requests.push_back(std::move(request));
            --overlay_budget;
        }
    }
    return requests;
}

void EarthRenderer::deliver_tile(uint64_t token, const uint8_t *rgba, int width, int height) {
    const auto pending = pending_.find(token);
    if (pending != pending_.end()) pending_.erase(pending);
    if (!rgba || width <= 0 || height <= 0) {
        failed_[token] = platform::monotonic_seconds() + 45.0;
        return;
    }
    TileSlot slot;
    slot.texture.create_rgba(width, height, rgba, true);
    slot.last_used = platform::monotonic_seconds();
    tiles_[token] = std::move(slot);
}

void EarthRenderer::fail_tile(uint64_t token) {
    pending_.erase(token);
    failed_[token] = platform::monotonic_seconds() + 35.0;
}

bool EarthRenderer::tile_loaded(uint64_t key) const { return tiles_.find(key) != tiles_.end(); }

void EarthRenderer::trim_cache(double now) {
    if (tiles_.size() < 260) return;
    for (auto it = tiles_.begin(); it != tiles_.end();) {
        if (visible_set_.find(it->first) == visible_set_.end() &&
            now - it->second.last_used > 60.0) {
            it = tiles_.erase(it);
        } else {
            ++it;
        }
        if (tiles_.size() < 200) break;
    }
    for (auto it = failed_.begin(); it != failed_.end();) {
        if (it->second < now - 30.0) it = failed_.erase(it);
        else ++it;
    }
}

void EarthRenderer::draw_tiles(const Camera &camera, const SkyState &sky, double now) {
    if (visible_nodes_.empty()) return;
    const double offset = clamp(camera.height * 0.00045, 0.8, 2500.0);
    // Tiles form a continuous ground layer and paint over the coarse globe
    // underlay; no depth test means no seams between zoom levels.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    tile_shader_.use();
    tile_shader_.set_mat4("uViewProj", camera.view_proj.m);
    tile_shader_.set_int("uTile", 0);
    tile_shader_.set_int("uOverlay", 1);
    tile_shader_.set_int("uNightTex", 2);
    tile_shader_.set_vec3("uSunDir", sky.sun_ecef.x, sky.sun_ecef.y, sky.sun_ecef.z);
    tile_shader_.set_vec3("uCamPos", camera.eye.x, camera.eye.y, camera.eye.z);
    tile_shader_.set_float("uOverlayMix", 0.92f);
    tile_shader_.set_float("uOverlayMode", 0.0f);
    tile_shader_.set_float("uSaturation", style_ == MapStyle::Satellite ? 1.08f : 1.0f);
    tile_shader_.set_float("uBrightness", style_ == MapStyle::Satellite ? 1.08f : 1.0f);
    tile_shader_.set_float("uNightStrength", style_ == MapStyle::Satellite ? 0.36f : 0.40f);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, night_.id);

    auto draw_one = [&](uint64_t key, bool overlay) {
        const auto found = tiles_.find(key);
        if (found == tiles_.end()) return;
        const int layer = static_cast<int>((key >> 60) & 0xF);
        const int z = static_cast<int>((key >> 54) & 0x3F);
        const int x = static_cast<int>((key >> 27) & 0x3FFFFFF);
        const int y = static_cast<int>(key & 0x3FFFFFF);
        const TileBounds b = tile_bounds(z, x, y);
        const Vec3 center = latlon_to_unit((b.lat0 + b.lat1) * 0.5,
                                           (b.lon0 + b.lon1) * 0.5);
        const float ndl = center.dot(sky.sun_ecef);
        const float night = 1.0f - smoothstep(-0.16, 0.22, ndl);
        tile_shader_.set_float("uRadius",
                               static_cast<float>(kEarthRadius + offset + (overlay ? 1.2 : 0.0)));
        tile_shader_.set_float("uLon0", static_cast<float>(b.lon0));
        tile_shader_.set_float("uLon1", static_cast<float>(b.lon1));
        tile_shader_.set_float("uMerc0", static_cast<float>(b.merc0));
        tile_shader_.set_float("uMerc1", static_cast<float>(b.merc1));
        tile_shader_.set_float("uNightMix", night);
        tile_shader_.set_float("uNightLights",
                                  style_ == MapStyle::Satellite
                                      ? (z >= 9 ? 0.22f : z >= 7 ? 0.08f : 0.0f)
                                      : 0.0f);
        tile_shader_.set_float("uOverlayMode", overlay ? 1.0f : 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, found->second.texture.id);
        if (overlay) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        tile_grid_.draw();
        if (overlay) glDisable(GL_BLEND);
        (void)layer;
    };

    for (const TileNode &node : visible_nodes_) draw_one(node.key, false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const TileNode &node : visible_nodes_) {
        if (node.z < 10) continue;
        const int layer = overlay_layer(style_, node.z);
        if (layer < 0) continue;
        draw_one(tile_key(layer, node.z, node.x, node.y), true);
    }
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    (void)now;
}

void EarthRenderer::draw_weather(const Camera &camera, const SkyState &sky) {
    if (!weather_valid_ || weather_layer_ < 0 || weather_layer_ > 2) return;
    const double offset = clamp(camera.height * 0.0008, 1.5, 5000.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    weather_shader_.use();
    weather_shader_.set_mat4("uViewProj", camera.view_proj.m);
    weather_shader_.set_float("uRadius", static_cast<float>(kEarthRadius + offset));
    weather_shader_.set_int("uData", 0);
    weather_shader_.set_int("uLayer", weather_layer_);
    weather_shader_.set_float("uTempMin", weather_.temp_min);
    weather_shader_.set_float("uTempMax", weather_.temp_max);
    weather_shader_.set_float("uHumidityMin", weather_.humidity_min);
    weather_shader_.set_float("uHumidityMax", weather_.humidity_max);
    weather_shader_.set_float("uPressureMin", weather_.pressure_min);
    weather_shader_.set_float("uPressureMax", weather_.pressure_max);
    float layer_opacity = weather_opacity_;
    if (weather_layer_ == 1) layer_opacity = std::min(0.28f, weather_opacity_ * 1.7f);
    else if (weather_layer_ == 2) layer_opacity = std::min(0.22f, weather_opacity_ * 1.3f);
    weather_shader_.set_float("uOpacity", layer_opacity);
    weather_shader_.set_float("uContourStep", camera.height > 2000000.0 ? 8.0f : 4.0f);
    weather_shader_.set_float("uContourAlpha", camera.height > 2000000.0 ? 0.10f : 0.36f);
    weather_shader_.set_vec3("uSunDir", sky.sun_ecef.x, sky.sun_ecef.y, sky.sun_ecef.z);
    weather_shader_.set_float("uTime", static_cast<float>(sky.unix_time));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, weather_tex_.id);
    sphere_.draw();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void EarthRenderer::draw_atmosphere(const Camera &camera, const SkyState &sky) {
    if (camera.height < 280000.0) return;
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_CULL_FACE);
    atmosphere_shader_.use();
    atmosphere_shader_.set_mat4("uViewProj", camera.view_proj.m);
    atmosphere_shader_.set_float("uRadius", static_cast<float>(kEarthRadius + 95000.0));
    atmosphere_shader_.set_vec3("uSunDir", sky.sun_ecef.x, sky.sun_ecef.y, sky.sun_ecef.z);
    atmosphere_shader_.set_vec3("uCamPos", camera.eye.x, camera.eye.y, camera.eye.z);
    atmosphere_shader_.set_float("uNightBoost", 1.0f);
    sphere_.draw();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void EarthRenderer::draw_lines(const Camera &camera, double now) {
    if ((!borders_visible_ && !coastlines_visible_) || !line_mesh_ready()) return;
    double line_alpha = 1.0;
    if (!camera.orbit_mode) {
        line_alpha = smoothstep(22000.0, 350000.0, camera.height);
    }
    if (line_alpha <= 0.01) return;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    line_shader_.use();
    line_shader_.set_mat4("uViewProj", camera.view_proj.m);
    line_shader_.set_float("uRadius", static_cast<float>(kEarthRadius * 1.0018));
    line_shader_.set_float("uAlpha", static_cast<float>(line_alpha));
    if (coastlines_visible_ && coastline_mesh_.vertex_count > 0) coastline_mesh_.draw();
    if (borders_visible_ && border_mesh_.vertex_count > 0) border_mesh_.draw();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    (void)now;
}

bool EarthRenderer::line_mesh_ready() const {
    return border_mesh_.vertex_count > 0 || coastline_mesh_.vertex_count > 0;
}

void EarthRenderer::update_particles(float dt) {
    if (!wind_valid_ || !wind_visible_ || dt <= 0) return;
    dt = std::min(dt, 0.1f);
    for (auto &p : particles_) {
        float east = 0, north = 0;
        float east_n = 0, north_n = 0;
        const bool ok = weather_.wind_at(p.lat, p.lon, east, north);
        p.prev_lat = p.lat;
        p.prev_lon = p.lon;
        if (!ok) {
            p.age = p.life;
        } else {
            // Exaggerate the advection so the circulation is readable at globe
            // scale; the direction and relative speed are from the real field.
            const float time_scale = 700.0f;
            const double cos_lat = std::max(0.08, std::cos(p.lat * kDeg));
            p.lat += north * time_scale * dt / static_cast<float>(kEarthRadius) / static_cast<float>(kDeg);
            p.lon += east * time_scale * dt /
                     static_cast<float>(kEarthRadius * cos_lat) / static_cast<float>(kDeg);
            p.age += dt;
            if (p.lon > 180) p.lon -= 360;
            if (p.lon < -180) p.lon += 360;
            if (p.lat > 89) p.lat = 89;
            if (p.lat < -89) p.lat = -89;
        }
        if (p.age > p.life || std::fabs(p.lat) > 88.5f || !std::isfinite(p.lat)) {
            static uint32_t seed = 0x9e3779b9u;
            seed = seed * 1664525u + 1013904223u;
            p.lat = static_cast<float>(std::asin(2.0 * ((seed & 0xFFFFFF) / 16777216.0) - 1.0) / kDeg);
            seed = seed * 1664525u + 1013904223u;
            p.lon = static_cast<float>(((seed & 0xFFFFFF) / 16777216.0) * 360.0 - 180.0);
            p.prev_lat = p.lat;
            p.prev_lon = p.lon;
            p.age = 0;
            p.life = 1.4f + ((seed >> 8) & 255) / 255.0f * 3.8f;
            east_n = north_n = 0;
        }
        (void)east_n;
        (void)north_n;
    }
}

void EarthRenderer::draw_wind(const Camera &camera, float dt) {
    if (!wind_visible_ || !wind_valid_) return;
    update_particles(dt);
    const float offset = static_cast<float>(clamp(camera.height * 0.0011, 4.0, 9000.0));
    const float radius = static_cast<float>(kEarthRadius) + offset;
    const float tail_seconds = static_cast<float>(clamp(camera.height / 700000.0, 1.0, 12.0));
    const float altitude_fade =
        0.42f + 0.58f * (1.0f - static_cast<float>(smoothstep(2.0e6, 10.0e6, camera.height)));
    wind_lines_.begin();
    for (const auto &p : particles_) {
        float u = 0, v = 0;
        weather_.wind_at(p.lat, p.lon, u, v);
        const float speed = std::sqrt(u * u + v * v);
        const float t = clamp(speed / 30.0f, 0.0f, 1.0f);
        double tail_lat = p.lat - v * 700.0f * tail_seconds / static_cast<float>(kEarthRadius) /
                                       static_cast<float>(kDeg);
        double tail_lon = p.lon - u * 700.0f * tail_seconds /
                                       static_cast<float>(kEarthRadius * std::cos(p.lat * kDeg)) /
                                       static_cast<float>(kDeg);
        if (tail_lat > 89) tail_lat = 89;
        if (tail_lat < -89) tail_lat = -89;
        if (tail_lon > 180) tail_lon -= 360;
        if (tail_lon < -180) tail_lon += 360;
        const Vec3 a = latlon_to_unit(tail_lat, tail_lon) * radius;
        const Vec3 b = latlon_to_unit(p.lat, p.lon) * radius;
        const float alpha = (0.12f + 0.40f * t) * altitude_fade;
        const float ca[] = {a.x, a.y, a.z, 0.55f + 0.38f * t, 0.95f, 0.86f, alpha * 0.15f};
        const float cb[] = {b.x, b.y, b.z, 0.65f + 0.30f * t, 1.0f, 0.90f, alpha};
        wind_lines_.add_colored(ca, ca + 3, cb, cb + 3);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    line_shader_.use();
    line_shader_.set_mat4("uViewProj", camera.view_proj.m);
    line_shader_.set_float("uRadius", 1.0f);
    line_shader_.set_float("uAlpha", 0.9f);
    wind_lines_.flush();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void EarthRenderer::render(const Camera &camera, const SkyState &sky, double now, float frame_dt,
                           bool draw_earth) {
    if (!ready_ || !draw_earth) return;
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    draw_base(camera, sky, now);
    if (camera.height < 2.5e6) draw_tiles(camera, sky, now);
    glDisable(GL_CULL_FACE);
    draw_weather(camera, sky);
    draw_atmosphere(camera, sky);
    draw_lines(camera, now);
    draw_wind(camera, frame_dt);
}

void EarthRenderer::draw_orbit_path(const Camera &camera, const std::vector<Vec3> &points, float r,
                                    float g, float b, float a) {
    if (points.size() < 2 || !line_shader_.valid()) return;
    LineBatch batch;
    batch.begin();
    for (size_t i = 1; i < points.size(); ++i) {
        const Vec3 &p0 = points[i - 1];
        const Vec3 &p1 = points[i];
        const float c0[] = {p0.x, p0.y, p0.z, r, g, b, a * 0.18f};
        const float c1[] = {p1.x, p1.y, p1.z, r, g, b, a};
        batch.add_colored(c0, c0 + 3, c1, c1 + 3);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    line_shader_.use();
    line_shader_.set_mat4("uViewProj", camera.view_proj.m);
    line_shader_.set_float("uRadius", 1.0f);
    line_shader_.set_float("uAlpha", 1.0f);
    glBindVertexArray(0);
    batch.flush();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

}  // namespace nimbus::gfx
