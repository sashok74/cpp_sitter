#pragma once

#include "core/TreeSitterParser.hpp"
#include "core/QueryEngine.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <optional>

using json = nlohmann::json;

namespace ts_mcp {

/**
 * @brief Cached parse result for a file
 */
struct CachedFile {
    std::unique_ptr<Tree> tree;
    std::string source;
    std::filesystem::file_time_type mtime;
};

/**
 * @brief High-level API for C++ code analysis
 *
 * Provides file-level caching and JSON serialization of analysis results.
 * Uses TreeSitterParser for parsing and QueryEngine for querying.
 */
class ASTAnalyzer {
public:
    /**
     * @brief Construct a new ASTAnalyzer
     */
    ASTAnalyzer();

    /**
     * @brief Analyze a C++ file and return metadata
     * @param filepath Path to the file to analyze
     * @return JSON object with metadata (class_count, function_count, has_errors)
     */
    json analyze_file(const std::filesystem::path& filepath);

    /**
     * @brief Find all class declarations in a file
     * @param filepath Path to the file
     * @return JSON array of classes with names and locations
     */
    json find_classes(const std::filesystem::path& filepath);

    /**
     * @brief Find all function definitions in a file
     * @param filepath Path to the file
     * @return JSON array of functions with signatures and locations
     */
    json find_functions(const std::filesystem::path& filepath);

    /**
     * @brief Find all #include directives in a file
     * @param filepath Path to the file
     * @return JSON array of includes with locations
     */
    json find_includes(const std::filesystem::path& filepath);

    /**
     * @brief Execute a custom query on a file
     * @param filepath Path to the file
     * @param query_string S-expression query string
     * @return JSON array of query matches
     */
    json execute_query(const std::filesystem::path& filepath, std::string_view query_string);

    /**
     * @brief Clear the file cache
     */
    void clear_cache();

    /**
     * @brief Get the number of cached files
     */
    size_t cache_size() const { return cache_.size(); }

private:
    TreeSitterParser parser_;
    QueryEngine query_engine_;
    std::map<std::filesystem::path, CachedFile> cache_;

    /**
     * @brief Get or parse a file (with caching)
     * @param filepath Path to the file
     * @return Optional containing tree and source, or nullopt on error
     */
    std::optional<std::pair<const Tree*, std::string_view>> get_or_parse_file(
        const std::filesystem::path& filepath
    );

    /**
     * @brief Check if cached file is still valid
     * @param filepath Path to the file
     * @param cached Cached file data
     * @return true if cache is valid
     */
    bool is_cache_valid(const std::filesystem::path& filepath, const CachedFile& cached) const;

    /**
     * @brief Convert query matches to JSON array
     */
    json matches_to_json(const std::vector<QueryMatch>& matches) const;
};

} // namespace ts_mcp
