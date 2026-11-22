#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nimbus {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    explicit Json(bool value) : type_(Type::Bool), bool_(value) {}
    explicit Json(double value) : type_(Type::Number), number_(value) {}
    explicit Json(std::string value) : type_(Type::String), string_(std::move(value)) {}

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_object() const { return type_ == Type::Object; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }

    bool as_bool(bool fallback = false) const;
    double as_number(double fallback = 0.0) const;
    int as_int(int fallback = 0) const;
    const std::string &as_string() const;
    std::string string_or(const std::string &fallback) const;

    size_t size() const;
    const Json &at(size_t index) const;
    const Json &operator[](size_t index) const { return at(index); }
    const Json *find(std::string_view key) const;
    const Json &get(std::string_view key) const;
    bool has(std::string_view key) const { return find(key) != nullptr; }

    static Json object();
    static Json array();
    void set(std::string key, Json value);
    void push(Json value);

private:
    friend class JsonParser;
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    std::vector<std::pair<std::string, Json>> object_;
};

bool parse_json(std::string_view text, Json &out, std::string &error);
std::string json_escape(const std::string &text);

}  // namespace nimbus
