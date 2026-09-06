
#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <typeindex>
#include <functional>
#include <filesystem>
#include <unordered_map>

namespace turbubestia::settings {

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
        using DecayT = std::decay_t<T>;
        if constexpr (std::is_same_v<DecayT, const char *> || std::is_same_v<DecayT, char*>) {
            return lhs.type() == typeid(std::string) && lhs.get<std::string>() == rhs;
        } else {
            return lhs.type() == typeid(T) && lhs.get<T>() == rhs;
        }
    }

    template<typename T>
    friend bool operator==(const T& lhs, const setting_value& rhs) {
        using DecayT = std::decay_t<T>;
        if constexpr (std::is_same_v<DecayT, const char *> || std::is_same_v<DecayT, char*>) {
            return rhs.type() == typeid(std::string) && rhs.get<std::string>() == lhs;
        } else {
            return rhs.type() == typeid(T) && rhs.get<T>() == lhs;
        }
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

    // validations ------------------------------------------------------------
    bool is_type(std::type_index target_type) const { return type() == target_type; }

    // accessors --------------------------------------------------------------
    auto to_bool() const -> bool { return get<bool>(); }
    auto to_int() const -> int { return get<int>(); }
    auto to_double() const -> double { return get<double>(); }
    auto to_string() const -> const std::string & { return get<std::string>(); }

private:
    std::optional<std::variant<bool, int, double, std::string>> _value {};
};

class setting_schema {
public:
    // ctor & dtor ------------------------------------------------------------
    setting_schema() = delete;
    // setting_schema(const std::string &key, std::type_index type);

    template<typename T>
    setting_schema(const std::string &key, T default_value) 
        : _key(key) 
    {
        using DecayT = std::decay_t<T>;
        static_assert(std::is_same_v<DecayT, bool> || 
            std::is_same_v<DecayT, int> || 
            std::is_same_v<DecayT, double> || 
            std::is_same_v<DecayT, std::string> ||
            std::is_same_v<DecayT, const char*> ||
            std::is_same_v<DecayT, char*>, 
            "setting_schema only supports bool, int, double, std::string and const char* types.");
       
        if constexpr (std::is_same_v<DecayT, const char*> || std::is_same_v<DecayT, char*>) {
            _type = typeid(std::string);
            _default_value = setting_value(std::string(default_value));
        } else if constexpr (std::is_same_v<DecayT, std::string>) {
            _type = typeid(std::string);
            _default_value = setting_value(default_value);
        } else {
            _type = typeid(DecayT);
            _default_value = setting_value(default_value);
        }
    }

    // properties -------------------------------------------------------------
    auto key() const -> const std::string & { return _key; }
    auto type() const -> std::type_index { return _type; }

    auto title() const -> const std::string & { return _title; }
    auto title(std::string value) -> setting_schema & { _title = std::move(value); return *this; }

    auto description() const -> const std::string & { return _description; }
    auto description(std::string value) -> setting_schema & { _description = std::move(value); return *this; }

    auto default_value() const -> const setting_value & { return _default_value; }

    auto enum_options() const -> const std::vector<std::string> & { return _enum_options; }
    auto enum_options(std::vector<std::string> value) -> setting_schema & { _enum_options = std::move(value); return *this; }

    auto validator(std::function<bool(const setting_value &)> value) -> setting_schema & { _validator = std::move(value); return *this; }

    // methods ----------------------------------------------------------------
    auto is_valid(const setting_value &val) const -> bool;
    auto to_display_format() const -> std::vector<std::string>;

private:
    std::string _key;
    std::string _title;
    std::string _description;
    setting_value _default_value;
    std::type_index _type = typeid(std::string);

    // enumeration constraints
    std::vector<std::string> _enum_options;

    // validator
    std::function<bool(const setting_value &)> _validator;
};

class settings_manager {
public:
    // ctor & dtor ------------------------------------------------------------

    explicit settings_manager() = default;

    // accessors --------------------------------------------------------------
    auto get(const std::string &key) const -> setting_value;

    template<typename T>
    auto set(const std::string &key, T val) -> bool {
        return set(key, setting_value(val));
    }

    auto set(const std::string &key, const setting_value &value) -> bool;

    // methods ----------------------------------------------------------------
    auto active_values() const -> const std::unordered_map<std::string, setting_value> &;
    bool register_schema(const setting_schema &schema);
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
    std::unordered_map<std::string, setting_value> _values;          // Flat value store ($O(1)$)
    std::unordered_map<std::string, setting_schema> _schema_registry; // Schema metadata ($O(1)$)
};

} // namespace turbubestia::settings

