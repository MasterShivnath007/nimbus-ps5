// Small, locale-free text helpers.
//
// The PS5's libc offers gmtime()/mktime() but not the complete locale surface
// that libc++'s <fstream>/<sstream> facets reference.  Nimbus only needs to
// read tab-separated assets and write a small settings file, so it does that
// with stdio and plain string splitting.
#pragma once

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace nimbus {

inline bool read_text_file(const std::string &path, std::string &out) {
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.clear();
    if (length > 0) {
        out.resize(static_cast<size_t>(length));
        const size_t read = std::fread(out.data(), 1, out.size(), file);
        out.resize(read);
    }
    std::fclose(file);
    return true;
}

inline std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (;;) {
        const size_t end = text.find('\n', start);
        const size_t stop = end == std::string_view::npos ? text.size() : end;
        std::string line(text.substr(start, stop - start));
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return lines;
}

inline std::vector<std::string> split_tabs(const std::string &line) {
    std::vector<std::string> fields;
    size_t start = 0;
    for (;;) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    return fields;
}

}  // namespace nimbus
