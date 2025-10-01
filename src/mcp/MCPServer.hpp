#pragma once

#include "ITransport.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <atomic>
#include <nlohmann/json.hpp>

namespace ts_mcp {

using json = nlohmann::json;

/**
 * @brief Metadata for an MCP tool
 */
struct ToolInfo {
    std::string name;
    std::string description;
    json input_schema;  // JSON Schema for tool arguments
};

/**
 * @brief Function signature for tool execution
 * @param args JSON object with tool arguments
 * @return JSON result or error
 */
using ToolHandler = std::function<json(const json& args)>;

/**
 * @brief MCP Server implementing JSON-RPC 2.0 protocol
 *
 * Handles tool registration and request routing.
 * Supports methods: tools/list, tools/call
 */
class MCPServer {
public:
    /**
     * @brief Construct MCP server with transport
     * @param transport Unique pointer to transport implementation
     */
    explicit MCPServer(std::unique_ptr<ITransport> transport);

    /**
     * @brief Register a tool with handler
     * @param info Tool metadata with JSON schema
     * @param handler Function to execute when tool is called
     */
    void register_tool(const ToolInfo& info, ToolHandler handler);

    /**
     * @brief Start server main loop
     *
     * Blocks until stop() is called or transport closes.
     * Reads requests, dispatches to handlers, sends responses.
     */
    void run();

    /**
     * @brief Signal server to stop gracefully
     */
    void stop();

private:
    /**
     * @brief Handle incoming JSON-RPC request
     * @param request JSON-RPC request message
     * @return JSON-RPC response message
     */
    json handle_request(const json& request);

    /**
     * @brief Handle tools/list method
     * @return JSON array of available tools with schemas
     */
    json handle_tools_list();

    /**
     * @brief Handle tools/call method
     * @param params Request parameters with tool name and arguments
     * @return Tool execution result
     */
    json handle_tools_call(const json& params);

    /**
     * @brief Handle initialize method (MCP handshake)
     * @param params Client capabilities and info
     * @return Server capabilities and info
     */
    json handle_initialize(const json& params);

    /**
     * @brief Handle notifications/initialized notification
     * @param params Notification parameters (unused)
     */
    void handle_initialized_notification(const json& params);

    /**
     * @brief Create JSON-RPC error response
     * @param id Request ID (or null)
     * @param code Error code
     * @param message Error message
     * @return JSON-RPC error response
     */
    json create_error_response(const json& id, int code, const std::string& message);

    std::unique_ptr<ITransport> transport_;
    std::map<std::string, ToolInfo> tools_;
    std::map<std::string, ToolHandler> handlers_;
    std::atomic<bool> running_{false};
    bool initialized_{false};
};

} // namespace ts_mcp
