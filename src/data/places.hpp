#pragma once

#include <string>
#include <vector>

namespace nimbus {

struct City {
    std::string name;
    std::string country;
    std::string admin;
    std::string iso;
    float lat = 0;
    float lon = 0;
    int population = 0;
    bool capital = false;
};

struct Country {
    std::string name;
    std::string iso;
    std::string continent;
    int population = 0;
    float label_lat = 0;
    float label_lon = 0;
};

struct Destination {
    const char *name;
    const char *country;
    double lat;
    double lon;
    const char *description;
};

class Places {
public:
    bool load(const std::string &asset_dir);
    const std::vector<City> &cities() const { return cities_; }
    const std::vector<Country> &countries() const { return countries_; }
    const Country *country_for_iso(const std::string &iso) const;
    const City *nearest(double lat, double lon, double max_degrees, double *distance_degrees = nullptr) const;
    const City *find_exact(double lat, double lon, double tolerance_degrees = 0.08) const;
    std::vector<const City *> search(const std::string &query, int limit = 18) const;
    std::vector<const City *> label_cities(double lat, double lon, double half_lat, double half_lon,
                                           int limit, int min_population) const;
    const std::vector<Destination> &destinations() const { return destinations_; }
    const Destination *random_destination(unsigned seed) const;

private:
    std::vector<City> cities_;
    std::vector<Country> countries_;
    std::vector<Destination> destinations_;
};

std::string lower_ascii(const std::string &text);

}  // namespace nimbus
