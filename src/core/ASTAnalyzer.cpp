#include "core/ASTAnalyzer.hpp"
#include "core/Language.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

namespace ts_mcp {

ASTAnalyzer::ASTAnalyzer()
    : parsers_(), query_engine_(), cache_() {
    spdlog::debug("ASTAnalyzer created");
}

TreeSitterParser& ASTAnalyzer::get_parser_for_language(Language lang) {
    auto it = parsers_.find(lang);
    if (it != parsers_.end()) {
        return it->second;
    }

    // Create new parser for this language
    spdlog::debug("Creating parser for language: {}", LanguageUtils::to_string(lang));
    auto [new_it, inserted] = parsers_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(lang),
        std::forward_as_tuple(lang)
    );

    return new_it->second;
}

Language ASTAnalyzer::detect_language(const std::filesystem::path& filepath,
                                     std::optional<Language> lang_override) const {
    if (lang_override) {
        return *lang_override;
    }

    Language detected = LanguageUtils::detect_from_extension(filepath);
    if (detected == Language::UNKNOWN) {
        spdlog::warn("Unknown file extension for {}, defaulting to C++", filepath.string());
        return Language::CPP;
    }

    return detected;
}

json ASTAnalyzer::analyze_file(const std::filesystem::path& filepath, std::optional<Language> lang) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    Language detected_lang = detect_language(filepath, lang);
    result["language"] = std::string(LanguageUtils::to_string(detected_lang));

    auto file_data = get_or_parse_file(filepath, detected_lang);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = std::get<0>(*file_data);
    std::string_view source = std::get<1>(*file_data);
    Language actual_lang = std::get<2>(*file_data);

    result["success"] = true;
    result["has_errors"] = tree->has_error();

    // Count classes
    auto class_query_str = QueryEngine::get_predefined_query(QueryType::CLASSES, actual_lang);
    if (class_query_str) {
        auto class_query = query_engine_.compile_query(*class_query_str, actual_lang);
        if (class_query) {
            auto class_matches = query_engine_.execute(*tree, *class_query, source);
            result["class_count"] = class_matches.size();
        } else {
            result["class_count"] = 0;
        }
    } else {
        result["class_count"] = 0;
    }

    // Count functions
    auto func_query_str = QueryEngine::get_predefined_query(QueryType::FUNCTIONS, actual_lang);
    if (func_query_str) {
        auto func_query = query_engine_.compile_query(*func_query_str, actual_lang);
        if (func_query) {
            auto func_matches = query_engine_.execute(*tree, *func_query, source);
            result["function_count"] = func_matches.size();
        } else {
            result["function_count"] = 0;
        }
    } else {
        result["function_count"] = 0;
    }

    // Count includes/imports
    auto include_query_str = QueryEngine::get_predefined_query(QueryType::INCLUDES, actual_lang);
    if (include_query_str) {
        auto include_query = query_engine_.compile_query(*include_query_str, actual_lang);
        if (include_query) {
            auto include_matches = query_engine_.execute(*tree, *include_query, source);
            result["include_count"] = include_matches.size();
        } else {
            result["include_count"] = 0;
        }
    } else {
        result["include_count"] = 0;
    }

    spdlog::debug("Analyzed {} ({}): {} classes, {} functions",
                 filepath.string(),
                 LanguageUtils::to_string(actual_lang),
                 result["class_count"].get<int>(),
                 result["function_count"].get<int>());

    return result;
}

json ASTAnalyzer::find_classes(const std::filesystem::path& filepath, std::optional<Language> lang) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    Language detected_lang = detect_language(filepath, lang);
    result["language"] = std::string(LanguageUtils::to_string(detected_lang));

    auto file_data = get_or_parse_file(filepath, detected_lang);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = std::get<0>(*file_data);
    std::string_view source = std::get<1>(*file_data);
    Language actual_lang = std::get<2>(*file_data);

    auto query_str = QueryEngine::get_predefined_query(QueryType::CLASSES, actual_lang);
    if (!query_str) {
        result["error"] = "Classes query not supported for this language";
        return result;
    }

    auto query = query_engine_.compile_query(*query_str, actual_lang);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["classes"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::find_functions(const std::filesystem::path& filepath, std::optional<Language> lang) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    Language detected_lang = detect_language(filepath, lang);
    result["language"] = std::string(LanguageUtils::to_string(detected_lang));

    auto file_data = get_or_parse_file(filepath, detected_lang);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = std::get<0>(*file_data);
    std::string_view source = std::get<1>(*file_data);
    Language actual_lang = std::get<2>(*file_data);

    auto query_str = QueryEngine::get_predefined_query(QueryType::FUNCTIONS, actual_lang);
    if (!query_str) {
        result["error"] = "Functions query not supported for this language";
        return result;
    }

    auto query = query_engine_.compile_query(*query_str, actual_lang);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["functions"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::find_includes(const std::filesystem::path& filepath, std::optional<Language> lang) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    Language detected_lang = detect_language(filepath, lang);
    result["language"] = std::string(LanguageUtils::to_string(detected_lang));

    auto file_data = get_or_parse_file(filepath, detected_lang);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = std::get<0>(*file_data);
    std::string_view source = std::get<1>(*file_data);
    Language actual_lang = std::get<2>(*file_data);

    auto query_str = QueryEngine::get_predefined_query(QueryType::INCLUDES, actual_lang);
    if (!query_str) {
        result["error"] = "Includes query not supported for this language";
        return result;
    }

    auto query = query_engine_.compile_query(*query_str, actual_lang);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["includes"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::execute_query(const std::filesystem::path& filepath,
                                std::string_view query_string,
                                std::optional<Language> lang) {
    json result = {
        {"filepath", filepath.string()},
        {"success", false}
    };

    Language detected_lang = detect_language(filepath, lang);
    result["language"] = std::string(LanguageUtils::to_string(detected_lang));

    auto file_data = get_or_parse_file(filepath, detected_lang);
    if (!file_data) {
        result["error"] = "Failed to parse file";
        return result;
    }

    const Tree* tree = std::get<0>(*file_data);
    std::string_view source = std::get<1>(*file_data);
    Language actual_lang = std::get<2>(*file_data);

    auto query = query_engine_.compile_query(query_string, actual_lang);
    if (!query) {
        result["error"] = "Failed to compile query";
        return result;
    }

    auto matches = query_engine_.execute(*tree, *query, source);

    result["success"] = true;
    result["matches"] = matches_to_json(matches);

    return result;
}

json ASTAnalyzer::analyze_files(const std::vector<std::filesystem::path>& filepaths) {
    json results = json::array();
    int total = filepaths.size();
    int failed = 0;

    for (const auto& file : filepaths) {
        try {
            auto result = analyze_file(file);
            result["filepath"] = file.string();
            if (!result.value("success", false)) {
                failed++;
            }
            results.push_back(result);
        } catch (const std::exception& e) {
            results.push_back({
                {"filepath", file.string()},
                {"error", e.what()},
                {"success", false}
            });
            failed++;
        }
    }

    // Aggregated response
    return {
        {"success", failed == 0},
        {"total_files", total},
        {"processed_files", total - failed},
        {"failed_files", failed},
        {"results", results}
    };
}

json ASTAnalyzer::find_classes_in_files(const std::vector<std::filesystem::path>& filepaths) {
    json results = json::array();
    int total = filepaths.size();
    int failed = 0;

    for (const auto& file : filepaths) {
        try {
            auto result = find_classes(file);
            result["filepath"] = file.string();
            if (!result.value("success", false)) {
                failed++;
            }
            results.push_back(result);
        } catch (const std::exception& e) {
            results.push_back({
                {"filepath", file.string()},
                {"error", e.what()},
                {"success", false}
            });
            failed++;
        }
    }

    // Aggregated response
    return {
        {"success", failed == 0},
        {"total_files", total},
        {"processed_files", total - failed},
        {"failed_files", failed},
        {"results", results}
    };
}

json ASTAnalyzer::find_functions_in_files(const std::vector<std::filesystem::path>& filepaths) {
    json results = json::array();
    int total = filepaths.size();
    int failed = 0;

    for (const auto& file : filepaths) {
        try {
            auto result = find_functions(file);
            result["filepath"] = file.string();
            if (!result.value("success", false)) {
                failed++;
            }
            results.push_back(result);
        } catch (const std::exception& e) {
            results.push_back({
                {"filepath", file.string()},
                {"error", e.what()},
                {"success", false}
            });
            failed++;
        }
    }

    // Aggregated response
    return {
        {"success", failed == 0},
        {"total_files", total},
        {"processed_files", total - failed},
        {"failed_files", failed},
        {"results", results}
    };
}

json ASTAnalyzer::execute_query_on_files(
    const std::vector<std::filesystem::path>& filepaths,
    std::string_view query_string
) {
    json results = json::array();
    int total = filepaths.size();
    int failed = 0;

    for (const auto& file : filepaths) {
        try {
            auto result = execute_query(file, query_string);
            result["filepath"] = file.string();
            if (!result.value("success", false)) {
                failed++;
            }
            results.push_back(result);
        } catch (const std::exception& e) {
            results.push_back({
                {"filepath", file.string()},
                {"error", e.what()},
                {"success", false}
            });
            failed++;
        }
    }

    // Aggregated response
    return {
        {"success", failed == 0},
        {"total_files", total},
        {"processed_files", total - failed},
        {"failed_files", failed},
        {"results", results}
    };
}

void ASTAnalyzer::clear_cache() {
    cache_.clear();
    spdlog::debug("Cache cleared");
}

std::optional<std::tuple<const Tree*, std::string_view, Language>> ASTAnalyzer::get_or_parse_file(
    const std::filesystem::path& filepath,
    Language lang
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
        if (is_cache_valid(filepath, it->second, lang)) {
            spdlog::debug("Using cached parse for {} ({})",
                         filepath.string(),
                         LanguageUtils::to_string(lang));
            return std::make_tuple(it->second.tree.get(),
                                  std::string_view(it->second.source),
                                  it->second.language);
        } else {
            spdlog::debug("Cache invalid for {}, re-parsing", filepath.string());
            cache_.erase(it);
        }
    }

    // Parse file
    spdlog::debug("Parsing file: {} with language {}",
                 filepath.string(),
                 LanguageUtils::to_string(lang));

    // Read file contents
    std::ifstream file(filepath);
    if (!file) {
        spdlog::error("Failed to open file: {}", filepath.string());
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Get parser for this language
    TreeSitterParser& parser = get_parser_for_language(lang);

    // Parse
    auto tree = parser.parse_string(source);
    if (!tree) {
        spdlog::error("Failed to parse file: {}", filepath.string());
        return std::nullopt;
    }

    // Cache result
    CachedFile cached;
    cached.tree = std::move(tree);
    cached.source = std::move(source);
    cached.mtime = mtime;
    cached.language = lang;

    auto [cache_it, inserted] = cache_.emplace(filepath, std::move(cached));

    spdlog::debug("Cached parse for {} ({}, cache size: {})",
                 filepath.string(),
                 LanguageUtils::to_string(lang),
                 cache_.size());

    return std::make_tuple(cache_it->second.tree.get(),
                          std::string_view(cache_it->second.source),
                          cache_it->second.language);
}

bool ASTAnalyzer::is_cache_valid(const std::filesystem::path& filepath,
                                 const CachedFile& cached,
                                 Language lang) const {
    // Check if language matches
    if (cached.language != lang) {
        return false;
    }

    // Check if file has been modified
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
