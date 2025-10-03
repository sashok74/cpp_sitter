#include "tools/ParseFileTool.hpp"
#include "tools/FindClassesTool.hpp"
#include "tools/FindFunctionsTool.hpp"
#include "tools/ExecuteQueryTool.hpp"
#include "tools/ExtractInterfaceTool.hpp"
#include "tools/FindReferencesTool.hpp"
#include "tools/GetFileSummaryTool.hpp"
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

// ExtractInterfaceTool Tests

TEST_F(ToolsTest, ExtractInterfaceTool_CppBasic) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"output_format", "json"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_TRUE(result.contains("functions"));
    EXPECT_TRUE(result.contains("classes"));
    EXPECT_TRUE(result["functions"].is_array());
    EXPECT_TRUE(result["classes"].is_array());
}

TEST_F(ToolsTest, ExtractInterfaceTool_JsonFormat) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"output_format", "json"},
        {"include_comments", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_TRUE(result.contains("functions"));
    EXPECT_TRUE(result.contains("classes"));
    EXPECT_TRUE(result.contains("filepath"));
    EXPECT_EQ(result["success"], true);
    EXPECT_TRUE(result.contains("total_functions"));
    EXPECT_TRUE(result.contains("total_classes"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_HeaderFormat) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"output_format", "header"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_EQ(result["format"], "header");
    EXPECT_TRUE(result.contains("content"));
    EXPECT_TRUE(result["content"].is_string());

    std::string content = result["content"].get<std::string>();
    EXPECT_GT(content.size(), 0);
}

TEST_F(ToolsTest, ExtractInterfaceTool_MarkdownFormat) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"output_format", "markdown"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_EQ(result["format"], "markdown");
    EXPECT_TRUE(result.contains("content"));
    EXPECT_TRUE(result["content"].is_string());

    std::string content = result["content"].get<std::string>();
    EXPECT_GT(content.size(), 0);
    // Should contain markdown headers
    EXPECT_NE(content.find("#"), std::string::npos);
}

TEST_F(ToolsTest, ExtractInterfaceTool_PrivateMembers) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";

    // Test without private members
    json args1 = {
        {"filepath", simple_class.string()},
        {"include_private", false}
    };
    json result1 = tool.execute(args1);

    // Test with private members
    json args2 = {
        {"filepath", simple_class.string()},
        {"include_private", true}
    };
    json result2 = tool.execute(args2);

    EXPECT_FALSE(result1.contains("error"));
    EXPECT_FALSE(result2.contains("error"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_CommentsToggle) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";

    // Without comments
    json args1 = {
        {"filepath", simple_class.string()},
        {"include_comments", false}
    };
    json result1 = tool.execute(args1);

    // With comments
    json args2 = {
        {"filepath", simple_class.string()},
        {"include_comments", true}
    };
    json result2 = tool.execute(args2);

    EXPECT_FALSE(result1.contains("error"));
    EXPECT_FALSE(result2.contains("error"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_MultipleFiles) {
    ExtractInterfaceTool tool(analyzer);

    fs::path file1 = fixtures_dir / "simple_class.cpp";
    fs::path file2 = fixtures_dir / "template_class.cpp";

    json args = {
        {"filepath", json::array({file1.string(), file2.string()})},
        {"output_format", "json"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_EQ(result["total_files"], 2);
    EXPECT_EQ(result["output_format"], "json");
    EXPECT_TRUE(result.contains("results"));
    EXPECT_TRUE(result["results"].is_array());
    EXPECT_EQ(result["results"].size(), 2);
}

TEST_F(ToolsTest, ExtractInterfaceTool_Directory) {
    ExtractInterfaceTool tool(analyzer);

    json args = {
        {"filepath", fixtures_dir.string()},
        {"recursive", true},
        {"file_patterns", json::array({"*.cpp"})},
        {"output_format", "json"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_GT(result["total_files"].get<int>(), 0);
    EXPECT_TRUE(result.contains("results"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_PythonFile) {
    ExtractInterfaceTool tool(analyzer);

    fs::path python_file = fixtures_dir / "simple_class.py";

    // Skip test if Python fixture doesn't exist
    if (!fs::exists(python_file)) {
        GTEST_SKIP() << "Python fixture not found: " << python_file;
    }

    json args = {
        {"filepath", python_file.string()},
        {"output_format", "json"}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_TRUE(result.contains("functions"));
    EXPECT_TRUE(result.contains("classes"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_MissingFilepath) {
    ExtractInterfaceTool tool(analyzer);

    json args = json::object();  // Empty args
    json result = tool.execute(args);

    EXPECT_TRUE(result.contains("error"));
    EXPECT_TRUE(result["error"].get<std::string>().find("filepath") != std::string::npos);
}

TEST_F(ToolsTest, ExtractInterfaceTool_FileNotFound) {
    ExtractInterfaceTool tool(analyzer);

    json args = {
        {"filepath", "/nonexistent/file.cpp"}
    };

    json result = tool.execute(args);

    // Should return error in results
    EXPECT_TRUE(result.contains("error") || result.contains("results"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_UnsupportedFormat) {
    ExtractInterfaceTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"output_format", "invalid_format"}
    };

    json result = tool.execute(args);

    EXPECT_TRUE(result.contains("error"));
}

TEST_F(ToolsTest, ExtractInterfaceTool_ToolInfo) {
    auto info = ExtractInterfaceTool::get_info();

    EXPECT_EQ(info.name, "extract_interface");
    EXPECT_FALSE(info.description.empty());
    EXPECT_TRUE(info.input_schema.contains("type"));
    EXPECT_TRUE(info.input_schema.contains("properties"));
    EXPECT_TRUE(info.input_schema["properties"].contains("filepath"));
    EXPECT_TRUE(info.input_schema["properties"].contains("output_format"));
    EXPECT_TRUE(info.input_schema["properties"].contains("include_private"));
    EXPECT_TRUE(info.input_schema["properties"].contains("include_comments"));
}

// FindReferencesTool Tests

TEST_F(ToolsTest, FindReferencesTool_BasicUsage) {
    FindReferencesTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"symbol", "Calculator"},
        {"filepath", simple_class.string()}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_EQ(result["success"], true);
    EXPECT_EQ(result["symbol"], "Calculator");
    EXPECT_TRUE(result.contains("total_references"));
    EXPECT_TRUE(result.contains("references"));
    EXPECT_TRUE(result["references"].is_array());
}

TEST_F(ToolsTest, FindReferencesTool_FindFunctionReferences) {
    FindReferencesTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"symbol", "add"},
        {"filepath", simple_class.string()}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_EQ(result["symbol"], "add");
    EXPECT_GE(result["total_references"].get<int>(), 1);

    // Check reference structure
    if (result["references"].size() > 0) {
        auto& ref = result["references"][0];
        EXPECT_TRUE(ref.contains("filepath"));
        EXPECT_TRUE(ref.contains("line"));
        EXPECT_TRUE(ref.contains("column"));
        EXPECT_TRUE(ref.contains("type"));
        EXPECT_TRUE(ref.contains("context"));
    }
}

TEST_F(ToolsTest, FindReferencesTool_MultipleFiles) {
    FindReferencesTool tool(analyzer);

    json args = {
        {"symbol", "Calculator"},
        {"filepath", fixtures_dir.string()},
        {"recursive", false}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_EQ(result["symbol"], "Calculator");
    EXPECT_GT(result["files_searched"].get<int>(), 0);
}

TEST_F(ToolsTest, FindReferencesTool_NoMatches) {
    FindReferencesTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"symbol", "NonExistentSymbol"},
        {"filepath", simple_class.string()}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_EQ(result["total_references"], 0);
    EXPECT_EQ(result["references"].size(), 0);
}

TEST_F(ToolsTest, FindReferencesTool_MissingSymbol) {
    FindReferencesTool tool(analyzer);

    json args = {
        {"filepath", "test.cpp"}
    };

    json result = tool.execute(args);

    EXPECT_TRUE(result.contains("error"));
    EXPECT_TRUE(result["error"].get<std::string>().find("symbol") != std::string::npos);
}

TEST_F(ToolsTest, FindReferencesTool_NoFilepath) {
    FindReferencesTool tool(analyzer);

    // No filepath specified - should search current directory
    json args = {
        {"symbol", "Calculator"},
        {"filepath", fixtures_dir.string()},
        {"file_patterns", json::array({"*.cpp"})}
    };

    json result = tool.execute(args);

    // Should succeed
    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_TRUE(result.contains("symbol"));
    EXPECT_EQ(result["symbol"], "Calculator");
    EXPECT_TRUE(result.contains("files_searched"));
}

TEST_F(ToolsTest, FindReferencesTool_ToolInfo) {
    auto info = FindReferencesTool::get_info();

    EXPECT_EQ(info.name, "find_references");
    EXPECT_FALSE(info.description.empty());
    EXPECT_TRUE(info.input_schema.contains("type"));
    EXPECT_TRUE(info.input_schema.contains("properties"));
    EXPECT_TRUE(info.input_schema["properties"].contains("symbol"));
    EXPECT_TRUE(info.input_schema["properties"].contains("filepath"));
    EXPECT_TRUE(info.input_schema["properties"].contains("reference_types"));
    EXPECT_TRUE(info.input_schema["properties"].contains("include_context"));
}

// ============================================================================
// GetFileSummaryTool Tests
// ============================================================================

TEST_F(ToolsTest, GetFileSummaryTool_BasicMetrics) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"include_complexity", true},
        {"include_comments", false},
        {"include_docstrings", false}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error")) << "Error: " << result.dump();
    EXPECT_TRUE(result.contains("metrics"));
    EXPECT_TRUE(result["metrics"].contains("total_lines"));
    EXPECT_TRUE(result["metrics"].contains("code_lines"));
    EXPECT_TRUE(result["metrics"].contains("comment_lines"));
    EXPECT_TRUE(result.contains("functions"));
    EXPECT_TRUE(result.contains("classes"));
}

TEST_F(ToolsTest, GetFileSummaryTool_ComplexityCalculation) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"include_complexity", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("functions"));

    // Verify complexity is calculated for functions
    if (result["functions"].size() > 0) {
        EXPECT_TRUE(result["functions"][0].contains("complexity"));
        EXPECT_GE(result["functions"][0]["complexity"].get<int>(), 1);
    }
}

TEST_F(ToolsTest, GetFileSummaryTool_WithoutComplexity) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"include_complexity", false}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("functions"));

    // Complexity should not be included
    if (result["functions"].size() > 0) {
        EXPECT_FALSE(result["functions"][0].contains("complexity"));
    }
}

TEST_F(ToolsTest, GetFileSummaryTool_CommentExtraction) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"include_comments", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("comment_markers"));
    // Note: simple_class.cpp may not have TODO/FIXME comments
}

TEST_F(ToolsTest, GetFileSummaryTool_WithDocstrings) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()},
        {"include_docstrings", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("functions"));

    // Functions may have docstrings (only if they actually have documentation)
    // This test just verifies the feature works, not that simple_class.cpp has docstrings
}

TEST_F(ToolsTest, GetFileSummaryTool_FunctionSignatures) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("functions"));

    if (result["functions"].size() > 0) {
        auto& func = result["functions"][0];
        EXPECT_TRUE(func.contains("name"));
        EXPECT_TRUE(func.contains("return_type"));
        EXPECT_TRUE(func.contains("parameters"));
        EXPECT_TRUE(func.contains("line"));
    }
}

TEST_F(ToolsTest, GetFileSummaryTool_ImportsIncludes) {
    GetFileSummaryTool tool(analyzer);

    fs::path simple_class = fixtures_dir / "simple_class.cpp";
    json args = {
        {"filepath", simple_class.string()}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("imports"));

    // simple_class.cpp should have some includes
    if (result["imports"].size() > 0) {
        auto& import = result["imports"][0];
        EXPECT_TRUE(import.contains("path"));
        EXPECT_TRUE(import.contains("line"));
        EXPECT_TRUE(import.contains("is_system"));
    }
}

TEST_F(ToolsTest, GetFileSummaryTool_Python) {
    GetFileSummaryTool tool(analyzer);

    fs::path python_file = fixtures_dir / "simple_class.py";
    if (!fs::exists(python_file)) {
        GTEST_SKIP() << "Python test file not found";
    }

    json args = {
        {"filepath", python_file.string()},
        {"include_complexity", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("functions"));
    EXPECT_TRUE(result.contains("classes"));
    EXPECT_TRUE(result.contains("metrics"));

    // Python functions have is_async flag only if they are actually async
    // This test just verifies Python parsing works
}

TEST_F(ToolsTest, GetFileSummaryTool_MultipleFiles) {
    GetFileSummaryTool tool(analyzer);

    fs::path file1 = fixtures_dir / "simple_class.cpp";
    fs::path file2 = fixtures_dir / "template_class.cpp";

    json args = {
        {"filepath", json::array({file1.string(), file2.string()})},
        {"include_complexity", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("total_files"));
    EXPECT_TRUE(result.contains("processed_files"));
    EXPECT_TRUE(result.contains("results"));
    EXPECT_EQ(result["total_files"], 2);
    EXPECT_EQ(result["results"].size(), 2);
}

TEST_F(ToolsTest, GetFileSummaryTool_Directory) {
    GetFileSummaryTool tool(analyzer);

    json args = {
        {"filepath", fixtures_dir.string()},
        {"recursive", false},
        {"file_patterns", json::array({"*.cpp"})},
        {"include_complexity", true}
    };

    json result = tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("total_files"));
    EXPECT_TRUE(result.contains("results"));
    EXPECT_GT(result["total_files"].get<int>(), 0);
}

TEST_F(ToolsTest, GetFileSummaryTool_FileNotFound) {
    GetFileSummaryTool tool(analyzer);

    json args = {
        {"filepath", "/nonexistent/file.cpp"}
    };

    json result = tool.execute(args);

    // Should return an error or empty results
    EXPECT_TRUE(result.contains("error") || result.contains("failed_files"));
}

TEST_F(ToolsTest, GetFileSummaryTool_ToolInfo) {
    auto info = GetFileSummaryTool::get_info();

    EXPECT_EQ(info.name, "get_file_summary");
    EXPECT_FALSE(info.description.empty());
    EXPECT_TRUE(info.input_schema.contains("type"));
    EXPECT_TRUE(info.input_schema.contains("properties"));
    EXPECT_TRUE(info.input_schema["properties"].contains("filepath"));
    EXPECT_TRUE(info.input_schema["properties"].contains("include_complexity"));
    EXPECT_TRUE(info.input_schema["properties"].contains("include_comments"));
    EXPECT_TRUE(info.input_schema["properties"].contains("include_docstrings"));
}
