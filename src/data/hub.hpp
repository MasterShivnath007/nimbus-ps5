#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "app/astro.hpp"
#include "app/camera.hpp"
#include "app/settings.hpp"
#include "data/net.hpp"
#include "data/places.hpp"
#include "data/weather.hpp"
#include "gfx/earth.hpp"
#include "TLE.h"

namespace nimbus {

struct Aircraft {
    std::string hex;
    std::string callsign;
    std::string country;
    double lat = 0;
    double lon = 0;
    double altitude_ft = 0;
    double speed_kt = 0;
    double heading = 0;
    double vertical_rate = 0;
    std::string squawk;
    std::string category;
    std::string nearest_city;
    std::string nearest_country;
    double distance_km = 0;
    bool on_ground = false;
    bool world = false;
};

struct Satellite {
    int norad = 0;
    std::string name;
    std::string owner_code;
    std::string country;
    std::string category;
    std::string launch_date;
    double inclination = 0;
    double period_minutes = 0;
    double apogee_km = 0;
    double perigee_km = 0;
    bool has_tle = false;
    double tle_epoch_unix = 0;
    TLE tle{};
    // Per-frame state.
    Vec3 ecef{0, 0, 0};
    double lat = 0;
    double lon = 0;
    double altitude_km = 0;
    double velocity_km_s = 0;
    double elevation_home = 0;
    double azimuth_home = 0;
    double range_home_km = 0;
    bool sunlit = false;
    bool overhead = false;
    std::string above_city;
    std::string above_country;
    std::string orbit_class;
};

struct Cyclone {
    std::string name;
    std::string source;
    std::string basin;
    double lat = 0;
    double lon = 0;
    double wind_kt = 0;
    double pressure_hpa = 0;
    double updated_unix = 0;
    std::string category;
    bool nhc = false;
};

struct DataStatus {
    bool weather_loading = false;
    bool weather_live = false;
    bool flights_loading = false;
    bool flights_live = false;
    bool point_loading = false;
    bool cyclones_loading = false;
    double weather_updated = 0;
    double flights_updated = 0;
    double point_updated = 0;
    double cyclones_updated = 0;
    std::string weather_error;
    std::string flights_error;
    std::string point_error;
    std::string cyclones_error;
    int weather_chunks = 0;
    double last_network_error = 0;
};

class DataHub {
public:
    bool init(const std::string &asset_dir, const std::string &data_dir, const std::string &ca_path,
              Settings *settings);
    void shutdown();
    void update(double now, const Camera &camera, int screen, bool online, gfx::EarthRenderer &earth);
    void request_point(double lat, double lon, const std::string &name);
    void request_flights(double lat, double lon, bool world);
    void request_weather_now();
    void request_cyclones_now();

    const WeatherGrid &grid() const { return grid_; }
    const WeatherPoint &point() const { return point_; }
    const std::vector<Aircraft> &flights() const { return flights_; }
    const std::vector<Satellite> &satellites() const { return satellites_; }
    const std::vector<Cyclone> &cyclones() const { return cyclones_; }
    const DataStatus &status() const { return status_; }
    const Places &places() const { return places_; }
    int pending_requests() const { return net_.pending(); }
    bool online() const { return net_.online(); }
    void set_online(bool online) { net_.set_online(online); }
    void set_home(double lat, double lon);
    void refresh_satellites(double now);
    const Satellite *selected_satellite() const { return selected_satellite_; }
    void select_satellite(int index);
    int selected_satellite_index() const { return selected_satellite_index_; }
    std::vector<Vec3> orbit_path(int satellite_index, double now, int samples = 120);
    bool selected_pass(double now, double &rise_unix, double &peak_unix, double &set_unix,
                       double &peak_elevation) const;
    std::string tle_epoch_text() const;

private:
    void drain_network(double now, gfx::EarthRenderer &earth);
    void handle_weather_grid(FetchResult &result);
    void handle_weather_point(FetchResult &result);
    void handle_flights(FetchResult &result, bool world);
    void handle_cyclones(FetchResult &result);
    void schedule(double now, int screen, bool online, gfx::EarthRenderer &earth,
                  const Camera &camera);
    void compute_satellites(double now);
    void compute_satellite(Satellite &satellite, double now, double home_lat, double home_lon,
                           const SkyState &sky);
    void load_satellites(const std::string &asset_dir);
    void apply_tle_lines(char *line1, char *line2, Satellite &satellite);
    std::string owner_country(const std::string &code) const;
    std::string satellite_category(const std::string &name, const std::string &owner) const;
    double weather_interval(double base) const;
    unsigned destination_seed_ = 0;

    NetClient net_;
    Places places_;
    Settings *settings_ = nullptr;
    std::string asset_dir_;
    std::string data_dir_;
    WeatherGrid grid_;
    WeatherPoint point_;
    std::vector<Aircraft> flights_;
    std::vector<Satellite> satellites_;
    std::vector<Cyclone> cyclones_;
    DataStatus status_;
    std::vector<Vec3> orbit_cache_;
    int orbit_cache_sat_ = -1;
    double orbit_cache_time_ = 0;
    double weather_next_ = 0;
    double point_next_ = 0;
    double flights_next_ = 0;
    double cyclones_next_ = 0;
    double satellites_next_ = 0;
    int weather_inflight_ = 0;
    int point_inflight_ = 0;
    int flights_inflight_ = 0;
    int cyclones_inflight_ = 0;
    bool world_flights_ = false;
    double request_lat_ = 0;
    double request_lon_ = 0;
    double point_lat_ = 0;
    double point_lon_ = 0;
    std::string request_point_name_;
    int selected_satellite_index_ = 0;
    const Satellite *selected_satellite_ = nullptr;
    double last_frame_now_ = 0;
    int frame_counter_ = 0;
    double tle_epoch_ = 0;
    double weather_uploaded_unix_ = -1;
    bool weather_scheduled_ = false;
    bool satcat_loaded_ = false;
};

}  // namespace nimbus
