#include "data/json.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace nimbus {

bool Json::as_bool(bool fallback) const { return type_ == Type::Bool ? bool_ : fallback; }

double Json::as_number(double fallback) const {
    if (type_ == Type::Number) return number_;
    if (type_ == Type::Bool) return bool_ ? 1.0 : 0.0;
    return fallback;
}

int Json::as_int(int fallback) const {
    if (type_ != Type::Number || !std::isfinite(number_)) return fallback;
    return static_cast<int>(number_);
}

const std::string &Json::as_string() const {
    static const std::string empty;
    return type_ == Type::String ? string_ : empty;
}

std::string Json::string_or(const std::string &fallback) const {
    return type_ == Type::String ? string_ : fallback;
}

size_t Json::size() const {
    if (type_ == Type::Array) return array_.size();
    if (type_ == Type::Object) return object_.size();
    return 0;
}

const Json &Json::at(size_t index) const {
    static const Json null;
    if (type_ != Type::Array || index >= array_.size()) return null;
    return array_[index];
}

const Json *Json::find(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto &entry : object_) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

const Json &Json::get(std::string_view key) const {
    static const Json null;
    const Json *found = find(key);
    return found ? *found : null;
}

Json Json::object() {
    Json value;
    value.type_ = Type::Object;
    return value;
}

Json Json::array() {
    Json value;
    value.type_ = Type::Array;
    return value;
}

void Json::set(std::string key, Json value) {
    if (type_ != Type::Object) return;
    for (auto &entry : object_) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    object_.emplace_back(std::move(key), std::move(value));
}

void Json::push(Json value) {
    if (type_ != Type::Array) return;
    array_.push_back(std::move(value));
}

namespace {

class Parser {
public:
    Parser(std::string_view text) : at_(text.data()), end_(text.data() + text.size()) {}

    bool parse(Json &out, std::string &error) {
        skip_space();
        if (!parse_value(out, 0)) {
            error = error_;
            if (error.empty()) error = "invalid JSON";
            return false;
        }
        skip_space();
        if (at_ != end_) {
            error = "trailing characters";
            return false;
        }
        return true;
    }

private:
    void skip_space() {
        while (at_ < end_ && (*at_ == ' ' || *at_ == '\t' || *at_ == '\n' || *at_ == '\r')) ++at_;
    }

    bool fail(const char *message) {
        if (error_.empty()) error_ = message;
        return false;
    }

    bool parse_value(Json &out, int depth) {
        if (depth > 40) return fail("JSON nesting too deep");
        skip_space();
        if (at_ >= end_) return fail("unexpected end of JSON");
        switch (*at_) {
            case '{': return parse_object(out, depth + 1);
            case '[': return parse_array(out, depth + 1);
            case '"': {
                std::string value;
                if (!parse_string(value)) return false;
                out = Json(std::move(value));
                return true;
            }
            case 't':
                if (end_ - at_ >= 4 && std::memcmp(at_, "true", 4) == 0) {
                    at_ += 4;
                    out = Json(true);
                    return true;
                }
                return fail("invalid literal");
            case 'f':
                if (end_ - at_ >= 5 && std::memcmp(at_, "false", 5) == 0) {
                    at_ += 5;
                    out = Json(false);
                    return true;
                }
                return fail("invalid literal");
            case 'n':
                if (end_ - at_ >= 4 && std::memcmp(at_, "null", 4) == 0) {
                    at_ += 4;
                    out = Json();
                    return true;
                }
                return fail("invalid literal");
            default:
                return parse_number(out);
        }
    }

    bool parse_number(Json &out) {
        const char *start = at_;
        if (at_ < end_ && (*at_ == '-' || *at_ == '+')) ++at_;
        bool digits = false;
        while (at_ < end_ && *at_ >= '0' && *at_ <= '9') { ++at_; digits = true; }
        if (at_ < end_ && *at_ == '.') {
            ++at_;
            while (at_ < end_ && *at_ >= '0' && *at_ <= '9') { ++at_; digits = true; }
        }
        if (at_ < end_ && (*at_ == 'e' || *at_ == 'E')) {
            ++at_;
            if (at_ < end_ && (*at_ == '-' || *at_ == '+')) ++at_;
            while (at_ < end_ && *at_ >= '0' && *at_ <= '9') ++at_;
        }
        if (!digits) return fail("invalid number");
        std::string text(start, at_ - start);
        out = Json(std::strtod(text.c_str(), nullptr));
        return true;
    }

    static void append_utf8(std::string &out, unsigned code) {
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    static int hex_digit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool parse_string(std::string &out) {
        if (at_ >= end_ || *at_ != '"') return fail("expected string");
        ++at_;
        out.clear();
        while (at_ < end_) {
            const unsigned char c = static_cast<unsigned char>(*at_++);
            if (c == '"') return true;
            if (c < 0x20) return fail("control character in string");
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (at_ >= end_) return fail("unterminated escape");
            const char escape = *at_++;
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned code = 0;
                    if (end_ - at_ < 4) return fail("short unicode escape");
                    for (int i = 0; i < 4; ++i) {
                        const int digit = hex_digit(at_[i]);
                        if (digit < 0) return fail("bad unicode escape");
                        code = code * 16u + static_cast<unsigned>(digit);
                    }
                    at_ += 4;
                    if (code >= 0xD800 && code <= 0xDBFF && end_ - at_ >= 6 && at_[0] == '\\' &&
                        at_[1] == 'u') {
                        unsigned low = 0;
                        bool ok = true;
                        for (int i = 0; i < 4; ++i) {
                            const int digit = hex_digit(at_[2 + i]);
                            if (digit < 0) { ok = false; break; }
                            low = low * 16u + static_cast<unsigned>(digit);
                        }
                        if (ok && low >= 0xDC00 && low <= 0xDFFF) {
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            at_ += 6;
                        } else {
                            code = '?';
                        }
                    } else if (code >= 0xD800 && code <= 0xDFFF) {
                        code = '?';
                    }
                    append_utf8(out, code);
                    break;
                }
                default: return fail("invalid escape");
            }
        }
        return fail("unterminated string");
    }

    bool parse_array(Json &out, int depth) {
        out = Json::array();
        ++at_;
        skip_space();
        if (at_ < end_ && *at_ == ']') { ++at_; return true; }
        while (at_ < end_) {
            Json value;
            if (!parse_value(value, depth)) return false;
            out.push(value);
            skip_space();
            if (at_ < end_ && *at_ == ',') { ++at_; skip_space(); continue; }
            if (at_ < end_ && *at_ == ']') { ++at_; return true; }
            return fail("expected , or ]");
        }
        return fail("unterminated array");
    }

    bool parse_object(Json &out, int depth) {
        out = Json::object();
        ++at_;
        skip_space();
        if (at_ < end_ && *at_ == '}') { ++at_; return true; }
        while (at_ < end_) {
            std::string key;
            if (!parse_string(key)) return false;
            skip_space();
            if (at_ >= end_ || *at_ != ':') return fail("expected :");
            ++at_;
            Json value;
            if (!parse_value(value, depth)) return false;
            out.set(std::move(key), std::move(value));
            skip_space();
            if (at_ < end_ && *at_ == ',') { ++at_; skip_space(); continue; }
            if (at_ < end_ && *at_ == '}') { ++at_; return true; }
            return fail("expected , or }");
        }
        return fail("unterminated object");
    }

    const char *at_;
    const char *end_;
    std::string error_;
};

}  // namespace

bool parse_json(std::string_view text, Json &out, std::string &error) {
    Parser parser(text);
    return parser.parse(out, error);
}

std::string json_escape(const std::string &text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                    out += buffer;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

}  // namespace nimbus
