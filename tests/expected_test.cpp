
#include <expected.hpp>

#include <gtest/gtest.h>

using namespace turbubestia::settings;

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ============================================================================
// expected<T> success state tests

TEST(expected_test, test_make_expected_has_value)
{
    auto exp = expected<int>::make_expected(42);
    EXPECT_TRUE(exp.has_value());
}

TEST(expected_test, test_make_expected_value_returns_stored) {
    auto exp = expected<int>::make_expected(42);
    EXPECT_EQ(exp.value(), 42);
}

TEST(expected_test, test_make_expected_value_or_returns_value) {
    auto exp = expected<std::string>::make_expected("hello");
    EXPECT_EQ(exp.value_or("default"), std::string("hello"));
}

// ============================================================================
// expected<T> error state tests

TEST(expected_test, test_make_expected_error_no_value) {
    auto exp = expected<int>::make_expected_error(404, "Not found");
    EXPECT_TRUE(!exp.has_value());
}

TEST(expected_test, test_make_expected_error_code_and_desc) {
    auto exp = expected<int>::make_expected_error(500, "Server error");
    EXPECT_EQ(exp.error().error_code, 500);
    EXPECT_EQ(exp.error().description, std::string("Server error"));
}

TEST(expected_test, test_make_expected_error_value_or_returns_default) {
    auto exp = expected<std::string>::make_expected_error(404, "Not found");
    EXPECT_EQ(exp.value_or("default"), std::string("default"));
}

TEST(expected_test, test_make_expected_error_value_throws) {
    auto exp = expected<int>::make_expected_error(500, "Server error");
    EXPECT_THROW(exp.value(), std::runtime_error);
}

// ============================================================================
// Copy semantics tests
// ============================================================================

TEST(expected_test, test_copy_success_state) {
    auto original = expected<int>::make_expected(42);
    auto copy = original;
    EXPECT_TRUE(copy.has_value());
    EXPECT_EQ(copy.value(), 42);
}

TEST(expected_test, test_copy_error_state) {
    auto original = expected<int>::make_expected_error(500, "Error");
    auto copy = original;
    EXPECT_TRUE(!copy.has_value());
    EXPECT_EQ(copy.error().error_code, 500);
}

// ============================================================================
// Move semantics tests
// ============================================================================

TEST(expected_test, test_move_success_state) {
    auto original = expected<std::string>::make_expected("moved");
    auto moved = std::move(original);
    EXPECT_TRUE(moved.has_value());
    EXPECT_EQ(moved.value(), std::string("moved"));
}

TEST(expected_test, test_move_error_state) {
    auto original = expected<int>::make_expected_error(500, "Error");
    auto moved = std::move(original);
    EXPECT_TRUE(!moved.has_value());
    EXPECT_EQ(moved.error().error_code, 500);
}

// ============================================================================
// error_frame tests
// ============================================================================

TEST(expected_test, test_error_frame_source_location) {
    error_frame frame(42, "Test error");
    EXPECT_EQ(frame.error_code, 42);
    EXPECT_EQ(frame.description, std::string("Test error"));
    // source_location should capture this file and a non-zero line
    EXPECT_TRUE(frame.location.line() > 0);
}

// ============================================================================
// error_stack tests
// ============================================================================

TEST(expected_test, test_error_stack_push_and_frames) {
    error_stack stack;
    stack.push(error_frame(1, "First"));
    stack.push(error_frame(2, "Second"));

    auto frames = stack.frames();
    EXPECT_EQ(static_cast<int>(frames.size()), 2);
    EXPECT_EQ(frames[0].error_code, 1);
    EXPECT_EQ(frames[1].error_code, 2);
}

TEST(expected_test, test_error_stack_clear) {
    error_stack stack;
    stack.push(error_frame(1, "First"));
    stack.clear();
    EXPECT_TRUE(stack.frames().empty());
}

TEST(expected_test, test_error_stack_to_string) {
    error_stack stack;
    stack.push(error_frame(404, "Not found"));

    std::string output = stack.to_string();
    EXPECT_TRUE(!output.empty());
    EXPECT_TRUE(output.find("404") != std::string::npos);
    EXPECT_TRUE(output.find("Not found") != std::string::npos);
}