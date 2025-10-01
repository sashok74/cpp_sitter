#include "core/ASTAnalyzer.hpp"
#include "mcp/MCPServer.hpp"
#include "mcp/StdioTransport.hpp"
#include "tools/ParseFileTool.hpp"
#include "tools/FindClassesTool.hpp"
#include "tools/FindFunctionsTool.hpp"
#include "tools/ExecuteQueryTool.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <csignal>
#include <memory>
#include <atomic>

namespace {
    std::atomic<bool> shutdown_requested{false};
    ts_mcp::MCPServer* global_server = nullptr;

    void signal_handler(int signal) {
        spdlog::info("Received signal {}, shutting down gracefully", signal);
        shutdown_requested = true;
        if (global_server) {
            global_server->stop();
        }
    }

    void setup_signal_handlers() {
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
    }
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    CLI::App app{"MCP Stdio Server - Tree-sitter Code Analysis (C++ & Python)"};

    std::string log_level = "info";
    app.add_option("-l,--log-level", log_level, "Log level (trace, debug, info, warn, error, critical)")
        ->default_val("info");

    bool version = false;
    app.add_flag("-v,--version", version, "Print version information");

    CLI11_PARSE(app, argc, argv);

    if (version) {
        std::cout << "tree-sitter-mcp version 1.0.0" << std::endl;
        return 0;
    }

    // Configure logging
    if (log_level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (log_level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (log_level == "info") {
        spdlog::set_level(spdlog::level::info);
    } else if (log_level == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (log_level == "error") {
        spdlog::set_level(spdlog::level::err);
    } else if (log_level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else {
        std::cerr << "Invalid log level: " << log_level << std::endl;
        return 1;
    }

    spdlog::info("Starting MCP Stdio Server");
    spdlog::info("Log level: {}", log_level);

    try {
        // Setup signal handlers for graceful shutdown
        setup_signal_handlers();

        // Create core components
        auto analyzer = std::make_shared<ts_mcp::ASTAnalyzer>();
        auto transport = std::make_unique<ts_mcp::StdioTransport>();
        auto server = std::make_unique<ts_mcp::MCPServer>(std::move(transport));

        // Store global reference for signal handler
        global_server = server.get();

        // Create and register tools
        auto parse_tool = std::make_shared<ts_mcp::ParseFileTool>(analyzer);
        server->register_tool(
            ts_mcp::ParseFileTool::get_info(),
            [parse_tool](const nlohmann::json& args) {
                return parse_tool->execute(args);
            }
        );

        auto find_classes_tool = std::make_shared<ts_mcp::FindClassesTool>(analyzer);
        server->register_tool(
            ts_mcp::FindClassesTool::get_info(),
            [find_classes_tool](const nlohmann::json& args) {
                return find_classes_tool->execute(args);
            }
        );

        auto find_functions_tool = std::make_shared<ts_mcp::FindFunctionsTool>(analyzer);
        server->register_tool(
            ts_mcp::FindFunctionsTool::get_info(),
            [find_functions_tool](const nlohmann::json& args) {
                return find_functions_tool->execute(args);
            }
        );

        auto execute_query_tool = std::make_shared<ts_mcp::ExecuteQueryTool>(analyzer);
        server->register_tool(
            ts_mcp::ExecuteQueryTool::get_info(),
            [execute_query_tool](const nlohmann::json& args) {
                return execute_query_tool->execute(args);
            }
        );

        spdlog::info("All tools registered, starting server");

        // Run server (blocks until stopped)
        server->run();

        global_server = nullptr;
        spdlog::info("Server stopped cleanly");
        return 0;

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }
}
