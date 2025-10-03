#include "GetSymbolContextTool.hpp"
#include "core/TreeSitterParser.hpp"
#include "core/QueryEngine.hpp"
#include "core/Language.hpp"
#include "core/PathResolver.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>

using json = nlohmann::json;

namespace ts_mcp {

ToolInfo GetSymbolContextTool::get_info() {
    return ToolInfo{
        .name = "get_symbol_context",
        .description = "Get comprehensive context for a symbol (function/class/method) including definition, dependencies, and usage examples",
        .input_schema = {
            {"type", "object"},
            {"properties", {
                {"symbol_name", {
                    {"type", "string"},
                    {"description", "Name of symbol to analyze (e.g., 'MyClass::method', 'function_name', 'ClassName')"}
                }},
                {"filepath", {
                    {"type", "string"},
                    {"description", "Path to file containing the symbol"}
                }},
                {"include_dependencies", {
                    {"type", "boolean"},
                    {"description", "Whether to include definitions of used symbols (default: true)"}
                }},
                {"max_dependencies", {
                    {"type", "integer"},
                    {"description", "Maximum number of dependencies to include (default: 10)"}
                }},
                {"resolve_external_types", {
                    {"type", "boolean"},
                    {"description", "Search for type definitions in other files (default: false)"}
                }},
                {"include_usage_examples", {
                    {"type", "boolean"},
                    {"description", "Find and include usage examples from codebase (default: false)"}
                }},
                {"context_lines", {
                    {"type", "integer"},
                    {"description", "Number of context lines around usage examples (default: 3)"}
                }},
                {"search_paths", {
                    {"type", "array"},
                    {"items", {{"type", "string"}}},
                    {"description", "Paths to search for external type definitions (auto-detected if not specified)"}
                }}
            }},
            {"required", json::array({"symbol_name", "filepath"})}
        }
    };
}

json GetSymbolContextTool::execute(const json& args) {
    std::string symbol_name = args["symbol_name"];
    std::string filepath = args["filepath"];
    bool include_deps = args.value("include_dependencies", true);
    int max_deps = args.value("max_dependencies", 10);

    // NEW PARAMETERS
    bool resolve_external_types = args.value("resolve_external_types", false);
    bool include_usage_examples = args.value("include_usage_examples", false);
    int context_lines = args.value("context_lines", 3);

    // Default search paths: try to infer from filepath
    std::vector<std::string> search_paths;
    if (args.contains("search_paths") && args["search_paths"].is_array()) {
        for (const auto& path : args["search_paths"]) {
            search_paths.push_back(path.get<std::string>());
        }
    } else if (resolve_external_types) {
        // Auto-detect search paths from filepath
        std::filesystem::path file_path(filepath);
        std::string base = file_path.parent_path().string();

        // Go up to find project root
        while (!base.empty() && base != "/") {
            if (std::filesystem::exists(base + "/src") ||
                std::filesystem::exists(base + "/include")) {
                search_paths.push_back(base + "/include");
                search_paths.push_back(base + "/src");
                break;
            }
            file_path = file_path.parent_path();
            base = file_path.string();
        }

        // Fallback: use parent directory
        if (search_paths.empty()) {
            search_paths.push_back(std::filesystem::path(filepath).parent_path().string());
        }
    }

    spdlog::info("Getting context for symbol '{}' in {} (resolve_external={}, usage_examples={})",
                 symbol_name, filepath, resolve_external_types, include_usage_examples);

    // Determine language
    Language language = LanguageUtils::detect_from_extension(std::string_view(filepath));
    if (language == Language::UNKNOWN) {
        return {
            {"error", "Unsupported file type"},
            {"filepath", filepath}
        };
    }

    // Stage 1: Locate symbol
    auto location = locate_symbol(symbol_name, filepath, language);
    if (!location) {
        return {
            {"error", "Symbol not found"},
            {"symbol_name", symbol_name},
            {"filepath", filepath}
        };
    }

    // Read source
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {{"error", "Cannot open file"}, {"filepath", filepath}};
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Stage 2: Extract definition
    SymbolDefinition definition = extract_definition(*location, source);

    json result = {
        {"symbol", {
            {"name", definition.name},
            {"type", definition.type},
            {"filepath", definition.filepath},
            {"start_line", definition.start_line},
            {"end_line", definition.end_line},
            {"signature", definition.signature},
            {"full_code", definition.full_code}
        }}
    };

    if (!definition.parent_class.empty()) {
        result["symbol"]["parent_class"] = definition.parent_class;
    }

    if (include_deps) {
        // Stage 3: Analyze dependencies
        std::vector<UsedSymbol> used = analyze_dependencies(
            definition, source, location->start_byte, location->end_byte
        );

        // Stage 4: Enrich context (WITH NEW PARAMETERS)
        EnrichedContext enriched = enrich_context(
            definition,
            used,
            filepath,
            language,
            resolve_external_types,
            include_usage_examples,
            context_lines,
            search_paths
        );

        // Limit dependencies
        int dep_count = std::min(max_deps, static_cast<int>(enriched.dependencies.size()));

        json deps_array = json::array();
        for (int i = 0; i < dep_count; i++) {
            const auto& dep = enriched.dependencies[i];
            json dep_json = {
                {"name", dep.name},
                {"type", dep.type},
                {"filepath", dep.filepath},
                {"start_line", dep.start_line},
                {"end_line", dep.end_line},
                {"signature", dep.signature}
            };

            // Include full definition for external types
            if (resolve_external_types && !dep.full_code.empty()) {
                dep_json["definition"] = dep.full_code;
            }

            deps_array.push_back(dep_json);
        }
        result["dependencies"] = deps_array;

        if (!enriched.includes.empty()) {
            result["required_includes"] = enriched.includes;
        }

        // NEW: Add usage examples
        if (!enriched.usage_examples.empty()) {
            json examples_array = json::array();
            for (const auto& example : enriched.usage_examples) {
                examples_array.push_back({
                    {"filepath", example.filepath},
                    {"line", example.line},
                    {"context", example.context_lines},
                    {"parent_scope", example.parent_scope}
                });
            }
            result["usage_examples"] = examples_array;
        }

        result["used_symbols_count"] = used.size();
        result["dependencies_found"] = enriched.dependencies.size();
        result["usage_examples_found"] = enriched.usage_examples.size();
    }

    return result;
}

// ============================================================================
// Stage 1: Localization
// ============================================================================

std::optional<GetSymbolContextTool::SymbolLocation>
GetSymbolContextTool::locate_symbol(
    const std::string& symbol_name,
    const std::string& filepath,
    Language language
) {
    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::warn("Cannot open file {}", filepath);
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse with tree-sitter
    TreeSitterParser parser(language);
    auto tree = parser.parse_string(source);
    if (!tree) {
        spdlog::warn("Failed to parse {}", filepath);
        return std::nullopt;
    }

    TSNode root = ts_tree_root_node(tree->get());

    // Check if qualified name (e.g., "MyClass::method")
    size_t scope_pos = symbol_name.find("::");
    bool is_qualified = (scope_pos != std::string::npos);

    std::string class_name, method_name;
    if (is_qualified) {
        class_name = symbol_name.substr(0, scope_pos);
        method_name = symbol_name.substr(scope_pos + 2);
    }

    // Search for symbol
    std::function<std::optional<SymbolLocation>(TSNode)> search;
    search = [&](TSNode node) -> std::optional<SymbolLocation> {
        std::string node_type = ts_node_type(node);

        // Check function definitions
        if (node_type == "function_definition") {
            TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
            if (!ts_node_is_null(declarator)) {
                std::string func_name = get_node_text(declarator, source);

                // Extract just the name (remove parameters)
                size_t paren_pos = func_name.find('(');
                if (paren_pos != std::string::npos) {
                    func_name = func_name.substr(0, paren_pos);
                }

                if (func_name == symbol_name || func_name.find(symbol_name) != std::string::npos) {
                    TSPoint start = ts_node_start_point(node);
                    TSPoint end = ts_node_end_point(node);
                    return SymbolLocation{
                        .name = symbol_name,
                        .type = "function",
                        .filepath = filepath,
                        .start_line = static_cast<int>(start.row + 1),
                        .end_line = static_cast<int>(end.row + 1),
                        .start_byte = ts_node_start_byte(node),
                        .end_byte = ts_node_end_byte(node)
                    };
                }
            }
        }

        // Check class definitions
        if (node_type == "class_specifier" || node_type == "struct_specifier") {
            TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
            if (!ts_node_is_null(name_node)) {
                std::string class_nm = get_node_text(name_node, source);

                if (class_nm == symbol_name || class_nm == class_name) {
                    // If looking for method, search in class body
                    if (is_qualified && class_nm == class_name) {
                        TSNode body = ts_node_child_by_field_name(node, "body", 4);
                        if (!ts_node_is_null(body)) {
                            uint32_t child_count = ts_node_child_count(body);
                            for (uint32_t i = 0; i < child_count; i++) {
                                TSNode child = ts_node_child(body, i);
                                std::string child_type = ts_node_type(child);

                                if (child_type == "function_definition" ||
                                    child_type == "field_declaration") {
                                    TSNode decl = ts_node_child_by_field_name(child, "declarator", 10);
                                    if (!ts_node_is_null(decl)) {
                                        std::string method_text = get_node_text(decl, source);
                                        if (method_text.find(method_name) != std::string::npos) {
                                            TSPoint start = ts_node_start_point(child);
                                            TSPoint end = ts_node_end_point(child);
                                            return SymbolLocation{
                                                .name = symbol_name,
                                                .type = "method",
                                                .filepath = filepath,
                                                .start_line = static_cast<int>(start.row + 1),
                                                .end_line = static_cast<int>(end.row + 1),
                                                .start_byte = ts_node_start_byte(child),
                                                .end_byte = ts_node_end_byte(child)
                                            };
                                        }
                                    }
                                }
                            }
                        }
                    } else {
                        // Just the class itself
                        TSPoint start = ts_node_start_point(node);
                        TSPoint end = ts_node_end_point(node);
                        return SymbolLocation{
                            .name = symbol_name,
                            .type = "class",
                            .filepath = filepath,
                            .start_line = static_cast<int>(start.row + 1),
                            .end_line = static_cast<int>(end.row + 1),
                            .start_byte = ts_node_start_byte(node),
                            .end_byte = ts_node_end_byte(node)
                        };
                    }
                }
            }
        }

        // Recurse to children
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            auto result = search(ts_node_child(node, i));
            if (result) return result;
        }

        return std::nullopt;
    };

    return search(root);
}

// ============================================================================
// Stage 2: Extraction
// ============================================================================

GetSymbolContextTool::SymbolDefinition
GetSymbolContextTool::extract_definition(
    const SymbolLocation& location,
    const std::string& source
) {
    SymbolDefinition def;
    def.name = location.name;
    def.type = location.type;
    def.filepath = location.filepath;
    def.start_line = location.start_line;
    def.end_line = location.end_line;
    def.full_code = source.substr(location.start_byte, location.end_byte - location.start_byte);

    // Parse again to get signature
    Language language = LanguageUtils::detect_from_extension(std::string_view(location.filepath));
    TreeSitterParser parser(language);
    auto tree = parser.parse_string(source);
    if (tree) {
        TSNode root = ts_tree_root_node(tree->get());
        // Find the node at this location
        TSNode node = ts_node_descendant_for_byte_range(root, location.start_byte, location.end_byte);
        def.signature = extract_signature(node, source);
    }

    // Extract parent class if method
    if (location.type == "method") {
        size_t scope_pos = location.name.find("::");
        if (scope_pos != std::string::npos) {
            def.parent_class = location.name.substr(0, scope_pos);
        }
    }

    return def;
}

std::string GetSymbolContextTool::extract_signature(
    TSNode node,
    const std::string& source
) {
    std::string node_type = ts_node_type(node);

    if (node_type == "function_definition") {
        // Get everything except the body
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
        TSNode return_type = ts_node_child_by_field_name(node, "type", 4);

        std::string signature;
        if (!ts_node_is_null(return_type)) {
            signature += get_node_text(return_type, source) + " ";
        }
        if (!ts_node_is_null(declarator)) {
            signature += get_node_text(declarator, source);
        }
        signature += ";";
        return signature;
    }

    if (node_type == "class_specifier" || node_type == "struct_specifier") {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        std::string keyword = (node_type == "class_specifier") ? "class" : "struct";
        std::string name = ts_node_is_null(name_node) ? "" : get_node_text(name_node, source);

        // Check for base classes
        TSNode base_clause = {};
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_child(node, i);
            if (std::string(ts_node_type(child)) == "base_class_clause") {
                base_clause = child;
                break;
            }
        }

        std::string signature = keyword + " " + name;
        if (!ts_node_is_null(base_clause)) {
            signature += " " + get_node_text(base_clause, source);
        }
        signature += ";";
        return signature;
    }

    return get_node_text(node, source);
}

// ============================================================================
// Stage 3: Dependency Analysis
// ============================================================================

std::vector<GetSymbolContextTool::UsedSymbol>
GetSymbolContextTool::analyze_dependencies(
    const SymbolDefinition& definition,
    const std::string& source,
    uint32_t start_byte,
    uint32_t end_byte
) {
    std::vector<UsedSymbol> used_symbols;
    std::set<std::string> seen_names; // dedup

    // Parse to get AST node
    Language language = LanguageUtils::detect_from_extension(std::string_view(definition.filepath));
    TreeSitterParser parser(language);
    auto tree = parser.parse_string(source);
    if (!tree) return used_symbols;

    TSNode root = ts_tree_root_node(tree->get());
    TSNode node = ts_node_descendant_for_byte_range(root, start_byte, end_byte);

    std::function<void(TSNode)> traverse;
    traverse = [&](TSNode n) {
        std::string node_type = ts_node_type(n);

        // Type identifiers (e.g., variable types)
        if (node_type == "type_identifier") {
            std::string name = get_node_text(n, source);
            if (seen_names.insert(name).second) {
                used_symbols.push_back({
                    .name = name,
                    .type = "type",
                    .context = "variable type"
                });
            }
        }

        // Function calls
        if (node_type == "call_expression") {
            TSNode func_node = ts_node_child_by_field_name(n, "function", 8);
            if (!ts_node_is_null(func_node)) {
                std::string name = get_node_text(func_node, source);
                if (seen_names.insert(name).second) {
                    used_symbols.push_back({
                        .name = name,
                        .type = "function",
                        .context = "function call"
                    });
                }
            }
        }

        // Qualified identifiers (e.g., MyClass::member)
        if (node_type == "qualified_identifier") {
            std::string name = get_node_text(n, source);
            if (seen_names.insert(name).second) {
                used_symbols.push_back({
                    .name = name,
                    .type = "qualified",
                    .context = "qualified access"
                });
            }
        }

        // Recurse
        uint32_t child_count = ts_node_child_count(n);
        for (uint32_t i = 0; i < child_count; i++) {
            traverse(ts_node_child(n, i));
        }
    };

    traverse(node);

    spdlog::debug("Found {} used symbols in {}", used_symbols.size(), definition.name);
    return used_symbols;
}

// ============================================================================
// Stage 4: Enrichment
// ============================================================================

GetSymbolContextTool::EnrichedContext
GetSymbolContextTool::enrich_context(
    const SymbolDefinition& target,
    const std::vector<UsedSymbol>& used_symbols,
    const std::string& filepath,
    Language language,
    bool resolve_external_types,
    bool include_usage_examples,
    int context_lines,
    const std::vector<std::string>& search_paths
) {
    EnrichedContext enriched;
    enriched.target = target;

    // Extract includes from file
    enriched.includes = extract_includes(filepath, language);

    // Try to find each used symbol
    for (const auto& used : used_symbols) {
        if (used.type != "type") continue;  // Only resolve types

        // 1. Try current file first
        auto def = find_symbol_in_file(used.name, filepath, language);

        // 2. If not found and external resolution enabled - search in other files
        if (!def && resolve_external_types && !search_paths.empty()) {
            spdlog::debug("Symbol '{}' not found in current file, searching externally", used.name);
            def = find_in_search_paths(used.name, search_paths);
        }

        if (def) {
            enriched.dependencies.push_back(*def);
        }
    }

    // 3. Find usage examples if requested
    if (include_usage_examples) {
        // Extract base path from filepath (go up to project root)
        std::filesystem::path file_path(filepath);
        std::string base_path = file_path.parent_path().string();

        // Try to find project root (go up until we find src/ or include/)
        while (!base_path.empty() && base_path != "/") {
            if (std::filesystem::exists(base_path + "/src") ||
                std::filesystem::exists(base_path + "/include")) {
                break;
            }
            file_path = file_path.parent_path();
            base_path = file_path.string();
        }

        if (base_path.empty() || base_path == "/") {
            base_path = std::filesystem::path(filepath).parent_path().string();
        }

        spdlog::debug("Searching for usage examples in base path: {}", base_path);

        enriched.usage_examples = find_usage_examples(
            target.name,
            base_path,
            context_lines,
            5  // max 5 examples
        );
    }

    spdlog::info("Enriched context: {} includes, {} dependencies, {} usage examples",
                 enriched.includes.size(),
                 enriched.dependencies.size(),
                 enriched.usage_examples.size());

    return enriched;
}

std::optional<GetSymbolContextTool::SymbolDefinition>
GetSymbolContextTool::find_symbol_in_file(
    const std::string& symbol_name,
    const std::string& filepath,
    Language language
) {
    auto location = locate_symbol(symbol_name, filepath, language);
    if (!location) return std::nullopt;

    std::ifstream file(filepath);
    if (!file.is_open()) return std::nullopt;
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    return extract_definition(*location, source);
}

std::vector<std::string> GetSymbolContextTool::extract_includes(
    const std::string& filepath,
    Language language
) {
    std::vector<std::string> includes;

    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) return includes;
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse with tree-sitter
    TreeSitterParser parser(language);
    auto tree = parser.parse_string(source);
    if (!tree) return includes;

    TSNode root = ts_tree_root_node(tree->get());

    std::function<void(TSNode)> traverse;
    traverse = [&](TSNode node) {
        std::string node_type = ts_node_type(node);

        if (node_type == "preproc_include") {
            std::string include_text = get_node_text(node, source);
            includes.push_back(include_text);
        }

        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            traverse(ts_node_child(node, i));
        }
    };

    traverse(root);
    return includes;
}

// ============================================================================
// Helpers
// ============================================================================

std::string GetSymbolContextTool::get_node_text(TSNode node, const std::string& source) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start >= source.size() || end > source.size() || start >= end) {
        return "";
    }
    return source.substr(start, end - start);
}

int GetSymbolContextTool::get_line_number(const std::string& source, uint32_t byte_offset) {
    int line = 1;
    for (uint32_t i = 0; i < byte_offset && i < source.size(); i++) {
        if (source[i] == '\n') line++;
    }
    return line;
}

// ============================================================================
// NEW: Cross-file type resolution
// ============================================================================

std::optional<GetSymbolContextTool::SymbolDefinition>
GetSymbolContextTool::find_in_search_paths(
    const std::string& symbol_name,
    const std::vector<std::string>& search_paths
) {
    spdlog::debug("Searching for symbol '{}' in {} search paths", symbol_name, search_paths.size());

    for (const auto& base_path : search_paths) {
        // Resolve all header files in this search path
        std::vector<std::filesystem::path> files;
        try {
            files = PathResolver::resolve_paths(
                {base_path},
                true,  // recursive
                {"*.hpp", "*.h"}
            );
        } catch (const std::exception& e) {
            spdlog::warn("Failed to resolve path {}: {}", base_path, e.what());
            continue;
        }

        spdlog::debug("Searching in {} header files from {}", files.size(), base_path);

        // Search each file for the symbol
        for (const auto& file : files) {
            Language lang = LanguageUtils::detect_from_extension(file);
            if (lang == Language::UNKNOWN) continue;

            auto location = locate_symbol(symbol_name, file.string(), lang);
            if (location) {
                spdlog::info("Found symbol '{}' in {}", symbol_name, file.string());

                // Read the file
                std::ifstream f(file);
                if (!f.is_open()) {
                    spdlog::warn("Cannot open file {}", file.string());
                    continue;
                }
                std::string source((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());

                // Extract definition
                return extract_definition(*location, source);
            }
        }
    }

    spdlog::debug("Symbol '{}' not found in any search path", symbol_name);
    return std::nullopt;
}

// ============================================================================
// NEW: Extended context reading
// ============================================================================

std::vector<std::string> GetSymbolContextTool::read_context_lines(
    const std::string& filepath,
    int center_line,
    int context_size
) {
    std::vector<std::string> result;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::warn("Cannot open file {} for context reading", filepath);
        return result;
    }

    int start_line = std::max(1, center_line - context_size);
    int end_line = center_line + context_size;

    std::string line;
    int current_line = 1;

    while (std::getline(file, line)) {
        if (current_line >= start_line && current_line <= end_line) {
            result.push_back(line);
        }
        if (current_line > end_line) {
            break;
        }
        current_line++;
    }

    spdlog::debug("Read {} context lines around line {} in {}", result.size(), center_line, filepath);
    return result;
}

// ============================================================================
// NEW: Find usage examples
// ============================================================================

std::vector<GetSymbolContextTool::UsageExample>
GetSymbolContextTool::find_usage_examples(
    const std::string& symbol_name,
    const std::string& base_path,
    int context_lines,
    int max_examples
) {
    std::vector<UsageExample> examples;

    spdlog::debug("Finding usage examples for '{}' in {}", symbol_name, base_path);

    // Get all C++ files in the base path
    std::vector<std::filesystem::path> files;
    try {
        files = PathResolver::resolve_paths(
            {base_path},
            true,
            {"*.cpp", "*.hpp", "*.h"}
        );
    } catch (const std::exception& e) {
        spdlog::warn("Failed to resolve path {}: {}", base_path, e.what());
        return examples;
    }

    int found_count = 0;

    for (const auto& file : files) {
        if (found_count >= max_examples) break;

        // Read file
        std::ifstream f(file);
        if (!f.is_open()) continue;

        std::stringstream buffer;
        buffer << f.rdbuf();
        std::string source = buffer.str();

        // Parse file
        Language lang = LanguageUtils::detect_from_extension(file);
        if (lang == Language::UNKNOWN) continue;

        TreeSitterParser parser(lang);
        auto tree = parser.parse_string(source);
        if (!tree) continue;

        TSNode root = ts_tree_root_node(tree->get());

        // Search for call expressions
        std::function<void(TSNode)> traverse;
        traverse = [&](TSNode node) {
            if (found_count >= max_examples) return;

            std::string node_type = ts_node_type(node);

            // Look for call expressions
            if (node_type == "call_expression") {
                TSNode func_node = ts_node_child_by_field_name(node, "function", 8);
                if (!ts_node_is_null(func_node)) {
                    std::string func_name = get_node_text(func_node, source);

                    // Check if this is our symbol
                    if (func_name.find(symbol_name) != std::string::npos) {
                        TSPoint start = ts_node_start_point(node);
                        int line = static_cast<int>(start.row + 1);

                        UsageExample example;
                        example.filepath = file.string();
                        example.line = line;
                        example.context_lines = read_context_lines(file.string(), line, context_lines);

                        // Try to find parent scope
                        TSNode parent = ts_node_parent(node);
                        while (!ts_node_is_null(parent)) {
                            std::string parent_type = ts_node_type(parent);
                            if (parent_type == "function_definition") {
                                TSNode decl = ts_node_child_by_field_name(parent, "declarator", 10);
                                if (!ts_node_is_null(decl)) {
                                    example.parent_scope = get_node_text(decl, source);
                                    // Remove parameter list
                                    size_t paren = example.parent_scope.find('(');
                                    if (paren != std::string::npos) {
                                        example.parent_scope = example.parent_scope.substr(0, paren);
                                    }
                                }
                                break;
                            }
                            parent = ts_node_parent(parent);
                        }

                        examples.push_back(example);
                        found_count++;
                        spdlog::debug("Found usage example at {}:{}", file.string(), line);
                    }
                }
            }

            // Recurse
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count && found_count < max_examples; i++) {
                traverse(ts_node_child(node, i));
            }
        };

        traverse(root);
    }

    spdlog::info("Found {} usage examples for '{}'", examples.size(), symbol_name);
    return examples;
}

} // namespace ts_mcp
