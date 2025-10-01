#include "tools/ParseFileTool.hpp"
#include "tools/FindClassesTool.hpp"
#include "tools/FindFunctionsTool.hpp"
#include "tools/ExecuteQueryTool.hpp"
#include <gtest/gtest.h>
#include <filesystem>

using namespace ts_mcp;
using json = nlohmann::json;
namespace fs = std::filesystem;

class ToolsTest : public ::testing::Test {
protected:
    void SetUp() override {
        analyzer = std::make_shared<ASTAnalyzer>();

        // Determine fixtures path relative to test executable
        fs::path test_dir = fs::path(__FILE__).parent_path();
        fixtures_dir = test_dir / ".." / "fixtures";

        // Ensure fixtures exist
        ASSERT_TRUE(fs::exists(fixtures_dir)) << "Fixtures directory not found: " << fixtures_dir;
    }

    std::shared_ptr<ASTAnalyzer> analyzer;
    fs::path fixtures_dir;
};

TEST_F(ToolsTest, ParseFileTool_Success) {
    ParseFileTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {{"filepath", simple_class.string()}};

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_EQ(result["success"], true);
    EXPECT_EQ(result["class_count"], 1);
    EXPECT_EQ(result["function_count"], 2);
    EXPECT_EQ(result["has_errors"], false);
}

TEST_F(ToolsTest, ParseFileTool_FileNotFound) {
    ParseFileTool tool(analyzer);

    json args = {{"filepath", "/nonexistent/file.cpp"}};
    json result = tool.execute(args);

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(ToolsTest, ParseFileTool_MissingParameter) {
    ParseFileTool tool(analyzer);

    json args = json::object();  // Empty args
    json result = tool.execute(args);

    EXPECT_TRUE(result.contains("error"));
    EXPECT_TRUE(result["error"].get<std::string>().find("filepath") != std::string::npos);
}

TEST_F(ToolsTest, FindClassesTool_MultipleClasses) {
    FindClassesTool tool(analyzer);

    fs::path template_class = fixtures_dir / "template_class.cpp";
    json args = {{"filepath", template_class.string()}};

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("classes"));
    EXPECT_TRUE(result["classes"].is_array());

    // template_class.cpp has multiple classes
    EXPECT_GT(result["classes"].size(), 0);
}

TEST_F(ToolsTest, FindFunctionsTool_Success) {
    FindFunctionsTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {{"filepath", simple_class.string()}};

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("functions"));
    EXPECT_TRUE(result["functions"].is_array());
    EXPECT_GT(result["functions"].size(), 0);
}

TEST_F(ToolsTest, ExecuteQueryTool_CustomQuery) {
    ExecuteQueryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"query", "(class_specifier name: (type_identifier) @class_name)"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("matches"));
    EXPECT_TRUE(result["matches"].is_array());
}

TEST_F(ToolsTest, ExecuteQueryTool_InvalidQuery) {
    ExecuteQueryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"query", "(invalid_syntax"}  // Malformed query
    };

    json result = tool.execute(args);

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(ToolsTest, ExecuteQueryTool_MissingParameters) {
    ExecuteQueryTool tool(analyzer);

    // Missing query parameter
    json args1 = {{"filepath", "test.cpp"}};
    json result1 = tool.execute(args1);
    EXPECT_TRUE(result1.contains("error"));

    // Missing filepath parameter
    json args2 = {{"query", "(class_specifier)"}};
    json result2 = tool.execute(args2);
    EXPECT_TRUE(result2.contains("error"));
}

TEST_F(ToolsTest, ToolInfoSchemas) {
    // Verify all tools return proper schema info
    auto parse_info = ParseFileTool::get_info();
    EXPECT_EQ(parse_info.name, "parse_file");
    EXPECT_FALSE(parse_info.description.empty());
    EXPECT_TRUE(parse_info.input_schema.contains("type"));
    EXPECT_TRUE(parse_info.input_schema.contains("properties"));

    auto find_classes_info = FindClassesTool::get_info();
    EXPECT_EQ(find_classes_info.name, "find_classes");

    auto find_functions_info = FindFunctionsTool::get_info();
    EXPECT_EQ(find_functions_info.name, "find_functions");

    auto execute_query_info = ExecuteQueryTool::get_info();
    EXPECT_EQ(execute_query_info.name, "execute_query");
}

// NEW TESTS: Batch operations with multiple files

TEST_F(ToolsTest, ParseFileTool_MultipleFiles) {
    ParseFileTool tool(analyzer);

    fs::path file1 = fixtures_dir / "simple_class.cpp";
    fs::path file2 = fixtures_dir / "template_class.cpp";

    json args = {
        {"filepath", json::array({file1.string(), file2.string()})}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Should not have error: " << result.dump();
    EXPECT_EQ(result["success"], true);
    EXPECT_EQ(result["total_files"], 2);
    EXPECT_EQ(result["processed_files"], 2);
    EXPECT_EQ(result["failed_files"], 0);

    EXPECT_TRUE(result.contains("results"));
    EXPECT_TRUE(result["results"].is_array());
    EXPECT_EQ(result["results"].size(), 2);
}

TEST_F(ToolsTest, ParseFileTool_DirectoryRecursive) {
    ParseFileTool tool(analyzer);

    json args = {
        {"filepath", fixtures_dir.string()},
        {"recursive", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Should not have error: " << result.dump();
    // Should find multiple .cpp files in fixtures
    EXPECT_GT(result["total_files"].get<int>(), 1);
}

TEST_F(ToolsTest, FindClassesTool_MultipleFiles) {
    FindClassesTool tool(analyzer);

    fs::path file1 = fixtures_dir / "simple_class.cpp";
    fs::path file2 = fixtures_dir / "template_class.cpp";

    json args = {
        {"filepath", json::array({file1.string(), file2.string()})}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_EQ(result["success"], true);
    EXPECT_EQ(result["total_files"], 2);

    EXPECT_TRUE(result.contains("results"));
    EXPECT_TRUE(result["results"].is_array());
    EXPECT_EQ(result["results"].size(), 2);

    // Each result should have classes array
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("classes"));
        EXPECT_TRUE(file_result["classes"].is_array());
    }
}

TEST_F(ToolsTest, FindClassesTool_DirectoryRecursive) {
    FindClassesTool tool(analyzer);

    json args = {
        {"filepath", fixtures_dir.string()},
        {"recursive", false},  // Non-recursive
        {"file_patterns", json::array({"*.cpp"})}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_GT(result["total_files"].get<int>(), 0);
}

TEST_F(ToolsTest, FindFunctionsTool_MultipleFiles) {
    FindFunctionsTool tool(analyzer);

    fs::path file1 = fixtures_dir / "simple_class.cpp";
    fs::path file2 = fixtures_dir / "template_class.cpp";

    json args = {
        {"filepath", json::array({file1.string(), file2.string()})}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_EQ(result["success"], true);
    EXPECT_EQ(result["total_files"], 2);

    EXPECT_TRUE(result.contains("results"));
    EXPECT_TRUE(result["results"].is_array());

    // Each result should have functions array
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("functions"));
        EXPECT_TRUE(file_result["functions"].is_array());
    }
}

TEST_F(ToolsTest, FindFunctionsTool_DirectoryRecursive) {
    FindFunctionsTool tool(analyzer);

    json args = {
        {"filepath", fixtures_dir.string()},
        {"recursive", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_GT(result["total_files"].get<int>(), 0);
}

TEST_F(ToolsTest, ExecuteQueryTool_MultipleFiles) {
    ExecuteQueryTool tool(analyzer);

    fs::path file1 = fixtures_dir / "simple_class.cpp";
    fs::path file2 = fixtures_dir / "template_class.cpp";

    json args = {
        {"filepath", json::array({file1.string(), file2.string()})},
        {"query", "(class_specifier) @class"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_EQ(result["success"], true);
    EXPECT_EQ(result["total_files"], 2);

    EXPECT_TRUE(result.contains("results"));
    EXPECT_TRUE(result["results"].is_array());

    // Each result should have matches array
    for (const auto& file_result : result["results"]) {
        EXPECT_TRUE(file_result.contains("matches"));
        EXPECT_TRUE(file_result["matches"].is_array());
    }
}

TEST_F(ToolsTest, ExecuteQueryTool_DirectoryRecursive) {
    ExecuteQueryTool tool(analyzer);

    json args = {
        {"filepath", fixtures_dir.string()},
        {"query", "(function_definition) @func"},
        {"recursive", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_GT(result["total_files"].get<int>(), 0);
}
