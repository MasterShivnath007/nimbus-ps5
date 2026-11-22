#include "data/net.hpp"

#ifndef NIMBUS_PS5
#include <curl/curl.h>
#else
extern "C" {
int sceNetInit(void);
int sceNetPoolCreate(const char *name, int size, int flags);
int sceSslInit(size_t pool_size);
int sceHttpInit(int net_pool, int ssl_context, size_t pool_size);
int sceHttpCreateTemplate(int context, const char *user_agent, int http_version, int auto_proxy);
int sceHttpSetResponseHeaderMaxSize(int template_id, size_t size);
int sceHttpSetConnectTimeOut(int id, unsigned int usec);
int sceHttpSetResolveTimeOut(int id, unsigned int usec);
int sceHttpSetSendTimeOut(int id, unsigned int usec);
int sceHttpSetRecvTimeOut(int id, unsigned int usec);
int sceHttpCreateConnectionWithURL(int template_id, const char *url, int keep_alive);
int sceHttpDeleteConnection(int connection);
int sceHttpCreateRequestWithURL(int connection, int method, const char *url,
                                unsigned long long content_length);
int sceHttpDeleteRequest(int request);
int sceHttpSendRequest(int request, const void *data, size_t size);
int sceHttpGetStatusCode(int request, int *status);
int sceHttpGetResponseContentLength(int request, int *known, unsigned long long *length);
int sceHttpReadData(int request, void *data, size_t size);
int sceHttpsSetSslCallback(int template_id, void *callback, void *user_arg);
}
static int ps5_ssl_callback(void) { return 0; }
#endif

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <map>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

#include <sys/stat.h>
#include <unistd.h>
#include <ctime>

#include "stb_image.h"

namespace nimbus {

namespace {

const char *kUserAgent =
    "NimbusPS5/1.0 (native PS5 homebrew; +https://github.com/MasterShivnath007/nimbus-ps5)";

#ifndef NIMBUS_PS5
size_t write_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *body = static_cast<std::vector<uint8_t> *>(userdata);
    const size_t bytes = size * nmemb;
    if (body->size() + bytes > 12 * 1024 * 1024) return 0;
    body->insert(body->end(), reinterpret_cast<uint8_t *>(ptr),
                 reinterpret_cast<uint8_t *>(ptr) + bytes);
    return bytes;
}
#endif

bool http_get(const std::string &url, const std::string &ca_path, std::vector<uint8_t> &body,
              int &status_out, std::string &error) {
#ifndef NIMBUS_PS5
    CURL *curl = curl_easy_init();
    if (!curl) { error = "curl_easy_init failed"; return false; }
    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    if (!ca_path.empty()) curl_easy_setopt(curl, CURLOPT_CAINFO, ca_path.c_str());
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    status_out = static_cast<int>(status);
    if (code != CURLE_OK) {
        error = error_buffer[0] ? error_buffer : curl_easy_strerror(code);
        return false;
    }
    if (status < 200 || status >= 300) {
        error = "HTTP " + std::to_string(status);
        return false;
    }
    return !body.empty();
#else
    (void)ca_path;
    struct Session {
        bool ready = false;
        int template_id = -1;
    };
    static thread_local Session session;
    if (!session.ready) {
        const int net_pool = sceNetPoolCreate("nimbus-net", 512 * 1024, 0);
        const int ssl_context = sceSslInit(384 * 1024);
        const int http_context = sceHttpInit(net_pool, ssl_context, 256 * 1024);
        if (net_pool < 0 || ssl_context < 0 || http_context < 0) {
            error = "sceHttpInit failed";
            return false;
        }
        session.template_id =
            sceHttpCreateTemplate(http_context, kUserAgent, 2, 1);
        if (session.template_id < 0) {
            error = "sceHttpCreateTemplate failed";
            return false;
        }
        sceHttpSetResponseHeaderMaxSize(session.template_id, 0x4000);
        for (int id : {session.template_id}) {
            sceHttpSetConnectTimeOut(id, 10 * 1000 * 1000);
            sceHttpSetResolveTimeOut(id, 10 * 1000 * 1000);
            sceHttpSetSendTimeOut(id, 25 * 1000 * 1000);
            sceHttpSetRecvTimeOut(id, 25 * 1000 * 1000);
        }
        (void)sceHttpsSetSslCallback(session.template_id, (void *)ps5_ssl_callback, nullptr);
        session.ready = true;
    }
    const int connection =
        sceHttpCreateConnectionWithURL(session.template_id, url.c_str(), 0);
    if (connection < 0) {
        error = "sceHttpCreateConnection failed";
        return false;
    }
    const int request = sceHttpCreateRequestWithURL(connection, 0, url.c_str(), 0);
    if (request < 0) {
        sceHttpDeleteConnection(connection);
        error = "sceHttpCreateRequest failed";
        return false;
    }
    int status = -1;
    bool ok = false;
    if (sceHttpSendRequest(request, nullptr, 0) >= 0 &&
        sceHttpGetStatusCode(request, &status) >= 0) {
        status_out = status;
        if (status >= 200 && status < 300) {
            int known = 1;
            unsigned long long expected = 0;
            if (sceHttpGetResponseContentLength(request, &known, &expected) < 0) {
                known = 1;
                expected = 0;
            }
            body.reserve(known == 0 && expected > 0 && expected < 12 * 1024 * 1024
                             ? static_cast<size_t>(expected)
                             : 65536);
            uint8_t buffer[65536];
            for (;;) {
                const int got = sceHttpReadData(request, buffer, sizeof(buffer));
                if (got == 0) break;
                if (got < 0) {
                    error = "sceHttpReadData failed";
                    body.clear();
                    break;
                }
                body.insert(body.end(), buffer, buffer + got);
                if (body.size() > 12 * 1024 * 1024) {
                    error = "response too large";
                    body.clear();
                    break;
                }
            }
            if (!body.empty() && error.empty()) ok = true;
        } else {
            error = "HTTP " + std::to_string(status);
        }
    } else {
        error = "sceHttpSendRequest failed";
    }
    sceHttpDeleteRequest(request);
    sceHttpDeleteConnection(connection);
    return ok;
#endif
}

void pace_host(const std::string &url) {
    static std::mutex mutex;
    static std::map<std::string, std::chrono::steady_clock::time_point> next_allowed;
    std::string host;
    if (url.find("api.open-meteo.com") != std::string::npos) host = "open-meteo";
    else if (url.find("tiles.maps.eox.at") != std::string::npos) host = "eox";
    else if (url.find("adsb.lol") != std::string::npos) host = "adsb";
    else if (url.find("opensky-network.org") != std::string::npos) host = "opensky";
    if (host.empty()) return;
    const double interval = host == "open-meteo" ? 1.35 : host == "eox" ? 0.05 : 0.6;
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        auto &slot = next_allowed[host];
        if (now >= slot) {
            slot = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                              std::chrono::duration<double>(interval));
            return;
        }
        const auto wait = slot - now;
        lock.unlock();
        std::this_thread::sleep_for(wait);
        lock.lock();
    }
}

bool make_dirs(const std::string &path) {
    std::string current;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!current.empty()) ::mkdir(current.c_str(), 0755);
        }
        if (i < path.size()) current.push_back(path[i]);
    }
    return true;
}

}  // namespace

NetClient::~NetClient() { shutdown(); }

bool NetClient::init(std::string data_dir, std::string ca_path, int workers) {
#ifdef NIMBUS_PS5
    if (sceNetInit() < 0) {
        std::fprintf(stderr, "[net] sceNetInit failed\n");
        return false;
    }
#endif
    data_dir_ = std::move(data_dir);
    ca_path_ = std::move(ca_path);
    worker_count_ = std::max(1, std::min(6, workers));
    make_dirs(data_dir_ + "/cache");
#ifndef NIMBUS_PS5
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::fprintf(stderr, "[net] curl_global_init failed\n");
        return false;
    }
#endif
    stopping_ = false;
    for (int i = 0; i < worker_count_; ++i) {
        workers_.emplace_back([this] { worker_main(); });
    }
    return true;
}

void NetClient::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
    }
    wake_.notify_all();
    for (auto &thread : workers_) {
        if (thread.joinable()) thread.join();
    }
    workers_.clear();
#ifndef NIMBUS_PS5
    curl_global_cleanup();
#endif
}

std::string NetClient::cache_path(const std::string &key) const {
    std::string clean = key;
    while (!clean.empty() && clean.front() == '/') clean.erase(clean.begin());
    return data_dir_ + "/cache/" + clean;
}

bool NetClient::read_file(const std::string &path, std::vector<uint8_t> &out) const {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    if (size <= 0) { std::fclose(file); return false; }
    std::fseek(file, 0, SEEK_SET);
    out.resize(static_cast<size_t>(size));
    const size_t read = std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    out.resize(read);
    return read > 0;
}

double NetClient::file_age(const std::string &path) const {
    struct stat info;
    if (::stat(path.c_str(), &info) != 0) return 1e18;
    return std::max(0.0, std::difftime(std::time(nullptr), info.st_mtime));
}

bool NetClient::write_file(const std::string &path, const std::vector<uint8_t> &data) const {
    const std::string directory = path.substr(0, path.find_last_of('/'));
    if (!directory.empty()) make_dirs(directory);
    const std::string temporary = path + ".tmp";
    std::FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) return false;
    const size_t written = std::fwrite(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (written != data.size()) {
        std::remove(temporary.c_str());
        return false;
    }
    std::remove(path.c_str());
    if (std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool NetClient::enqueue(FetchRequest request) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return false;
        if (!request.url.empty() && active_urls_.find(request.url) != active_urls_.end())
            return false;
        if (queue_.size() > 300) return false;
        if (!request.url.empty()) active_urls_.insert(request.url);
        queue_.push({std::move(request), next_order_++});
    }
    wake_.notify_one();
    return true;
}

FetchResult NetClient::perform(const FetchRequest &request) const {
    FetchResult result;
    result.token = request.token;
    result.kind = request.kind;
    result.url = request.url;

    std::vector<uint8_t> cached;
    bool have_cache = false;
    bool fresh_cache = false;
    if (!request.cache_key.empty()) {
        have_cache = read_file(cache_path(request.cache_key), cached);
        if (have_cache) {
            const double age = file_age(cache_path(request.cache_key));
            fresh_cache = request.max_cache_age < 0.0 || age <= request.max_cache_age;
        }
    }
    if (have_cache && fresh_cache) {
        result.body = cached;
        result.ok = true;
        result.from_cache = true;
        result.http_status = 200;
    } else {
        if (!online_) {
            if (have_cache) {
                result.body = cached;
                result.ok = true;
                result.from_cache = true;
                result.http_status = 200;
            } else {
                result.error = "offline";
            }
            return result;
        }
        pace_host(request.url);
        std::vector<uint8_t> body;
        int status = 0;
        std::string http_error;
        if (!http_get(request.url, ca_path_, body, status, http_error)) {
            result.http_status = status;
            result.error = http_error;
            if (have_cache) {
                result.body = cached;
                result.ok = true;
                result.from_cache = true;
                result.http_status = 200;
            }
            return result;
        }
        if (body.size() > request.max_bytes) {
            result.error = "response too large";
            return result;
        }
        result.body = std::move(body);
        result.ok = true;
        if (!request.cache_key.empty()) write_file(cache_path(request.cache_key), result.body);
    }

    if (request.decode_image && result.ok) {
        int width = 0, height = 0, channels = 0;
        unsigned char *pixels = stbi_load_from_memory(
            result.body.data(), static_cast<int>(result.body.size()), &width, &height, &channels, 4);
        if (!pixels) {
            result.ok = false;
            result.error = "image decode failed";
            return result;
        }
        result.width = width;
        result.height = height;
        result.pixels.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
        stbi_image_free(pixels);
    }
    return result;
}

void NetClient::worker_main() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            job = queue_.top();
            queue_.pop();
        }
        FetchResult result = perform(job.request);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_urls_.erase(job.request.url);
            results_.push_back(std::move(result));
        }
        done_.notify_one();
    }
}

bool NetClient::poll(FetchResult &result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (results_.empty()) return false;
    result = std::move(results_.front());
    results_.erase(results_.begin());
    return true;
}

int NetClient::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(queue_.size() + results_.size());
}

}  // namespace nimbus
