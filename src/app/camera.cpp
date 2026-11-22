#include "app/camera.hpp"

#include <algorithm>
#include <cmath>

namespace nimbus {

namespace {
constexpr double kOrbitThreshold = 5000000.0;
}

void Camera::update(int pixel_width, int pixel_height) {
    viewport_w = std::max(1, pixel_width);
    viewport_h = std::max(1, pixel_height);
    height = clamp(height, 24.0, 2.4e7);
    tilt = clamp(tilt, 0.0, 6.0 * kDeg);
    const bool new_orbit = height > kOrbitThreshold;
    if (new_orbit != orbit_mode) {
        if (new_orbit) tilt = 0.0;
        orbit_mode = new_orbit;
    }
    if (orbit_mode) tilt = 0.0;

    const double surface_distance = clamp(height / kEarthRadius, 0.0, 20.0);
    const double fov_surface = 55.0 * kDeg;
    const double fov_space = 36.0 * kDeg;
    fov = static_cast<float>(lerp(fov_space, fov_surface,
                                  smoothstep(0.0, 2500000.0, height)));

    const Vec3 unit = latlon_to_unit(lat, lon);
    if (orbit_mode) {
        eye = unit * static_cast<float>(kEarthRadius + height);
        target = Vec3(0, 0, 0);
        up = (std::fabs(lat) > 88.0) ? Vec3(0, 1, 0) : Vec3(0, 0, 1);
    } else {
        const Vec3 north_raw = Vec3(0, 0, 1).cross(unit);
        Vec3 east = Vec3(0, 0, 1).cross(unit).normalized();
        if (east.length() < 0.1f) east = Vec3(0, 1, 0);
        Vec3 north = unit.cross(east).normalized();
        if (north.length() < 0.1f) north = Vec3(0, 1, 0);
        (void)north_raw;
        const Vec3 forward = (north * static_cast<float>(std::cos(heading)) +
                              east * static_cast<float>(std::sin(heading)))
                                 .normalized();
        const double back = height * std::tan(clamp(tilt, 0.0, 6.0 * kDeg));
        const Vec3 base = unit * static_cast<float>(kEarthRadius);
        eye = base + unit * static_cast<float>(height) - forward * static_cast<float>(back);
        target = base;
        const float blend = static_cast<float>(smoothstep(0.02, 0.45, tilt));
        up = (north * (1.0f - blend) + unit * blend).normalized();
    }

    const double eye_height = std::max(1.0, static_cast<double>(eye.length()) - kEarthRadius);
    if (orbit_mode) {
        near_plane = clamp(eye_height * 0.01, 100.0, 500000.0);
        far_plane = 1.0e9;
    } else {
        near_plane = clamp(eye_height * 0.0004, 0.25, 50.0);
        far_plane = std::max(5.0e6, eye_height * 30.0);
    }
    view = mat4_look_at(eye, target, up);
    proj = mat4_perspective(static_cast<float>(fov),
                            static_cast<float>(viewport_w) / viewport_h,
                            static_cast<float>(near_plane), static_cast<float>(far_plane));
    view_proj = proj * view;
    forward = (target - eye).normalized();
    right = forward.cross(up).normalized();
    true_up = right.cross(forward).normalized();
    tan_half_fov = std::tan(static_cast<float>(fov) * 0.5f);
    aspect = static_cast<float>(viewport_w) / std::max(1, viewport_h);
    (void)surface_distance;
}

void Camera::pan_pixels(float dx, float dy) {
    if (std::fabs(dx) < 0.001f && std::fabs(dy) < 0.001f) return;
    if (orbit_mode) {
        // Drag the globe with the pointer.
        const double degrees_per_pixel =
            52.0 * (height / kEarthRadius) / static_cast<double>(viewport_h);
        lat = clamp(lat + dy * degrees_per_pixel, -85.0, 85.0);
        lon = wrap_longitude(lon - dx * degrees_per_pixel /
                                        std::max(0.12, std::cos(lat * kDeg)));
    } else {
        const double mpp = 2.0 * height * std::tan(fov * 0.5) / viewport_h;
        const double east_m = -dx * mpp;
        const double north_m = dy * mpp;
        const Vec3 unit = latlon_to_unit(lat, lon);
        Vec3 east = Vec3(0, 0, 1).cross(unit).normalized();
        if (east.length() < 0.1f) east = Vec3(0, 1, 0);
        const Vec3 north = unit.cross(east).normalized();
        const Vec3 moved = unit +
                           (east * static_cast<float>(east_m) +
                            north * static_cast<float>(north_m)) /
                               static_cast<float>(kEarthRadius);
        double new_lat = 0, new_lon = 0;
        unit_to_latlon(moved, new_lat, new_lon);
        lat = clamp(new_lat, -89.0, 89.0);
        lon = wrap_longitude(new_lon);
    }
    update(viewport_w, viewport_h);
}

void Camera::zoom_by(double factor) {
    const double old = height;
    height = clamp(height * factor, 24.0, 2.4e7);
    if (height < kOrbitThreshold) {
        // Keep the ground under the camera while diving in.
        if (orbit_mode) tilt = 0.0;
    }
    if (old >= kOrbitThreshold && height < kOrbitThreshold) tilt = 0.0;
    update(viewport_w, viewport_h);
}

void Camera::nudge(double dlat, double dlon) {
    lat = clamp(lat + dlat, -89.0, 89.0);
    lon = wrap_longitude(lon + dlon);
    update(viewport_w, viewport_h);
}

void Camera::set_focus(double new_lat, double new_lon, double new_height) {
    lat = clamp(new_lat, -89.0, 89.0);
    lon = wrap_longitude(new_lon);
    height = clamp(new_height, 24.0, 2.4e7);
    update(viewport_w, viewport_h);
}

double Camera::meters_per_pixel() const {
    if (orbit_mode) {
        const double angular = 2.0 * std::tan(fov * 0.5) * (height / kEarthRadius);
        return std::max(100.0, angular * kEarthRadius / viewport_h);
    }
    return std::max(0.05, 2.0 * height * std::tan(fov * 0.5) / viewport_h);
}

void Camera::fly_to(double new_lat, double new_lon, double new_height, double new_tilt,
                    double duration, bool arc) {
    if (flying) {
        // Start from where the interpolated camera currently is.
    }
    fly_from = latlon_to_unit(lat, lon);
    fly_target_unit = latlon_to_unit(new_lat, new_lon);
    fly_h0 = height;
    fly_h1 = clamp(new_height, 24.0, 2.4e7);
    fly_tilt0 = tilt;
    fly_tilt1 = clamp(new_tilt, 0.0, 84.0 * kDeg);
    fly_heading0 = heading;
    const double arc_angle =
        std::acos(clamp(static_cast<double>(fly_from.dot(fly_target_unit)), -1.0, 1.0));
    fly_peak = arc ? std::max(0.0, std::min(2.2e7, arc_angle * kEarthRadius * 0.55)) : 0.0;
    fly_peak = std::max(fly_peak, std::max(fly_h0, fly_h1) * 0.2);
    if (arc_angle < 0.005) fly_peak = 0.0;
    // Keep the shortest route over the pole when that is shorter.
    double target_lon = new_lon;
    if (std::fabs(new_lon - lon) > 180.0) {
        target_lon += (new_lon > lon) ? -360.0 : 360.0;
    }
    // Use a straight longitude interpolation for the camera follow, which is
    // visually smooth even for near-polar routes.
    fly_target_unit = latlon_to_unit(clamp(new_lat, -89.0, 89.0), wrap_longitude(target_lon));
    fly_duration = std::max(0.4, duration);
    fly_t = 0.0;
    flying = true;
    update(viewport_w, viewport_h);
}

void Camera::fly_update(double dt) {
    if (!flying) return;
    fly_t += dt / fly_duration;
    const double t = clamp(fly_t, 0.0, 1.0);
    const double ease = ease_in_out(t);
    const Vec3 unit = slerp_unit(fly_from, fly_target_unit, ease).normalized();
    double new_lat = 0, new_lon = 0;
    unit_to_latlon(unit, new_lat, new_lon);
    lat = clamp(new_lat, -89.0, 89.0);
    lon = wrap_longitude(new_lon);
    height = lerp(fly_h0, fly_h1, ease) + fly_peak * std::sin(kPi * t);
    heading = lerpf(static_cast<float>(fly_heading0), static_cast<float>(fly_heading1),
                    static_cast<float>(ease));
    // Don't tilt until we are below the orbital threshold.
    const double tilt_blend = smoothstep(kOrbitThreshold, 35000.0, height);
    tilt = lerp(fly_tilt0, fly_tilt1, ease) * tilt_blend;
    if (height > kOrbitThreshold) tilt = 0.0;
    update(viewport_w, viewport_h);
    if (t >= 1.0) {
        flying = false;
        height = fly_h1;
        tilt = fly_tilt1;
        update(viewport_w, viewport_h);
    }
}

Vec3 Camera::ray_direction(float ndc_x, float ndc_y) const {
    const Vec3 direction = forward +
                           right * (ndc_x * tan_half_fov * aspect) +
                           true_up * (ndc_y * tan_half_fov);
    return direction.normalized();
}

bool Camera::project(const Vec3 &point, float &screen_x, float &screen_y) const {
    const Vec4 clip = view_proj * Vec4(point.x, point.y, point.z, 1.0f);
    if (clip.w <= 0.0001f) return false;
    const float ndc_x = clip.x / clip.w;
    const float ndc_y = clip.y / clip.w;
    screen_x = (ndc_x * 0.5f + 0.5f) * viewport_w;
    screen_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * viewport_h;
    return ndc_x > -1.2f && ndc_x < 1.2f && ndc_y > -1.2f && ndc_y < 1.2f;
}

bool Camera::visible(const Vec3 &point) const {
    const Vec3 p = point;
    const Vec3 e = eye;
    const Vec3 d = p - e;
    const float a = d.length_sq();
    if (a < 1e-6f) return true;
    const float t = -e.dot(d) / a;
    if (t <= 0.0f || t >= 1.0f) return true;
    const Vec3 closest = e + d * t;
    const float dist = closest.length();
    if (dist < static_cast<float>(kEarthRadius * 0.9995)) return false;
    return true;
}

Vec3 Camera::surface_point(double point_lat, double point_lon) const {
    return latlon_to_unit(point_lat, point_lon) * static_cast<float>(kEarthRadius);
}

}  // namespace nimbus
