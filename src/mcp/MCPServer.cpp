#include "MCPServer.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace ts_mcp {

MCPServer::MCPServer(std::unique_ptr<ITransport> transport)
    : transport_(std::move(transport)) {
    if (!transport_) {
        throw std::invalid_argument("Transport cannot be null");
    }
    spdlog::info("MCPServer initialized");
}

void MCPServer::register_tool(const ToolInfo& info, ToolHandler handler) {
    if (info.name.empty()) {
        throw std::invalid_argument("Tool name cannot be empty");
    }
    if (!handler) {
        throw std::invalid_argument("Tool handler cannot be null");
    }

    tools_[info.name] = info;
    handlers_[info.name] = std::move(handler);
    spdlog::info("Registered tool: {}", info.name);
}

void MCPServer::run() {
    running_ = true;
    spdlog::info("MCPServer starting main loop");

    while (running_ && transport_->is_open()) {
        try {
            json request = transport_->read_message();

            // Empty message indicates EOF or closed transport
            if (request.empty() || request.is_null()) {
                spdlog::info("Received empty message, stopping server");
                break;
            }

            json response = handle_request(request);

            // Only send response if it's not empty (notifications return empty)
            if (!response.empty() && !response.is_null()) {
                transport_->write_message(response);
            }

        } catch (const std::exception& e) {
            spdlog::error("Error in main loop: {}", e.what());
            // Send generic error response if possible
            try {
                json error_response = create_error_response(json(), -32603,
                    std::string("Internal error: ") + e.what());
                transport_->write_message(error_response);
            } catch (...) {
                spdlog::error("Failed to send error response");
            }
        }
    }

    running_ = false;
    spdlog::info("MCPServer stopped");
}

void MCPServer::stop() {
    spdlog::info("MCPServer stop requested");
    running_ = false;
}

json MCPServer::handle_request(const json& request) {
    // Validate JSON-RPC 2.0 format
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return create_error_response(json(), -32600, "Invalid Request: missing or invalid jsonrpc field");
    }

    json id = request.value("id", json());

    if (!request.contains("method")) {
        return create_error_response(id, -32600, "Invalid Request: missing method field");
    }

    std::string method = request["method"];
    json params = request.value("params", json::object());

    spdlog::debug("Handling request: method={}, id={}", method, id.dump());

    try {
        if (method == "initialize") {
            json result = handle_initialize(params);
            initialized_ = true;
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", result}
            };
        } else if (method == "notifications/initialized") {
            // This is a notification (no response expected)
            handle_initialized_notification(params);
            return json();  // Return empty to skip response
        } else if (method == "tools/list") {
            json result = handle_tools_list();
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", result}
            };
        } else if (method == "tools/call") {
            json result = handle_tools_call(params);
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", result}
            };
        } else {
            return create_error_response(id, -32601, "Method not found: " + method);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling method {}: {}", method, e.what());
        return create_error_response(id, -32603, std::string("Internal error: ") + e.what());
    }
}

json MCPServer::handle_tools_list() {
    json tools_array = json::array();

    for (const auto& [name, info] : tools_) {
        tools_array.push_back({
            {"name", info.name},
            {"description", info.description},
            {"inputSchema", info.input_schema}
        });
    }

    spdlog::debug("Returning {} tools", tools_array.size());
    return {{"tools", tools_array}};
}

json MCPServer::handle_tools_call(const json& params) {
    if (!params.contains("name")) {
        throw std::invalid_argument("Missing required parameter: name");
    }

    std::string tool_name = params["name"];
    json arguments = params.value("arguments", json::object());

    spdlog::debug("Calling tool: {} with args: {}", tool_name, arguments.dump());

    auto handler_it = handlers_.find(tool_name);
    if (handler_it == handlers_.end()) {
        throw std::invalid_argument("Unknown tool: " + tool_name);
    }

    // Execute tool handler
    json result = handler_it->second(arguments);

    return {
        {"content", json::array({
            {
                {"type", "text"},
                {"text", result.dump()}
            }
        })}
    };
}

json MCPServer::handle_initialize(const json& params) {
    spdlog::info("Handling initialize request");

    // Extract client info if provided
    if (params.contains("clientInfo")) {
        std::string client_name = params["clientInfo"].value("name", "unknown");
        std::string client_version = params["clientInfo"].value("version", "unknown");
        spdlog::info("Client: {} version {}", client_name, client_version);
    }

    // Return server capabilities
    return {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"tools", json::object()}  // We support tools
        }},
        {"serverInfo", {
            {"name", "tree-sitter-mcp"},
            {"version", "1.0.0"}
        }}
    };
}

void MCPServer::handle_initialized_notification(const json& params) {
    spdlog::info("Client sent initialized notification, server is ready");
    // Nothing to do for notifications - they don't expect responses
}

json MCPServer::create_error_response(const json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}

} // namespace ts_mcp
