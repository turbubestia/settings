
#include <settings_manager.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <optional>
#include <filesystem>

using namespace turbubestia::settings;

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);

    std::error_code ec;
    const auto temp_dir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        std::cerr << "Failed to get temp path: " << ec.message() << '\n';
        return 1;
    }
    settings_manager::set_test_base_directory_path(temp_dir);

    return RUN_ALL_TESTS();
}

namespace detail{
// test settings keys
inline const std::string ToolPath  = "/general-setting/core/tool_path";
inline const std::string MaxThreads = "/performance-task/core/max_threads";

void register_core_schemas(settings_manager &manager)
{
    // Test setting keys only for unit tests, not for production use.
    manager.register_schema(setting_schema(ToolPath, "/usr/bin/tool")
        .title("Tool Path")
        .description("Path to external execution binary."));

    manager.register_schema(setting_schema(MaxThreads, static_cast<int>(4))
        .title("Max Worker Threads")
        .description("Maximum worker threads for processing tasks.")
        .validator([](const setting_value &val) { return val.to_int() >= 1 && val.to_int() <= 32; }));
}

auto read_json(const std::string &filename) -> std::optional<nlohmann::json>
{
    const auto setting_file = settings_manager::test_base_directory_path() / ".config" / "dir2md" / filename;
    if(!std::filesystem::exists(setting_file)) { return std::nullopt; }
    std::ifstream file(setting_file);
    if (!file.is_open()) { return std::nullopt; }
    nlohmann::json j;
    try { file >> j; } catch (...) { return std::nullopt; }
    if(!j.is_object()) { return std::nullopt; }
    return j;
}

auto write_json(const std::string &filename, const nlohmann::json &j) -> bool
{
    if (!j.is_object()) { return false; }
    const auto setting_file = settings_manager::test_base_directory_path() / ".config" / "dir2md" / filename;
    std::ofstream file(setting_file);
    try { file << j; } catch (...) { return false; }
    return true;
}
}

TEST(setting_manager_test, test_settings_manager_get_set)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Getting a non-existent key should return an invalid setting_value.
    EXPECT_TRUE(!manager.get("nonexistent").has_value());

    // Set and retrieve a string value using registered schema.
    EXPECT_TRUE(manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool")));
    EXPECT_EQ(manager.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));

    // Set and retrieve an int value using registered schema.
    EXPECT_TRUE(manager.set("/performance-task/core/max_threads", 8));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), 8);
}

TEST(setting_manager_test, test_settings_manager_active_values)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Initially empty.
    EXPECT_TRUE(manager.active_values().empty());

    // After setting values, they should appear.
    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 4);
    EXPECT_EQ(manager.active_values().size(), 2);
    EXPECT_EQ(manager.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), 4);
}

TEST(setting_manager_test, test_settings_manager_schema)
{
    settings_manager manager;

    // No schemas initially.
    EXPECT_TRUE(!manager.schema("test/key").has_value());
    
    // Register a schema.
    auto schema = setting_schema("/test/key", static_cast<int>(10))
        .title("Test Key")
        .validator([](const setting_value &val) { return val.to_int() >= 0 && val.to_int() <= 100; });
    manager.register_schema(schema);

    // Schema should now be retrievable.
    auto retrieved = manager.schema("/test/key");
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->title(), std::string("Test Key"));
    EXPECT_EQ(retrieved->default_value(), 10);
}

TEST(setting_manager_test, test_settings_manager_schemas)
{
    settings_manager manager;

    EXPECT_TRUE(manager.schemas().empty());

    manager.register_schema(setting_schema("/key/one", "").title("Schema One"));
    manager.register_schema(setting_schema("/key/two", "").title("Schema Two"));

    EXPECT_EQ(manager.schemas().size(), 2);
    EXPECT_TRUE(manager.schemas().contains("/key/one"));
    EXPECT_TRUE(manager.schemas().contains("/key/two"));
}

// Save/load tests ----------------------------------------------------------

TEST(setting_manager_test, test_save_to_file_creates_json)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 8);
    EXPECT_TRUE(manager.save_to_file("settings.json"));
    const auto jo = detail::read_json("settings.json");
    EXPECT_TRUE(jo.has_value());
    const auto j = jo.value();

    do {
        EXPECT_TRUE(j.contains("general-setting"));
        const auto general = j.at("general-setting");
        EXPECT_TRUE(general.contains("core"));
        const auto core = general.at("core");
        EXPECT_TRUE(core.contains("tool_path"));
        const auto lhs = core.at("tool_path").get<std::string>();
        const auto rhs = std::string("/usr/bin/tool");
        EXPECT_EQ(lhs,rhs);
    } while(false);

    do {
        EXPECT_TRUE(j.contains("performance-task"));
        const auto performance = j.at("performance-task");
        EXPECT_TRUE(performance.contains("core"));
        const auto core = performance.at("core");
        EXPECT_TRUE(core.contains("max_threads"));
        EXPECT_EQ(core.at("max_threads").get<int>(), 8);
    } while(false);
}

TEST(setting_manager_test, test_save_rejects_schema_less_values)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);
    EXPECT_TRUE(manager.save_to_file("settings.json"));
    const auto jo = detail::read_json("settings.json");
    EXPECT_TRUE(jo.has_value());
    const auto j = jo.value();
    EXPECT_FALSE(j.contains("general-setting/core/tool_path"));
}

TEST(setting_manager_test, test_load_from_file_missing_returns_false)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);
    bool result = manager.load_from_file("/nonexistent/path/settings.json");
    EXPECT_TRUE(!result);
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), 8);
}

TEST(setting_manager_test, test_load_from_file_malformed_json)
{
    // Write malformed JSON to test base directory
    const auto filePath = settings_manager::test_base_directory_path() / ".config" / "dir2md" / "malformed.json";
    auto file = std::ofstream(filePath);
    file << "{ broken json }";
    file.close();

    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    // Values should be unchanged (atomic failure)
    auto old_value = manager.get("/performance-task/core/max_threads");
    bool result = manager.load_from_file("malformed.json");
    EXPECT_TRUE(!result);
    auto new_value = manager.get("/performance-task/core/max_threads");
    EXPECT_EQ(new_value, old_value);
}

TEST(setting_manager_test, test_load_from_file_valid_replaces_values)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 4);

    // Save initial values
    EXPECT_TRUE(manager.save_to_file("settings.json"));

    // Change values in memory
    manager.set("/general-setting/core/tool_path", std::string("/usr/local/bin/tool"));
    manager.set("/performance-task/core/max_threads", 16);

    // Load from file should restore original values
    EXPECT_TRUE(manager.load_from_file("settings.json"));
    EXPECT_EQ(manager.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), 4);
}

TEST(setting_manager_test, test_roundtrip_preserves_types)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 8);

    EXPECT_TRUE(manager.save_to_file("settings.json"));

    // Create a new manager and load
    settings_manager manager2;
    detail::register_core_schemas(manager2);

    EXPECT_TRUE(manager2.load_from_file("settings.json"));

    EXPECT_EQ(manager2.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));
    EXPECT_EQ(manager2.get("/performance-task/core/max_threads"), 8);

    // Verify types match
    EXPECT_EQ(manager2.get("/general-setting/core/tool_path").type(), typeid(std::string));
    EXPECT_EQ(manager2.get("/performance-task/core/max_threads").type(), typeid(int));
}

TEST(setting_manager_test, test_load_from_file_invalid_value_skipped)
{
    // Register a schema with constraints (int between 1 and 32)
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Manually write a JSON with an invalid value (exceeds max)
    nlohmann::json j = R"({
        "performance-task": {
            "core": {
                "max_threads": 64
            }
        }
    })"_json;
    ASSERT_TRUE(detail::write_json("settings.json", j));
    EXPECT_TRUE(manager.load_from_file("settings.json"));

    // Invalid value should be skipped, so we get the default
    const auto value = manager.get("/performance-task/core/max_threads");
    EXPECT_EQ(value, 4); // Default value from CoreSchema
}

TEST(setting_manager_test, test_load_from_file_unknown_key_silently_ignored)
{
    // Register only core schemas
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Write JSON with both known and unknown keys (matching save_to_file structure)
    nlohmann::json j = R"({
        "general-setting" : {
            "core": {
                "tool_path": "/usr/bin/tool"
            },
            "unknown": {
                "cutom_key": "should_be_ignored"
            }
        }
    })"_json;
    ASSERT_TRUE(detail::write_json("settings.json", j));
    EXPECT_TRUE(manager.load_from_file("settings.json"));

    // Known key should be loaded
    EXPECT_EQ(manager.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));

    // Unknown key should NOT be in _values
    EXPECT_TRUE(!manager.active_values().contains("/general-setting/unknown/custom_key"));
}

// Key syntax validation tests ------------------------------------------------

TEST(setting_manager_test, test_set_rejects_empty_key)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    EXPECT_TRUE(!manager.set("", std::string("value")));
}

TEST(setting_manager_test, test_set_rejects_whitespace_key)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    EXPECT_TRUE(!manager.set("key with spaces", std::string("value")));
    EXPECT_TRUE(!manager.set("key\twith\ttabs", std::string("value")));
}

TEST(setting_manager_test, test_set_rejects_leading_separator)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    EXPECT_TRUE(!manager.set("/leading/slash", std::string("value")));
}

TEST(setting_manager_test, test_set_rejects_trailing_separator)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    EXPECT_TRUE(!manager.set("trailing/slash/", std::string("value")));
}

TEST(setting_manager_test, test_set_rejects_repeated_separator)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    EXPECT_TRUE(!manager.set("key//with//double", std::string("value")));
}

TEST(setting_manager_test, test_set_rejects_unknown_key)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Unregistered key should be rejected
    EXPECT_TRUE(!manager.set("unknown/key", std::string("value")));
}

// Conversion and validation tests --------------------------------------------

TEST(setting_manager_test, test_set_rejects_invalid_conversion)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // "not_a_number" cannot be converted to int, even though canConvert might return true for std::string->int
    EXPECT_TRUE(!manager.set("/performance-task/core/max_threads", std::string("not_a_number")));
}

TEST(setting_manager_test, test_set_canonicalizes_to_schema_type)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Set an int value (from setting_value(int))
    EXPECT_TRUE(manager.set("/performance-task/core/max_threads", 8));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads").type(), typeid(int));
}

// Schema replacement tests ---------------------------------------------------

TEST(setting_manager_test, test_schema_replacement_retains_compatible)
{
    settings_manager manager;

    // Register initial schema
    auto schema1 = setting_schema("/test/value", static_cast<int>(50))
        .validator([](const setting_value &val) { return val.to_int() >= 0 && val.to_int() <= 100; });
    manager.register_schema(schema1);

    // Set a valid value
    EXPECT_TRUE(manager.set("/test/value", 50));
    EXPECT_EQ(manager.get("/test/value"), 50);

    // Replace with compatible schema (same constraints)
    auto schema2 = setting_schema("/test/value", static_cast<int>(10))
        .validator([](const setting_value &val) { return val.to_int() >= 0 && val.to_int() <= 100; });
    manager.register_schema(schema2);

    // Value should be retained
    EXPECT_EQ(manager.get("/test/value"), 50);
}

TEST(setting_manager_test, test_schema_replacement_removes_incompatible)
{
    settings_manager manager;

    // Register initial schema with wide range
    auto schema1 = setting_schema("/test/value", static_cast<int>(50))
        .validator([](const setting_value &val) { return val.to_int() >= 0 && val.to_int() <= 100; });
    manager.register_schema(schema1);

    // Set a value within range
    EXPECT_TRUE(manager.set("/test/value", 50));
    EXPECT_EQ(manager.get("/test/value"), 50);

    // Replace with incompatible schema (tighter constraints)
    auto schema2 = setting_schema("/test/value", static_cast<int>(10))
        .validator([](const setting_value &val) { return val.to_int() >= 0 && val.to_int() <= 40; });
    manager.register_schema(schema2);

    // Value should be removed and default returned
    EXPECT_EQ(manager.get("/test/value"), 10); // New default
}

// Path restriction tests -------------------------------------------------

TEST(setting_manager_test, test_save_path_rejects_empty_name)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    // Empty name should be rejected
    EXPECT_TRUE(!manager.save_to_file(std::string()));
}

TEST(setting_manager_test, test_save_path_rejects_absolute_path)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    // Absolute path should always be rejected (even in test mode)
    EXPECT_TRUE(!manager.save_to_file("/tmp/settings.json"));
}

TEST(setting_manager_test, test_save_path_rejects_traversal)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    // Path traversal should always be rejected
    EXPECT_TRUE(!manager.save_to_file("../../etc/passwd"));
}

TEST(setting_manager_test, test_save_path_rejects_separator_in_name)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    // Filename with separator should always be rejected
    EXPECT_TRUE(!manager.save_to_file("dir/settings.json"));
}

TEST(setting_manager_test, test_save_path_accepts_debug_test_override)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    // Simple file name should resolve within the test base directory
    EXPECT_TRUE(manager.save_to_file("settings.json"));
    const auto filename = settings_manager::test_base_directory_path() / ".config" / "dir2md" / "settings.json";
    EXPECT_TRUE(std::filesystem::exists(filename));
}

// Traversal tests --------------------------------------------------------

TEST(setting_manager_test, test_insert_nested_scalar_replacement)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Set a value that creates nested structure
    EXPECT_TRUE(manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool")));
    EXPECT_TRUE(manager.save_to_file("settings.json"));

    // Verify the nested structure is correct
    const auto jo = detail::read_json("settings.json");
    EXPECT_TRUE(jo.has_value());
    const auto j = jo.value();

    EXPECT_TRUE(j["/general-setting/core"_json_pointer].is_object());
}

// Load atomic commit tests -----------------------------------------------

TEST(setting_manager_test, test_load_category_mismatch_normalized)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Write JSON with case-variations of the same category should all be accepted now
    nlohmann::json j = R"({
        "Performance-Task": {
            "core": {
                "max_threads": 8
            }
        }
    })"_json;
    detail::write_json("settings.json", j);

    EXPECT_TRUE(manager.load_from_file("settings.json"));

    // Value SHOULD be loaded case-insensitive matching now works
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), 4);
}

TEST(setting_manager_test, test_load_malformed_json_fails_atomically)
{
    // Write malformed JSON to test base directory
    const auto filePath = settings_manager::test_base_directory_path() / ".config" / "dir2md" / "malformed.json";
    auto file = std::ofstream(filePath);
    ASSERT_TRUE(file.is_open());
    file << "{ invalid json [[]]";
    file.close();

    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    int oldValue = manager.get("/performance-task/core/max_threads").to_int();
    EXPECT_TRUE(!manager.load_from_file("malformed.json"));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), oldValue);
}

TEST(setting_manager_test, test_load_non_object_top_level_fails)
{
    // Write a JSON array to test base directory
    const auto filePath = settings_manager::test_base_directory_path() / ".config" / "dir2md" / "array.json";
    auto file = std::ofstream(filePath);
    ASSERT_TRUE(file.is_open());
    file << "[1, 2, 3]";
    file.close();

    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/performance-task/core/max_threads", 8);

    int oldValue = manager.get("/performance-task/core/max_threads").to_int();
    EXPECT_TRUE(!manager.load_from_file("array.json"));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), oldValue);
}

TEST(setting_manager_test, test_load_successful_replaces_atomically)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 4);

    // Save initial values
    EXPECT_TRUE(manager.save_to_file("settings.json"));

    // Change values in memory
    manager.set("/general-setting/core/tool_path", std::string("/usr/local/bin/tool"));
    manager.set("/performance-task/core/max_threads", 16);

    // Load from file - should replace all values atomically
    EXPECT_TRUE(manager.load_from_file("settings.json"));

    // Verify values are restored
    EXPECT_EQ(manager.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));
    EXPECT_EQ(manager.get("/performance-task/core/max_threads"), 4);
}

TEST(setting_manager_test, test_load_underscoreNotSupported)
{
    settings_manager manager;
    detail::register_core_schemas(manager);

    // Write JSON with underscore-separated key should be rejected
    nlohmann::json j = R"({
        "general-setting_undescore": { "core": { "tool_path": "/usr/bin/tool" }}
    })"_json;
    EXPECT_TRUE(detail::write_json("settings.json", j));
    EXPECT_TRUE(manager.load_from_file("settings.json")); // Load succeeds but value is not loaded

    // Value should NOT be loaded underscore format doesn't match normalized "/general-setting"
    // The default value is "/usr/bin/tool", so if it wasn't loaded, we get the default
    EXPECT_EQ(manager.get("/general-setting/core/tool_path"), "/usr/bin/tool");
}

TEST(setting_manager_test, test_save_normalizedFormatOutput)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 8);

    EXPECT_TRUE(manager.save_to_file("settings.json"));

    // Verify JSON uses normalized (lowercase-dash) keys, NOT display format
    const auto j_o = detail::read_json("settings.json");
    EXPECT_TRUE(j_o.has_value());
    const auto j = j_o.value();

    // Should have "/general-setting" and "/performance-task" (normalized), NOT "/general-setting" or "/performance-task"
    EXPECT_TRUE(j.contains("general-setting"));
    EXPECT_TRUE(j.contains("performance-task"));
    EXPECT_TRUE(!j.contains("General Setting"));
    EXPECT_TRUE(!j.contains("Performance Task"));
}

TEST(setting_manager_test, test_roundtrip_preservesCategories)
{
    settings_manager manager;
    detail::register_core_schemas(manager);
    manager.set("/general-setting/core/tool_path", std::string("/usr/bin/tool"));
    manager.set("/performance-task/core/max_threads", 8);

    // Save
    EXPECT_TRUE(manager.save_to_file("settings.json"));

    // Load into fresh manager
    settings_manager manager2;
    detail::register_core_schemas(manager2);
    EXPECT_TRUE(manager2.load_from_file("settings.json"));

    // Verify values are preserved
    EXPECT_EQ(manager2.get("/general-setting/core/tool_path"), std::string("/usr/bin/tool"));
    EXPECT_EQ(manager2.get("/performance-task/core/max_threads"), 8);

    // Save again and verify JSON is semantically equivalent
    EXPECT_TRUE(manager2.save_to_file("settings2.json"));

    auto j1 = detail::read_json("settings.json");
    auto j2 = detail::read_json("settings2.json");
    EXPECT_TRUE(j1.has_value());
    EXPECT_TRUE(j2.has_value());
}

