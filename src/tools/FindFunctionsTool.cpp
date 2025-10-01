#include "FindFunctionsTool.hpp"
#include "core/PathResolver.hpp"
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
        "Find all function definitions in C++ file(s) with their names and line numbers",
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

json FindFunctionsTool::execute(const json& args) {
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

    spdlog::debug("FindFunctionsTool: resolving {} paths", input_paths.size());

    // Resolve paths using PathResolver
    auto resolved_files = PathResolver::resolve_paths(input_paths, recursive, patterns);

    if (resolved_files.empty()) {
        return {
            {"error", "No C++ files found at specified path(s)"},
            {"success", false}
        };
    }

    spdlog::debug("FindFunctionsTool: analyzing {} files", resolved_files.size());

    try {
        // For single file - return old format for backward compatibility
        if (resolved_files.size() == 1) {
            return analyzer_->find_functions(resolved_files[0]);
        }

        // For multiple files - return batch format
        return analyzer_->find_functions_in_files(resolved_files);
    } catch (const std::exception& e) {
        spdlog::error("FindFunctionsTool error: {}", e.what());
        return {
            {"error", e.what()},
            {"success", false}
        };
    }
}

} // namespace ts_mcp
