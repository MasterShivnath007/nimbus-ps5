#include "data/places.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "data/text.hpp"

#include "gfx/math.hpp"

namespace nimbus {

std::string lower_ascii(const std::string &text) {
    std::string out;
    out.resize(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    }
    return out;
}

namespace {
}  // namespace

bool Places::load(const std::string &asset_dir) {
    destinations_ = {
        {"Taj Mahal", "India", 27.1751, 78.0421, "A marble mausoleum on the Yamuna, best at sunrise."},
        {"Grand Canyon", "United States", 36.0544, -112.1401, "A mile-deep incision through red desert rock."},
        {"Great Barrier Reef", "Australia", -18.2871, 147.6992, "The largest living structure on Earth."},
        {"Mount Fuji", "Japan", 35.3606, 138.7274, "A symmetrical stratovolcano above the clouds."},
        {"Serengeti", "Tanzania", -2.3333, 34.8333, "Grass plains that carry the great migration."},
        {"Venice", "Italy", 45.4408, 12.3155, "A city of canals, stone and lagoon light."},
        {"Machu Picchu", "Peru", -13.1631, -72.5450, "Inca stonework on a saddle above the Urubamba."},
        {"Vatnajokull", "Iceland", 64.4167, -16.8000, "Ice caps, black sand and glacial rivers."},
        {"Dubai", "United Arab Emirates", 25.2048, 55.2708, "A skyline rising from the Gulf coast."},
        {"Singapore", "Singapore", 1.3521, 103.8198, "A garden city lit by a tropical skyline."},
        {"Tokyo", "Japan", 35.6762, 139.6503, "The world's largest metropolitan skyline."},
        {"New York", "United States", 40.7128, -74.0060, "Manhattan's grid between two rivers."},
        {"London", "United Kingdom", 51.5072, -0.1276, "The Thames winding through a green city."},
        {"Paris", "France", 48.8566, 2.3522, "Boulevards, rooftops and the Seine."},
        {"Cairo", "Egypt", 30.0444, 31.2357, "The Nile, the delta and the edge of the Sahara."},
        {"Cape Town", "South Africa", -33.9249, 18.4241, "A city between Table Mountain and the Atlantic."},
        {"Rio de Janeiro", "Brazil", -22.9068, -43.1729, "Granite peaks, beaches and a dense urban forest."},
        {"Patagonia", "Argentina", -49.3315, -72.8863, "Ice fields and wind-carved granite."},
        {"Everest", "Nepal", 27.9881, 86.9250, "The highest point on the planet."},
        {"Maldives", "Maldives", 3.2028, 73.2207, "Atolls and shallow turquoise reefs."},
        {"Santorini", "Greece", 36.3932, 25.4615, "A caldera rim of white villages above the Aegean."},
        {"Kyoto", "Japan", 35.0116, 135.7681, "Temple gardens and a valley ringed by hills."},
        {"Hong Kong", "China", 22.3193, 114.1694, "A vertical city between mountains and harbour."},
        {"Sydney", "Australia", -33.8688, 151.2093, "A harbour city of sandstone and eucalypt."},
        {"Reykjavik", "Iceland", 64.1466, -21.9426, "A small capital under big northern skies."},
        {"Marrakech", "Morocco", 31.6295, -7.9811, "Ochre walls and the High Atlas on the horizon."},
        {"Petra", "Jordan", 30.3285, 35.4444, "A rock-cut city hidden in a desert canyon."},
        {"Angkor", "Cambodia", 13.4125, 103.8670, "A vast temple city in the Cambodian forest."},
        {"Ha Long Bay", "Vietnam", 20.9101, 107.1839, "Limestone islands scattered across green water."},
        {"Banff", "Canada", 51.1784, -115.5708, "Glacier-fed lakes beneath the Canadian Rockies."},
        {"Queenstown", "New Zealand", -45.0312, 168.6626, "Lake Wakatipu and the Southern Alps."},
        {"Sossusvlei", "Namibia", -24.7260, 15.2915, "Red dunes and white salt pans in the Namib."},
        {"Ayers Rock", "Australia", -25.3444, 131.0369, "Uluru rising from the red centre."},
        {"Antarctic Peninsula", "Antarctica", -64.0000, -60.0000, "Blue ice, black water and immense silence."},
    };

    std::string city_text;
    if (!read_text_file(asset_dir + "/cities.tsv", city_text)) {
        std::fprintf(stderr, "[places] missing cities.tsv\n");
        return false;
    }
    const auto city_lines = split_lines(city_text);
    for (size_t line_index = 1; line_index < city_lines.size(); ++line_index) {
        if (city_lines[line_index].empty()) continue;
        const auto fields = split_tabs(city_lines[line_index]);
        if (fields.size() < 8) continue;
        City city;
        city.name = fields[0];
        city.country = fields[1];
        city.admin = fields[2];
        city.iso = fields[3];
        city.lat = static_cast<float>(std::atof(fields[4].c_str()));
        city.lon = static_cast<float>(std::atof(fields[5].c_str()));
        city.population = std::atoi(fields[6].c_str());
        city.capital = fields[7] == "1";
        if (!city.name.empty()) cities_.push_back(std::move(city));
    }
    std::sort(cities_.begin(), cities_.end(), [](const City &a, const City &b) {
        if (a.population != b.population) return a.population > b.population;
        return a.name < b.name;
    });

    std::string country_text;
    if (read_text_file(asset_dir + "/countries.tsv", country_text)) {
        const auto country_lines = split_lines(country_text);
        for (size_t line_index = 1; line_index < country_lines.size(); ++line_index) {
            if (country_lines[line_index].empty()) continue;
            const auto fields = split_tabs(country_lines[line_index]);
            if (fields.size() < 6) continue;
            Country country;
            country.name = fields[0];
            country.iso = fields[1];
            country.continent = fields[2];
            country.population = std::atoi(fields[3].c_str());
            country.label_lat = static_cast<float>(std::atof(fields[4].c_str()));
            country.label_lon = static_cast<float>(std::atof(fields[5].c_str()));
            if (!country.name.empty()) countries_.push_back(std::move(country));
        }
    }
    std::fprintf(stderr, "[places] %zu cities, %zu countries\n", cities_.size(), countries_.size());
    return !cities_.empty();
}

const Country *Places::country_for_iso(const std::string &iso) const {
    for (const auto &country : countries_) {
        if (country.iso == iso) return &country;
    }
    return nullptr;
}

const City *Places::nearest(double lat, double lon, double max_degrees, double *distance_degrees) const {
    const City *best = nullptr;
    double best_distance = max_degrees;
    for (const auto &city : cities_) {
        double dlat = std::fabs(city.lat - lat);
        if (dlat > best_distance) continue;
        double dlon = std::fabs(wrap_longitude(city.lon - lon));
        if (dlon > best_distance * 2.2) continue;
        const double distance = great_circle_degrees(lat, lon, city.lat, city.lon);
        if (distance < best_distance) {
            best_distance = distance;
            best = &city;
        }
    }
    if (distance_degrees) *distance_degrees = best_distance;
    return best;
}

const City *Places::find_exact(double lat, double lon, double tolerance_degrees) const {
    for (const auto &city : cities_) {
        if (std::fabs(city.lat - lat) < tolerance_degrees &&
            std::fabs(wrap_longitude(city.lon - lon)) < tolerance_degrees) {
            return &city;
        }
    }
    return nullptr;
}

std::vector<const City *> Places::search(const std::string &query, int limit) const {
    std::vector<const City *> results;
    const std::string needle = lower_ascii(query);
    if (needle.size() < 1) return results;
    for (const auto &city : cities_) {
        const std::string haystack = lower_ascii(city.name + " " + city.country + " " + city.admin);
        if (haystack.find(needle) == std::string::npos) continue;
        results.push_back(&city);
        if (static_cast<int>(results.size()) >= 600) break;
    }
    std::sort(results.begin(), results.end(), [&](const City *a, const City *b) {
        const bool a_prefix = lower_ascii(a->name).rfind(needle, 0) == 0;
        const bool b_prefix = lower_ascii(b->name).rfind(needle, 0) == 0;
        if (a_prefix != b_prefix) return a_prefix;
        if (a->capital != b->capital) return a->capital;
        if (a->population != b->population) return a->population > b->population;
        return a->name < b->name;
    });
    if (static_cast<int>(results.size()) > limit) results.resize(static_cast<size_t>(limit));
    return results;
}

std::vector<const City *> Places::label_cities(double lat, double lon, double half_lat,
                                               double half_lon, int limit,
                                               int min_population) const {
    std::vector<const City *> results;
    for (const auto &city : cities_) {
        if (city.population < min_population) continue;
        if (std::fabs(city.lat - lat) > half_lat) continue;
        if (std::fabs(wrap_longitude(city.lon - lon)) > half_lon) continue;
        results.push_back(&city);
        if (static_cast<int>(results.size()) >= limit * 3) break;
    }
    std::sort(results.begin(), results.end(), [](const City *a, const City *b) {
        return a->population > b->population;
    });
    if (static_cast<int>(results.size()) > limit) results.resize(static_cast<size_t>(limit));
    return results;
}

const Destination *Places::random_destination(unsigned seed) const {
    if (destinations_.empty()) return nullptr;
    seed = seed * 1664525u + 1013904223u;
    return &destinations_[seed % destinations_.size()];
}

}  // namespace nimbus
