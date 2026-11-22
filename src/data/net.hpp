#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace nimbus {

enum class FetchKind : int {
    WeatherGrid = 1,
    WeatherPoint = 2,
    Flights = 3,
    Cyclones = 4,
    TleGroup = 5,
    Satcat = 6,
    Tile = 7,
    WorldFlights = 8,
};

struct FetchRequest {
    uint64_t token = 0;
    FetchKind kind = FetchKind::WeatherGrid;
    std::string url;
    std::string cache_key;  // relative to <data>/cache, empty = no cache
    bool decode_image = false;
    int priority = 10;
    size_t max_bytes = 8 * 1024 * 1024;
    double max_cache_age = -1.0;  // seconds; <0 = cache never expires
};

struct FetchResult {
    uint64_t token = 0;
    FetchKind kind = FetchKind::WeatherGrid;
    int http_status = 0;
    bool ok = false;
    bool from_cache = false;
    std::vector<uint8_t> body;
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    std::string url;
    std::string error;
};

class NetClient {
public:
    NetClient() = default;
    ~NetClient();
    NetClient(const NetClient &) = delete;
    NetClient &operator=(const NetClient &) = delete;

    bool init(std::string data_dir, std::string ca_path, int workers = 3);
    void shutdown();
    bool enqueue(FetchRequest request);
    bool poll(FetchResult &result);
    void set_online(bool online) { online_ = online; }
    bool online() const { return online_; }
    int pending() const;

private:
    struct Job {
        FetchRequest request;
        uint64_t order = 0;
    };
    struct Compare {
        bool operator()(const Job &a, const Job &b) const {
            if (a.request.priority != b.request.priority) return a.request.priority < b.request.priority;
            return a.order > b.order;
        }
    };

    void worker_main();
    FetchResult perform(const FetchRequest &request) const;
    std::string cache_path(const std::string &key) const;
    bool read_file(const std::string &path, std::vector<uint8_t> &out) const;
    double file_age(const std::string &path) const;
    bool write_file(const std::string &path, const std::vector<uint8_t> &data) const;

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;
    std::priority_queue<Job, std::vector<Job>, Compare> queue_;
    std::vector<FetchResult> results_;
    std::vector<std::thread> workers_;
    std::unordered_set<std::string> active_urls_;
    bool stopping_ = false;
    bool online_ = true;
    uint64_t next_order_ = 0;
    std::string data_dir_;
    std::string ca_path_;
    int worker_count_ = 3;
};

}  // namespace nimbus
