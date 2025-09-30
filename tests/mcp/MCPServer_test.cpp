#include "mcp/MCPServer.hpp"
#include "MockTransport.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace ts_mcp;
using json = nlohmann::json;

class MCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_transport_raw = new MockTransport();
        auto transport = std::unique_ptr<ITransport>(mock_transport_raw);
        server = std::make_unique<MCPServer>(std::move(transport));
    }

    MockTransport* mock_transport_raw = nullptr;
    std::unique_ptr<MCPServer> server;
};

TEST_F(MCPServerTest, ToolsListEmpty) {
    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"},
        {"params", json::object()}
    };

    mock_transport_raw->push_request(request);
    mock_transport_raw->push_request(json());  // Empty message to signal EOF

    server->run();

    ASSERT_TRUE(mock_transport_raw->has_responses());
    json response = mock_transport_raw->pop_response();

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_TRUE(response.contains("result"));
    EXPECT_TRUE(response["result"]["tools"].is_array());
    EXPECT_EQ(response["result"]["tools"].size(), 0);
}

TEST_F(MCPServerTest, RegisterAndCallTool) {
    // Register a simple tool
    ToolInfo info{
        "test_tool",
        "A test tool",
        {
            {"type", "object"},
            {"properties", {
                {"input", {{"type", "string"}}}
            }}
        }
    };

    bool tool_called = false;
    server->register_tool(info, [&tool_called](const json& args) -> json {
        tool_called = true;
        return {{"result", "success"}, {"input", args["input"]}};
    });

    // Request tools list
    json list_request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/list"},
        {"params", json::object()}
    };

    // Call the tool
    json call_request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "tools/call"},
        {"params", {
            {"name", "test_tool"},
            {"arguments", {{"input", "test_value"}}}
        }}
    };

    mock_transport_raw->push_request(list_request);
    mock_transport_raw->push_request(call_request);
    mock_transport_raw->push_request(json());  // Empty message to signal EOF

    server->run();

    ASSERT_TRUE(mock_transport_raw->has_responses());

    // Check tools/list response
    json list_response = mock_transport_raw->pop_response();
    EXPECT_EQ(list_response["result"]["tools"].size(), 1);
    EXPECT_EQ(list_response["result"]["tools"][0]["name"], "test_tool");

    // Check tools/call response
    json call_response = mock_transport_raw->pop_response();
    EXPECT_EQ(call_response["id"], 2);
    EXPECT_TRUE(call_response.contains("result"));
    EXPECT_TRUE(tool_called);
}

TEST_F(MCPServerTest, CallNonexistentTool) {
    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {
            {"name", "nonexistent_tool"},
            {"arguments", json::object()}
        }}
    };

    mock_transport_raw->push_request(request);
    mock_transport_raw->push_request(json());  // Empty message to signal EOF

    server->run();

    ASSERT_TRUE(mock_transport_raw->has_responses());
    json response = mock_transport_raw->pop_response();

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32603);  // Internal error
}

TEST_F(MCPServerTest, InvalidMethod) {
    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "invalid/method"},
        {"params", json::object()}
    };

    mock_transport_raw->push_request(request);
    mock_transport_raw->push_request(json());  // Empty message to signal EOF

    server->run();

    ASSERT_TRUE(mock_transport_raw->has_responses());
    json response = mock_transport_raw->pop_response();

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], -32601);  // Method not found
}

TEST_F(MCPServerTest, MultipleRequests) {
    // Register tool
    ToolInfo info{"counter", "Counts calls", {{"type", "object"}}};
    int call_count = 0;

    server->register_tool(info, [&call_count](const json&) -> json {
        call_count++;
        return {{"count", call_count}};
    });

    // Send multiple requests
    for (int i = 1; i <= 3; i++) {
        json request = {
            {"jsonrpc", "2.0"},
            {"id", i},
            {"method", "tools/call"},
            {"params", {
                {"name", "counter"},
                {"arguments", json::object()}
            }}
        };
        mock_transport_raw->push_request(request);
    }
    mock_transport_raw->push_request(json());  // Empty message to signal EOF

    server->run();

    EXPECT_EQ(call_count, 3);
    EXPECT_EQ(mock_transport_raw->has_responses(), true);

    // Check all responses received
    int response_count = 0;
    while (mock_transport_raw->has_responses()) {
        json response = mock_transport_raw->pop_response();
        response_count++;
        EXPECT_EQ(response["jsonrpc"], "2.0");
    }
    EXPECT_EQ(response_count, 3);
}
