
#include <fzy.hpp>

#include <gtest/gtest.h>

#include <math.h>

#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY

using namespace turbubestia::settings::fzy;

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ============================================================================
// Single-target has_match tests
// ============================================================================

TEST(fzy_test, test_has_match_empty_needle) {
    // Empty needle always returns zero (never a match).
    EXPECT_EQ(has_match("", "any_target"), 0);
    EXPECT_EQ(has_match("", ""), 0);
    EXPECT_EQ(has_match("", "editor.fontSize"), 0);
}

TEST(fzy_test, test_has_match_non_matching) {
    // Completely non-matching needle returns zero.
    EXPECT_EQ(has_match("xyz", "no_match_here"), 0);
    EXPECT_EQ(has_match("zzz", "abcdef"), 0);
}

TEST(fzy_test, test_has_match_matching) {
    // Valid fuzzy match returns non-zero.
    EXPECT_NE(has_match("edf", "editor.fontSize"), 0);
    EXPECT_NE(has_match("efs", "editor.fontSize"), 0);
    EXPECT_NE(has_match("files", "files.autoSave"), 0);
}

// ============================================================================
// Single-target match score tests
// ============================================================================

TEST(fzy_test, test_match_empty_needle) {
    // Empty needle returns SCORE_MIN.
    EXPECT_EQ(match("", "any_target"), lowest_score_t);
    EXPECT_EQ(match("", ""), lowest_score_t);
}

TEST(fzy_test, test_match_non_matching) {
    // Non-matching target returns SCORE_MIN.
    EXPECT_EQ(match("xyz", "no_match_here"), lowest_score_t);
    EXPECT_EQ(match("zzz", "abcdef"), lowest_score_t);
}

TEST(fzy_test, test_match_matching) {
    // Valid match returns score greater than SCORE_MIN.
    EXPECT_GT(match("edf", "editor.fontSize"), lowest_score_t);
    EXPECT_GT(match("efs", "editor.fontSize"), lowest_score_t);
    EXPECT_GT(match("files", "files.autoSave"), lowest_score_t);
}

// ============================================================================
// Single-target match_positions tests
// ============================================================================

TEST(fzy_test, test_match_positions_empty_needle) {
    // Empty needle returns {SCORE_MIN, {}}.
    auto [score, positions] = match_positions("", "any_target");
    EXPECT_EQ(score, lowest_score_t);
    EXPECT_TRUE(positions.empty());
}

TEST(fzy_test, test_match_positions_non_matching) {
    // Non-match returns {SCORE_MIN, {}}.
    auto [score, positions] = match_positions("xyz", "no_match_here");
    EXPECT_EQ(score, lowest_score_t);
    EXPECT_TRUE(positions.empty());
}

TEST(fzy_test, test_match_positions_matching) {
    // Valid match returns score and position vector with correct size.
    auto [score, positions] = match_positions("edf", "editor.fontSize");
    EXPECT_GT(score, lowest_score_t);
    EXPECT_EQ(static_cast<int>(positions.size()), 3); // One position per needle char.

    // Verify positions identify the correct characters in order.
    std::string haystack = "editor.fontSize";
    EXPECT_EQ(haystack[positions[0]], 'e');
    EXPECT_EQ(haystack[positions[1]], 'd');
    EXPECT_EQ(haystack[positions[2]], 'f');
}

// ============================================================================
// Batch match tests
// ============================================================================

TEST(fzy_test, test_batch_match_sample) {
    // The supplied sample from the plan: "edfont" against three targets.
    std::string needle = "edfont";
    std::vector<std::string> targets = {
        "files.autoSave",
        "terminal.integrated.fontSize",
        "editor.fontSize"
    };

    auto result = match(needle, targets);

    // Expected: indices [2, 1] (editor.fontSize and terminal.integrated.fontSize).
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 2u);
    EXPECT_EQ(result[1], 1u);
}

TEST(fzy_test, test_batch_match_empty_collection) {
    // Empty target vector returns empty result.
    std::vector<std::string> targets;
    auto result = match("test", targets);
    EXPECT_TRUE(result.empty());
}

TEST(fzy_test, test_batch_match_empty_targets) {
    // Non-empty needle against an empty target string does not match.
    std::vector<std::string> targets = {"", "editor.fontSize", "files.autoSave"};
    auto result = match("efs", targets);

    // Empty string should not appear in results.
    for (size_t idx : result) {
        EXPECT_GT(targets[idx].size(), 0u);
    }
}

TEST(fzy_test, test_batch_match_no_matches) {
    // Needle that matches nothing returns empty result.
    std::vector<std::string> targets = {"abc", "def", "ghi"};
    auto result = match("xyz", targets);
    EXPECT_TRUE(result.empty());
}

TEST(fzy_test, test_batch_match_duplicate_targets) {
    // Duplicate target strings are distinguished by original index.
    std::vector<std::string> targets = {"editor.fontSize", "editor.fontSize", "files.autoSave"};
    auto result = match("efs", targets);

    // Both indices 0 and 1 should appear since they have identical content.
    EXPECT_TRUE(result.size() >= 2u);
    bool has_0 = false, has_1 = false;
    for (size_t idx : result) {
        if (idx == 0) has_0 = true;
        if (idx == 1) has_1 = true;
    }
    EXPECT_TRUE(has_0);
    EXPECT_TRUE(has_1);
}

TEST(fzy_test, test_batch_match_equal_scores) {
    // Equal scores retain input order (ascending original index).
    std::vector<std::string> targets = {"abc", "abc", "abc"};
    auto result = match("ac", targets);

    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 0u);
    EXPECT_EQ(result[1], 1u);
    EXPECT_EQ(result[2], 2u);
}

// ============================================================================
// Batch match_positions tests
// ============================================================================

TEST(fzy_test, test_batch_match_positions_sample) {
    // The supplied sample: "edfont" against three targets.
    std::string needle = "edfont";
    std::vector<std::string> targets = {
        "files.autoSave",
        "terminal.integrated.fontSize",
        "editor.fontSize"
    };

    auto result = match_positions(needle, targets);

    // Expected: two pairs indexed 2 and 1.
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].first, 2u); // editor.fontSize
    EXPECT_EQ(result[1].first, 1u); // terminal.integrated.fontSize

    // Verify position vectors match expected offsets.
    // editor.fontSize: "edfont" -> positions [0, 1, 7, 8, 9, 10]
    std::vector<size_t> expected_pos_0 = {0, 1, 7, 8, 9, 10};
    EXPECT_EQ(result[0].second, expected_pos_0);

    // terminal.integrated.fontSize: "edfont" -> positions [17, 18, 20, 21, 22, 23]
    std::vector<size_t> expected_pos_1 = {17, 18, 20, 21, 22, 23};
    EXPECT_EQ(result[1].second, expected_pos_1);
}

TEST(fzy_test, test_batch_match_positions_consistency) {
    // Batch index order matches batch position order for the same input.
    std::string needle = "efs";
    std::vector<std::string> targets = {
        "files.autoSave",
        "terminal.integrated.fontSize",
        "editor.fontSize"
    };

    auto indices = match(needle, targets);
    auto positions = match_positions(needle, targets);

    EXPECT_EQ(indices.size(), positions.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        EXPECT_EQ(indices[i], positions[i].first);
    }
}

TEST(fzy_test, test_batch_match_positions_position_validity) {
    // Every returned position is in range for its indexed target and identifies
    // the corresponding needle character.
    std::string needle = "edfont";
    std::vector<std::string> targets = {
        "files.autoSave",
        "terminal.integrated.fontSize",
        "editor.fontSize"
    };

    auto result = match_positions(needle, targets);

    for (const auto &[idx, positions] : result) {
        const std::string &target = targets[idx];
        EXPECT_EQ(static_cast<size_t>(positions.size()), needle.size());

        for (size_t i = 0; i < positions.size(); ++i) {
            EXPECT_TRUE(positions[i] < target.size());
            EXPECT_EQ(target[positions[i]], needle[i]);
        }
    }
}

// ============================================================================
// Edge case tests
// ============================================================================

TEST(fzy_test, test_needle_longer_than_target) {
    // Needle longer than target does not cause undefined behavior.
    std::string long_needle = "verylongneedle";
    std::string short_target = "ab";

    // Single-target: should not match.
    EXPECT_EQ(has_match(long_needle, short_target), 0);
    EXPECT_EQ(match(long_needle, short_target), lowest_score_t);

    auto [score, positions] = match_positions(long_needle, short_target);
    EXPECT_EQ(score, lowest_score_t);
    EXPECT_TRUE(positions.empty());

    // Batch: should not match.
    std::vector<std::string> targets = {"ab", "cd", "ef"};
    auto result = match(long_needle, targets);
    EXPECT_TRUE(result.empty());

    auto pos_result = match_positions(long_needle, targets);
    EXPECT_TRUE(pos_result.empty());
}

TEST(fzy_test, test_empty_needle_batch_all_overloads) {
    // Empty needle produces empty results from all batch overloads.
    std::vector<std::string> targets = {"abc", "def", "ghi"};

    auto indices = match("", targets);
    EXPECT_TRUE(indices.empty());

    auto positions = match_positions("", targets);
    EXPECT_TRUE(positions.empty());
}
