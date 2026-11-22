#pragma once

#include <ctime>
#include <string>

#include "gfx/math.hpp"

namespace nimbus {

struct SkyState {
    double unix_time = 0.0;
    double julian_date = 2451545.0;
    double gmst = 0.0;  // radians
    Vec3 sun_eci{1, 0, 0};
    Vec3 moon_eci{0, 1, 0};
    Vec3 sun_ecef{1, 0, 0};
    Vec3 moon_ecef{0, 1, 0};
    double subsolar_lat = 0.0;
    double subsolar_lon = 0.0;
    double moon_illumination = 0.5;
    double moon_phase_angle = 0.0;
    bool moon_waxing = true;
};

SkyState compute_sky(double unix_time);
double julian_date(double unix_time);
double gmst_radians(double jd);
double sun_elevation_degrees(const SkyState &sky, double lat, double lon);
double sun_azimuth_degrees(const SkyState &sky, double lat, double lon);
void sun_rise_set(const SkyState &sky, double lat, double lon, double &rise_unix,
                  double &set_unix, bool &has_rise, bool &has_set);
double moon_elevation_degrees(const SkyState &sky, double lat, double lon);
std::string format_utc(double unix_time);
std::string format_local(double unix_time, int utc_offset_seconds, bool use_24h);

}  // namespace nimbus
