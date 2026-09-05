
#pragma once

#include <any>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <typeindex>
#include <filesystem>
#include <unordered_map>

namespace turbubestia::settings {

using setting_bound = std::variant<std::monostate, int, double>;

class setting_value
{
    template<typename T>
    auto get() const -> std::conditional_t<std::is_fundamental_v<T>, T, const T&> { 
        if (!has_value()) { throw std::runtime_error("setting_value object has no value."); }
        if (!std::holds_alternative<T>(_value.value())) { throw std::runtime_error("setting_value missmatch type request."); }
        return std::get<T>(_value.value()); 
    }

public:
    // ctor & dtor ------------------------------------------------------------
    setting_value() = default;
    setting_value(const setting_value& other) = default;
    setting_value(setting_value &&other) = default;

    explicit setting_value(bool val) : _value(val) {}
    explicit setting_value(int val) : _value(val) {}
    explicit setting_value(double val) : _value(val) {}
    explicit setting_value(std::string val) : _value(std::move(val)) {}

    // operators --------------------------------------------------------------
    explicit operator bool() const { return has_value(); }

    template<typename T>
    auto operator=(const T &rhs) -> setting_value & {
        _value = rhs;
        return *this;
    }

    auto operator=(const setting_value &rhs) -> setting_value & = default;
    auto operator=(setting_value &&rhs) -> setting_value & = default;

    template<typename T>
    friend bool operator==(const setting_value& lhs, const T& rhs) {
        return lhs.type() == typeid(T) && lhs.get<T>() == rhs;
    }

    template<typename T>
    friend bool operator==(const T& lhs, const setting_value& rhs) {
        return rhs.type() == typeid(T) && rhs.get<T>() == lhs;
    }

    friend bool operator==(const setting_value& lhs, const setting_value& rhs) {
        return lhs.type() == rhs.type() && lhs._value == rhs._value;
    }

    // properties -------------------------------------------------------------
    bool has_value() const { return _value != std::nullopt; }

    auto type() const -> std::type_index {
        if (!_value.has_value()) { return typeid(void); }
        return std::visit([](const auto& v) -> std::type_index {
            return typeid(std::decay_t<decltype(v)>);
        }, _value.value());
    }

    // accessors --------------------------------------------------------------
    auto to_bool() const -> bool { return get<bool>(); }
    auto to_int() const -> int { return get<int>(); }
    auto to_double() const -> double { return get<double>(); }
    auto to_string() const -> const std::string & { return get<std::string>(); }

private:
    std::optional<std::variant<bool, int, double, std::string>> _value {};
};

struct setting_schema {
    std::string key;          // e.g., "editor/tab_size"
    std::string title;        // e.g., "Tab Size"
    std::string description;  // e.g., "Number of spaces per tab"
    std::string category;     // e.g., "Editor"
    setting_value default_value;
    std::type_index type = typeid(std::string);     // QMetaType::Int, QMetaType::Bool, etc.

    // Optional constraints
    setting_bound min = std::monostate{}; // For numeric types
    setting_bound max = std::monostate{}; // For numeric types
    std::vector<std::string> enum_options;

    // Validate a candidate value against this schema
    auto is_valid(const setting_value &val) const -> bool;
};

class settings_manager {
public:
    // ctor & dtor ------------------------------------------------------------

    explicit settings_manager() = default;

    // accessors --------------------------------------------------------------
    auto get(const std::string &key) const -> const setting_value &;

    template<typename T>
    auto set(const std::string &key, T val) -> bool {
        return set(key, setting_value(val));
    }

    auto set(const std::string &key, const setting_value &value) -> bool;

    // methods ----------------------------------------------------------------
    auto active_values() const -> const std::unordered_map<std::string, setting_value> &;
    void register_schema(const setting_schema &schema);
    auto schema(const std::string &key) const -> std::optional<setting_schema>;
    auto schemas() const -> const std::unordered_map<std::string, setting_schema> &;

    // serialization to file
    bool save_to_file(const std::string &filePath);
    bool load_from_file(const std::string &filePath);

    // test-only API -----------------------------------------------------------
    // Sets a base directory for persistence paths. When active, simple file
    // names (no separators) are resolved inside this directory instead of
    // ~/.config/dir2md/. Intended exclusively for unit-test harnesses.
    static void set_test_base_directory_path(const std::filesystem::path &path);
    static void clear_test_base_directory();
    static auto test_base_directory_path() -> std::filesystem::path &;
    static auto test_base_directory() -> std::string;

private:
    std::unordered_map<std::string, setting_value> m_values;          // Flat value store ($O(1)$)
    std::unordered_map<std::string, setting_schema> m_schemaRegistry; // Schema metadata ($O(1)$)
};

} // namespace turbubestia::settings

