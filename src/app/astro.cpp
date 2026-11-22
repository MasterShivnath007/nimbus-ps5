#include "app/astro.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "app/timeutil.hpp"

namespace nimbus {

double julian_date(double unix_time) {
    return unix_time / 86400.0 + 2440587.5;
}

double gmst_radians(double jd) {
    const double d = jd - 2451545.0;
    double gmst = 280.46061837 + 360.98564736629 * d + 0.000387933 * d * d -
                  d * d * d / 38710000.0;
    gmst = std::fmod(gmst, 360.0);
    if (gmst < 0) gmst += 360.0;
    return gmst * kDeg;
}

namespace {

Vec3 sun_eci(double jd) {
    const double n = jd - 2451545.0;
    const double L = std::fmod(280.460 + 0.9856474 * n, 360.0);
    const double g = std::fmod(357.528 + 0.9856003 * n, 360.0) * kDeg;
    const double lambda = (L + 1.915 * std::sin(g) + 0.020 * std::sin(2 * g)) * kDeg;
    const double epsilon = (23.439 - 0.0000004 * n) * kDeg;
    return Vec3(std::cos(lambda),
                std::cos(epsilon) * std::sin(lambda),
                std::sin(epsilon) * std::sin(lambda))
        .normalized();
}

Vec3 moon_eci(double jd) {
    const double d = jd - 2451545.0;
    const double Lp = std::fmod(218.316 + 13.176396 * d, 360.0) * kDeg;
    const double M = std::fmod(134.963 + 13.064993 * d, 360.0) * kDeg;
    const double F = std::fmod(93.272 + 13.229350 * d, 360.0) * kDeg;
    const double lambda = Lp + 6.289 * kDeg * std::sin(M);
    const double beta = 5.128 * kDeg * std::sin(F);
    const double epsilon = 23.439 * kDeg;
    const double x = std::cos(beta) * std::cos(lambda);
    const double y = std::cos(epsilon) * std::cos(beta) * std::sin(lambda) -
                     std::sin(epsilon) * std::sin(beta);
    const double z = std::sin(epsilon) * std::cos(beta) * std::sin(lambda) +
                     std::cos(epsilon) * std::sin(beta);
    return Vec3(x, y, z).normalized();
}

}  // namespace

SkyState compute_sky(double unix_time) {
    SkyState sky;
    sky.unix_time = unix_time;
    sky.julian_date = julian_date(unix_time);
    sky.gmst = gmst_radians(sky.julian_date);
    sky.sun_eci = sun_eci(sky.julian_date);
    sky.moon_eci = moon_eci(sky.julian_date);

    const double c = std::cos(sky.gmst);
    const double s = std::sin(sky.gmst);
    auto ecef = [&](const Vec3 &eci) {
        return Vec3(c * eci.x + s * eci.y, -s * eci.x + c * eci.y, eci.z).normalized();
    };
    sky.sun_ecef = ecef(sky.sun_eci);
    sky.moon_ecef = ecef(sky.moon_eci);
    sky.subsolar_lat = std::asin(clamp(static_cast<double>(sky.sun_ecef.z), -1.0, 1.0)) / kDeg;
    sky.subsolar_lon = std::atan2(static_cast<double>(sky.sun_ecef.y),
                                  static_cast<double>(sky.sun_ecef.x)) / kDeg;

    const double elong = std::acos(clamp(static_cast<double>(sky.sun_eci.dot(sky.moon_eci)), -1.0, 1.0));
    sky.moon_illumination = 0.5 * (1.0 - std::cos(elong));
    sky.moon_phase_angle = kPi - elong;
    const Vec3 cross = sky.sun_eci.cross(sky.moon_eci);
    sky.moon_waxing = cross.z > 0;
    return sky;
}

double sun_elevation_degrees(const SkyState &sky, double lat, double lon) {
    const Vec3 up = latlon_to_unit(lat, lon);
    return std::asin(clamp(static_cast<double>(up.dot(sky.sun_ecef)), -1.0, 1.0)) / kDeg;
}

double sun_azimuth_degrees(const SkyState &sky, double lat, double lon) {
    const Vec3 up = latlon_to_unit(lat, lon);
    Vec3 east = Vec3(0, 0, 1).cross(up).normalized();
    if (east.length() < 0.1f) east = Vec3(0, 1, 0);
    const Vec3 north = up.cross(east).normalized();
    const double e = sky.sun_ecef.dot(east);
    const double n = sky.sun_ecef.dot(north);
    return std::atan2(e, n) / kDeg;
}

double moon_elevation_degrees(const SkyState &sky, double lat, double lon) {
    const Vec3 up = latlon_to_unit(lat, lon);
    return std::asin(clamp(static_cast<double>(up.dot(sky.moon_ecef)), -1.0, 1.0)) / kDeg;
}

void sun_rise_set(const SkyState &sky, double lat, double lon, double &rise_unix,
                  double &set_unix, bool &has_rise, bool &has_set) {
    // Sample the next 25 hours.  This handles polar day/night without special
    // cases and is more than accurate enough for an ambient display.
    constexpr double step = 120.0;
    const double start = sky.unix_time;
    double previous = sun_elevation_degrees(sky, lat, lon);
    has_rise = has_set = false;
    rise_unix = set_unix = 0;
    for (double t = step; t <= 25.0 * 3600.0; t += step) {
        const SkyState sample = compute_sky(start + t);
        const double elevation = sun_elevation_degrees(sample, lat, lon);
        const double threshold = -0.833;
        if (previous < threshold && elevation >= threshold && !has_rise) {
            rise_unix = start + t;
            has_rise = true;
        }
        if (previous >= threshold && elevation < threshold && !has_set) {
            set_unix = start + t;
            has_set = true;
        }
        previous = elevation;
    }
    if (has_rise && has_set && set_unix < rise_unix) {
        // Already after sunset today: keep both next events, no adjustment.
    }
}

std::string format_utc(double unix_time) {
    const UtcTime utc = utc_from_unix(static_cast<int64_t>(unix_time));
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", utc.hour, utc.minute, utc.second);
    return buffer;
}

std::string format_local(double unix_time, int utc_offset_seconds, bool use_24h) {
    const UtcTime local =
        utc_from_unix(static_cast<int64_t>(unix_time) + utc_offset_seconds);
    char buffer[32];
    if (use_24h) {
        std::snprintf(buffer, sizeof(buffer), "%02u:%02u", local.hour, local.minute);
    } else {
        unsigned hour = local.hour % 12;
        if (hour == 0) hour = 12;
        std::snprintf(buffer, sizeof(buffer), "%u:%02u %s", hour, local.minute,
                      local.hour >= 12 ? "PM" : "AM");
    }
    return buffer;
}

}  // namespace nimbus
