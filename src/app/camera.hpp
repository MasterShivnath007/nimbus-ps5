#pragma once

#include <string>

#include "gfx/math.hpp"

namespace nimbus {

struct Camera {
    double lat = 19.076;
    double lon = 72.8777;
    double height = 14500000.0;  // metres above mean surface
    double tilt = 0.0;           // radians from nadir, surface mode only
    double heading = 0.0;        // radians, clockwise from north
    double fov = 38.0 * kDeg;
    int viewport_w = 1920;
    int viewport_h = 1080;

    Vec3 eye{0, 0, 0};
    Vec3 target{0, 0, 0};
    Vec3 up{0, 0, 1};
    Mat4 view = Mat4::identity();
    Mat4 proj = Mat4::identity();
    Mat4 view_proj = Mat4::identity();
    Vec3 forward{0, 0, -1};
    Vec3 right{1, 0, 0};
    Vec3 true_up{0, 1, 0};
    float tan_half_fov = 0.3f;
    float aspect = 1.777f;
    bool orbit_mode = true;
    double near_plane = 10.0;
    double far_plane = 2.0e9;

    bool flying = false;
    double fly_t = 0.0;
    double fly_duration = 3.4;
    Vec3 fly_from{0, 0, 1};
    Vec3 fly_target_unit{0, 0, 1};
    double fly_h0 = 0, fly_h1 = 0;
    double fly_tilt0 = 0, fly_tilt1 = 0;
    double fly_peak = 0;
    double fly_heading0 = 0, fly_heading1 = 0;

    void update(int width, int height);
    void pan_pixels(float dx, float dy);
    void zoom_by(double factor);
    void nudge(double dlat, double dlon);
    void fly_to(double lat, double lon, double height, double tilt, double duration,
                bool arc = true);
    void fly_update(double dt);
    void set_focus(double lat, double lon, double height);
    double meters_per_pixel() const;
    bool project(const Vec3 &point, float &screen_x, float &screen_y) const;
    Vec3 ray_direction(float ndc_x, float ndc_y) const;
    bool visible(const Vec3 &point) const;
    Vec3 surface_point(double lat, double lon) const;
};

}  // namespace nimbus
