
#include <assert.hpp>
#include <settings_manager.hpp>

#include <nlohmann/json.hpp>

#include <ranges>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace nlohmann {
    template <>
    struct adl_serializer<turbubestia::settings::setting_value> {
        static void to_json(json& j, const turbubestia::settings::setting_value& v) {
            if (v.type() == typeid(bool)) { j = v.to_bool(); return; }
            if (v.type() == typeid(int)) { j = v.to_int(); return; }
            if (v.type() == typeid(double)) { j = v.to_double(); return; }
            if (v.type() == typeid(std::string)) { j = v.to_string(); return; }
        }

        static void from_json(const json& j, turbubestia::settings::setting_value& v) {
            // Standard std::variant deserialization via built-in get
            if (j.is_boolean()) { v = j.get<bool>(); return; }
            if (j.is_number_integer()) { v = j.get<int>(); return; }
            if (j.is_number_float()) { v = j.get<double>(); return; }
            if (j.is_string()) { v = j.get<std::string>(); return; }
        }
    };
}

namespace turbubestia::settings 
{
namespace { namespace detail 
{
auto is_valid_key_syntax(const std::string &key) -> bool
{
    if (key.empty()) return false;
    if (key.find(' ') != std::string::npos) return false;
    if (key.at(0) != '/') { return false; }
    const auto key_path = std::string_view(key).substr(1);
    for (const auto part : key_path | std::views::split('/')) {
        if (part.empty()) return false;
    }
    return true;
}

auto is_within_path(const std::string &_path, const std::string &_root) -> bool
{
    const auto path = std::filesystem::weakly_canonical( std::filesystem::absolute(_path)).string();
    const auto root = std::filesystem::weakly_canonical( std::filesystem::absolute(_root) / "").string();

    // Path is inside root (case-insensitive prefix check)
    if (path.size() < root.size()) return false;

    return std::equal(root.begin(), root.end(), path.begin(), 
        [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        }
    );
}

auto home_directory() -> std::filesystem::path
{
    if (const char *home = std::getenv("USERPROFILE")) return home; // Windows
    if (const char *home = std::getenv("HOME")) return home;        // POSIX
    return std::filesystem::temp_directory_path(); // fallback
}

auto resolve_persistence_path(const std::string &filePath) -> std::string
{
    if (filePath.empty()) return {};
    if (filePath.find_first_of("/\\") != std::string::npos) { return {}; }
    if (std::filesystem::path(filePath).is_absolute()) { return {}; }

    const auto &test_base = settings_manager::test_base_directory();
    const auto config_path = (test_base.empty() ? home_directory() : test_base) / ".config" / "dir2md";
    const auto resolved_path = (config_path / filePath).lexically_normal().string();
    return is_within_path(resolved_path, config_path.string()) ? resolved_path : std::string();
}

auto to_display_format(const std::string &normalized) -> std::string
{
    std::string out;
    out.reserve(normalized.size());

    auto it_end = normalized.end();
    auto state = 0;

    for (auto it = normalized.begin(); it != it_end; ++it) {
        const auto c = static_cast<unsigned char>(*it);
        switch(state) {
            case 0:
                if (std::isalpha(c)) {
                    out += static_cast<unsigned char>(std::toupper(c));
                    state = 1;
                }
                break;
            case 1:
                if (c == '-') {
                    out += ' ';
                    state = 0;
                } else {
                    out += static_cast<unsigned char>(std::tolower(c));
                }
                break;
        }
    }
    return out;
}

auto section(std::string_view str, char sep, int first, int last) noexcept -> std::string_view
{
    if (str.empty()) return {};

    // Count sections
    int count = 1;
    for (char c : str)
        if (c == sep) ++count;

    // Normalize negative indices (count from the end)
    if (first < 0) first += count;
    if (last  < 0) last  += count;

    // Clamp into valid range
    first = std::clamp(first, 0, count - 1);
    last  = std::clamp(last,  0, count - 1);

    if (first > last) return {};

    // Advance to the start of section `first`
    auto it = str.begin();
    for (int i = 0; i < first; ++i) {
        it = std::find(it, str.end(), sep);
        if (it == str.end()) return {};
        ++it;
    }

    // Advance past the end of section `last`
    auto end_it = it;
    for (int i = first; i <= last; ++i) {
        end_it = std::find(end_it, str.end(), sep);
        if (end_it == str.end()) {
            end_it = str.end();
            break;
        }
        ++end_it;
    }

    return {it, --end_it};
}
}} // detail namespace

// ============================================================================
// setting_schema

setting_schema::setting_schema(const std::string &key, std::type_index type)
    : _key(key), _type(type)
{
    RUNTIME_ASSERT(detail::is_valid_key_syntax(key));
}

auto setting_schema::is_valid(const setting_value &val) const -> bool
{
    // First check if conversion is even possible
    if (!val.is_type(_type)) { return false; }

    // now validate against the schema constraints
    if (_validator) { if (!_validator(val)) { return false; } }

    // For enum types: validate exact string membership in enum_options
    if (!_enum_options.empty() && val.is_type(typeid(std::string))) {
        const auto str = val.to_string();
        const auto it = std::ranges::find(_enum_options, str);
        if (it == _enum_options.end()) { return false; }
    }

    return true;
}

// ============================================================================
// settings_manager::accessors

auto settings_manager::get(const std::string &key) const -> const setting_value &
{
    static setting_value no_value;

    // 1. Return user-configured value if present
    const auto it_v = _values.find(key);
    if (it_v != _values.end()) {
        return it_v->second;
    }

    // 2. Fallback to default value from schema
    const auto it_s = _schema_registry.find(key);
    if (it_s != _schema_registry.end()) {
        return it_s->second.default_value();
    }

    // 3. Fallback to invalid if unknown key
    return no_value;
}

auto settings_manager::set(const std::string &key, const setting_value &value) -> bool
{
    // Enforce key syntax validation
    if (!detail::is_valid_key_syntax(key)) {
        return false;
    }

    // Reject unknown keys (must be registered in schema registry)
    const auto it_s = _schema_registry.find(key);
    if (it_s == _schema_registry.end()) { return false; }

    // Validate and canonicalize against the registered schema.
    const auto &schema = _schema_registry.at(key);
    if (!schema.is_valid(value)) { return false; }

    if (!value.is_type(schema.type())) {
        return false;
    }

    // Store value
    auto it = _values.find(key);
    if (it == _values.end()) {
        _values.emplace(key, value);
    } else {
        it->second = value;
    }

    return true;
}

// ============================================================================
// settings_manager::methods

auto settings_manager::active_values() const -> const std::unordered_map<std::string, setting_value> &
{
    return _values;
}

auto settings_manager::register_schema(const setting_schema &_schema) -> void
{
    auto schema = _schema; // Make a copy to modify
    RUNTIME_ASSERT(detail::is_valid_key_syntax(schema.key()));

    // If the key already exists, revalidate its active value against the replacement.
    const auto it_s = _schema_registry.find(schema.key());
    if (it_s != _schema_registry.end()) {
        const auto it_v = _values.find(schema.key());
        if (it_v == _values.end() || !schema.is_valid(it_v->second)) {
            _values.erase(it_v);
        }
        it_s->second = schema;
    } else {
        _schema_registry.emplace(schema.key(), schema);
    }
}

auto settings_manager::schema(const std::string &key) const -> std::optional<setting_schema> 
{
    const auto it = _schema_registry.find(key);
    if (it == _schema_registry.end()) { return std::nullopt; }
    return it->second;
}

auto settings_manager::schemas() const -> const std::unordered_map<std::string, setting_schema> &
{
    return _schema_registry;
}

// ============================================================================
// settings_manager::file serialization

bool settings_manager::save_to_file(const std::string &filePath)
{
    const auto resolved_path = detail::resolve_persistence_path(filePath);
    if (resolved_path.empty()) {
        std::cerr << "Invalid save path:" << filePath << std::endl;
        return false;
    }
    const auto folder_path = std::filesystem::path(resolved_path).parent_path();
    if (!std::filesystem::exists(folder_path) && !std::filesystem::create_directories(folder_path)) {
        std::cerr << "Could not create setting folder path " << folder_path << std::endl;
    }

    // nlohmann::json has an flatten/unflatten method, we can build a simple flatended json
    // and use the builtin unflatten
    nlohmann::json j_flat = _values;
    nlohmann::json j = j_flat.unflatten();

    std::ofstream file(resolved_path);
    if (!file.is_open()) {
        std::cerr << "Fail to open settings.json file for writing." << std::endl;
    }
    file << j.dump(4);
    return true;
}

bool settings_manager::load_from_file(const std::string &filePath)
{
    const auto resolved_path = detail::resolve_persistence_path(filePath);
    if (resolved_path.empty()) {
        std::cerr << "Invalid load path:" << filePath << std::endl;
        return false;
    }

    std::ifstream file(resolved_path);
    nlohmann::json j;
    try {
        file >> j;
    } catch (const std::exception &e) {
        return false;
    }
    
    if (!j.is_object()) { return false; }

    nlohmann::json j_flat = j.flatten();
    for (const auto& [key, value] : j_flat.items()) {
        const auto v = value.get<setting_value>();
        const auto it_s = _schema_registry.find(key);
        if (it_s == _schema_registry.end()) { continue; }
        const auto &schema = it_s->second;
        if (schema.is_valid(v)) { _values[key] = v; }
    }

    return true;
}

// ============================================================================
// settings_manager::test helpers

void settings_manager::set_test_base_directory_path(const std::filesystem::path &path)
{
    test_base_directory_path() = path;
}

void settings_manager::clear_test_base_directory()
{
    test_base_directory_path() = std::filesystem::path("");
}

auto settings_manager::test_base_directory_path() -> std::filesystem::path &
{
    static std::filesystem::path s_testBase;
    return s_testBase;
}

auto settings_manager::test_base_directory() -> std::string
{
    
    return test_base_directory_path().string();
}
} // namespace turbubestia::settings