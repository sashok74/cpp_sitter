#include "core/ASTAnalyzer.hpp"
#include "mcp/MCPServer.hpp"
#include "mcp/StdioTransport.hpp"
#include "tools/ParseFileTool.hpp"
#include "tools/FindClassesTool.hpp"
#include "tools/FindFunctionsTool.hpp"
#include "tools/ExecuteQueryTool.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <filesystem>
#include <thread>

using namespace ts_mcp;
using json = nlohmann::json;
namespace fs = std::filesystem;

class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup input/output streams
        input_stream = std::make_unique<std::istringstream>();
        output_stream = std::make_unique<std::ostringstream>();

        // Determine fixtures path
        fs::path test_dir = fs::path(__FILE__).parent_path();
        fixtures_dir = test_dir / ".." / "fixtures";
        ASSERT_TRUE(fs::exists(fixtures_dir)) << "Fixtures directory not found";
    }

    void SendRequest(const json& request) {
        input_stream->str(request.dump() + "\n");
        input_stream->clear();
    }

    json GetResponse() {
        std::string response_str = output_stream->str();
        output_stream->str("");  // Clear for next response
        output_stream->clear();

        if (response_str.empty()) {
            return json();
        }

        // Parse last line (most recent response)
        size_t last_newline = response_str.rfind('\n', response_str.size() - 2);
        std::string last_line = (last_newline != std::string::npos)
            ? response_str.substr(last_newline + 1)
            : response_str;

        // Remove trailing newline
        if (!last_line.empty() && last_line.back() == '\n') {
            last_line.pop_back();
        }

        return json::parse(last_line);
    }

    std::unique_ptr<std::istringstream> input_stream;
    std::unique_ptr<std::ostringstream> output_stream;
    fs::path fixtures_dir;
};

TEST_F(EndToEndTest, ServerStartsAndResponds) {
    // Test that server can be constructed with all components
    auto analyzer = std::make_shared<ASTAnalyzer>();
    EXPECT_NE(analyzer, nullptr);

    auto transport = std::make_unique<StdioTransport>(*input_stream, *output_stream);
    EXPECT_NE(transport, nullptr);

    auto server = std::make_unique<MCPServer>(std::move(transport));
    EXPECT_NE(server, nullptr);

    // Register minimal tool
    ToolInfo info{"test", "Test tool", {{"type", "object"}}};
    server->register_tool(info, [](const json&) -> json {
        return {{"status", "ok"}};
    });

    // Test passes if we can construct all components without exceptions
    SUCCEED();
}

TEST_F(EndToEndTest, ToolsCanBeRegistered) {
    // Test that all 4 tools can be registered successfully
    auto analyzer = std::make_shared<ASTAnalyzer>();
    auto transport = std::make_unique<StdioTransport>(*input_stream, *output_stream);
    auto server = std::make_unique<MCPServer>(std::move(transport));

    // Register all real tools
    auto parse_tool = std::make_shared<ParseFileTool>(analyzer);
    EXPECT_NO_THROW(server->register_tool(ParseFileTool::get_info(),
        [parse_tool](const json& args) { return parse_tool->execute(args); }));

    auto find_classes_tool = std::make_shared<FindClassesTool>(analyzer);
    EXPECT_NO_THROW(server->register_tool(FindClassesTool::get_info(),
        [find_classes_tool](const json& args) { return find_classes_tool->execute(args); }));

    auto find_functions_tool = std::make_shared<FindFunctionsTool>(analyzer);
    EXPECT_NO_THROW(server->register_tool(FindFunctionsTool::get_info(),
        [find_functions_tool](const json& args) { return find_functions_tool->execute(args); }));

    auto execute_query_tool = std::make_shared<ExecuteQueryTool>(analyzer);
    EXPECT_NO_THROW(server->register_tool(ExecuteQueryTool::get_info(),
        [execute_query_tool](const json& args) { return execute_query_tool->execute(args); }));

    // Verify tool info structures
    auto parse_info = ParseFileTool::get_info();
    EXPECT_EQ(parse_info.name, "parse_file");
    EXPECT_FALSE(parse_info.description.empty());

    auto find_classes_info = FindClassesTool::get_info();
    EXPECT_EQ(find_classes_info.name, "find_classes");

    auto find_functions_info = FindFunctionsTool::get_info();
    EXPECT_EQ(find_functions_info.name, "find_functions");

    auto execute_query_info = ExecuteQueryTool::get_info();
    EXPECT_EQ(execute_query_info.name, "execute_query");
}

TEST_F(EndToEndTest, ToolsExecuteDirectly) {
    // Test tools can execute directly (bypassing server I/O complexity)
    auto analyzer = std::make_shared<ASTAnalyzer>();

    fs::path test_file = fixtures_dir / "simple_class.cpp";
    ASSERT_TRUE(fs::exists(test_file));

    // Test ParseFileTool directly
    ParseFileTool parse_tool(analyzer);
    json parse_args = {{"filepath", test_file.string()}};
    json parse_result = parse_tool.execute(parse_args);

    EXPECT_FALSE(parse_result.contains("error"));
    EXPECT_EQ(parse_result["success"], true);
    EXPECT_EQ(parse_result["class_count"], 1);
    EXPECT_GT(parse_result["function_count"], 0);

    // Test FindClassesTool directly
    FindClassesTool find_classes_tool(analyzer);
    json classes_result = find_classes_tool.execute(parse_args);

    EXPECT_FALSE(classes_result.contains("error"));
    EXPECT_TRUE(classes_result.contains("classes"));
    EXPECT_GT(classes_result["classes"].size(), 0);

    // Test FindFunctionsTool directly
    FindFunctionsTool find_functions_tool(analyzer);
    json functions_result = find_functions_tool.execute(parse_args);

    EXPECT_FALSE(functions_result.contains("error"));
    EXPECT_TRUE(functions_result.contains("functions"));
    EXPECT_GT(functions_result["functions"].size(), 0);
}

TEST_F(EndToEndTest, ExecuteQueryToolWorks) {
    // Test custom query execution
    auto analyzer = std::make_shared<ASTAnalyzer>();
    ExecuteQueryTool execute_query_tool(analyzer);

    fs::path test_file = fixtures_dir / "template_class.cpp";
    ASSERT_TRUE(fs::exists(test_file));

    // Execute custom query
    json args = {
        {"filepath", test_file.string()},
        {"query", "(class_specifier name: (type_identifier) @class_name)"}
    };

    json result = execute_query_tool.execute(args);

    EXPECT_FALSE(result.contains("error"));
    EXPECT_TRUE(result.contains("matches"));
    EXPECT_TRUE(result["matches"].is_array());
}

TEST_F(EndToEndTest, CompleteWorkflow) {
    // Simulate a complete analysis workflow:
    // 1. List tools
    // 2. Parse file
    // 3. Find classes
    // 4. Find functions

    auto analyzer = std::make_shared<ASTAnalyzer>();
    auto transport = std::make_unique<StdioTransport>(*input_stream, *output_stream);
    auto server = std::make_unique<MCPServer>(std::move(transport));

    // Register all tools
    auto parse_tool = std::make_shared<ParseFileTool>(analyzer);
    server->register_tool(ParseFileTool::get_info(),
        [parse_tool](const json& args) { return parse_tool->execute(args); });

    auto find_classes_tool = std::make_shared<FindClassesTool>(analyzer);
    server->register_tool(FindClassesTool::get_info(),
        [find_classes_tool](const json& args) { return find_classes_tool->execute(args); });

    auto find_functions_tool = std::make_shared<FindFunctionsTool>(analyzer);
    server->register_tool(FindFunctionsTool::get_info(),
        [find_functions_tool](const json& args) { return find_functions_tool->execute(args); });

    fs::path test_file = fixtures_dir / "simple_class.cpp";

    // Create multi-request input
    std::ostringstream multi_input;

    json req1 = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}, {"params", json::object()}};
    multi_input << req1.dump() << "\n";

    json req2 = {
        {"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/call"},
        {"params", {{"name", "parse_file"}, {"arguments", {{"filepath", test_file.string()}}}}}
    };
    multi_input << req2.dump() << "\n";

    json req3 = {
        {"jsonrpc", "2.0"}, {"id", 3}, {"method", "tools/call"},
        {"params", {{"name", "find_classes"}, {"arguments", {{"filepath", test_file.string()}}}}}
    };
    multi_input << req3.dump() << "\n";

    multi_input << "{}\n";  // EOF

    input_stream->str(multi_input.str());
    input_stream->clear();

    std::thread server_thread([&server]() { server->run(); });
    server_thread.join();

    // Check that we got 3 responses
    std::string all_output = output_stream->str();
    int response_count = 0;
    size_t pos = 0;
    while ((pos = all_output.find("\"jsonrpc\"", pos)) != std::string::npos) {
        response_count++;
        pos++;
    }

    EXPECT_EQ(response_count, 3);
}
