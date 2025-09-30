#include "FindClassesTool.hpp"
#include <spdlog/spdlog.h>

namespace ts_mcp {

FindClassesTool::FindClassesTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)) {
    if (!analyzer_) {
        throw std::invalid_argument("Analyzer cannot be null");
    }
}

ToolInfo FindClassesTool::get_info() {
    return {
        "find_classes",
        "Find all class declarations in a C++ file with their names and line numbers",
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

json FindClassesTool::execute(const json& args) {
    if (!args.contains("filepath")) {
        return {
            {"error", "Missing required parameter: filepath"}
        };
    }

    std::string filepath = args["filepath"];
    spdlog::debug("FindClassesTool: analyzing file: {}", filepath);

    try {
        auto result = analyzer_->find_classes(filepath);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("FindClassesTool error: {}", e.what());
        return {
            {"error", e.what()}
        };
    }
}

} // namespace ts_mcp
