#include "ExecuteQueryTool.hpp"
#include <spdlog/spdlog.h>

namespace ts_mcp {

ExecuteQueryTool::ExecuteQueryTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)) {
    if (!analyzer_) {
        throw std::invalid_argument("Analyzer cannot be null");
    }
}

ToolInfo ExecuteQueryTool::get_info() {
    return {
        "execute_query",
        "Execute a custom tree-sitter S-expression query on a C++ file",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"type", "string"},
                    {"description", "Absolute or relative path to the C++ file to analyze"}
                }},
                {"query", {
                    {"type", "string"},
                    {"description", "Tree-sitter S-expression query (e.g., '(class_specifier name: (type_identifier) @name)')"}
                }}
            }},
            {"required", json::array({"filepath", "query"})}
        }
    };
}

json ExecuteQueryTool::execute(const json& args) {
    if (!args.contains("filepath")) {
        return {
            {"error", "Missing required parameter: filepath"}
        };
    }

    if (!args.contains("query")) {
        return {
            {"error", "Missing required parameter: query"}
        };
    }

    std::string filepath = args["filepath"];
    std::string query = args["query"];
    spdlog::debug("ExecuteQueryTool: file={}, query={}", filepath, query);

    try {
        auto result = analyzer_->execute_query(filepath, query);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("ExecuteQueryTool error: {}", e.what());
        return {
            {"error", e.what()}
        };
    }
}

} // namespace ts_mcp
