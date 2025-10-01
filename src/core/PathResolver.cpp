#include "PathResolver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <set>
#include <regex>

namespace ts_mcp {

bool PathResolver::is_cpp_file(const std::filesystem::path& path) {
    static const std::set<std::string> cpp_extensions = {
        ".cpp", ".hpp", ".h", ".cc", ".cxx", ".hxx", ".C", ".H"
    };

    auto ext = path.extension().string();
    return cpp_extensions.find(ext) != cpp_extensions.end();
}

bool PathResolver::matches_pattern(const std::filesystem::path& path, const std::string& pattern) {
    // Convert glob pattern to regex
    // Example: "*.cpp" -> ".*\.cpp"
    std::string regex_pattern = pattern;

    // Escape special regex characters except *
    std::string escaped;
    for (char c : regex_pattern) {
        if (c == '*') {
            escaped += ".*";
        } else if (c == '.') {
            escaped += "\\.";
        } else if (c == '?') {
            escaped += ".";
        } else {
            escaped += c;
        }
    }

    std::regex re("^" + escaped + "$");
    return std::regex_match(path.filename().string(), re);
}

void PathResolver::scan_directory(
    const std::filesystem::path& dir,
    bool recursive,
    const std::vector<std::string>& patterns,
    std::vector<std::filesystem::path>& results
) {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        spdlog::warn("Path is not a directory: {}", dir.string());
        return;
    }

    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    const auto& file_path = entry.path();

                    // Check against patterns
                    bool matches = false;
                    for (const auto& pattern : patterns) {
                        if (matches_pattern(file_path, pattern)) {
                            matches = true;
                            break;
                        }
                    }

                    if (matches) {
                        results.push_back(file_path);
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    const auto& file_path = entry.path();

                    // Check against patterns
                    bool matches = false;
                    for (const auto& pattern : patterns) {
                        if (matches_pattern(file_path, pattern)) {
                            matches = true;
                            break;
                        }
                    }

                    if (matches) {
                        results.push_back(file_path);
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("Error scanning directory {}: {}", dir.string(), e.what());
    }
}

std::vector<std::filesystem::path> PathResolver::resolve_paths(
    const std::vector<std::string>& paths,
    bool recursive,
    const std::vector<std::string>& patterns
) {
    std::vector<std::filesystem::path> results;
    std::set<std::filesystem::path> unique_paths; // For deduplication

    for (const auto& path_str : paths) {
        std::filesystem::path path(path_str);

        if (!std::filesystem::exists(path)) {
            spdlog::warn("Path does not exist: {}", path_str);
            continue;
        }

        if (std::filesystem::is_regular_file(path)) {
            // Check if file matches patterns
            bool matches = false;
            for (const auto& pattern : patterns) {
                if (matches_pattern(path, pattern)) {
                    matches = true;
                    break;
                }
            }

            if (matches) {
                unique_paths.insert(std::filesystem::canonical(path));
            } else {
                spdlog::debug("File {} does not match any pattern", path.string());
            }
        } else if (std::filesystem::is_directory(path)) {
            std::vector<std::filesystem::path> dir_results;
            scan_directory(path, recursive, patterns, dir_results);

            // Add to unique set (canonicalized)
            for (const auto& file : dir_results) {
                try {
                    unique_paths.insert(std::filesystem::canonical(file));
                } catch (const std::filesystem::filesystem_error& e) {
                    spdlog::warn("Cannot canonicalize path {}: {}", file.string(), e.what());
                }
            }
        } else {
            spdlog::warn("Path is neither file nor directory: {}", path_str);
        }
    }

    // Convert set to sorted vector
    results.assign(unique_paths.begin(), unique_paths.end());
    std::sort(results.begin(), results.end());

    spdlog::debug("Resolved {} paths from {} input paths", results.size(), paths.size());

    return results;
}

} // namespace ts_mcp
