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
    Language language;  // Language of the cached file
};

/**
 * @brief High-level API for multi-language code analysis
 *
 * Provides file-level caching and JSON serialization of analysis results.
 * Supports C++ and Python with automatic language detection from file extensions.
 * Uses TreeSitterParser for parsing and QueryEngine for querying.
 */
class ASTAnalyzer {
public:
    /**
     * @brief Construct a new ASTAnalyzer
     */
    ASTAnalyzer();

    /**
     * @brief Analyze a file and return metadata
     * @param filepath Path to the file to analyze
     * @param lang Optional language override (auto-detected if nullopt)
     * @return JSON object with metadata (class_count, function_count, has_errors, language)
     */
    json analyze_file(const std::filesystem::path& filepath,
                     std::optional<Language> lang = std::nullopt);

    /**
     * @brief Find all class declarations in a file
     * @param filepath Path to the file
     * @param lang Optional language override (auto-detected if nullopt)
     * @return JSON array of classes with names and locations
     */
    json find_classes(const std::filesystem::path& filepath,
                     std::optional<Language> lang = std::nullopt);

    /**
     * @brief Find all function definitions in a file
     * @param filepath Path to the file
     * @param lang Optional language override (auto-detected if nullopt)
     * @return JSON array of functions with signatures and locations
     */
    json find_functions(const std::filesystem::path& filepath,
                       std::optional<Language> lang = std::nullopt);

    /**
     * @brief Find all include/import directives in a file
     * @param filepath Path to the file
     * @param lang Optional language override (auto-detected if nullopt)
     * @return JSON array of includes/imports with locations
     */
    json find_includes(const std::filesystem::path& filepath,
                      std::optional<Language> lang = std::nullopt);

    /**
     * @brief Execute a custom query on a file
     * @param filepath Path to the file
     * @param query_string S-expression query string
     * @param lang Optional language override (auto-detected if nullopt)
     * @return JSON array of query matches
     */
    json execute_query(const std::filesystem::path& filepath,
                      std::string_view query_string,
                      std::optional<Language> lang = std::nullopt);

    /**
     * @brief Analyze multiple C++ files and return aggregated metadata
     * @param filepaths Vector of file paths to analyze
     * @return JSON object with total_files, processed_files, failed_files, and results array
     */
    json analyze_files(const std::vector<std::filesystem::path>& filepaths);

    /**
     * @brief Find all class declarations in multiple files
     * @param filepaths Vector of file paths
     * @return JSON object with aggregated results
     */
    json find_classes_in_files(const std::vector<std::filesystem::path>& filepaths);

    /**
     * @brief Find all function definitions in multiple files
     * @param filepaths Vector of file paths
     * @return JSON object with aggregated results
     */
    json find_functions_in_files(const std::vector<std::filesystem::path>& filepaths);

    /**
     * @brief Execute a custom query on multiple files
     * @param filepaths Vector of file paths
     * @param query_string S-expression query string
     * @return JSON object with aggregated results
     */
    json execute_query_on_files(
        const std::vector<std::filesystem::path>& filepaths,
        std::string_view query_string
    );

    /**
     * @brief Clear the file cache
     */
    void clear_cache();

    /**
     * @brief Get the number of cached files
     */
    size_t cache_size() const { return cache_.size(); }

private:
    std::map<Language, TreeSitterParser> parsers_;  // Parser cache per language
    QueryEngine query_engine_;
    std::map<std::filesystem::path, CachedFile> cache_;

    /**
     * @brief Get or create parser for a specific language
     * @param lang Programming language
     * @return Reference to parser for this language
     */
    TreeSitterParser& get_parser_for_language(Language lang);

    /**
     * @brief Detect language from filepath or use override
     * @param filepath Path to the file
     * @param lang_override Optional language override
     * @return Detected or overridden language
     */
    Language detect_language(const std::filesystem::path& filepath,
                            std::optional<Language> lang_override) const;

    /**
     * @brief Get or parse a file (with caching)
     * @param filepath Path to the file
     * @param lang Language to use for parsing
     * @return Optional containing tree, source, and language, or nullopt on error
     */
    std::optional<std::tuple<const Tree*, std::string_view, Language>> get_or_parse_file(
        const std::filesystem::path& filepath,
        Language lang
    );

    /**
     * @brief Check if cached file is still valid
     * @param filepath Path to the file
     * @param cached Cached file data
     * @param lang Expected language
     * @return true if cache is valid
     */
    bool is_cache_valid(const std::filesystem::path& filepath,
                       const CachedFile& cached,
                       Language lang) const;

    /**
     * @brief Convert query matches to JSON array
     */
    json matches_to_json(const std::vector<QueryMatch>& matches) const;
};

} // namespace ts_mcp
