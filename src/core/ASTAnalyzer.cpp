#include "core/ASTAnalyzer.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace ts_mcp {

ASTAnalyzer::ASTAnalyzer()
    : parser_(), query_engine_(), cache_() {
    spdlog::debug("ASTAnalyzer created");
}

json ASTAnalyzer::analyze_file(const std::filesystem::path& filepath) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    auto file_data = get_or_parse_file(filepath);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = file_data->first;
    std::string_view source = file_data->second;

    result["success"] = true;
    result["has_errors"] = tree->has_error();

    // Count classes
    auto class_query = query_engine_.compile_query(QueryEngine::PredefinedQueries::ALL_CLASSES);
    if (class_query) {
        auto class_matches = query_engine_.execute(*tree, *class_query, source);
        result["class_count"] = class_matches.size();
    } else {
        result["class_count"] = 0;
    }

    // Count functions
    auto func_query = query_engine_.compile_query(QueryEngine::PredefinedQueries::ALL_FUNCTIONS);
    if (func_query) {
        auto func_matches = query_engine_.execute(*tree, *func_query, source);
        result["function_count"] = func_matches.size();
    } else {
        result["function_count"] = 0;
    }

    // Count includes
    auto include_query = query_engine_.compile_query(QueryEngine::PredefinedQueries::INCLUDES);
    if (include_query) {
        auto include_matches = query_engine_.execute(*tree, *include_query, source);
        result["include_count"] = include_matches.size();
    } else {
        result["include_count"] = 0;
    }

    spdlog::debug("Analyzed {}: {} classes, {} functions",
                 filepath.string(),
                 result["class_count"].get<int>(),
                 result["function_count"].get<int>());

    return result;
}

json ASTAnalyzer::find_classes(const std::filesystem::path& filepath) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    auto file_data = get_or_parse_file(filepath);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = file_data->first;
    std::string_view source = file_data->second;

    auto query = query_engine_.compile_query(QueryEngine::PredefinedQueries::ALL_CLASSES);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["classes"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::find_functions(const std::filesystem::path& filepath) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    auto file_data = get_or_parse_file(filepath);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = file_data->first;
    std::string_view source = file_data->second;

    auto query = query_engine_.compile_query(QueryEngine::PredefinedQueries::ALL_FUNCTIONS);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["functions"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::find_includes(const std::filesystem::path& filepath) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    auto file_data = get_or_parse_file(filepath);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = file_data->first;
    std::string_view source = file_data->second;

    auto query = query_engine_.compile_query(QueryEngine::PredefinedQueries::INCLUDES);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["includes"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::execute_query(const std::filesystem::path& filepath, std::string_view query_string) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    auto file_data = get_or_parse_file(filepath);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = file_data->first;
    std::string_view source = file_data->second;

    auto query = query_engine_.compile_query(query_string);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["matches"] = matches_to_json(matches);

    return result;
}

void ASTAnalyzer::clear_cache() {
    cache_.clear();
    spdlog::debug("Cache cleared");
}

std::optional<std::pair<const Tree*, std::string_view>> ASTAnalyzer::get_or_parse_file(
    const std::filesystem::path& filepath
) {
    // Check if file exists
    if (!std::filesystem::exists(filepath)) {
        spdlog::error("File does not exist: {}", filepath.string());
        return std::nullopt;
    }

    // Get file modification time
    std::filesystem::file_time_type mtime;
    try {
        mtime = std::filesystem::last_write_time(filepath);
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::error("Failed to get file mtime: {}", e.what());
        return std::nullopt;
    }

    // Check cache
    auto it = cache_.find(filepath);
    if (it != cache_.end()) {
        if (is_cache_valid(filepath, it->second)) {
            spdlog::debug("Using cached parse for {}", filepath.string());
            return std::make_pair(it->second.tree.get(), std::string_view(it->second.source));
        } else {
            spdlog::debug("Cache invalid for {}, re-parsing", filepath.string());
            cache_.erase(it);
        }
    }

    // Parse file
    spdlog::debug("Parsing file: {}", filepath.string());

    // Read file contents
    std::ifstream file(filepath);
    if (!file) {
        spdlog::error("Failed to open file: {}", filepath.string());
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse
    auto tree = parser_.parse_string(source);
    if (!tree) {
        spdlog::error("Failed to parse file: {}", filepath.string());
        return std::nullopt;
    }

    // Cache result
    CachedFile cached;
    cached.tree = std::move(tree);
    cached.source = std::move(source);
    cached.mtime = mtime;

    auto [cache_it, inserted] = cache_.emplace(filepath, std::move(cached));

    spdlog::debug("Cached parse for {} (cache size: {})", filepath.string(), cache_.size());

    return std::make_pair(cache_it->second.tree.get(), std::string_view(cache_it->second.source));
}

bool ASTAnalyzer::is_cache_valid(const std::filesystem::path& filepath, const CachedFile& cached) const {
    try {
        auto current_mtime = std::filesystem::last_write_time(filepath);
        return current_mtime == cached.mtime;
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::warn("Failed to check file mtime: {}", e.what());
        return false;
    }
}

json ASTAnalyzer::matches_to_json(const std::vector<QueryMatch>& matches) const {
    json result = json::array();

    for (const auto& match : matches) {
        json item = {
            {"capture_name", match.capture_name},
            {"line", match.line},
            {"column", match.column},
            {"text", match.text}
        };
        result.push_back(item);
    }

    return result;
}

} // namespace ts_mcp
