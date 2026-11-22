#include "app/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/astro.hpp"
#include "gfx/math.hpp"

namespace nimbus {

using namespace nimbus::gfx;

namespace {
const Color kText = hex(0xF1F7FF);
const Color kMuted = hex(0x8EA3B5);
const Color kDim = hex(0x5A7083);
const Color kAccent = hex(0x5EEAD4);
const Color kBorder = hex(0x2C4052, 0.55f);
const Color kPanel = hex(0x0D1B26, 0.94f);
const Color kBlue = hex(0x60A5FA);
const Color kWarm = hex(0xFFB86B);


std::string compact_population_setting(int population) {
    char buffer[32];
    if (population >= 1000000) std::snprintf(buffer, sizeof(buffer), "%.1fM", population / 1000000.0);
    else if (population >= 1000) std::snprintf(buffer, sizeof(buffer), "%.0fk", population / 1000.0);
    else std::snprintf(buffer, sizeof(buffer), "%d", population);
    return buffer;
}

TextStyle style(int font, float size, Color color, int align = 0) {
    TextStyle out;
    out.font = font;
    out.size = size;
    out.color = color;
    out.align = align;
    return out;
}

const char *kSettingLabels[] = {
    "Home location", "Map style", "Temperature", "Wind speed", "Distance",
    "Clock", "Default layer", "Wind streamlines", "Borders", "Clouds",
    "City labels", "Auto orbit", "Ambient mode", "Bloom", "Render quality",
    "Performance overlay", "Data saver", "Refresh live data", "Controls & help",
    "Data sources",
};

const char *kSettingDescriptions[] = {
    "Where every launch begins", "Satellite, atlas or streets", "Celsius or Fahrenheit",
    "km/h or mph", "Kilometres or miles", "24-hour or 12-hour", "Weather on launch",
    "Animated 10 m flow", "Country outlines", "Real cloud layer", "Names and temperatures",
    "Slow drift when idle", "Screensaver timeout", "Cinematic glow", "Resolution and effects",
    "Frame rate overlay", "Longer refresh intervals", "Reload weather now", "Controller map",
    "Credits and licenses",
};

const char *kSettingIcons[] = {
    "map-pin", "layers", "thermometer", "wind", "navigation", "clock", "gauge",
    "wind", "globe", "cloud", "map-pin", "orbit", "moon", "sun", "sliders-horizontal",
    "activity", "radio", "refresh-cw", "info", "github",
};
}  // namespace

void App::draw_settings_hud() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    ui_.rect({0, 0, w, h}, hex(0x02060B, 0.80f));
    ui_.rounded_rect_gradient({0, 0, w, 280}, 0, hex(0x03080E, 0.94f), hex(0x03080E, 0.0f));
    draw_brand(44, 26, 1.0f);
    ui_.text(style(ui_.font_semibold(), 9.5f, kAccent), "CONTROL ROOM", 44, 96);
    ui_.text(style(ui_.font_display_bold(), 35, kText), "Settings", 44, 114);
    ui_.text(style(ui_.font_ui(), 12, kMuted),
             "A planet-scale instrument should feel like your own. Tune it.", 44, 161);
    ui_.text(style(ui_.font_ui(), 9.5f, kDim, 2), "OPTIONS  ·  BACK", w - 44, 30);

    const float panel_top = 214;
    const float panel_h = 650;
    const Rect left_panel(56, panel_top, w * 0.5f - 84, panel_h);
    const Rect right_panel(w * 0.5f + 28, panel_top, w * 0.5f - 84, panel_h);
    ui_.panel(left_panel, 22, hex(0x08121B, 0.84f), kBorder);
    ui_.panel(right_panel, 22, hex(0x08121B, 0.84f), kBorder);
    ui_.text(style(ui_.font_semibold(), 8.5f, kDim), "PLANET · MAP · HOME", left_panel.x + 20,
             panel_top + 16);
    ui_.text(style(ui_.font_semibold(), 8.5f, kDim), "EXPERIENCE · DATA", right_panel.x + 20,
             panel_top + 16);

    const float top = panel_top + 42;
    const float row_h = 54;
    for (int i = 0; i < SetCount; ++i) {
        const int column = i / 10;
        const int row = i % 10;
        const Rect panel = column == 0 ? left_panel : right_panel;
        const float x = panel.x + 12;
        const float y = top + row * row_h;
        const Rect rect(x, y, panel.w - 24, row_h - 6);
        const bool active = i == settings_focus_;
        if (active) {
            ui_.rounded_rect(rect, 12, hex(0x123B3C, 0.66f));
            ui_.rounded_rect({rect.x, rect.y + 10, 2.5f, rect.h - 20}, 1.25f, kAccent);
        }
        ui_.icon(kSettingIcons[i], rect.x + 22, rect.y + rect.h * 0.5f, 17,
                 active ? kAccent : kMuted);
        ui_.text(style(ui_.font_medium(), 13.5f, active ? kText : with_alpha(kText, 0.84f)),
                 kSettingLabels[i], rect.x + 42, rect.y + 7);
        ui_.text(style(ui_.font_ui(), 9.5f, active ? with_alpha(kMuted, 0.95f) : kDim),
                 kSettingDescriptions[i], rect.x + 42, rect.y + 27);
        const std::string value = setting_value(static_cast<SettingId>(i));
        ui_.text(style(ui_.font_ui(), 12, active ? with_alpha(kAccent, 0.98f) : kMuted),
                 ellipsize(ui_.font_ui(), 12, value, 200), rect.x + 330, rect.y + 10);
        if (i == SetHome || i == SetRefresh || i == SetHelp || i == SetCredits) {
            ui_.icon("chevron-right", rect.x + rect.w - 18, rect.y + rect.h * 0.5f, 13, kDim);
        } else {
            ui_.icon("chevron-left", rect.x + rect.w - 44, rect.y + rect.h * 0.5f, 12, kDim);
            ui_.icon("chevron-right", rect.x + rect.w - 18, rect.y + rect.h * 0.5f, 12,
                     active ? kAccent : kDim);
        }
        add_hit(rect, [this, i] {
            settings_focus_ = i;
            activate_setting(static_cast<SettingId>(i));
        });
    }

    const float footer = h - 60;
    draw_prompt(64, footer, "cross", "Change");
    draw_prompt(192, footer, "dpad", "Navigate");
    draw_prompt(326, footer, "l1", "Adjust");
    draw_prompt(444, footer, "circle", "Back");
    draw_prompt(560, footer, "triangle", "Controls");
    const std::string status = online_ ? (hub_.status().weather_live ? "LIVE · data current" : "SYNCING · first data")
                                       : "OFFLINE · cached tiles only";
    ui_.text(style(ui_.font_ui(), 9.5f, kDim, 2), status, w - 64, footer + 2);
    ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2),
             "NIMBUS 01.000.000 · PPSA42716", w - 64, footer + 20);
}

void App::draw_location_modal() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    ui_.rect({0, 0, w, h}, hex(0x02060B, 0.72f));
    const float panel_w = 1120;
    const float panel_h = 760;
    const Rect panel((w - panel_w) * 0.5f, (h - panel_h) * 0.5f - 18, panel_w, panel_h);
    ui_.panel(panel, 28, kPanel, kBorder);
    ui_.text(style(ui_.font_semibold(), 9.5f, kAccent), "NIMBUS · HOME",
             panel.x + 36, panel.y + 30);
    ui_.text(style(ui_.font_display_bold(), 34, kText),
             location_for_home_ ? "Choose your home sky" : "Fly somewhere", panel.x + 36,
             panel.y + 48);
    ui_.text(style(ui_.font_ui(), 12.5f, kMuted),
             "Every launch begins here. You can change it any time from Settings.", panel.x + 36,
             panel.y + 92);

    const Rect search(panel.x + 36, panel.y + 126, 430, 44);
    ui_.rounded_rect(search, 14, hex(0xFFFFFF, 0.045f));
    ui_.ring(search.cx() - search.h * 0.5f + 18, search.cy() + 1, 7, 1.2f, kDim);
    ui_.line(search.cx() - search.h * 0.5f + 13, search.cy() + 6, search.cx() - search.h * 0.5f + 19,
             search.cy() + 12, 1.2f, kDim);
    const float query_x = search.x + 38;
    const bool empty = location_query_.empty();
    ui_.text(style(ui_.font_medium(), 14, empty ? kDim : kText),
             empty ? "Search cities, countries…" : location_query_, query_x, search.y + 13);
    if (location_keyboard_) {
        const float cursor = std::fmod(static_cast<float>(frame_phase_ * 2.4), 2.0f) < 1.0f ? 1.0f : 0.0f;
        const float qw = ui_.text_width(ui_.font_medium(), 13, location_query_);
        ui_.rounded_rect({query_x + qw + 2, search.y + 12, 1.4f, 18}, 0.7f,
                         with_alpha(kAccent, cursor));
    }
    add_hit(search, [this] {
        location_keyboard_ = true;
        keyboard_x_ = 0;
        keyboard_y_ = 0;
    });

    const float keyboard_x = panel.x + 36;
    const float keyboard_y = panel.y + 190;
    const float key_w = 39;
    const float key_h = 42;
    const char *key_rows[] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM<>_"};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 10; ++col) {
            const Rect key(keyboard_x + col * (key_w + 3), keyboard_y + row * (key_h + 4),
                           key_w, key_h);
            const bool focus = location_keyboard_ && keyboard_x_ == col && keyboard_y_ == row;
            const char c = key_rows[row][col];
            if (focus) {
                ui_.rounded_rect(key, 9, hex(0x123B3C, 0.85f));
                ui_.rounded_rect({key.x, key.y + key.h - 2, key.w, 2}, 1, kAccent);
            } else {
                ui_.rounded_rect(key, 9, hex(0xFFFFFF, 0.045f));
            }
            std::string label(1, c);
            if (c == '<') label = "DEL";
            else if (c == '>') label = "OK";
            else if (c == '_') label = "SPACE";
            TextStyle key_style = style(ui_.font_semibold(), label.size() > 1 ? 8.0f : 13,
                                        focus ? kAccent : with_alpha(kText, 0.78f), 1);
            key_style.max_width = key.w;
            ui_.text(key_style, label, key.x, key.y + (key_h - 15) * 0.5f);
            add_hit(key, [this, c] {
                location_keyboard_ = true;
                if (c == '<') {
                    if (!location_query_.empty()) location_query_.pop_back();
                    refresh_location_results();
                } else if (c == '>') {
                    location_keyboard_ = false;
                } else if (c == '_') {
                    location_query_.push_back(' ');
                    refresh_location_results();
                } else {
                    on_keyboard_character(static_cast<unsigned char>(c));
                }
            });
        }
    }
    ui_.text(style(ui_.font_ui(), 9, kDim), "SQUARE DEL · TRIANGLE RESULTS", keyboard_x,
             keyboard_y + 4 * (key_h + 4) + 8);

    const Rect results(panel.x + 506, panel.y + 126, 578, 560);
    ui_.text(style(ui_.font_semibold(), 8.5f, kDim), "PLACES", results.x, results.y - 20);
    ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2),
             location_query_.empty() ? "Curated starting points" : "Best matches",
             results.x + results.w, results.y - 20);
    if (location_results_.empty()) {
        ui_.text(style(ui_.font_medium(), 12, kMuted, 1), "No place matches that search",
                 results.x, results.y + 30);
    }
    ui_.push_clip({results.x - 8, results.y, results.w + 16, results.h});
    const int visible = 9;
    const int first = std::max(0, location_focus_ - visible + 2);
    for (int i = 0; i < visible; ++i) {
        const int index = first + i;
        if (index >= static_cast<int>(location_results_.size())) break;
        const City *city = location_results_[index];
        const Rect row(results.x, results.y + i * 55, results.w, 50);
        const bool focus = index == location_focus_;
        if (focus) {
            ui_.rounded_rect(row, 12, hex(0x123B3C, 0.66f));
            ui_.rounded_rect({row.x, row.y + 9, 2.4f, 32}, 1.2f, kAccent);
        }
        ui_.icon("map-pin", row.x + 22, row.y + 24, 15, focus ? kAccent : kDim);
        ui_.text(style(ui_.font_semibold(), 14.5f, focus ? kText : with_alpha(kText, 0.86f)),
                 city->name, row.x + 40, row.y + 8);
        ui_.text(style(ui_.font_ui(), 10.5f, kMuted),
                 city->country + (city->admin.empty() ? "" : " · " + city->admin), row.x + 40,
                 row.y + 26);
        ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2), compact_population_setting(city->population),
                 row.x + row.w - 16, row.y + 9);
        ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2), latlon_string(city->lat, city->lon),
                 row.x + row.w - 16, row.y + 26);
        add_hit(row, [this, index] {
            location_focus_ = index;
            if (!location_results_.empty()) {
                const City *choice = location_results_[std::min<int>(location_focus_, location_results_.size() - 1)];
                if (location_for_home_) select_home(*choice);
                else {
                    focus_city(*choice, true);
                    modal_ = ModalNone;
                }
            }
        });
    }
    ui_.pop_clip();

    const float footer_y = panel.y + panel.h - 54;
    ui_.text(style(ui_.font_ui(), 9.5f, kDim), "Physical keyboard works too. Search is offline, city data © Natural Earth.",
             panel.x + 36, footer_y);
    draw_prompt(panel.x + 36, footer_y + 20, "dpad", "Navigate");
    draw_prompt(panel.x + 180, footer_y + 20, "square", "Keyboard");
    draw_prompt(panel.x + 328, footer_y + 20, "cross", "Use place");
    draw_prompt(panel.x + 486, footer_y + 20, "circle", "Close");
    const Rect skip(panel.x + panel.w - 208, footer_y + 8, 172, 36);
    ui_.rounded_rect(skip, 18, hex(0xFFFFFF, 0.06f));
    TextStyle skip_style = style(ui_.font_semibold(), 10, kMuted, 1);
    skip_style.max_width = skip.w;
    ui_.text(skip_style, settings_.first_run ? "USE CURRENT HOME" : "CANCEL", skip.x, skip.y + 12);
    add_hit(skip, [this] {
        if (settings_.first_run) {
            settings_.first_run = false;
            settings_dirty_ = true;
            modal_ = ModalNone;
        } else {
            modal_ = ModalNone;
        }
    });
}

void App::draw_discover_modal() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    ui_.rect({0, 0, w, h}, hex(0x02060B, 0.7f));
    const Rect panel((w - 1220) * 0.5f, (h - 760) * 0.5f - 16, 1220, 760);
    ui_.panel(panel, 28, kPanel, kBorder);
    ui_.text(style(ui_.font_semibold(), 9.5f, kAccent), "FIELD NOTES", panel.x + 36, panel.y + 28);
    ui_.text(style(ui_.font_display_bold(), 34, kText), "Places worth the descent", panel.x + 36,
             panel.y + 46);
    ui_.text(style(ui_.font_ui(), 11.5f, kMuted),
             "Press Cross to fly. The camera arcs out of orbit and dives into the street grid.",
             panel.x + 36, panel.y + 90);

    const auto &destinations = hub_.places().destinations();
    const int columns = 3;
    const int page = std::max(0, std::min(static_cast<int>(destinations.size()) / 6,
                                        discover_focus_ / 6));
    const int start = page * 6;
    const float card_w = 378;
    const float card_h = 256;
    for (int i = 0; i < 6; ++i) {
        const int index = start + i;
        if (index >= static_cast<int>(destinations.size())) break;
        const Destination &destination = destinations[index];
        const int col = i % columns;
        const int row = i / columns;
        const Rect card(panel.x + 36 + col * (card_w + 14), panel.y + 128 + row * (card_h + 14),
                        card_w, card_h);
        const bool focus = index == discover_focus_;
        const Color tint = mix(hex(0x123B3C), hex(0x1B2A43), (index % 5) / 4.0f);
        ui_.rounded_rect_gradient(card, 18, with_alpha(tint, 0.92f), hex(0x0A131C, 0.95f));
        if (focus) {
            ui_.rounded_rect({card.x - 2, card.y - 2, card.w + 4, card.h + 4}, 20, with_alpha(kAccent, 0.9f));
            ui_.rounded_rect({card.x, card.y, card.w, card.h}, 18, with_alpha(tint, 0.98f));
        }
        // A small abstract map: meridians, a coordinate cross and a pin.
        for (int g = 1; g < 5; ++g) {
            ui_.line(card.x + card.w * g / 5.0f, card.y + 16, card.x + card.w * g / 5.0f,
                     card.y + 128, 0.8f, hex(0xFFFFFF, 0.045f));
            ui_.line(card.x + 16, card.y + 16 + 112.0f * g / 5.0f, card.x + card.w - 16,
                     card.y + 16 + 112.0f * g / 5.0f, 0.8f, hex(0xFFFFFF, 0.045f));
        }
        ui_.icon("map-pin", card.x + 28, card.y + 42, 26, with_alpha(kAccent, 0.9f));
        ui_.text(style(ui_.font_semibold(), 23, kText),
                 ellipsize(ui_.font_semibold(), 23, destination.name, card.w - 56),
                 card.x + 24, card.y + 74);
        ui_.text(style(ui_.font_ui(), 10, kAccent, 0), destination.country, card.x + 24,
                 card.y + 104);
        TextStyle description = style(ui_.font_ui(), 11.5f, with_alpha(kText, 0.78f));
        description.max_width = card.w - 48;
        ui_.text(description, destination.description, card.x + 24, card.y + 128);
        ui_.text(style(ui_.font_ui(), 8.5f, kDim), latlon_string(destination.lat, destination.lon),
                 card.x + 24, card.y + card.h - 28);
        ui_.icon("chevron-right", card.x + card.w - 24, card.y + card.h - 22, 12,
                 focus ? kAccent : kDim);
        add_hit(card, [this, index] {
            discover_focus_ = index;
            const auto &list = hub_.places().destinations();
            const Destination &d = list[index];
            modal_ = ModalNone;
            screen_ = ScreenGlobe;
            fly_to(d.lat, d.lon, settings_.map_style == 0 ? 16.0 : 2.6,
                   0.0, 5.0);
            hub_.request_point(d.lat, d.lon, d.name);
            toast(std::string("Diving into ") + d.name);
        });
    }

    const float footer_y = panel.y + panel.h - 52;
    draw_prompt(panel.x + 36, footer_y, "cross", "Fly here");
    draw_prompt(panel.x + 178, footer_y, "dpad", "Browse");
    draw_prompt(panel.x + 306, footer_y, "l1", "Page");
    draw_prompt(panel.x + 416, footer_y, "square", "Surprise me");
    draw_prompt(panel.x + 576, footer_y, "circle", "Close");
    ui_.text(style(ui_.font_ui(), 9, kDim, 2),
             std::to_string(page + 1) + " / " + std::to_string((destinations.size() + 5) / 6),
             panel.x + panel.w - 36, footer_y + 4);
}

void App::draw_help_modal() {
    const float w = ui_.virtual_width();
    const float h = ui_.virtual_height();
    ui_.rect({0, 0, w, h}, hex(0x02060B, 0.76f));
    const Rect panel((w - 1120) * 0.5f, (h - 740) * 0.5f - 10, 1120, 740);
    ui_.panel(panel, 28, kPanel, kBorder);
    const bool credits = modal_ == ModalCredits;
    ui_.text(style(ui_.font_semibold(), 9.5f, kAccent), credits ? "TRANSPARENCY" : "DUALSENSE",
             panel.x + 36, panel.y + 28);
    ui_.text(style(ui_.font_display_bold(), 30, kText), credits ? "Data sources & credits" : "Controls & craft",
             panel.x + 36, panel.y + 46);
    ui_.text(style(ui_.font_ui(), 11.5f, kMuted),
             credits ? "Nimbus is built from open data and open source. Nothing is hidden."
                     : "Built to be read at 10 feet. Every action is reachable from the controller.",
             panel.x + 36, panel.y + 90);

    const float col1 = panel.x + 42;
    const float col2 = panel.x + 576;
    float y1 = panel.y + 132;
    float y2 = panel.y + 132;
    if (!credits) {
        struct Row { const char *glyph; const char *label; };
        const Row rows1[] = {
            {"dpad", "Orbit the Earth / move the map"},
            {"r2", "Zoom · triggers with resistance"},
            {"cross", "Dive to the city under the reticle"},
            {"circle", "Pull back to orbit"},
            {"l1", "Switch Earth / Flights / Satellites"},
            {"square", "Toggle wind streamlines"},
            {"left", "Cycle weather layer"},
            {"l3", "Hold + right stick to scrub time"},
            {"r3", "Surprise destination"},
            {"options", "Open this control room"},
        };
        const Row rows2[] = {
            {"dpad", "D-pad up/down changes map style"},
            {"touchpad", "Return home"},
            {"triangle", "Clean view / hide interface"},
            {"l2", "Zoom out"},
            {"plane", "Flights: D-pad selects, Square local/world"},
            {"satellite", "Satellites: L/R filters, Triangle orbits"},
            {"moon", "Ambient: leave the controller alone"},
            {"clock", "L3 then right stick: time-lapse"},
            {"search", "Location search supports a keyboard"},
            {"info", "Data: Open-Meteo, EOX, OSM, Celestrak, adsb.lol"},
        };
        auto draw_rows = [&](const Row *rows, int count, float x, float &y) {
            for (int i = 0; i < count; ++i) {
                if (std::string(rows[i].glyph) == "plane" || std::string(rows[i].glyph) == "satellite" ||
                    std::string(rows[i].glyph) == "moon" || std::string(rows[i].glyph) == "clock" ||
                    std::string(rows[i].glyph) == "search" || std::string(rows[i].glyph) == "info") {
                    ui_.circle(x + 10, y + 10, 10, hex(0xFFFFFF, 0.06f));
                    ui_.icon(rows[i].glyph, x + 10, y + 10, 12, kAccent);
                } else {
                    draw_prompt(x, y, rows[i].glyph, "");
                }
                ui_.text(style(ui_.font_medium(), 11.5f, kText), rows[i].label, x + 32, y + 2);
                y += 43;
            }
        };
        draw_rows(rows1, 10, col1, y1);
        draw_rows(rows2, 10, col2, y2);
        ui_.rounded_rect({panel.x + 36, panel.y + panel.h - 96, panel.w - 72, 1}, 0.5f,
                         hex(0xFFFFFF, 0.06f));
        ui_.text(style(ui_.font_ui(), 9.5f, kDim),
                 "Tip: click a city label with the mouse. The right stick tilts the camera once you are below 260 km.",
                 panel.x + 36, panel.y + panel.h - 76);
    } else {
        const char *lines1[] = {
            "Earth imagery — Solar System Scope (CC BY 4.0), based on NASA data.",
            "City imagery — Sentinel-2 cloudless 2025 by EOX IT Services (CC BY 4.0).",
            "Atlas — Terrain Light by EOX::Maps, data © OpenStreetMap contributors.",
            "Streets & labels — © OpenStreetMap contributors, rendered by EOX.",
            "Weather grid & point forecasts — Open-Meteo (CC BY 4.0).",
            "Live flights — adsb.lol community ADS-B network.",
            "World flight snapshot — OpenSky Network (non-commercial research).",
            "Orbital elements — CelesTrak GP data; SGP4 by CSSI / A. Holinch.",
            "Tropical systems — NASA EONET and NOAA NHC.",
            "Cities & borders — Natural Earth (public domain).",
        };
        const char *lines2[] = {
            "Fonts — Manrope and Space Grotesk (SIL OFL 1.1).",
            "Icons — Lucide (ISC).",
            "Satellite propagation — SGP4/SDP4, public domain implementation.",
            "Map tile access — EOX::Maps public WMTS, with attribution.",
            "Nothing here is affiliated with or endorsed by Sony Interactive Entertainment.",
            "Nimbus stores settings and a tile cache on this console only.",
            "No account, no analytics, no telemetry. Weather coordinates leave the console only",
            "when a forecast or flight scan is requested, to the public services named here.",
            "License — GPL-3.0-or-later for Nimbus' own source.",
            "Build — 01.000.000 (PPSA42716) · native PS5 homebrew.",
        };
        for (int i = 0; i < 10; ++i) {
            ui_.circle(col1 + 4, y1 + 8, 1.8f, with_alpha(kAccent, 0.7f));
            TextStyle text = style(ui_.font_ui(), 10.5f, with_alpha(kText, 0.85f));
            text.max_width = 500;
            ui_.text(text, lines1[i], col1 + 16, y1);
            y1 += 38;
            ui_.circle(col2 + 4, y2 + 8, 1.8f, with_alpha(kAccent, 0.7f));
            text.max_width = 500;
            ui_.text(text, lines2[i], col2 + 16, y2);
            y2 += 38;
        }
    }

    draw_prompt(panel.x + 36, panel.y + panel.h - 34, "circle", "Close");
    draw_prompt(panel.x + 168, panel.y + panel.h - 34, "cross", "Close");
    ui_.text(style(ui_.font_ui(), 8.5f, kDim, 2), "NIMBUS · OPEN DATA · OPEN SOURCE",
             panel.x + panel.w - 36, panel.y + panel.h - 30);
}

}  // namespace nimbus
