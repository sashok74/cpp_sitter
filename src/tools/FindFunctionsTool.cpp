#include "FindFunctionsTool.hpp"
#include <spdlog/spdlog.h>

namespace ts_mcp {

FindFunctionsTool::FindFunctionsTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)) {
    if (!analyzer_) {
        throw std::invalid_argument("Analyzer cannot be null");
    }
}

ToolInfo FindFunctionsTool::get_info() {
    return {
        "find_functions",
        "Find all function definitions in a C++ file with their names and line numbers",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"type", "string"},
                    {"description", "Absolute or relative path to the C++ file to analyze"}
                }}
            }},
            {"required", json::array({"filepath"})}
        }
    };
}

json FindFunctionsTool::execute(const json& args) {
    if (!args.contains("filepath")) {
        return {
            {"error", "Missing required parameter: filepath"}
        };
    }

    std::string filepath = args["filepath"];
    spdlog::debug("FindFunctionsTool: analyzing file: {}", filepath);

    try {
        auto result = analyzer_->find_functions(filepath);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("FindFunctionsTool error: {}", e.what());
        return {
            {"error", e.what()}
        };
    }
}

} // namespace ts_mcp
