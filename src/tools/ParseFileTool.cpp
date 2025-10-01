#include "ParseFileTool.hpp"
#include "core/PathResolver.hpp"
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
        "Parse C++ file(s) and return metadata (class count, function count, errors)",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"oneOf", json::array({
                        {{"type", "string"}, {"description", "Single file or directory path"}},
                        {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Multiple file or directory paths"}}
                    })}
                }},
                {"recursive", {
                    {"type", "boolean"},
                    {"default", true},
                    {"description", "Recursively scan directories for C++ files"}
                }},
                {"file_patterns", {
                    {"type", "array"},
                    {"items", {{"type", "string"}}},
                    {"default", json::array({"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx"})},
                    {"description", "File patterns to include (glob patterns)"}
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

    // Extract parameters
    bool recursive = args.value("recursive", true);
    std::vector<std::string> patterns = args.value("file_patterns",
        std::vector<std::string>{"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx"});

    // Determine filepath type and collect paths
    std::vector<std::string> input_paths;
    if (args["filepath"].is_string()) {
        input_paths.push_back(args["filepath"].get<std::string>());
    } else if (args["filepath"].is_array()) {
        input_paths = args["filepath"].get<std::vector<std::string>>();
    } else {
        return {
            {"error", "filepath must be a string or array of strings"}
        };
    }

    spdlog::debug("ParseFileTool: resolving {} paths", input_paths.size());

    // Resolve paths using PathResolver
    auto resolved_files = PathResolver::resolve_paths(input_paths, recursive, patterns);

    if (resolved_files.empty()) {
        return {
            {"error", "No C++ files found at specified path(s)"},
            {"success", false}
        };
    }

    spdlog::debug("ParseFileTool: analyzing {} files", resolved_files.size());

    try {
        // For single file - return old format for backward compatibility
        if (resolved_files.size() == 1) {
            return analyzer_->analyze_file(resolved_files[0]);
        }

        // For multiple files - return batch format
        return analyzer_->analyze_files(resolved_files);
    } catch (const std::exception& e) {
        spdlog::error("ParseFileTool error: {}", e.what());
        return {
            {"error", e.what()},
            {"success", false}
        };
    }
}

} // namespace ts_mcp
