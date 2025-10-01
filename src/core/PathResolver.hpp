#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ts_mcp {

/**
 * @brief Resolves file paths from mixed input (files and directories)
 *
 * Handles single files, directories, and arrays of paths.
 * Supports recursive directory scanning with file pattern matching.
 */
class PathResolver {
public:
    /**
     * @brief Resolve paths to actual C++ files
     *
     * @param paths Array of file paths or directory paths
     * @param recursive If true, scan directories recursively
     * @param patterns Glob patterns for file filtering (e.g., "*.cpp", "*.hpp")
     * @return Vector of resolved file paths (sorted, no duplicates)
     */
    static std::vector<std::filesystem::path> resolve_paths(
        const std::vector<std::string>& paths,
        bool recursive = true,
        const std::vector<std::string>& patterns = {"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx"}
    );

private:
    /**
     * @brief Check if file has a C++ extension
     */
    static bool is_cpp_file(const std::filesystem::path& path);

    /**
     * @brief Check if filename matches glob pattern
     *
     * Supports simple wildcards: *.cpp, test_*.hpp, etc.
     */
    static bool matches_pattern(const std::filesystem::path& path, const std::string& pattern);

    /**
     * @brief Recursively scan directory for matching files
     */
    static void scan_directory(
        const std::filesystem::path& dir,
        bool recursive,
        const std::vector<std::string>& patterns,
        std::vector<std::filesystem::path>& results
    );
};

} // namespace ts_mcp
