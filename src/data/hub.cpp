#include "data/hub.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "SGP4.h"
#include "app/timeutil.hpp"
#include "data/json.hpp"
#include "gfx/math.hpp"
#include "platform/platform.hpp"

namespace nimbus {

namespace {

std::string trim(const std::string &value) {
    size_t start = 0, end = value.size();
    while (start < end && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' ||
                           value[start] == '\n'))
        ++start;
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\r' || value[end - 1] == '\n'))
        --end;
    return value.substr(start, end - start);
}

double number_or(const Json &object, const char *key, double fallback) {
    const Json *value = object.find(key);
    if (!value) return fallback;
    if (value->is_number()) return value->as_number();
    if (value->is_string()) {
        const std::string &text = value->as_string();
        if (text.empty()) return fallback;
        return std::strtod(text.c_str(), nullptr);
    }
    return fallback;
}

std::string string_or(const Json &object, const char *key, const std::string &fallback = "") {
    const Json *value = object.find(key);
    return value && value->is_string() ? value->as_string() : fallback;
}

double parse_iso_local(const std::string &text, int utc_offset_seconds) {
    if (text.size() < 16) return 0;
    std::tm tm{};
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min) < 5)
        return 0;
    return static_cast<double>(unix_from_utc(tm.tm_year, static_cast<unsigned>(tm.tm_mon),
                                             static_cast<unsigned>(tm.tm_mday),
                                             static_cast<unsigned>(tm.tm_hour),
                                             static_cast<unsigned>(tm.tm_min), 0)) -
           utc_offset_seconds;
}

std::string format_epoch(double unix_time) {
    const UtcTime utc = utc_from_unix(static_cast<int64_t>(unix_time));
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u", utc.year, utc.month, utc.day);
    return buffer;
}

std::string classify_cyclone(double wind_kt, const std::string &fallback) {
    if (wind_kt >= 137) return "Category 5";
    if (wind_kt >= 113) return "Category 4";
    if (wind_kt >= 96) return "Category 3";
    if (wind_kt >= 83) return "Category 2";
    if (wind_kt >= 64) return "Category 1";
    if (wind_kt >= 34) return "Tropical storm";
    if (wind_kt > 0) return "Tropical depression";
    return fallback;
}

}  // namespace

bool DataHub::init(const std::string &asset_dir, const std::string &data_dir,
                   const std::string &ca_path, Settings *settings) {
    asset_dir_ = asset_dir;
    data_dir_ = data_dir;
    settings_ = settings;
    if (!places_.load(asset_dir)) return false;
    if (!net_.init(data_dir, ca_path, 3)) return false;
    load_satellites(asset_dir);
    grid_.reset();
    return true;
}

void DataHub::shutdown() { net_.shutdown(); }

void DataHub::set_home(double lat, double lon) {
    if (!settings_) return;
    settings_->home_lat = lat;
    settings_->home_lon = lon;
    request_point(lat, lon, settings_->home_name);
}

double DataHub::weather_interval(double base) const {
    if (settings_ && settings_->data_saver) base *= 2.0;
    return base;
}

void DataHub::load_satellites(const std::string &asset_dir) {
    satellites_.clear();
    std::ifstream tle_file(asset_dir + "/satellites.tle");
    if (!tle_file) {
        std::fprintf(stderr, "[hub] no satellites.tle\n");
        return;
    }
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(tle_file, line)) {
        line = trim(line);
        if (!line.empty()) lines.push_back(line);
    }
    std::unordered_map<int, std::pair<std::string, std::string>> satcat;
    std::ifstream cat_file(asset_dir + "/satcat.tsv");
    if (cat_file) {
        std::string header;
        std::getline(cat_file, header);
        while (std::getline(cat_file, line)) {
            std::stringstream stream(line);
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(stream, field, '\t')) fields.push_back(field);
            if (fields.size() < 11) continue;
            const int norad = std::atoi(fields[0].c_str());
            satcat[norad] = {fields[2], fields[3]};  // owner, launch
        }
    }

    for (size_t i = 0; i + 2 < lines.size(); i += 3) {
        const std::string &name = lines[i];
        const std::string &line1 = lines[i + 1];
        const std::string &line2 = lines[i + 2];
        if (line1.size() < 20 || line1[0] != '1' || line2.size() < 20 || line2[0] != '2') continue;
        Satellite satellite;
        satellite.name = name;
        satellite.norad = std::atoi(line1.substr(2, 5).c_str());
        char l1[80] = {0};
        char l2[80] = {0};
        std::strncpy(l1, line1.c_str(), sizeof(l1) - 1);
        std::strncpy(l2, line2.c_str(), sizeof(l2) - 1);
        parseLines(&satellite.tle, l1, l2);
        satellite.has_tle = satellite.tle.sgp4Error == 0;
        const auto cat = satcat.find(satellite.norad);
        if (cat != satcat.end()) {
            satellite.owner_code = cat->second.first;
            satellite.launch_date = cat->second.second;
        }
        satellite.country = owner_country(satellite.owner_code);
        satellite.category = satellite_category(satellite.name, satellite.owner_code);
        satellite.inclination = satellite.tle.incDeg;
        satellite.period_minutes = satellite.tle.n > 0 ? 1440.0 / satellite.tle.n : 0;
        satellite.tle_epoch_unix = (satellite.tle.rec.jdsatepoch - 2440587.5) * 86400.0;
        if (satellite.tle_epoch_unix > tle_epoch_) tle_epoch_ = satellite.tle_epoch_unix;
        satellites_.push_back(std::move(satellite));
    }
    double earliest = 1e18;
    for (const auto &sat : satellites_) {
        if (sat.tle_epoch_unix > 0) earliest = std::min(earliest, sat.tle_epoch_unix);
    }
    satcat_loaded_ = !satcat.empty();
    selected_satellite_index_ = 0;
    selected_satellite_ = satellites_.empty() ? nullptr : &satellites_[0];
    std::fprintf(stderr, "[hub] %zu satellites (TLE epoch %s)\n", satellites_.size(),
                 tle_epoch_ > 0 ? format_epoch(tle_epoch_).c_str() : "unknown");
    (void)earliest;
}

std::string DataHub::owner_country(const std::string &code) const {
    if (code == "US") return "United States";
    if (code == "CIS" || code == "RU") return "Russia";
    if (code == "PRC" || code == "CN") return "China";
    if (code == "ESA") return "Europe (ESA)";
    if (code == "FR") return "France";
    if (code == "GER" || code == "DE") return "Germany";
    if (code == "IT") return "Italy";
    if (code == "JPN" || code == "JP") return "Japan";
    if (code == "IND" || code == "IN") return "India";
    if (code == "CAN" || code == "CA") return "Canada";
    if (code == "AUS" || code == "AU") return "Australia";
    if (code == "UK" || code == "GB") return "United Kingdom";
    if (code == "SKOR" || code == "KR") return "South Korea";
    if (code == "ISRA" || code == "IL") return "Israel";
    if (code == "BRAZ" || code == "BR") return "Brazil";
    if (code == "MEX" || code == "MX") return "Mexico";
    if (code == "SPAIN" || code == "ES") return "Spain";
    if (code == "NETH" || code == "NL") return "Netherlands";
    if (code == "SWED" || code == "SE") return "Sweden";
    if (code == "NOR" || code == "NO") return "Norway";
    if (code == "DEN" || code == "DK") return "Denmark";
    if (code == "FIN" || code == "FI") return "Finland";
    if (code == "POL" || code == "PL") return "Poland";
    if (code == "TUR" || code == "TR") return "Türkiye";
    if (code == "SAUD" || code == "SA") return "Saudi Arabia";
    if (code == "UAE") return "United Arab Emirates";
    if (code == "EGYPT" || code == "EG") return "Egypt";
    if (code == "PAKI" || code == "PK") return "Pakistan";
    if (code == "BANG" || code == "BD") return "Bangladesh";
    if (code == "THAI" || code == "TH") return "Thailand";
    if (code == "MAL" || code == "MY") return "Malaysia";
    if (code == "INDO" || code == "ID") return "Indonesia";
    if (code == "PHIL" || code == "PH") return "Philippines";
    if (code == "VIET" || code == "VN") return "Vietnam";
    if (code == "TAIWAN" || code == "TW") return "Taiwan";
    if (code == "SING" || code == "SG") return "Singapore";
    if (code == "NZ") return "New Zealand";
    if (code == "ARGN" || code == "AR") return "Argentina";
    if (code == "CHLE" || code == "CL") return "Chile";
    if (code == "COL" || code == "CO") return "Colombia";
    if (code == "SAFR" || code == "ZA") return "South Africa";
    return code.empty() ? "Unknown" : code;
}

std::string DataHub::satellite_category(const std::string &name, const std::string &owner) const {
    const std::string upper = lower_ascii(name);
    if (upper.find("iss") != std::string::npos || upper.find("zarya") != std::string::npos ||
        upper.find("tianhe") != std::string::npos || upper.find("css") != std::string::npos ||
        upper.find("tiangong") != std::string::npos || upper.find("station") != std::string::npos)
        return "Station";
    if (upper.find("noaa") != std::string::npos || upper.find("metop") != std::string::npos ||
        upper.find("goes") != std::string::npos || upper.find("himawari") != std::string::npos ||
        upper.find("fengyun") != std::string::npos || upper.find("fy-") != std::string::npos ||
        upper.find("meteor") != std::string::npos || upper.find("dmsp") != std::string::npos ||
        upper.find("suomi") != std::string::npos || upper.find("jpss") != std::string::npos ||
        upper.find("elektro") != std::string::npos || upper.find("insat") != std::string::npos)
        return "Weather";
    if (upper.find("gps") != std::string::npos || upper.find("navstar") != std::string::npos ||
        upper.find("glonass") != std::string::npos || upper.find("galileo") != std::string::npos ||
        upper.find("beidou") != std::string::npos || upper.find("irnss") != std::string::npos ||
        upper.find("qzss") != std::string::npos || upper.find("gsat") != std::string::npos)
        return "Navigation";
    if (upper.find("hubble") != std::string::npos || upper.find("hst") != std::string::npos ||
        upper.find("swift") != std::string::npos || upper.find("fermi") != std::string::npos ||
        upper.find("chandra") != std::string::npos || upper.find("xmm") != std::string::npos ||
        upper.find("gaia") != std::string::npos || upper.find("tess") != std::string::npos ||
        upper.find("cheops") != std::string::npos || upper.find("JWST") != std::string::npos)
        return "Science";
    if (owner == "ESA") return "Science";
    return "Other";
}

void DataHub::compute_satellite(Satellite &satellite, double now, double home_lat, double home_lon,
                                const SkyState &sky) {
    if (!satellite.has_tle) return;
    double r[3] = {0, 0, 0};
    double v[3] = {0, 0, 0};
    const long millis = static_cast<long>(now * 1000.0);
    getRVForDate(&satellite.tle, millis, r, v);
    if (satellite.tle.sgp4Error != 0) {
        satellite.has_tle = false;
        return;
    }
    const double g = sky.gmst;
    const double c = std::cos(g), s = std::sin(g);
    const double ex = c * r[0] + s * r[1];
    const double ey = -s * r[0] + c * r[1];
    const double ez = r[2];
    const double radius_km = std::sqrt(ex * ex + ey * ey + ez * ez);
    satellite.ecef = Vec3(static_cast<float>(ex * 1000.0), static_cast<float>(ey * 1000.0),
                          static_cast<float>(ez * 1000.0));
    satellite.lat = std::asin(clamp(ez / std::max(1e-6, radius_km), -1.0, 1.0)) / kDeg;
    satellite.lon = std::atan2(ey, ex) / kDeg;
    satellite.altitude_km = radius_km - kEarthRadius / 1000.0;
    satellite.velocity_km_s = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (satellite.altitude_km < -100.0 || satellite.altitude_km > 60000.0 ||
        satellite.velocity_km_s < 0.4 || satellite.velocity_km_s > 15.0) {
        satellite.has_tle = false;
        return;
    }
    satellite.orbit_class = satellite.altitude_km < 2000 ? "LEO"
                            : satellite.altitude_km > 30000 ? "GEO / high"
                                                             : "MEO";
    // Cylindrical Earth-shadow model for the terminator state.
    const double sun_dot = r[0] * sky.sun_eci.x + r[1] * sky.sun_eci.y + r[2] * sky.sun_eci.z;
    const double px = r[0] - sky.sun_eci.x * sun_dot;
    const double py = r[1] - sky.sun_eci.y * sun_dot;
    const double pz = r[2] - sky.sun_eci.z * sun_dot;
    satellite.sunlit = sun_dot > 0 || std::sqrt(px * px + py * py + pz * pz) > kEarthRadius / 1000.0;

    const Vec3 up = latlon_to_unit(home_lat, home_lon);
    const Vec3 home = up * static_cast<float>(kEarthRadius);
    const Vec3 delta = satellite.ecef - home;
    const float range = std::max(1.0f, delta.length());
    satellite.range_home_km = range / 1000.0;
    satellite.elevation_home =
        std::asin(clamp(static_cast<double>(delta.dot(up) / range), -1.0, 1.0)) / kDeg;
    Vec3 east = Vec3(0, 0, 1).cross(up).normalized();
    if (east.length() < 0.1f) east = Vec3(0, 1, 0);
    const Vec3 north = up.cross(east).normalized();
    satellite.azimuth_home = std::atan2(delta.dot(east), delta.dot(north)) / kDeg;
    satellite.overhead = satellite.elevation_home > 10.0;
    satellite.above_city.clear();
    satellite.above_country.clear();
    double distance = 0;
    const City *city = places_.nearest(satellite.lat, satellite.lon, 4.0, &distance);
    if (city && distance < 4.5) {
        satellite.above_city = city->name;
        satellite.above_country = city->country;
    }
}

void DataHub::compute_satellites(double now) {
    if (satellites_.empty()) return;
    // Orbital propagation needs UTC, not the frame timer.  Nimbus' timers use
    // monotonic time; only the SGP4 math needs wall-clock Unix seconds.
    const double unix_now = platform::wall_seconds();
    const SkyState sky = compute_sky(unix_now);
    const double home_lat = settings_ ? settings_->home_lat : 0.0;
    const double home_lon = settings_ ? settings_->home_lon : 0.0;
    for (auto &satellite : satellites_) compute_satellite(satellite, unix_now, home_lat, home_lon, sky);
    (void)now;
    std::sort(satellites_.begin(), satellites_.end(), [](const Satellite &a, const Satellite &b) {
        if (a.overhead != b.overhead) return a.overhead;
        if (a.elevation_home != b.elevation_home) return a.elevation_home > b.elevation_home;
        return a.name < b.name;
    });
    if (selected_satellite_index_ >= static_cast<int>(satellites_.size()))
        selected_satellite_index_ = 0;
    selected_satellite_ = satellites_.empty() ? nullptr : &satellites_[selected_satellite_index_];
}

void DataHub::select_satellite(int index) {
    if (satellites_.empty()) return;
    const int count = static_cast<int>(satellites_.size());
    selected_satellite_index_ = ((index % count) + count) % count;
    selected_satellite_ = &satellites_[selected_satellite_index_];
    orbit_cache_.clear();
    orbit_cache_sat_ = -1;
}

void DataHub::refresh_satellites(double now) { compute_satellites(now); }

std::vector<Vec3> DataHub::orbit_path(int satellite_index, double now, int samples) {
    if (satellite_index < 0 || satellite_index >= static_cast<int>(satellites_.size())) return {};
    if (orbit_cache_sat_ == satellite_index && std::fabs(orbit_cache_time_ - now) < 20.0 &&
        !orbit_cache_.empty())
        return orbit_cache_;
    const Satellite &sat = satellites_[satellite_index];
    if (!sat.has_tle) return {};
    TLE tle = sat.tle;
    std::vector<Vec3> points;
    points.reserve(static_cast<size_t>(samples));
    const double start = now - 45.0 * 60.0;
    const double step = 90.0 * 60.0 / std::max(2, samples - 1);
    for (int i = 0; i < samples; ++i) {
        const double t = start + step * i;
        double r[3] = {0, 0, 0}, v[3] = {0, 0, 0};
        getRVForDate(&tle, static_cast<long>(t * 1000.0), r, v);
        if (tle.sgp4Error != 0) continue;
        const SkyState sky = compute_sky(t);
        const double c = std::cos(sky.gmst), s = std::sin(sky.gmst);
        const double ex = c * r[0] + s * r[1];
        const double ey = -s * r[0] + c * r[1];
        const double ez = r[2];
        const double len = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (len < 1) continue;
        points.push_back(Vec3(static_cast<float>(ex / len * (kEarthRadius + std::max(0.0, (len - kEarthRadius / 1000.0) * 1000.0))),
                              static_cast<float>(ey / len * (kEarthRadius + std::max(0.0, (len - kEarthRadius / 1000.0) * 1000.0))),
                              static_cast<float>(ez / len * (kEarthRadius + std::max(0.0, (len - kEarthRadius / 1000.0) * 1000.0)))));
    }
    orbit_cache_ = points;
    orbit_cache_sat_ = satellite_index;
    orbit_cache_time_ = now;
    return points;
}

bool DataHub::selected_pass(double now, double &rise_unix, double &peak_unix, double &set_unix,
                            double &peak_elevation) const {
    if (!selected_satellite_ || !selected_satellite_->has_tle) return false;
    TLE tle = selected_satellite_->tle;
    const double home_lat = settings_ ? settings_->home_lat : 0.0;
    const double home_lon = settings_ ? settings_->home_lon : 0.0;
    const Vec3 up = latlon_to_unit(home_lat, home_lon);
    const Vec3 home = up * static_cast<float>(kEarthRadius);

    auto elevation_at = [&](double t) {
        double r[3] = {0, 0, 0}, v[3] = {0, 0, 0};
        getRVForDate(&tle, static_cast<long>(t * 1000.0), r, v);
        if (tle.sgp4Error != 0) return -90.0;
        const SkyState sky = compute_sky(t);
        const double c = std::cos(sky.gmst), s = std::sin(sky.gmst);
        const double ex = c * r[0] + s * r[1];
        const double ey = -s * r[0] + c * r[1];
        const Vec3 sat(static_cast<float>(ex * 1000.0), static_cast<float>(ey * 1000.0),
                       static_cast<float>(r[2] * 1000.0));
        const Vec3 delta = sat - home;
        return std::asin(clamp(static_cast<double>(delta.normalized().dot(up)), -1.0, 1.0)) / kDeg;
    };

    constexpr double step = 30.0;
    constexpr int steps = 24 * 120;
    double previous = elevation_at(now);
    bool rising = false;
    double rise = 0, best_time = 0, best = -90;
    int index = 0;
    if (previous >= 10.0) {
        // Skip the pass that is already in progress; "next pass" means the next
        // rise, not the current footprint.
        while (index < steps && elevation_at(now + index * step) >= 10.0) ++index;
        previous = elevation_at(now + index * step);
    }
    for (; index <= steps; ++index) {
        const double t = now + index * step;
        const double elevation = elevation_at(t);
        if (!rising) {
            if (previous < 10.0 && elevation >= 10.0) {
                rising = true;
                rise = t;
                best = elevation;
                best_time = t;
            }
        } else {
            if (elevation > best) {
                best = elevation;
                best_time = t;
            }
            if (elevation < 10.0) {
                rise_unix = rise;
                peak_unix = best_time;
                set_unix = t;
                peak_elevation = best;
                return best_time >= rise && t > best_time;
            }
        }
        previous = elevation;
    }
    return false;
}

std::string DataHub::tle_epoch_text() const {
    if (tle_epoch_ <= 0) return "unknown";
    return format_epoch(tle_epoch_);
}

void DataHub::request_point(double lat, double lon, const std::string &name) {
    point_lat_ = lat;
    point_lon_ = lon;
    request_point_name_ = name;
    point_next_ = 0;
    status_.point_loading = true;
    status_.point_error.clear();
}

void DataHub::request_flights(double lat, double lon, bool world) {
    request_lat_ = lat;
    request_lon_ = lon;
    world_flights_ = world;
    flights_next_ = 0;
}

void DataHub::request_weather_now() {
    weather_next_ = 0;
}

void DataHub::request_cyclones_now() {
    cyclones_next_ = 0;
}

void DataHub::schedule(double now, int screen, bool online, gfx::EarthRenderer &earth,
                       const Camera &camera) {
    (void)earth;
    (void)camera;
    if (!online) return;
    if (weather_inflight_ == 0 && now >= weather_next_) {
        weather_next_ = now + weather_interval(1800.0);
        status_.weather_loading = true;
        status_.weather_error.clear();
        constexpr int rows = 4;
        constexpr int cols = 24;
        for (int chunk = 0; chunk < 3; ++chunk) {
            const int base_row = chunk * rows;
            std::string lat;
            std::string lon;
            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    const double la = -82.5 + 15.0 * (base_row + row);
                    const double lo = -172.5 + 15.0 * col;
                    char lat_buffer[24];
                    char lon_buffer[24];
                    std::snprintf(lat_buffer, sizeof(lat_buffer), "%.2f", la);
                    std::snprintf(lon_buffer, sizeof(lon_buffer), "%.2f", lo);
                    if (!lat.empty()) { lat += ','; lon += ','; }
                    lat += lat_buffer;
                    lon += lon_buffer;
                }
            }
            const std::string url =
                "https://api.open-meteo.com/v1/forecast?latitude=" + lat + "&longitude=" + lon +
                "&current=temperature_2m,relative_humidity_2m,surface_pressure,wind_speed_10m,"
                "wind_direction_10m,weather_code,cloud_cover,precipitation&wind_speed_unit=ms&timezone=UTC";
            FetchRequest request;
            request.token = static_cast<uint64_t>(chunk);
            request.kind = FetchKind::WeatherGrid;
            request.url = url;
            request.cache_key = "weather/grid_" + std::to_string(chunk) + ".json";
            request.max_cache_age = 1800.0;
            request.priority = 80 + chunk;
            request.max_bytes = 4 * 1024 * 1024;
            if (net_.enqueue(std::move(request))) ++weather_inflight_;
        }
    }

    if (point_inflight_ == 0 && now >= point_next_ && !request_point_name_.empty()) {
        point_next_ = now + weather_interval(600.0);
        char url[1024];
        std::snprintf(url, sizeof(url),
                      "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,"
                      "precipitation,weather_code,cloud_cover,surface_pressure,wind_speed_10m,"
                      "wind_direction_10m,wind_gusts_10m"
                      "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m"
                      "&forecast_hours=12&daily=sunrise,sunset&timezone=auto&wind_speed_unit=ms",
                      point_lat_, point_lon_);
        FetchRequest request;
        request.token = 0;
        request.kind = FetchKind::WeatherPoint;
        request.url = url;
        request.cache_key = "weather/point_" + std::to_string(static_cast<int>(point_lat_ * 10.0)) +
                            "_" + std::to_string(static_cast<int>(point_lon_ * 10.0)) + ".json";
        request.max_cache_age = 900.0;
        request.priority = 90;
        if (net_.enqueue(std::move(request))) {
            ++point_inflight_;
            status_.point_loading = true;
        }
    }

    if (screen == 1 && flights_inflight_ == 0 && now >= flights_next_) {
        flights_next_ = now + weather_interval(world_flights_ ? 600.0 : 30.0);
        FetchRequest request;
        request.kind = world_flights_ ? FetchKind::WorldFlights : FetchKind::Flights;
        request.priority = 85;
        request.max_bytes = 16 * 1024 * 1024;
        char url[512];
        if (world_flights_) {
            request.url = "https://opensky-network.org/api/states/all";
        } else {
            std::snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.3f/%.3f/250",
                          request_lat_, request_lon_);
            request.url = url;
        }
        if (net_.enqueue(std::move(request))) {
            ++flights_inflight_;
            status_.flights_loading = true;
            status_.flights_error.clear();
        }
    }

    if (cyclones_inflight_ == 0 && now >= cyclones_next_) {
        cyclones_next_ = now + weather_interval(1200.0);
        // A refresh replaces the active set.  If both sources fail, the UI
        // reports the outage rather than showing storms that have dissipated.
        cyclones_.clear();
        status_.cyclones_loading = true;
        status_.cyclones_error.clear();
        FetchRequest eonet;
        eonet.kind = FetchKind::Cyclones;
        eonet.token = 0;
        eonet.url = "https://eonet.gsfc.nasa.gov/api/v3/events?category=severeStorms&status=open&days=40";
        eonet.priority = 70;
        if (net_.enqueue(std::move(eonet))) ++cyclones_inflight_;
        FetchRequest nhc;
        nhc.kind = FetchKind::Cyclones;
        nhc.token = 1;
        nhc.url = "https://www.nhc.noaa.gov/CurrentStorms.json";
        nhc.priority = 71;
        if (net_.enqueue(std::move(nhc))) ++cyclones_inflight_;
    }
}

void DataHub::drain_network(double now, gfx::EarthRenderer &earth) {
    FetchResult result;
    while (net_.poll(result)) {
        switch (result.kind) {
            case FetchKind::Tile:
                if (result.ok && !result.pixels.empty())
                    earth.deliver_tile(result.token, result.pixels.data(), result.width, result.height);
                else
                    earth.fail_tile(result.token);
                break;
            case FetchKind::WeatherGrid:
                handle_weather_grid(result);
                break;
            case FetchKind::WeatherPoint:
                handle_weather_point(result);
                break;
            case FetchKind::Flights:
                handle_flights(result, false);
                break;
            case FetchKind::WorldFlights:
                handle_flights(result, true);
                break;
            case FetchKind::Cyclones:
                handle_cyclones(result);
                break;
            default:
                break;
        }
        if (!result.ok && result.kind != FetchKind::Tile && result.kind != FetchKind::Cyclones)
            status_.last_network_error = now;
    }
    if (grid_.complete()) {
        grid_.finalize();
        if (status_.weather_live && grid_.updated_unix != weather_uploaded_unix_) {
            earth.update_weather(grid_);
            earth.update_wind(grid_);
            weather_uploaded_unix_ = grid_.updated_unix;
        } else if (!status_.weather_live) {
            earth.update_weather(grid_);
            earth.update_wind(grid_);
            status_.weather_live = true;
            status_.weather_updated = now;
            weather_uploaded_unix_ = grid_.updated_unix;
        }
    }
}

void DataHub::handle_weather_grid(FetchResult &result) {
    if (weather_inflight_ > 0) --weather_inflight_;
    if (!result.ok) {
        status_.weather_error = result.error;
        status_.weather_loading = false;
        weather_next_ = platform::monotonic_seconds() + 45.0;
        return;
    }
    Json root;
    std::string error;
    if (!parse_json(std::string_view(reinterpret_cast<char *>(result.body.data()), result.body.size()),
                    root, error) ||
        !root.is_array()) {
        status_.weather_error = "Open-Meteo: " + error;
        return;
    }
    const int chunk = static_cast<int>(result.token % WeatherGrid::kChunks);
    constexpr int rows = 4;
    constexpr int cols = 24;
    const int base_row = chunk * rows;
    int index = 0;
    for (size_t i = 0; i < root.size(); ++i) {
        const Json &entry = root[i];
        const Json &current = entry.get("current");
        if (!current.is_object()) continue;
        const int row = base_row + index / cols;
        const int col = index % cols;
        ++index;
        if (row >= WeatherGrid::kRows || col >= WeatherGrid::kCols) continue;
        auto assign = [&](float &target, const Json &source, const char *key) {
            const Json *value = source.find(key);
            if (value && value->is_number()) target = static_cast<float>(value->as_number());
        };
        assign(grid_.temperature[row][col], current, "temperature_2m");
        assign(grid_.humidity[row][col], current, "relative_humidity_2m");
        assign(grid_.pressure[row][col], current, "surface_pressure");
        assign(grid_.cloud[row][col], current, "cloud_cover");
        assign(grid_.precipitation[row][col], current, "precipitation");
        float speed = 0, direction = 0;
        assign(speed, current, "wind_speed_10m");
        assign(direction, current, "wind_direction_10m");
        const double radians = direction * kDeg;
        grid_.wind_u[row][col] = static_cast<float>(-speed * std::sin(radians));
        grid_.wind_v[row][col] = static_cast<float>(-speed * std::cos(radians));
    }
    grid_.mark_chunk(chunk);
    grid_.finalize();
    status_.weather_chunks = grid_.chunks_ready;
    status_.weather_loading = weather_inflight_ > 0;
    if (grid_.complete()) {
        status_.weather_live = true;
        status_.weather_updated = grid_.updated_unix = platform::monotonic_seconds();
        status_.weather_error.clear();
        weather_next_ = platform::monotonic_seconds() + weather_interval(1800.0);
    }
}

void DataHub::handle_weather_point(FetchResult &result) {
    if (point_inflight_ > 0) --point_inflight_;
    status_.point_loading = false;
    if (!result.ok) {
        status_.point_error = result.error;
        point_next_ = platform::monotonic_seconds() + 30.0;
        return;
    }
    Json root;
    std::string error;
    if (!parse_json(std::string_view(reinterpret_cast<char *>(result.body.data()), result.body.size()),
                    root, error) ||
        !root.is_object()) {
        status_.point_error = "Open-Meteo: " + error;
        return;
    }
    const Json &current = root.get("current");
    if (!current.is_object()) {
        status_.point_error = "no current weather";
        return;
    }
    WeatherPoint point;
    point.valid = true;
    point.name = request_point_name_;
    point.latitude = number_or(root, "latitude", point_lat_);
    point.longitude = number_or(root, "longitude", point_lon_);
    point.utc_offset_seconds = root.get("utc_offset_seconds").as_int(0);
    point.timezone = string_or(root, "timezone");
    point.updated_unix = platform::monotonic_seconds();
    point.temperature = number_or(current, "temperature_2m", 0);
    point.apparent = number_or(current, "apparent_temperature", point.temperature);
    point.humidity = number_or(current, "relative_humidity_2m", 0);
    point.pressure = number_or(current, "surface_pressure", 0);
    point.wind_speed = number_or(current, "wind_speed_10m", 0);
    point.wind_direction = number_or(current, "wind_direction_10m", 0);
    point.wind_gust = number_or(current, "wind_gusts_10m", 0);
    point.precipitation = number_or(current, "precipitation", 0);
    point.cloud_cover = number_or(current, "cloud_cover", 0);
    point.weather_code = current.get("weather_code").as_int(0);
    point.is_day = current.get("is_day").as_int(1);
    const Json &hourly = root.get("hourly");
    if (hourly.is_object()) {
        const Json &temps = hourly.get("temperature_2m");
        const Json &codes = hourly.get("weather_code");
        const Json &precip = hourly.get("precipitation_probability");
        const Json &wind = hourly.get("wind_speed_10m");
        const int count = static_cast<int>(std::min<size_t>(12, temps.size()));
        point.hourly_count = count;
        for (int i = 0; i < count; ++i) {
            point.hourly_temp[i] = temps[i].as_number(0);
            point.hourly_code[i] = codes[i].as_int(0);
            point.hourly_precip[i] = precip[i].as_number(0);
            point.hourly_wind[i] = wind[i].as_number(0);
        }
    }
    const Json &daily = root.get("daily");
    if (daily.is_object()) {
        const Json &sunrise = daily.get("sunrise");
        const Json &sunset = daily.get("sunset");
        if (sunrise.size() > 0 && sunrise[0].is_string()) {
            point.sunrise_unix = parse_iso_local(sunrise[0].as_string(), point.utc_offset_seconds);
            point.has_sunrise = point.sunrise_unix > 0;
        }
        if (sunset.size() > 0 && sunset[0].is_string()) {
            point.sunset_unix = parse_iso_local(sunset[0].as_string(), point.utc_offset_seconds);
            point.has_sunset = point.sunset_unix > 0;
        }
    }
    point_ = point;
    status_.point_updated = platform::monotonic_seconds();
    status_.point_error.clear();
    if (settings_) {
        const double distance =
            great_circle_degrees(point.latitude, point.longitude, settings_->home_lat, settings_->home_lon);
        if (distance < 0.35) {
            settings_->home_utc_offset = point.utc_offset_seconds;
        }
    }
}

void DataHub::handle_flights(FetchResult &result, bool world) {
    if (flights_inflight_ > 0) --flights_inflight_;
    status_.flights_loading = false;
    if (!result.ok) {
        status_.flights_error = result.error;
        return;
    }
    Json root;
    std::string error;
    if (!parse_json(std::string_view(reinterpret_cast<char *>(result.body.data()), result.body.size()),
                    root, error)) {
        status_.flights_error = "flights: " + error;
        return;
    }
    std::vector<Aircraft> parsed;
    if (world) {
        const Json &states = root.get("states");
        if (!states.is_array()) {
            status_.flights_error = "OpenSky: no states";
            return;
        }
        for (size_t i = 0; i < states.size(); ++i) {
            const Json &state = states[i];
            if (!state.is_array() || state.size() < 17) continue;
            if (!state[6].is_number() || !state[5].is_number()) continue;
            Aircraft aircraft;
            aircraft.world = true;
            aircraft.hex = state[0].string_or("");
            aircraft.callsign = trim(state[1].string_or(""));
            aircraft.country = state[2].string_or("");
            aircraft.lon = state[5].as_number();
            aircraft.lat = state[6].as_number();
            aircraft.on_ground = state[8].as_bool(false);
            aircraft.altitude_ft = state[7].is_number() ? state[7].as_number() * 3.28084 : 0;
            aircraft.speed_kt = state[9].is_number() ? state[9].as_number() * 1.94384 : 0;
            aircraft.heading = state[10].as_number(0);
            aircraft.vertical_rate = state[11].is_number() ? state[11].as_number() * 196.85 : 0;
            aircraft.squawk = state[14].string_or("");
            if (aircraft.lat == 0 && aircraft.lon == 0) continue;
            parsed.push_back(std::move(aircraft));
        }
    } else {
        const Json &ac = root.get("ac");
        if (!ac.is_array()) {
            status_.flights_error = "adsb.lol: no aircraft";
            return;
        }
        for (size_t i = 0; i < ac.size(); ++i) {
            const Json &entry = ac[i];
            if (!entry.is_object()) continue;
            if (!entry.get("lat").is_number() || !entry.get("lon").is_number()) continue;
            Aircraft aircraft;
            aircraft.hex = string_or(entry, "hex");
            aircraft.callsign = trim(string_or(entry, "flight"));
            aircraft.lat = entry.get("lat").as_number();
            aircraft.lon = entry.get("lon").as_number();
            const Json &alt = entry.get("alt_baro");
            if (alt.is_number()) {
                aircraft.altitude_ft = alt.as_number();
            } else if (alt.is_string() && alt.as_string() == "ground") {
                aircraft.altitude_ft = 0;
                aircraft.on_ground = true;
            }
            const Json &geom = entry.get("alt_geom");
            if (aircraft.altitude_ft == 0 && geom.is_number()) aircraft.altitude_ft = geom.as_number();
            aircraft.speed_kt = number_or(entry, "gs", 0);
            aircraft.heading = number_or(entry, "track", 0);
            aircraft.vertical_rate = number_or(entry, "baro_rate", number_or(entry, "geom_rate", 0));
            aircraft.squawk = string_or(entry, "squawk");
            aircraft.category = string_or(entry, "category");
            if (aircraft.lat == 0 && aircraft.lon == 0) continue;
            parsed.push_back(std::move(aircraft));
        }
    }

    for (auto &aircraft : parsed) {
        aircraft.distance_km = great_circle_degrees(request_lat_, request_lon_, aircraft.lat,
                                                     aircraft.lon) *
                               111.0;
        double city_distance = 0;
        const City *city = places_.nearest(aircraft.lat, aircraft.lon, 1.5, &city_distance);
        if (city) {
            aircraft.nearest_city = city->name;
            aircraft.nearest_country = city->country;
            if (aircraft.country.empty()) aircraft.country = city->country;
        }
    }
    std::sort(parsed.begin(), parsed.end(), [](const Aircraft &a, const Aircraft &b) {
        return a.distance_km < b.distance_km;
    });
    const size_t limit = world ? 800 : 420;
    if (parsed.size() > limit) parsed.resize(limit);
    flights_ = std::move(parsed);
    status_.flights_live = !flights_.empty();
    status_.flights_updated = platform::monotonic_seconds();
    status_.flights_error.clear();
    flights_next_ = platform::monotonic_seconds() + weather_interval(world ? 600.0 : 30.0);
}

void DataHub::handle_cyclones(FetchResult &result) {
    if (cyclones_inflight_ > 0) --cyclones_inflight_;
    status_.cyclones_loading = cyclones_inflight_ > 0;
    if (!result.ok) {
        status_.cyclones_error = result.error;
        return;
    }
    Json root;
    std::string error;
    if (!parse_json(std::string_view(reinterpret_cast<char *>(result.body.data()), result.body.size()),
                    root, error)) {
        status_.cyclones_error = "cyclones: " + error;
        return;
    }
    if (result.token == 0) {  // EONET
        const Json &events = root.get("events");
        if (events.is_array()) {
            for (size_t i = 0; i < events.size(); ++i) {
                const Json &event = events[i];
                const Json &geometry = event.get("geometry");
                if (!geometry.is_array() || geometry.size() == 0) continue;
                const Json &last = geometry[geometry.size() - 1];
                const Json &coordinates = last.get("coordinates");
                if (!coordinates.is_array() || coordinates.size() < 2) continue;
                Cyclone storm;
                storm.name = string_or(event, "title", "Tropical cyclone");
                storm.source = "EONET / NASA";
                storm.lat = coordinates[1].as_number();
                storm.lon = coordinates[0].as_number();
                storm.updated_unix =
                    parse_iso_local(string_or(last, "date"), 0);
                storm.wind_kt = number_or(event, "magnitudeValue", 0);
                storm.basin = "";
                storm.category = classify_cyclone(storm.wind_kt, "Tropical cyclone");
                bool duplicate = false;
                for (const auto &existing : cyclones_) {
                    if (lower_ascii(existing.name) == lower_ascii(storm.name)) duplicate = true;
                }
                if (!duplicate) cyclones_.push_back(std::move(storm));
            }
        }
    } else {  // NHC
        const Json &storms = root.get("activeStorms");
        if (storms.is_array()) {
            for (size_t i = 0; i < storms.size(); ++i) {
                const Json &entry = storms[i];
                Cyclone storm;
                storm.nhc = true;
                storm.name = string_or(entry, "name", "Unnamed storm");
                storm.source = "NOAA NHC";
                storm.category = string_or(entry, "classification", "Tropical cyclone");
                storm.wind_kt = number_or(entry, "intensity", 0);
                storm.pressure_hpa = number_or(entry, "pressure", 0);
                const std::string lat_text = string_or(entry, "latitude");
                const std::string lon_text = string_or(entry, "longitude");
                double lat = std::atof(lat_text.c_str());
                double lon = std::atof(lon_text.c_str());
                if (!lat_text.empty() && (lat_text.back() == 'S' || lat_text.back() == 's')) lat = -lat;
                if (!lon_text.empty() && (lon_text.back() == 'W' || lon_text.back() == 'w')) lon = -lon;
                storm.lat = lat;
                storm.lon = lon;
                storm.basin = "NHC";
                storm.updated_unix = platform::monotonic_seconds();
                bool duplicate = false;
                for (auto &existing : cyclones_) {
                    if (lower_ascii(existing.name) == lower_ascii(storm.name)) {
                        existing = storm;
                        duplicate = true;
                    }
                }
                if (!duplicate) cyclones_.push_back(std::move(storm));
            }
        }
        if (cyclones_.empty()) {
            const Json &error_field = root.get("error");
            if (error_field.is_string()) status_.cyclones_error = error_field.as_string();
        }
    }
    status_.cyclones_updated = platform::monotonic_seconds();
    status_.cyclones_loading = cyclones_inflight_ > 0;
}

void DataHub::update(double now, const Camera &camera, int screen, bool online,
                     gfx::EarthRenderer &earth) {
    last_frame_now_ = now;
    net_.set_online(online);
    drain_network(now, earth);
    schedule(now, screen, online, earth, camera);
    if (now >= satellites_next_ || satellites_next_ == 0) {
        compute_satellites(now);
        satellites_next_ = now + 0.5;
    }

    if (settings_) {
        // The radar reads best on the bright street map; keep the user's
        // satellite choice for the Earth and satellite screens.
        const int effective_style = screen == 1 ? 2 : std::max(0, std::min(2, settings_->map_style));
        earth.set_style(static_cast<gfx::MapStyle>(effective_style));
        earth.set_weather_layer(settings_->weather_layer);
        earth.set_weather_opacity(0.16f);
        earth.set_wind_visible(settings_->wind_layer);
        earth.set_borders_visible(settings_->borders);
        earth.set_clouds_visible(settings_->clouds);
    }

    // Tiles are always offered to the cache-aware network layer; when offline
    // the worker still serves tiles already on disk.
    const auto tile_requests = earth.prepare_tiles(camera, true, 7, now);
    for (const auto &tile : tile_requests) {
        FetchRequest request;
        request.token = tile.token;
        request.kind = FetchKind::Tile;
        request.url = tile.url;
        request.cache_key = "tiles/" + tile.cache_key;
        request.decode_image = true;
        request.priority = 40;
        request.max_bytes = 2 * 1024 * 1024;
        request.max_cache_age = -1.0;
        net_.enqueue(std::move(request));
    }
    earth.trim_cache(now);
    ++frame_counter_;
    if (point_.valid && status_.point_updated > 0 && now - status_.point_updated > 900.0 &&
        point_inflight_ == 0) {
        point_next_ = 0;
    }
}

}  // namespace nimbus
