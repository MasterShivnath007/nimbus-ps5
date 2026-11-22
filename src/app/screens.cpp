#include "app/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/astro.hpp"
#include "gfx/math.hpp"

namespace nimbus {

using namespace nimbus::gfx;

namespace {
const Color kBg = hex(0x05080E);
const Color kPanel = hex(0x0D1B26, 0.94f);
const Color kPanelStrong = hex(0x0E1D29, 0.97f);
const Color kBorder = hex(0x2C4052, 0.55f);
const Color kText = hex(0xFFFFFF);
const Color kMuted = hex(0xB4C9D8);
const Color kDim = hex(0x7C93A5);
const Color kAccent = hex(0x5EEAD4);
const Color kBlue = hex(0x60A5FA);
const Color kWarm = hex(0xFFB86B);
const Color kDanger = hex(0xF87171);

TextStyle style(int font, float size, Color color, int align = 0) {
    TextStyle out;
    out.font = font;
    out.size = size;
    out.color = color;
    out.align = align;
    return out;
}

Color temperature_gradient(float t) {
    t = clampf(t, 0.0f, 1.0f);
    if (t < 0.2f) return mix(hex(0x1B3B9E), hex(0x22B8E6), t / 0.2f);
    if (t < 0.4f) return mix(hex(0x22B8E6), hex(0x41E0A3), (t - 0.2f) / 0.2f);
    if (t < 0.6f) return mix(hex(0x41E0A3), hex(0xF2E36A), (t - 0.4f) / 0.2f);
    if (t < 0.8f) return mix(hex(0xF2E36A), hex(0xFB8A4A), (t - 0.6f) / 0.2f);
    return mix(hex(0xFB8A4A), hex(0xE84A5F), (t - 0.8f) / 0.2f);
}

Color humidity_gradient(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return mix(mix(hex(0x4B1D6B), hex(0x0F84DC), smoothstep(0.0f, 0.55f, t)),
               hex(0x7CFFD1), smoothstep(0.55f, 1.0f, t));
}

Color pressure_gradient(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return mix(mix(hex(0x5B1B86), hex(0x158FB8), smoothstep(0.0f, 0.5f, t)),
               hex(0xE8E09E), smoothstep(0.5f, 1.0f, t));
}

}  // namespace

void App::draw_brand(float x, float y, float scale) {
    const Color mark = hex(0x6EE7D8);
    ui_.ring(x + 14 * scale, y + 14 * scale, 11 * scale, 1.6f * scale, mark);
    ui_.circle(x + 14 * scale, y + 14 * scale, 4.2f * scale, mark);
    ui_.line(x + 3 * scale, y + 20 * scale, x + 26 * scale, y + 7 * scale, 1.1f * scale,
             with_alpha(mark, 0.8f));
    ui_.circle(x + 27 * scale, y + 5 * scale, 2.1f * scale, hex(0xE9FFFC));
    TextStyle word = style(ui_.font_display_bold(), 23 * scale, kText);
    word.letter_spacing = 2.2f * scale;
    ui_.text(word, "NIMBUS", x + 36 * scale, y - 1 * scale);
    TextStyle sub = style(ui_.font_medium(), 9.5f * scale, kAccent);
    sub.letter_spacing = 3.2f * scale;
    ui_.text(sub, "LIVE EARTH", x + 37 * scale, y + 27 * scale);
}

void App::draw_top_bar() {
    const float w = ui_.virtual_width();
    ui_.rect({0, 0, w, 92}, hex(0x03070C, 0.92f));
    ui_.gradient_rect({0, 92, w, 88}, hex(0x03070C, 0.92f), hex(0x03070C, 0.0f));
    draw_brand(44, 24, 1.0f);

    const float tab_w = 152;
    const float tab_h = 38;
    const float tabs_x = w * 0.5f - tab_w * 1.5f;
    const char *labels[] = {"EARTH", "FLIGHTS", "SATELLITES"};
    const char *icons[] = {"globe", "plane", "satellite"};
    for (int i = 0; i < 3; ++i) {
        const Rect rect(tabs_x + i * tab_w, 22, tab_w - 8, tab_h);
        const bool active = static_cast<int>(screen_) == i;
        if (active) {
            ui_.rounded_rect(rect, tab_h * 0.5f, hex(0x123B3C, 0.88f));
            ui_.rounded_rect({rect.x + 16, rect.y + rect.h - 2, rect.w - 32, 2}, 1, kAccent);
        } else {
            ui_.rounded_rect(rect, tab_h * 0.5f, hex(0x0B141D, 0.5f));
        }
        ui_.icon(icons[i], rect.x + 23, rect.y + rect.h * 0.5f - 1, 16,
                 active ? kAccent : kMuted);
        TextStyle tab = style(ui_.font_semibold(), 12.5f, active ? kText : kMuted);
        tab.letter_spacing = 1.2f;
        ui_.text(tab, labels[i], rect.x + 38, rect.y + 11);
        add_hit(rect, [this, i] { switch_screen(i); });
    }

    const float right = w - 44;
    TextStyle clock = style(ui_.font_display_bold(), 29, kText, 2);
    clock.max_width = 430;
    ui_.text(clock, current_clock(), right - 430, 14);
    TextStyle home = style(ui_.font_medium(), 11.5f, kMuted, 2);
    home.max_width = 430;
    std::string home_temp;
    if (hub_.grid().valid) {
        home_temp = "  ·  " + temperature(hub_.grid().sample(settings_.home_lat, settings_.home_lon,
                                                            hub_.grid().temperature));
    } else if (hub_.point().valid) {
        home_temp = "  ·  " + temperature(hub_.point().temperature);
    }
    const std::string home_label = std::string("HOME · ") + settings_.home_name + home_temp;
    ui_.text(home, home_label, right - 430, 55);

    const float dot_x = right - 460;
    const bool live = hub_.status().weather_live;
    const Color dot_color = !online_ ? kDanger : (live ? kAccent : kWarm);
    ui_.circle(dot_x, 31, 3.6f, dot_color);
    if (online_ && !live) ui_.ring(dot_x, 31, 7.5f + std::sin(frame_phase_ * 3.0f) * 1.6f, 1.0f,
                                   with_alpha(dot_color, 0.5f));
    TextStyle net = style(ui_.font_semibold(), 10, kMuted, 2);
    net.max_width = 130;
    ui_.text(net, !online_ ? "OFFLINE" : live ? "LIVE" : "SYNCING", dot_x - 12, 25);
    ui_.rect({0, 132, w, 1}, hex(0xFFFFFF, 0.06f));
}

void App::draw_screen_title(const std::string &eyebrow, const std::string &title,
                            const std::string &subtitle, float x, float y) {
    TextStyle over = style(ui_.font_semibold(), 9.5f, kAccent);
    over.letter_spacing = 2.6f;
    ui_.text(over, eyebrow, x, y);
    TextStyle heading = style(ui_.font_display_bold(), 25, kText);
    ui_.text(heading, title, x, y + 20);
    TextStyle sub = style(ui_.font_ui(), 11.5f, kMuted);
    ui_.text(sub, subtitle, x, y + 54);
}

void App::draw_map_style_pill() {
    const Rect rect(44, 112, 300, 36);
    ui_.panel(rect, 18, hex(0x0A151F, 0.82f), kBorder);
    const char *icon = settings_.map_style == 0 ? "satellite" : settings_.map_style == 1 ? "layers" : "globe";
    ui_.icon(icon, rect.x + 20, rect.y + 18, 15, kAccent);
    TextStyle label = style(ui_.font_semibold(), 10.5f, kText);
    label.letter_spacing = 0.8f;
    ui_.text(label, map_style_name(settings_.map_style), rect.x + 36, rect.y + 10);
    char altitude[48];
    if (camera_.height >= 100000.0) std::snprintf(altitude, sizeof(altitude), "%.0f km", camera_.height / 1000.0);
    else if (camera_.height >= 1000.0) std::snprintf(altitude, sizeof(altitude), "%.1f km", camera_.height / 1000.0);
    else std::snprintf(altitude, sizeof(altitude), "%.0f m", camera_.height);
    TextStyle alt = style(ui_.font_ui(), 10, kMuted);
    ui_.text(alt, altitude, rect.x + 152, rect.y + 11);
    ui_.icon("chevron-down", rect.x + rect.w - 19, rect.y + 18, 13, kDim);
    add_hit(rect, [this] {
        settings_.map_style = (settings_.map_style + 1) % 3;
        settings_dirty_ = true;
        apply_settings();
        toast(map_style_name(settings_.map_style));
    });
    if (camera_.height < 2500000.0 && earth_.tile_stats().pending > 0) {
        ui_.spinner(rect.x + rect.w + 18, rect.y + 18, 7, frame_phase_ * 3.2f, kAccent);
    }
}

void App::draw_weather_card(const Rect &rect) {
    ui_.panel(rect, 22, kPanel, kBorder);
    const WeatherPoint &point = hub_.point();
    const float x = rect.x + 22;
    const float y = rect.y + 18;

    ui_.icon("map-pin", x + 7, y + 12, 15, kAccent);
    TextStyle city = style(ui_.font_semibold(), 22, kText);
    const std::string name = point.valid ? point.name : settings_.home_name;
    ui_.text(city, ellipsize(ui_.font_semibold(), 22, name, 270), x + 20, y + 1);
    TextStyle country = style(ui_.font_ui(), 10.5f, kMuted);
    std::string country_text = settings_.home_country;
    if (point.valid) {
        double distance = 0;
        const City *pc = hub_.places().nearest(point.latitude, point.longitude, 1.2, &distance);
        country_text = pc ? pc->country : "Open ocean";
        if (point.name == "Selected location") {
            const std::string coords = latlon_string(point.latitude, point.longitude);
            country_text = coords + "  ·  " + country_text;
        }
    }
    ui_.text(country, country_text, x + 20, y + 27);

    const bool at_home = great_circle_degrees(camera_.lat, camera_.lon, settings_.home_lat,
                                               settings_.home_lon) < 0.6;
    const Rect badge(rect.x + rect.w - 106, y + 2, 84, 24);
    ui_.rounded_rect(badge, 12, hex(at_home ? 0x123B3C : 0x1B2B3A, 0.9f));
    TextStyle badge_text = style(ui_.font_semibold(), 8.5f, at_home ? kAccent : kMuted, 1);
    badge_text.letter_spacing = 1.4f;
    badge_text.max_width = badge.w;
    ui_.text(badge_text, at_home ? "HOME" : "FOCUS", badge.x, badge.y + 8);

    if (!point.valid) {
        ui_.spinner(rect.x + rect.w - 38, rect.y + 42, 9, frame_phase_ * 2.5f, kAccent);
        TextStyle loading = style(ui_.font_ui(), 12, kMuted);
        ui_.text(loading, hub_.status().point_loading ? "Connecting to Open-Meteo…" : "Weather unavailable",
                 x, y + 68);
        for (int i = 0; i < 3; ++i) {
            const Rect bar(x, y + 100.0f + i * 24, rect.w - 44 - i * 50, 8);
            ui_.rounded_rect(bar, 4, hex(0xFFFFFF, 0.055f));
        }
        return;
    }

    const std::string condition = weather_condition(point.weather_code);
    ui_.icon(weather_icon(point.weather_code, point.is_day != 0), rect.x + rect.w - 70,
             rect.y + 88, 56, point.is_day ? hex(0xFFE0A3) : hex(0xBFD4FF));
    TextStyle temp = style(ui_.font_display_bold(), 74, kText);
    ui_.text(temp, temperature(point.temperature, false), x - 2, y + 46);
    const float temp_width = ui_.text_width(ui_.font_display_bold(), 74, temperature(point.temperature, false));
    TextStyle unit = style(ui_.font_display(), 22, kMuted);
    ui_.text(unit, settings_.celsius ? "C" : "F", x + temp_width + 2, y + 62);
    TextStyle cond = style(ui_.font_semibold(), 15, kText);
    ui_.text(cond, condition, x, y + 124);
    TextStyle feels = style(ui_.font_ui(), 12, kMuted);
    ui_.text(feels, "Feels like " + temperature(point.apparent), x, y + 144);

    const float chip_y = y + 170;
    const float chip_w = (rect.w - 52) / 4;
    const char *chip_labels[] = {"WIND", "HUMIDITY", "PRESSURE", "CLOUD"};
    const std::string chip_values[] = {
        wind_speed(point.wind_speed), std::to_string(static_cast<int>(point.humidity)) + "%",
        pressure_string(point.pressure), std::to_string(static_cast<int>(point.cloud_cover)) + "%"};
    for (int i = 0; i < 4; ++i) {
        const Rect chip(x + i * (chip_w + 4), chip_y, chip_w, 50);
        ui_.rounded_rect(chip, 10, hex(0xFFFFFF, 0.045f));
        TextStyle label = style(ui_.font_semibold(), 8, kDim);
        label.letter_spacing = 1.0f;
        ui_.text(label, chip_labels[i], chip.x + 10, chip.y + 8);
        TextStyle value = style(ui_.font_semibold(), 11.5f, kText);
        ui_.text(value, ellipsize(ui_.font_semibold(), 11.5f, chip_values[i], chip_w - 16),
                 chip.x + 10, chip.y + 24);
    }

    // Hourly temperature sparkline.
    const Rect spark(x, chip_y + 64, rect.w - 44, 54);
    if (point.hourly_count > 1) {
        float low = point.hourly_temp[0], high = point.hourly_temp[0];
        for (int i = 1; i < point.hourly_count; ++i) {
            low = std::min(low, static_cast<float>(point.hourly_temp[i]));
            high = std::max(high, static_cast<float>(point.hourly_temp[i]));
        }
        if (high - low < 1) high = low + 1;
        float previous_x = spark.x;
        float previous_y = spark.bottom() - (point.hourly_temp[0] - low) / (high - low) * spark.h;
        for (int i = 1; i < point.hourly_count; ++i) {
            const float px = spark.x + spark.w * i / (point.hourly_count - 1);
            const float py = spark.bottom() - (point.hourly_temp[i] - low) / (high - low) * spark.h;
            const Color c0 = mix(kBlue, kWarm, (point.hourly_temp[i - 1] - low) / (high - low));
            const Color c1 = mix(kBlue, kWarm, (point.hourly_temp[i] - low) / (high - low));
            ui_.triangle(previous_x, previous_y, px, py, previous_x, spark.bottom(), with_alpha(c0, 0.055f));
            ui_.triangle(previous_x, spark.bottom(), px, py, px, spark.bottom(), with_alpha(c1, 0.055f));
            ui_.line(previous_x, previous_y, px, py, 1.8f, mix(c0, c1, 0.5f));
            ui_.circle(px, py, 1.7f, with_alpha(c1, 0.9f));
            previous_x = px;
            previous_y = py;
        }
        TextStyle range = style(ui_.font_ui(), 9, kDim, 2);
        range.max_width = 90;
        ui_.text(range, temperature(high) + " / " + temperature(low), spark.x + spark.w, spark.y - 3);
        TextStyle hour = style(ui_.font_ui(), 8.5f, kDim);
        ui_.text(hour, "NEXT 12 HOURS", spark.x, spark.y - 3);
    } else {
        TextStyle hour = style(ui_.font_ui(), 9, kDim);
        ui_.text(hour, "HOURLY FORECAST UNAVAILABLE", spark.x, spark.y);
    }

    // Sunrise and sunset.
    const float sun_y = rect.y + rect.h - 28;
    if (point.has_sunrise) {
        ui_.icon("sun", x + 6, sun_y + 6, 12, kWarm);
        TextStyle sunrise = style(ui_.font_medium(), 10, kMuted);
        ui_.text(sunrise, "Rise " + format_local(point.sunrise_unix, point.utc_offset_seconds,
                                                  settings_.clock_24h),
                 x + 16, sun_y);
    }
    if (point.has_sunset) {
        ui_.icon("moon", x + 142, sun_y + 6, 12, kBlue);
        TextStyle sunset = style(ui_.font_medium(), 10, kMuted);
        ui_.text(sunset, "Set " + format_local(point.sunset_unix, point.utc_offset_seconds,
                                               settings_.clock_24h),
                 x + 152, sun_y);
    }
    TextStyle updated = style(ui_.font_ui(), 9, kDim, 2);
    updated.max_width = 210;
    const double age = std::max(0.0, platform::monotonic_seconds() - hub_.status().point_updated);
    char age_text[32];
    if (age < 60) std::snprintf(age_text, sizeof(age_text), "UPDATED %ds AGO", static_cast<int>(age));
    else std::snprintf(age_text, sizeof(age_text), "UPDATED %dm AGO", static_cast<int>(age / 60));
    ui_.text(updated, age_text, rect.x + rect.w - 22 - 180, sun_y);

    if (point.weather_code >= 95) {
        ui_.rounded_rect({rect.x + rect.w - 196, y + 30, 84, 22}, 11, hex(0x3A161B, 0.9f));
        TextStyle alert = style(ui_.font_semibold(), 8, kDanger, 1);
        ui_.text(alert, "STORM", rect.x + rect.w - 196, y + 37);
    }
}

void App::draw_layer_rail() {
    const float w = ui_.virtual_width();
    const float x = w - 272;
    const float y = 168;
    const float width = 228;
    ui_.panel({x, y, width, 44 + 5 * 54}, 20, kPanel, kBorder);
    TextStyle header = style(ui_.font_semibold(), 9, kMuted);
    header.letter_spacing = 2.2f;
    ui_.text(header, "WEATHER LAYERS", x + 18, y + 16);
    ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2), "D-PAD ◂ ▸", x + width - 18, y + 16);

    struct LayerRow {
        const char *icon;
        const char *label;
        const char *sub;
        int layer;
    };
    const LayerRow rows[] = {
        {"thermometer", "Temperature", "Heatmap", 0},
        {"droplets", "Humidity", "Relative", 1},
        {"gauge", "Pressure", "Isobars", 2},
        {"wind", "Wind", "10 m flow", -1},
        {"orbit", "Cyclones", "", -2},
    };
    for (int i = 0; i < 5; ++i) {
        const Rect row(x + 8, y + 38 + i * 54, width - 16, 46);
        bool active = false;
        std::string value = rows[i].sub;
        if (rows[i].layer >= 0) active = settings_.weather_layer == rows[i].layer;
        else if (rows[i].layer == -1) {
            active = settings_.wind_layer;
            value = settings_.wind_layer ? "ON" : "OFF";
        } else {
            active = false;
            value = std::to_string(hub_.cyclones().size()) + " active";
        }
        if (active) {
            ui_.rounded_rect(row, 12, hex(0x123B3C, 0.78f));
            ui_.rounded_rect({row.x, row.y + 10, 2.5f, row.h - 20}, 1.25f, kAccent);
        }
        ui_.icon(rows[i].icon, row.x + 22, row.y + row.h * 0.5f, 17,
                 active ? kAccent : kMuted);
        TextStyle label = style(ui_.font_semibold(), 12, active ? kText : kMuted);
        ui_.text(label, rows[i].label, row.x + 40, row.y + 8);
        ui_.text(style(ui_.font_ui(), 9.5f, active ? with_alpha(kAccent, 0.85f) : kDim),
                 value, row.x + 40, row.y + 26);
        add_hit(row, [this, i] {
            if (i < 3) {
                settings_.weather_layer = settings_.weather_layer == i ? -1 : i;
                settings_dirty_ = true;
                apply_settings();
                toast(layer_name(settings_.weather_layer));
            } else if (i == 3) {
                settings_.wind_layer = !settings_.wind_layer;
                settings_dirty_ = true;
                apply_settings();
                toast(settings_.wind_layer ? "Wind streamlines on" : "Wind streams off");
            } else {
                if (!hub_.cyclones().empty()) {
                    const Cyclone &storm = hub_.cyclones()[0];
                    fly_to(storm.lat, storm.lon, 4200.0, 0.0, 3.0);
                    toast(storm.name + " · " + number(storm.wind_kt) + " kt");
                } else {
                    toast("No active tropical cyclones");
                }
            }
        });
    }

    const float legend_y = y + 38 + 5 * 54 + 8;
    const Rect legend(x, legend_y, width, 132);
    ui_.panel(legend, 18, kPanel, kBorder);
    ui_.text(style(ui_.font_semibold(), 8.5f, kMuted), "LEGEND", x + 16, legend_y + 14);
    const Rect bar(x + 16, legend_y + 34, width - 32, 10);
    const int layer = settings_.weather_layer;
    if (layer >= 0 && hub_.grid().valid) {
        for (int i = 0; i < 48; ++i) {
            const float t = i / 47.0f;
            const Color c = layer == 0 ? temperature_gradient(t)
                            : layer == 1 ? humidity_gradient(t)
                                         : pressure_gradient(t);
            ui_.rect({bar.x + bar.w * i / 48.0f, bar.y, bar.w / 48.0f + 0.5f, bar.h}, c);
        }
        char low[32], high[32];
        if (layer == 0) {
            std::snprintf(low, sizeof(low), "%s", temperature(hub_.grid().temp_min).c_str());
            std::snprintf(high, sizeof(high), "%s", temperature(hub_.grid().temp_max).c_str());
        } else if (layer == 1) {
            std::snprintf(low, sizeof(low), "%.0f%%", hub_.grid().humidity_min);
            std::snprintf(high, sizeof(high), "%.0f%%", hub_.grid().humidity_max);
        } else {
            std::snprintf(low, sizeof(low), "%.0f", hub_.grid().pressure_min);
            std::snprintf(high, sizeof(high), "%.0f hPa", hub_.grid().pressure_max);
        }
        ui_.text(style(ui_.font_medium(), 9, kMuted), low, bar.x, bar.y + 16);
        ui_.text(style(ui_.font_medium(), 9, kMuted, 2), high, bar.x + bar.w, bar.y + 16);
        const char *note = layer == 0 ? "Surface temperature · global model"
                            : layer == 1 ? "Relative humidity · global model"
                                         : "Mean sea-level pressure · 4 hPa isobars";
        ui_.text(style(ui_.font_ui(), 8.5f, kDim), note, bar.x, bar.y + 34);
    } else if (layer < 0) {
        if (settings_.wind_layer) {
            ui_.text(style(ui_.font_semibold(), 9, kAccent), "WIND · 10 METRES", bar.x, bar.y);
            for (int i = 0; i < 18; ++i) {
                const float px = bar.x + i * 10.0f;
                ui_.line(px, bar.y + 24 + std::sin(i * 0.7f) * 3.0f, px + 8,
                         bar.y + 24 + std::sin(i * 0.7f + 0.8f) * 3.0f, 1.4f,
                         with_alpha(kAccent, 0.35f + 0.4f * (i % 3) / 2.0f));
            }
            ui_.text(style(ui_.font_ui(), 8.5f, kDim),
                     "Directional streamlines · Open-Meteo field", bar.x, bar.y + 38);
        } else {
            ui_.text(style(ui_.font_ui(), 10, kMuted), "Weather field hidden", bar.x, bar.y);
            ui_.text(style(ui_.font_ui(), 8.5f, kDim), "Choose a layer to paint the planet",
                     bar.x, bar.y + 20);
        }
    } else {
        ui_.spinner(bar.x + 10, bar.y + 4, 8, frame_phase_ * 2.8f, kAccent);
        ui_.text(style(ui_.font_ui(), 9.5f, kMuted), "Loading global weather grid…",
                 bar.x + 24, bar.y - 3);
        ui_.text(style(ui_.font_ui(), 8.5f, kDim), "Four Open-Meteo grid windows",
                 bar.x, bar.y + 20);
    }
    ui_.text(style(ui_.font_ui(), 8, kDim, 2), "OPEN-METEO · 30 MIN", x + width - 16,
             legend_y + 110);
}

void App::draw_city_labels() {
    if (!settings_.labels) return;
    const float half_arc = camera_.orbit_mode
                                ? std::acos(clamp(static_cast<double>(kEarthRadius) /
                                                      (kEarthRadius + camera_.height), -1.0, 1.0)) /
                                      kDeg
                                : std::max(0.3, camera_.height / kEarthRadius / kDeg * 1.2);
    double half_lat = std::min(80.0, half_arc * 1.05);
    double half_lon = std::min(160.0, half_arc * 1.05 /
                                         std::max(0.12, std::cos(camera_.lat * kDeg)) *
                                         static_cast<double>(camera_.viewport_w) / camera_.viewport_h);
    const int min_pop = camera_.height > 4000000.0 ? 1800000
                        : camera_.height > 700000.0 ? 500000
                                                    : 0;
    const auto cities = hub_.places().label_cities(camera_.lat, camera_.lon, half_lat, half_lon,
                                                   camera_.height > 4000000.0 ? 11 : 13, min_pop);
    const City *center_city = nearest_city_to_center(camera_.height > 1500000.0 ? 6.0 : 1.2);
    std::vector<Rect> occupied = label_rects_;
    const float scale = ui_.scale();
    for (const City *city : cities) {
        const Vec3 point = latlon_to_unit(city->lat, city->lon) * static_cast<float>(kEarthRadius);
        if (!camera_.visible(point)) continue;
        float sx = 0, sy = 0;
        if (!camera_.project(point, sx, sy)) continue;
        sx /= scale;
        sy /= scale;
        if (sx < 30 || sy < 150 || sx > ui_.virtual_width() - 30 || sy > ui_.virtual_height() - 80)
            continue;
        const double center_distance = great_circle_degrees(city->lat, city->lon, camera_.lat,
                                                            camera_.lon);
        if (city == center_city || center_distance < (camera_.height > 1000000.0 ? 1.0 : 0.12))
            continue;
        std::string label = city->name;
        if (hub_.grid().valid) {
            const float value = hub_.grid().sample(city->lat, city->lon, hub_.grid().temperature);
            label += "  " + temperature(value);
        }
        const float label_size = camera_.orbit_mode ? 10.8f : 12.0f;
        const float width = ui_.text_width(ui_.font_medium(), label_size, label);
        const Rect rect(sx + 6, sy - 10, width + 4, 19);
        bool blocked = false;
        for (const Rect &other : occupied) {
            if (rect.x < other.right() && rect.right() > other.x && rect.y < other.bottom() &&
                rect.bottom() > other.y) {
                blocked = true;
                break;
            }
        }
        if (blocked) continue;
        occupied.push_back(rect);
        ui_.rounded_rect({sx + 2, sy - 12, width + 12, 22}, 11, hex(0x06121A, 0.68f));
        ui_.circle(sx - 1, sy, camera_.height < 1000000.0 ? 2.6f : 2.0f, with_alpha(kAccent, 0.95f));
        ui_.text(style(ui_.font_medium(), label_size, hex(0xEAF7FF, 0.97f)), label, sx + 8, sy - 10);
        add_hit({sx - 7, sy - 13, width + 22, 24}, [this, city] { focus_city(*city, true); });
    }
    if (center_city && camera_.height < 12000000.0) {
        float sx = 0, sy = 0;
        const Vec3 point = latlon_to_unit(center_city->lat, center_city->lon) *
                           static_cast<float>(kEarthRadius);
        if (camera_.project(point, sx, sy)) {
            sx /= scale;
            sy /= scale;
            const float width = ui_.text_width(ui_.font_semibold(), 13, center_city->name) + 26;
            const Rect pill(sx - width * 0.5f, sy - 42, width, 26);
            ui_.rounded_rect(pill, 13, hex(0x0B2226, 0.92f));
            ui_.rounded_rect({pill.x, pill.y + pill.h - 2, pill.w, 2}, 1, kAccent);
            ui_.text(style(ui_.font_semibold(), 13, kText, 1), center_city->name, pill.x, pill.y + 5);
        }
    }
}

void App::draw_center_reticle() {
    const float cx = ui_.virtual_width() * 0.5f;
    const float cy = ui_.virtual_height() * 0.5f;
    const Color color = camera_.flying ? kWarm : kAccent;
    const float pulse = 1.0f + std::sin(frame_phase_ * 2.4f) * 0.15f;
    ui_.ring(cx, cy, 8.5f * pulse, 1.1f, with_alpha(color, 0.55f));
    ui_.circle(cx, cy, 2.0f, color);
    ui_.line(cx - 18, cy, cx - 9, cy, 1.2f, with_alpha(color, 0.8f));
    ui_.line(cx + 9, cy, cx + 18, cy, 1.2f, with_alpha(color, 0.8f));
    ui_.line(cx, cy - 18, cx, cy - 9, 1.2f, with_alpha(color, 0.8f));
    ui_.line(cx, cy + 9, cx, cy + 18, 1.2f, with_alpha(color, 0.8f));

    const City *city = nearest_city_to_center(camera_.height > 1500000.0 ? 6.0 : 1.2);
    std::string label;
    if (hub_.point().valid &&
        great_circle_degrees(camera_.lat, camera_.lon, hub_.point().latitude,
                             hub_.point().longitude) < 0.7)
        label = hub_.point().name;
    else if (city)
        label = city->name;
    if (label.empty()) return;
    if (camera_.flying) label = "EN ROUTE";
    const float width = std::max(110.0f, ui_.text_width(ui_.font_semibold(), 11, label) + 32);
    const Rect pill(cx - width * 0.5f, cy + 26, width, 27);
    ui_.rounded_rect(pill, 13, hex(0x061018, 0.82f));
    TextStyle name_style = style(ui_.font_semibold(), 11, kText, 1);
    name_style.max_width = pill.w;
    ui_.text(name_style, label, pill.x, pill.y + 6);
    if (!camera_.flying) {
        TextStyle hint = style(ui_.font_ui(), 9.5f, kMuted, 1);
        hint.max_width = pill.w;
        ui_.text(hint, "PRESS CROSS TO FLY HERE", pill.x, pill.y + 31);
    } else {
        const float progress = static_cast<float>(clamp(camera_.fly_t, 0.0, 1.0));
        ui_.rounded_rect({pill.x + 16, pill.y + pill.h + 7, (pill.w - 32) * progress, 2}, 1, kWarm);
    }
}

void App::draw_cyclones() {
    const auto &storms = hub_.cyclones();
    if (storms.empty()) return;
    const float w = ui_.virtual_width();
    const Rect strip(w * 0.5f - 220, 112, 440, 34);
    ui_.panel(strip, 17, hex(0x180C0D, 0.86f), hex(0x6B3026, 0.55f));
    const float pulse = 0.5f + 0.5f * std::sin(frame_phase_ * 2.2f);
    ui_.ring(strip.x + 20, strip.y + 17, 7 + pulse * 2, 1.2f, with_alpha(hex(0xFF8A6B), 0.8f));
    ui_.circle(strip.x + 20, strip.y + 17, 3.0f, hex(0xFF8A6B));
    TextStyle label = style(ui_.font_semibold(), 9.5f, hex(0xFFC2AE));
    label.letter_spacing = 0.8f;
    ui_.text(label, std::to_string(storms.size()) + " ACTIVE TROPICAL " +
                        (storms.size() == 1 ? "SYSTEM" : "SYSTEMS"),
             strip.x + 34, strip.y + 6);
    const Cyclone &strongest = storms[0];
    ui_.text(style(ui_.font_ui(), 9.5f, kMuted),
             strongest.name + "  ·  " +
                 (strongest.category.empty() || strongest.category == "Tropical cyclone"
                      ? "active system"
                      : strongest.category),
             strip.x + 34, strip.y + 20);
    add_hit(strip, [this, strongest] {
        fly_to(strongest.lat, strongest.lon, 4200.0, 0.0, 3.0);
        toast(strongest.name + " · " + strongest.category);
    });
    const float scale = ui_.scale();
    for (size_t i = 0; i < storms.size(); ++i) {
        const Cyclone &storm = storms[i];
        const Vec3 point = latlon_to_unit(storm.lat, storm.lon) * static_cast<float>(kEarthRadius);
        if (!camera_.visible(point)) continue;
        float sx = 0, sy = 0;
        if (!camera_.project(point, sx, sy)) continue;
        sx /= scale;
        sy /= scale;
        if (sx < 0 || sy < 140 || sx > ui_.virtual_width() || sy > ui_.virtual_height() - 40) continue;
        const float pulse = 0.5f + 0.5f * std::sin(frame_phase_ * 1.7f + i);
        const Color storm_color = i == 0 ? hex(0xFF8A6B) : hex(0xFFB86B);
        ui_.ring(sx, sy, 18 + pulse * 10, 1.2f, with_alpha(storm_color, 0.25f));
        ui_.ring(sx, sy, 12, 1.5f, with_alpha(storm_color, 0.8f));
        ui_.circle(sx, sy, 3.2f, storm_color);
        ui_.arc(sx, sy, 7, frame_phase_ * 1.3f, frame_phase_ * 1.3f + 2.4f, 1.5f, storm_color, 14);
        ui_.arc(sx, sy, 7, frame_phase_ * 1.3f + 3.4f, frame_phase_ * 1.3f + 5.2f, 1.5f,
                with_alpha(storm_color, 0.6f), 10);
        const std::string label = storm.name + " · " + (storm.wind_kt > 0 ? number(storm.wind_kt) + " kt" : storm.category);
        ui_.text_shadowed(style(ui_.font_semibold(), 10.5f, storm_color), label, sx + 22, sy - 9,
                          hex(0x000000, 0.8f));
        add_hit({sx - 22, sy - 22, 220, 44}, [this, storm] {
            fly_to(storm.lat, storm.lon, 4200.0, 0.0, 3.0);
            toast(storm.name + " · " + number(storm.wind_kt) + " kt");
        });
    }
}

void App::draw_globe_hud() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    const Rect card(44, h - 438, 460, 344);
    const Rect rail(w - 272, 168, 228, 44 + 5 * 54 + 140);
    label_rects_.push_back({0, 0, w, 138});
    label_rects_.push_back(card);
    label_rects_.push_back(rail);
    label_rects_.push_back({w * 0.5f - 190, h - 72, 380, 54});

    draw_city_labels();
    draw_cyclones();
    draw_center_reticle();
    draw_map_style_pill();
    draw_layer_rail();
    draw_weather_card(card);
    ui_.gradient_rect({0, h - 160, w, 160}, hex(0x02050A, 0.0f), hex(0x02050A, 0.92f));
    draw_controller_prompts();
    draw_attribution();
    if (modal_ != ModalNone) return;
    if (screen_fade_ < 1.0f) {
        ui_.rect({0, 0, w, h}, hex(0x000000, (1.0f - screen_fade_) * 0.55f));
    }
}

void App::draw_controller_prompts() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    const float y = h - 36;
    float x = w * 0.5f - 300;
    draw_prompt(x, y, "cross", "Fly here");
    x += 122;
    draw_prompt(x, y, "dpad", "Layers");
    x += 116;
    draw_prompt(x, y, "l2", "Zoom");
    x += 96;
    draw_prompt(x, y, "l3", "Time");
    x += 100;
    draw_prompt(x, y, "r3", "Discover");
    x += 114;
    draw_prompt(x, y, "options", "Menu");
}

void App::draw_prompt(float x, float y, const char *glyph, const std::string &label) {
    const std::string key = glyph;
    const Color line = hex(0xD7E6F1, 0.95f);
    float label_x = x + 31;
    if (key == "cross" || key == "circle" || key == "square" || key == "triangle") {
        ui_.circle(x + 12, y + 12, 11, hex(0xFFFFFF, 0.10f));
        ui_.ring(x + 12, y + 12, 11, 1.0f, hex(0xFFFFFF, 0.18f));
        if (key == "cross") {
            ui_.line(x + 6, y + 6, x + 16, y + 16, 1.5f, line);
            ui_.line(x + 16, y + 6, x + 6, y + 16, 1.5f, line);
        } else if (key == "circle") {
            ui_.ring(x + 11, y + 11, 5.2f, 1.4f, line);
        } else if (key == "square") {
            ui_.line(x + 6.5f, y + 6, x + 15.5f, y + 6, 1.3f, line);
            ui_.line(x + 6.5f, y + 16, x + 15.5f, y + 16, 1.3f, line);
            ui_.line(x + 6.5f, y + 6, x + 6.5f, y + 16, 1.3f, line);
            ui_.line(x + 15.5f, y + 6, x + 15.5f, y + 16, 1.3f, line);
        } else {
            ui_.line(x + 22, y + 15, x + 11, y + 4.5f, 1.3f, line);
            ui_.line(x + 11, y + 4.5f, x, y + 15, 1.3f, line);
            ui_.line(x, y + 15, x + 22, y + 15, 1.3f, line);
        }
    } else if (key == "dpad") {
        ui_.rounded_rect({x + 2, y + 7, 18, 8}, 3, hex(0xFFFFFF, 0.09f));
        ui_.rounded_rect({x + 7, y + 2, 8, 18}, 3, hex(0xFFFFFF, 0.09f));
        ui_.circle(x + 11, y + 11, 2.2f, line);
    } else {
        const float key_width = (key == "options" || key == "touchpad") ? 46.0f : 30.0f;
        const Rect pill(x, y + 2, key_width, 18);
        ui_.rounded_rect(pill, 9, hex(0xFFFFFF, 0.09f));
        ui_.ring(x + key_width * 0.5f, y + 11, 9, 0.8f, hex(0xFFFFFF, 0.14f));
        TextStyle key_style = style(ui_.font_semibold(), 8, line, 1);
        key_style.max_width = pill.w;
        std::string text = key;
        for (auto &c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        ui_.text(key_style, text, pill.x, pill.y + 5);
        label_x = x + key_width + 1.0f;
    }
    ui_.text(style(ui_.font_medium(), 12, hex(0xD9E6EF, 0.95f)), label, label_x, y + 4);
}

void App::draw_attribution() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    const char *text = "";
    if (screen_ == ScreenGlobe)
        text = "EOX::Maps · Esri World Imagery / Street Map · © OpenStreetMap contributors · Open-Meteo";
    else if (screen_ == ScreenFlights)
        text = "Live ADS-B © adsb.lol · World snapshot © OpenSky Network";
    else if (screen_ == ScreenSatellites)
        text = "TLEs © CelesTrak · SATCAT · SGP4 propagation";
    TextStyle attr = style(ui_.font_ui(), 8.5f, hex(0x9FB2C1, 0.55f), 2);
    attr.max_width = 700;
    ui_.text(attr, text, w - 44 - 700, h - 66);
}

void App::draw_flights_hud() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    const auto &flights = hub_.flights();
    if (!flights.empty()) {
        if (flight_focus_ < 0) flight_focus_ = 0;
        if (flight_focus_ >= static_cast<int>(flights.size()))
            flight_focus_ = static_cast<int>(flights.size()) - 1;
    } else {
        flight_focus_ = 0;
    }

    // Aircraft markers first, so panels can cover labels near the edges.
    const float scale = ui_.scale();
    for (size_t i = 0; i < flights.size(); ++i) {
        const Aircraft &aircraft = flights[i];
        const double altitude_m = std::max(0.0, aircraft.altitude_ft * 0.3048);
        const Vec3 point = latlon_to_unit(aircraft.lat, aircraft.lon) *
                           static_cast<float>(kEarthRadius + altitude_m);
        if (!camera_.visible(point)) continue;
        float sx = 0, sy = 0;
        if (!camera_.project(point, sx, sy)) continue;
        sx /= scale;
        sy /= scale;
        if (sx < 0 || sy < 140 || sx > w || sy > h - 50) continue;
        const bool selected = static_cast<int>(i) == flight_focus_;
        const float size = selected ? 12.5f : 9.0f;
        Color color = aircraft.on_ground ? kWarm : (aircraft.altitude_ft > 28000 ? kAccent : kBlue);
        if (selected) color = kText;
        const float heading = static_cast<float>(aircraft.heading / 57.2957795);
        const float dir_x = std::sin(heading);
        const float dir_y = -std::cos(heading);
        const float side_x = -dir_y;
        const float side_y = dir_x;
        ui_.circle(sx, sy, selected ? 10.0f : 7.0f, hex(0x03080D, selected ? 0.55f : 0.38f));
        ui_.line(sx - dir_x * size * 0.8f, sy - dir_y * size * 0.8f, sx + dir_x * size,
                 sy + dir_y * size, selected ? 2.4f : 1.5f, color);
        ui_.line(sx - side_x * size * 0.55f, sy - side_y * size * 0.55f,
                 sx + side_x * size * 0.55f, sy + side_y * size * 0.55f, 1.1f,
                 with_alpha(color, 0.85f));
        ui_.circle(sx, sy, selected ? 3.4f : 2.2f, color);
        if (selected) {
            ui_.ring(sx, sy, 16, 1.2f, with_alpha(kAccent, 0.9f));
        }
        if (settings_.labels && (selected || i < 10)) {
            const std::string label = aircraft.callsign.empty() ? aircraft.hex : aircraft.callsign;
            const float tw = ui_.text_width(ui_.font_semibold(), selected ? 11.5f : 9.5f, label);
            ui_.rounded_rect({sx + 9, sy - 20, tw + 10, 17}, 8, hex(0x03080D, 0.62f));
            ui_.text(style(ui_.font_semibold(), selected ? 11.5f : 9.5f,
                           selected ? kText : with_alpha(kText, 0.9f)),
                     label, sx + 14, sy - 18);
        }
        add_hit({sx - 13, sy - 13, 26, 26}, [this, i] { flight_focus_ = static_cast<int>(i); });
    }

    label_rects_.push_back({0, 0, w, 138});
    label_rects_.push_back({44, 142, 398, 512});
    label_rects_.push_back({44, 668, 398, 232});
    label_rects_.push_back({0, h - 154, w, 154});
    if (settings_.labels) draw_city_labels();
    const Rect list(44, 142, 398, 512);
    ui_.panel(list, 22, kPanel, kBorder);
    draw_screen_title("LIVE AIRSPACE", settings_.home_name,
                      flight_world_ ? "World snapshot · OpenSky" : "Local radar · adsb.lol",
                      list.x + 22, list.y + 18);
    const bool loading = hub_.status().flights_loading;
    const Color status_color = hub_.status().flights_live ? kAccent : (loading ? kWarm : kDanger);
    ui_.circle(list.x + list.w - 28, list.y + 33, 3.0f, status_color);
    if (loading)
        ui_.ring(list.x + list.w - 28, list.y + 33, 7 + std::sin(frame_phase_ * 3.0f) * 1.2f,
                 1.0f, with_alpha(status_color, 0.5f));
    TextStyle count = style(ui_.font_semibold(), 10, kMuted, 2);
    count.max_width = 120;
    ui_.text(count, std::to_string(flights.size()) + " AIRCRAFT", list.x + list.w - 42,
             list.y + 27);

    const Rect mode(list.x + 22, list.y + 96, 188, 32);
    ui_.rounded_rect(mode, 15, hex(0x0A1B22, 0.9f));
    if (flight_world_)
        ui_.rounded_rect({mode.x + 94, mode.y + 3, 90, 26}, 13, hex(0x23313F, 0.95f));
    else
        ui_.rounded_rect({mode.x + 3, mode.y + 3, 90, 26}, 13, hex(0x123B3C, 0.95f));
    TextStyle mode_style = style(ui_.font_semibold(), 10,
                                 flight_world_ ? kMuted : kAccent, 1);
    mode_style.max_width = 90;
    ui_.text(mode_style, "LOCAL", mode.x + 3, mode.y + 10);
    mode_style.color = flight_world_ ? kText : kMuted;
    ui_.text(mode_style, "WORLD", mode.x + 94, mode.y + 10);
    add_hit(mode, [this] {
        flight_world_ = !flight_world_;
        camera_.set_focus(settings_.home_lat, settings_.home_lon, flight_world_ ? 12000000.0 : 2200000.0);
        hub_.request_flights(settings_.home_lat, settings_.home_lon, flight_world_);
        toast(flight_world_ ? "World traffic · 10 minute snapshot" : "Local radar · 30 second refresh");
    });
    ui_.icon("refresh-cw", list.x + list.w - 26, list.y + 92, 15, kMuted);
    add_hit({list.x + list.w - 44, list.y + 76, 36, 32}, [this] {
        hub_.request_flights(camera_.lat, camera_.lon, flight_world_);
        toast("Rescanning airspace");
    });

    const float list_y = list.y + 138;
    ui_.push_clip({list.x + 10, list_y, list.w - 20, 376});
    if (flights.empty()) {
        ui_.spinner(list.x + list.w * 0.5f, list_y + 90, 13, frame_phase_ * 2.8f, kAccent);
        ui_.text(style(ui_.font_medium(), 13, kMuted, 1), loading ? "Scanning the airspace…" : "No aircraft in range",
                 list.x + list.w * 0.5f - 200, list_y + 124);
        ui_.text(style(ui_.font_ui(), 10, kDim, 1), "Move the map and press R1 to rescan",
                 list.x + list.w * 0.5f - 200, list_y + 146);
    } else {
        const int visible = 8;
        int first = std::max(0, flight_focus_ - visible + 2);
        for (int i = 0; i < visible; ++i) {
            const int index = first + i;
            if (index >= static_cast<int>(flights.size())) break;
            const Aircraft &aircraft = flights[index];
            const Rect row(list.x + 10, list_y + i * 46, list.w - 20, 42);
            if (index == flight_focus_) {
                ui_.rounded_rect(row, 10, hex(0x123B3C, 0.72f));
                ui_.rounded_rect({row.x, row.y + 8, 2, 26}, 1, kAccent);
            }
            const std::string callsign = aircraft.callsign.empty() ? aircraft.hex : aircraft.callsign;
            ui_.text(style(ui_.font_semibold(), 12.5f, index == flight_focus_ ? kText : with_alpha(kText, 0.82f)),
                     ellipsize(ui_.font_semibold(), 12.5f, callsign, 130), row.x + 12, row.y + 6);
            const std::string sub = aircraft.on_ground ? "ON GROUND"
                                    : number(aircraft.altitude_ft) + " ft · " + number(aircraft.speed_kt) + " kt";
            ui_.text(style(ui_.font_ui(), 9.5f, kMuted), sub, row.x + 12, row.y + 23);
            ui_.text(style(ui_.font_ui(), 9.5f, kDim, 2),
                     aircraft.nearest_city.empty() ? aircraft.country : aircraft.nearest_city,
                     row.x + row.w - 68, row.y + 6);
            ui_.text(style(ui_.font_ui(), 9, kDim, 2), distance_string(aircraft.distance_km),
                     row.x + row.w - 14, row.y + 6);
            add_hit(row, [this, index] { flight_focus_ = index; });
        }
    }
    ui_.pop_clip();

    if (!flights.empty()) {
        const Aircraft &aircraft = flights[flight_focus_];
        const Rect detail(list.x, list.y + list.h + 14, list.w, 232);
        ui_.panel(detail, 20, kPanel, kBorder);
        const std::string callsign = aircraft.callsign.empty() ? aircraft.hex : aircraft.callsign;
        ui_.text(style(ui_.font_display_bold(), 24, kText), callsign, detail.x + 20, detail.y + 16);
        ui_.rounded_rect({detail.x + detail.w - 116, detail.y + 16, 96, 22}, 11,
                         hex(0x123B3C, 0.8f));
        TextStyle badge = style(ui_.font_semibold(), 8.5f, kAccent, 1);
        badge.max_width = 96;
        ui_.text(badge, flight_world_ ? "WORLD" : "ADS-B", detail.x + detail.w - 116,
                 detail.y + 23);
        const std::string city = aircraft.nearest_city.empty() ? aircraft.country
                                                                : aircraft.nearest_city + ", " + aircraft.nearest_country;
        ui_.text(style(ui_.font_ui(), 10.5f, kMuted), city, detail.x + 20, detail.y + 48);
        const char *labels[] = {"ALTITUDE", "GROUND SPEED", "HEADING", "VERTICAL RATE", "SQUAWK", "DISTANCE"};
        const std::string values[] = {
            aircraft.on_ground ? "On ground" : number(aircraft.altitude_ft) + " ft",
            number(aircraft.speed_kt) + " kt",
            number(aircraft.heading) + "°",
            (aircraft.vertical_rate >= 0 ? "↑ " : "↓ ") + number(std::fabs(aircraft.vertical_rate)) + " fpm",
            aircraft.squawk.empty() ? "—" : aircraft.squawk,
            distance_string(aircraft.distance_km),
        };
        for (int i = 0; i < 6; ++i) {
            const int col = i % 3;
            const int row = i / 3;
            const float x = detail.x + 20 + col * 122;
            const float y = detail.y + 78 + row * 62;
            ui_.text(style(ui_.font_semibold(), 7.5f, kDim), labels[i], x, y);
            ui_.text(style(ui_.font_semibold(), 13, kText), values[i], x, y + 15);
        }
        ui_.text(style(ui_.font_ui(), 8.5f, kDim),
                 "ADS-B position reports are not flight plans; routes are inferred.", detail.x + 20,
                 detail.y + detail.h - 24);
    }
    ui_.gradient_rect({0, h - 160, w, 160}, hex(0x02050A, 0.0f), hex(0x02050A, 0.92f));
    draw_controller_prompts();
    draw_attribution();
    if (screen_fade_ < 1.0f)
        ui_.rect({0, 0, w, h}, hex(0x000000, (1.0f - screen_fade_) * 0.55f));
}

void App::draw_satellite_markers() {
    const auto indices = filtered_satellites();
    const float scale = ui_.scale();
    const Satellite *selected = selected_satellite();
    for (int index : indices) {
        const Satellite &sat = hub_.satellites()[index];
        if (!sat.has_tle || !camera_.visible(sat.ecef)) continue;
        float sx = 0, sy = 0;
        if (!camera_.project(sat.ecef, sx, sy)) continue;
        sx /= scale;
        sy /= scale;
        if (sx < 0 || sy < 138 || sx > ui_.virtual_width() || sy > ui_.virtual_height() - 50) continue;
        const bool is_selected = selected && selected->norad == sat.norad;
        const Color color = category_color(sat.category);
        const float size = is_selected ? 4.0f : (sat.overhead ? 2.6f : 1.8f);
        ui_.circle(sx, sy, size, is_selected ? kText : color);
        if (is_selected) {
            ui_.ring(sx, sy, 12, 1.1f, with_alpha(color, 0.9f));
            ui_.text_shadowed(style(ui_.font_semibold(), 11, kText), sat.name, sx + 16, sy - 7,
                              hex(0x000000, 0.8f));
        } else if (sat.overhead && settings_.labels) {
            ui_.text_shadowed(style(ui_.font_medium(), 9, with_alpha(kText, 0.78f)), sat.name,
                              sx + 8, sy - 5, hex(0x000000, 0.75f));
        }
        if (is_selected) add_hit({sx - 14, sy - 14, 28, 28}, [this, index] { hub_.select_satellite(index); });
    }
}

gfx::Color App::category_color(const std::string &category) const {
    if (category == "Station") return hex(0x5EEAD4);
    if (category == "Weather") return hex(0x7DB8FF);
    if (category == "Navigation") return hex(0xF4D06F);
    if (category == "Science") return hex(0xC49BFF);
    return hex(0xA3B3C2);
}

void App::draw_satellite_detail(const Rect &rect) {
    ui_.panel(rect, 20, kPanel, kBorder);
    const Satellite *sat = selected_satellite();
    if (!sat) {
        ui_.text(style(ui_.font_medium(), 13, kMuted, 1), "No satellites match this filter",
                 rect.x, rect.y + 32);
        return;
    }
    const Color category = category_color(sat->category);
    ui_.rounded_rect({rect.x + 20, rect.y + 20, 3, 32}, 1.5f, category);
    ui_.text(style(ui_.font_semibold(), 8.5f, kMuted), "SELECTED OBJECT", rect.x + 32, rect.y + 18);
    ui_.text(style(ui_.font_display_bold(), 21, kText),
             ellipsize(ui_.font_display_bold(), 21, sat->name, rect.w - 48), rect.x + 32,
             rect.y + 30);
    ui_.text(style(ui_.font_ui(), 10.5f, kMuted),
             sat->country + " · NORAD " + std::to_string(sat->norad), rect.x + 32, rect.y + 58);
    ui_.rounded_rect({rect.x + rect.w - 106, rect.y + 24, 86, 22}, 11, with_alpha(category, 0.15f));
    TextStyle category_style = style(ui_.font_semibold(), 8, category, 1);
    category_style.max_width = 86;
    ui_.text(category_style, sat->category, rect.x + rect.w - 106, rect.y + 31);

    const char *labels[] = {"ALTITUDE", "VELOCITY", "PERIOD", "INCLINATION", "LAUNCH", "ORBIT"};
    const std::string values[] = {
        number(sat->altitude_km) + " km", number(sat->velocity_km_s, 2) + " km/s",
        number(sat->period_minutes, 1) + " min", number(sat->inclination, 1) + "°",
        sat->launch_date.empty() ? "—" : sat->launch_date.substr(0, 10), sat->orbit_class};
    for (int i = 0; i < 6; ++i) {
        const int col = i % 3;
        const int row = i / 3;
        const float x = rect.x + 20 + col * 116;
        const float y = rect.y + 90 + row * 54;
        ui_.text(style(ui_.font_semibold(), 7.5f, kDim), labels[i], x, y);
        ui_.text(style(ui_.font_semibold(), 11.5f, kText), values[i], x, y + 13);
    }
    const std::string above = sat->above_city.empty()
                                  ? "Over open ocean or remote terrain"
                                  : "Over " + sat->above_city + ", " + sat->above_country;
    ui_.text(style(ui_.font_ui(), 10, kMuted), above, rect.x + 20, rect.y + 206);
    ui_.text(style(ui_.font_semibold(), 8, sat->sunlit ? kWarm : kBlue),
             sat->sunlit ? "SUNLIT" : "IN EARTH SHADOW", rect.x + 20, rect.y + 228);
    ui_.text(style(ui_.font_ui(), 9, kDim, 2),
             "ELEV " + number(sat->elevation_home) + "° FROM HOME", rect.x + rect.w - 20,
             rect.y + 228);

    ui_.rounded_rect({rect.x + 18, rect.y + 250, rect.w - 36, 1}, 0.5f, hex(0xFFFFFF, 0.06f));
    ui_.text(style(ui_.font_semibold(), 8, kMuted), "NEXT PASS OVER " + settings_.home_name,
             rect.x + 20, rect.y + 264);
    if (settings_.show_orbits) {
        // A small orbital sweep animation gives the card life without clutter.
        ui_.arc(rect.x + rect.w - 42, rect.y + 282, 18, frame_phase_ * 0.8f,
                frame_phase_ * 0.8f + 4.0f, 1.2f, with_alpha(category, 0.55f), 26);
        ui_.circle(rect.x + rect.w - 24, rect.y + 276, 2.4f, category);
    }
    if (selected_satellite()) {
        double rise = 0, peak = 0, set = 0, peak_elevation = 0;
        static int cached_norad = -1;
        static double cached_at = -1;
        static bool cached_ok = false;
        static double cached_rise = 0, cached_peak_time = 0, cached_set = 0, cached_elevation = 0;
        const double now = platform::monotonic_seconds();
        if (cached_norad != sat->norad || now - cached_at > 25.0) {
            cached_ok = hub_.selected_pass(sky_time_, rise, peak, set, peak_elevation);
            cached_norad = sat->norad;
            cached_at = now;
            cached_rise = rise;
            cached_peak_time = peak;
            cached_set = set;
            cached_elevation = peak_elevation;
        }
        if (cached_ok) {
            ui_.text(style(ui_.font_semibold(), 11, kText),
                     "Rise " + time_string(cached_rise, true), rect.x + 20, rect.y + 286);
            ui_.text(style(ui_.font_ui(), 9.5f, kMuted),
                     "Peak " + number(cached_elevation) + "° at " + time_string(cached_peak_time, true) +
                         " · sets " + time_string(cached_set, true),
                     rect.x + 20, rect.y + 304);
        } else {
            if (sat->period_minutes > 1400.0 && sat->elevation_home > 0.0) {
                ui_.text(style(ui_.font_ui(), 9.5f, kMuted),
                         "Geostationary — continuously above the horizon",
                         rect.x + 20, rect.y + 286);
            } else {
                ui_.text(style(ui_.font_ui(), 9.5f, kMuted), "No pass above 10° in the next 24 hours",
                         rect.x + 20, rect.y + 286);
            }
        }
    }
    ui_.text(style(ui_.font_ui(), 8, kDim, 2), "CELESTRAK · SGP4", rect.x + rect.w - 20,
             rect.y + rect.h - 22);
}

void App::draw_satellites_hud() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    draw_satellite_markers();
    const auto indices = filtered_satellites();
    if (!indices.empty()) {
        satellite_focus_ = std::max(0, std::min(static_cast<int>(indices.size()) - 1, satellite_focus_));
        hub_.select_satellite(indices[satellite_focus_]);
    } else {
        satellite_focus_ = 0;
    }
    const Rect list(44, 142, 398, 560);
    ui_.panel(list, 22, kPanel, kBorder);
    draw_screen_title("ORBITAL", "Satellites", "CelesTrak elements · live propagation", list.x + 22,
                      list.y + 16);
    ui_.circle(list.x + list.w - 28, list.y + 31, 3.0f,
               indices.empty() ? kDanger : kAccent);
    ui_.text(style(ui_.font_semibold(), 10, kMuted, 2), std::to_string(indices.size()) + " SHOWN",
             list.x + list.w - 42, list.y + 25);

    const char *filters[] = {"OVERHEAD", "STATIONS", "WEATHER", "NAV", "SCIENCE", "ALL"};
    float fx = list.x + 22;
    for (int i = 0; i < 6; ++i) {
        const float width = i == 0 ? 76 : i == 1 ? 70 : i == 2 ? 68 : i == 3 ? 44 : i == 4 ? 66 : 40;
        const Rect pill(fx, list.y + 72, width, 24);
        const bool active = satellite_filter_ == i;
        ui_.rounded_rect(pill, 12, active ? hex(0x123B3C, 0.9f) : hex(0xFFFFFF, 0.045f));
        ui_.text(style(ui_.font_semibold(), 7.5f, active ? kAccent : kMuted, 1), filters[i],
                 pill.x, pill.y + 8);
        add_hit(pill, [this, i] {
            satellite_filter_ = i;
            satellite_focus_ = 0;
            toast(std::string("Filter: ") + active_satellite_category());
        });
        fx += width + 6;
    }

    const float list_y = list.y + 110;
    ui_.push_clip({list.x + 10, list_y, list.w - 20, 410});
    if (indices.empty()) {
        ui_.text(style(ui_.font_medium(), 13, kMuted, 1), "No satellites in this filter",
                 list.x, list_y + 60);
    } else {
        const int visible = 8;
        const int first = std::max(0, satellite_focus_ - visible + 2);
        for (int i = 0; i < visible; ++i) {
            const int row_index = first + i;
            if (row_index >= static_cast<int>(indices.size())) break;
            const Satellite &sat = hub_.satellites()[indices[row_index]];
            const Rect row(list.x + 10, list_y + i * 49, list.w - 20, 45);
            if (row_index == satellite_focus_) {
                ui_.rounded_rect(row, 10, hex(0x123B3C, 0.72f));
                ui_.rounded_rect({row.x, row.y + 8, 2, 28}, 1, kAccent);
            }
            ui_.circle(row.x + 18, row.y + 22, 3.0f, category_color(sat.category));
            ui_.text(style(ui_.font_semibold(), 11.5f, row_index == satellite_focus_ ? kText : with_alpha(kText, 0.82f)),
                     ellipsize(ui_.font_semibold(), 11.5f, sat.name, 190), row.x + 30, row.y + 7);
            ui_.text(style(ui_.font_ui(), 9, kMuted),
                     sat.country + " · " + sat.category, row.x + 30, row.y + 24);
            ui_.text(style(ui_.font_semibold(), 10, kText, 2), number(sat.altitude_km) + " km",
                     row.x + row.w - 14, row.y + 7);
            ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2),
                     sat.overhead ? "OVERHEAD" : "ELEV " + number(sat.elevation_home) + "°",
                     row.x + row.w - 14, row.y + 24);
            add_hit(row, [this, row_index] {
                satellite_focus_ = row_index;
                const auto idx = filtered_satellites();
                if (row_index < static_cast<int>(idx.size())) hub_.select_satellite(idx[row_index]);
            });
        }
    }
    ui_.pop_clip();

    ui_.text(style(ui_.font_ui(), 8.5f, kDim), "TLE EPOCH " + hub_.tle_epoch_text(), list.x + 22,
             list.y + list.h - 24);

    const Rect detail(w - 430, 142, 386, 432);
    draw_satellite_detail(detail);
    draw_controller_prompts();
    draw_attribution();
    if (screen_fade_ < 1.0f)
        ui_.rect({0, 0, w, h}, hex(0x000000, (1.0f - screen_fade_) * 0.55f));
}

void App::draw_ambient() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    ui_.gradient_rect({0, 0, w, 170}, hex(0x02050A, 0.55f), hex(0x02050A, 0.0f));
    TextStyle clock = style(ui_.font_display_bold(), 46, hex(0xF5FBFF, 0.92f), 2);
    clock.max_width = 420;
    ui_.text(clock, current_clock(), w - 44 - 400, 26);
    TextStyle home = style(ui_.font_medium(), 11, hex(0xA9BECD, 0.72f), 2);
    home.max_width = 420;
    ui_.text(home, settings_.home_name + " · " + settings_.home_country, w - 44 - 400, 82);
    if (hub_.point().valid) {
        TextStyle temp = style(ui_.font_display_bold(), 34, hex(0xF5FBFF, 0.9f));
        ui_.text(temp, temperature(hub_.point().temperature), 44, h - 114);
        TextStyle condition = style(ui_.font_medium(), 13, hex(0xB9CDDA, 0.75f));
        ui_.text(condition, weather_condition(hub_.point().weather_code), 44, h - 68);
        ui_.icon(weather_icon(hub_.point().weather_code, hub_.point().is_day != 0), 44 + 8,
                 h - 140, 32, hex(0xE9F6FF, 0.7f));
    }
    TextStyle mark = style(ui_.font_display_bold(), 12, hex(0xFFFFFF, 0.35f));
    mark.letter_spacing = 3.0f;
    ui_.text(mark, "NIMBUS", 44, h - 44);
    ui_.text(style(ui_.font_ui(), 9, hex(0xFFFFFF, 0.22f)), "LIVE EARTH", 126, h - 43);
}

void App::draw_boot() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    ui_.rect({0, 0, w, h}, hex(0x03070C));
    const float cx = w * 0.5f;
    const float cy = h * 0.46f;
    const float t = static_cast<float>(std::min(1.0, boot_elapsed_ / 1.2));
    const float alpha = std::min(1.0f, t * 2.0f);
    ui_.ring(cx, cy - 42, 58 + std::sin(frame_phase_ * 1.6f) * 2, 1.0f,
             with_alpha(kAccent, 0.20f * alpha));
    ui_.ring(cx, cy - 42, 43, 1.4f, with_alpha(kAccent, 0.72f * alpha));
    ui_.arc(cx, cy - 42, 52, -0.3f + frame_phase_ * 0.4f, 1.5f, 1.2f,
            with_alpha(kAccent, 0.30f * alpha), 30);
    ui_.circle(cx, cy - 42, 8, with_alpha(kAccent, 0.9f * alpha));
    ui_.line(cx - 58, cy - 29, cx + 58, cy - 55, 1.1f, with_alpha(kAccent, 0.45f * alpha));
    ui_.circle(cx + 61, cy - 56, 2.8f, with_alpha(hex(0xE9FFFC), alpha));
    TextStyle title = style(ui_.font_display_bold(), 48, with_alpha(kText, alpha));
    title.letter_spacing = 6.0f;
    title.align = 1;
    title.max_width = w;
    ui_.text(title, "NIMBUS", 0, cy + 38);
    TextStyle sub = style(ui_.font_medium(), 10, with_alpha(kAccent, 0.75f * alpha));
    sub.letter_spacing = 4.5f;
    sub.align = 1;
    sub.max_width = w;
    ui_.text(sub, "LIVE EARTH", 0, cy + 100);
    const float progress = std::min(1.0f, static_cast<float>(boot_elapsed_ / 2.2));
    ui_.rounded_rect({cx - 110, cy + 142, 220, 2}, 1, hex(0xFFFFFF, 0.07f));
    ui_.rounded_rect({cx - 110, cy + 142, 220 * progress, 2}, 1, kAccent);
    ui_.text(style(ui_.font_ui(), 8.5f, kDim, 1), "ASSEMBLING THE PLANET · OPEN DATA", 0, cy + 164);
}

void App::draw_toast() {
    if (toast_text_.empty() || platform::monotonic_seconds() > toast_until_) return;
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    const float width = ui_.text_width(ui_.font_medium(), 11.5f, toast_text_) + 44;
    const Rect pill(w * 0.5f - width * 0.5f, h - 108, width, 34);
    ui_.panel(pill, 17, hex(0x08131C, 0.94f), hex(0x2C4052, 0.7f));
    ui_.circle(pill.x + 17, pill.y + 17, 3.0f, kAccent);
    ui_.text(style(ui_.font_medium(), 11.5f, kText), toast_text_, pill.x + 27, pill.y + 9);
}

}  // namespace nimbus
