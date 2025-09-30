#include "ParseFileTool.hpp"
#include <spdlog/spdlog.h>

namespace ts_mcp {

ParseFileTool::ParseFileTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)) {
    if (!analyzer_) {
        throw std::invalid_argument("Analyzer cannot be null");
    }
}

ToolInfo ParseFileTool::get_info() {
    return {
        "parse_file",
        "Parse a C++ file and return metadata (class count, function count, errors)",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"type", "string"},
                    {"description", "Absolute or relative path to the C++ file to parse"}
                }}
            }},
            {"required", json::array({"filepath"})}
        }
    };
}

json ParseFileTool::execute(const json& args) {
    if (!args.contains("filepath")) {
        return {
            {"error", "Missing required parameter: filepath"}
        };
    }

    std::string filepath = args["filepath"];
    spdlog::debug("ParseFileTool: analyzing file: {}", filepath);

    try {
        auto result = analyzer_->analyze_file(filepath);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("ParseFileTool error: {}", e.what());
        return {
            {"error", e.what()}
        };
    }
}

} // namespace ts_mcp
