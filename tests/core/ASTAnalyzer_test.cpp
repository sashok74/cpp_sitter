#include <gtest/gtest.h>
#include "core/ASTAnalyzer.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

using namespace ts_mcp;

// Test 1: AnalyzeFile - analyze simple_class.cpp, check JSON structure
TEST(ASTAnalyzerTest, AnalyzeFile) {
    ASTAnalyzer analyzer;
    std::filesystem::path fixture_path = "../fixtures/simple_class.cpp";

    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Fixture file should exist at " << fixture_path;

    json result = analyzer.analyze_file(fixture_path);

    // Check JSON structure
    ASSERT_TRUE(result.contains("success")) << "Result should have 'success' field";
    EXPECT_TRUE(result["success"].get<bool>()) << "Analysis should succeed";

    ASSERT_TRUE(result.contains("filepath")) << "Result should have 'filepath' field";
    EXPECT_FALSE(result["filepath"].get<std::string>().empty());

    ASSERT_TRUE(result.contains("has_errors")) << "Result should have 'has_errors' field";
    EXPECT_FALSE(result["has_errors"].get<bool>()) << "simple_class.cpp should have no errors";

    ASSERT_TRUE(result.contains("class_count")) << "Result should have 'class_count' field";
    EXPECT_EQ(result["class_count"].get<int>(), 1) << "Should find 1 class (Calculator)";

    ASSERT_TRUE(result.contains("function_count")) << "Result should have 'function_count' field";
    EXPECT_GE(result["function_count"].get<int>(), 2) << "Should find at least 2 functions";
}

// Test 2: FindClasses - find classes, verify JSON output format
TEST(ASTAnalyzerTest, FindClasses) {
    ASTAnalyzer analyzer;
    std::filesystem::path fixture_path = "../fixtures/simple_class.cpp";

    ASSERT_TRUE(std::filesystem::exists(fixture_path));

    json result = analyzer.find_classes(fixture_path);

    // Check success
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    // Check classes array
    ASSERT_TRUE(result.contains("classes")) << "Result should have 'classes' array";
    ASSERT_TRUE(result["classes"].is_array()) << "'classes' should be a JSON array";

    json classes = result["classes"];
    ASSERT_GT(classes.size(), 0u) << "Should find at least one class";

    // Verify first class structure
    const auto& first_class = classes[0];
    EXPECT_TRUE(first_class.contains("capture_name")) << "Class should have 'capture_name'";
    EXPECT_TRUE(first_class.contains("line")) << "Class should have 'line'";
    EXPECT_TRUE(first_class.contains("column")) << "Class should have 'column'";
    EXPECT_TRUE(first_class.contains("text")) << "Class should have 'text'";

    // Verify the class name is "Calculator"
    EXPECT_EQ(first_class["text"].get<std::string>(), "Calculator")
        << "First class should be named 'Calculator'";
}

// Test 3: FindFunctions - find functions in simple_class.cpp
TEST(ASTAnalyzerTest, FindFunctions) {
    ASTAnalyzer analyzer;
    std::filesystem::path fixture_path = "../fixtures/simple_class.cpp";

    ASSERT_TRUE(std::filesystem::exists(fixture_path));

    json result = analyzer.find_functions(fixture_path);

    // Check success
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    // Check functions array
    ASSERT_TRUE(result.contains("functions")) << "Result should have 'functions' array";
    ASSERT_TRUE(result["functions"].is_array()) << "'functions' should be a JSON array";

    json functions = result["functions"];
    EXPECT_GE(functions.size(), 2u) << "Should find at least 2 functions (add, subtract)";

    // Verify at least one function has expected structure
    if (functions.size() > 0) {
        const auto& first_func = functions[0];
        EXPECT_TRUE(first_func.contains("capture_name"));
        EXPECT_TRUE(first_func.contains("line"));
        EXPECT_TRUE(first_func.contains("column"));
        EXPECT_TRUE(first_func.contains("text"));
    }
}

// Test 4: CacheValidation - test that cache speeds up second call
TEST(ASTAnalyzerTest, CacheValidation) {
    ASTAnalyzer analyzer;
    std::filesystem::path fixture_path = "../fixtures/simple_class.cpp";

    ASSERT_TRUE(std::filesystem::exists(fixture_path));

    // First call - should parse and cache
    EXPECT_EQ(analyzer.cache_size(), 0u) << "Cache should be empty initially";

    json result1 = analyzer.analyze_file(fixture_path);
    EXPECT_TRUE(result1["success"].get<bool>());
    EXPECT_EQ(analyzer.cache_size(), 1u) << "Cache should have 1 entry after first parse";

    // Second call - should use cache
    json result2 = analyzer.analyze_file(fixture_path);
    EXPECT_TRUE(result2["success"].get<bool>());
    EXPECT_EQ(analyzer.cache_size(), 1u) << "Cache size should remain 1";

    // Results should be identical
    EXPECT_EQ(result1["class_count"], result2["class_count"])
        << "Both calls should return same class_count";
    EXPECT_EQ(result1["function_count"], result2["function_count"])
        << "Both calls should return same function_count";

    // Clear cache and verify
    analyzer.clear_cache();
    EXPECT_EQ(analyzer.cache_size(), 0u) << "Cache should be empty after clear";

    // Third call - should re-parse
    json result3 = analyzer.analyze_file(fixture_path);
    EXPECT_TRUE(result3["success"].get<bool>());
    EXPECT_EQ(analyzer.cache_size(), 1u) << "Cache should have 1 entry after re-parse";
}

// Test 5: ExecuteCustomQuery - execute custom query string
TEST(ASTAnalyzerTest, ExecuteCustomQuery) {
    ASTAnalyzer analyzer;
    std::filesystem::path fixture_path = "../fixtures/with_includes.cpp";

    ASSERT_TRUE(std::filesystem::exists(fixture_path));

    // Execute custom query to find includes
    std::string custom_query = "(preproc_include) @include";
    json result = analyzer.execute_query(fixture_path, custom_query);

    // Check success
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    // Check matches array
    ASSERT_TRUE(result.contains("matches")) << "Result should have 'matches' array";
    ASSERT_TRUE(result["matches"].is_array()) << "'matches' should be a JSON array";

    json matches = result["matches"];
    EXPECT_GE(matches.size(), 4u) << "Should find at least 4 include directives";

    // Verify match structure
    if (matches.size() > 0) {
        const auto& first_match = matches[0];
        EXPECT_TRUE(first_match.contains("capture_name"));
        EXPECT_TRUE(first_match.contains("line"));
        EXPECT_TRUE(first_match.contains("column"));
        EXPECT_TRUE(first_match.contains("text"));
        EXPECT_EQ(first_match["capture_name"].get<std::string>(), "include");
    }
}

// Test 6: FileNotFound - expect error on nonexistent file
TEST(ASTAnalyzerTest, FileNotFound) {
    ASTAnalyzer analyzer;
    std::filesystem::path nonexistent = "../fixtures/does_not_exist.cpp";

    ASSERT_FALSE(std::filesystem::exists(nonexistent))
        << "Test file should not exist";

    json result = analyzer.analyze_file(nonexistent);

    // Should return error result
    ASSERT_TRUE(result.contains("success"));
    EXPECT_FALSE(result["success"].get<bool>()) << "Should fail for nonexistent file";

    ASSERT_TRUE(result.contains("error")) << "Result should have 'error' field";
    EXPECT_FALSE(result["error"].get<std::string>().empty())
        << "Error message should not be empty";
}

// Test 7: AnalyzeMultipleFiles - batch analyze multiple files
TEST(ASTAnalyzerTest, AnalyzeMultipleFiles) {
    ASTAnalyzer analyzer;
    std::vector<std::filesystem::path> files = {
        "../fixtures/simple_class.cpp",
        "../fixtures/template_class.cpp"
    };

    for (const auto& file : files) {
        ASSERT_TRUE(std::filesystem::exists(file))
            << "Fixture file should exist: " << file;
    }

    json result = analyzer.analyze_files(files);

    // Check aggregated response structure
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    ASSERT_TRUE(result.contains("total_files"));
    EXPECT_EQ(result["total_files"].get<int>(), 2);

    ASSERT_TRUE(result.contains("processed_files"));
    EXPECT_EQ(result["processed_files"].get<int>(), 2);

    ASSERT_TRUE(result.contains("failed_files"));
    EXPECT_EQ(result["failed_files"].get<int>(), 0);

    // Check results array
    ASSERT_TRUE(result.contains("results"));
    ASSERT_TRUE(result["results"].is_array());
    EXPECT_EQ(result["results"].size(), 2u);

    // Verify each result has expected structure
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("success"));
        EXPECT_TRUE(file_result.contains("filepath"));
        EXPECT_TRUE(file_result.contains("class_count"));
        EXPECT_TRUE(file_result.contains("function_count"));
    }
}

// Test 8: FindClassesInMultipleFiles - batch find classes
TEST(ASTAnalyzerTest, FindClassesInMultipleFiles) {
    ASTAnalyzer analyzer;
    std::vector<std::filesystem::path> files = {
        "../fixtures/simple_class.cpp",
        "../fixtures/template_class.cpp"
    };

    json result = analyzer.find_classes_in_files(files);

    // Check aggregated response
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    ASSERT_TRUE(result.contains("total_files"));
    EXPECT_EQ(result["total_files"].get<int>(), 2);

    ASSERT_TRUE(result.contains("results"));
    ASSERT_TRUE(result["results"].is_array());
    EXPECT_EQ(result["results"].size(), 2u);

    // Verify each result has classes array
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("success"));
        EXPECT_TRUE(file_result.contains("classes"));
        EXPECT_TRUE(file_result["classes"].is_array());
    }
}

// Test 9: FindFunctionsInMultipleFiles - batch find functions
TEST(ASTAnalyzerTest, FindFunctionsInMultipleFiles) {
    ASTAnalyzer analyzer;
    std::vector<std::filesystem::path> files = {
        "../fixtures/simple_class.cpp",
        "../fixtures/template_class.cpp"
    };

    json result = analyzer.find_functions_in_files(files);

    // Check aggregated response
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    ASSERT_TRUE(result.contains("results"));
    ASSERT_TRUE(result["results"].is_array());

    // Verify each result has functions array
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("success"));
        EXPECT_TRUE(file_result.contains("functions"));
        EXPECT_TRUE(file_result["functions"].is_array());
    }
}

// Test 10: ExecuteQueryOnMultipleFiles - batch execute custom query
TEST(ASTAnalyzerTest, ExecuteQueryOnMultipleFiles) {
    ASTAnalyzer analyzer;
    std::vector<std::filesystem::path> files = {
        "../fixtures/simple_class.cpp",
        "../fixtures/template_class.cpp"
    };

    std::string query = "(class_specifier) @class";
    json result = analyzer.execute_query_on_files(files, query);

    // Check aggregated response
    ASSERT_TRUE(result.contains("success"));
    EXPECT_TRUE(result["success"].get<bool>());

    ASSERT_TRUE(result.contains("results"));
    ASSERT_TRUE(result["results"].is_array());

    // Verify each result has matches array
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("success"));
        EXPECT_TRUE(file_result.contains("matches"));
        EXPECT_TRUE(file_result["matches"].is_array());
    }
}
