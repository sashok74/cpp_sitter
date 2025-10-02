#include "tools/FindReferencesTool.hpp"
#include "core/PathResolver.hpp"
#include "core/TreeSitterParser.hpp"
#include "core/Language.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ts_mcp {

FindReferencesTool::FindReferencesTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(analyzer)
    , query_engine_() {
}

ToolInfo FindReferencesTool::get_info() {
    ToolInfo info;
    info.name = "find_references";
    info.description = "Find all references to a symbol (function, class, variable) in codebase with AST-based validation";

    info.input_schema = {
        {"type", "object"},
        {"properties", {
            {"symbol", {
                {"type", "string"},
                {"description", "Symbol name to search for (function, class, variable)"}
            }},
            {"filepath", {
                {"oneOf", json::array({
                    {{"type", "string"}, {"description", "Single file or directory path"}},
                    {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Multiple file or directory paths"}}
                })},
                {"description", "Optional: limit search scope (default: searches entire codebase)"}
            }},
            {"reference_types", {
                {"type", "array"},
                {"items", {{"type", "string"}, {"enum", json::array({"call", "declaration", "definition", "member_access", "type_usage", "all"})}}},
                {"default", json::array({"all"})},
                {"description", "Filter by reference types"}
            }},
            {"include_context", {
                {"type", "boolean"},
                {"default", true},
                {"description", "Include code context and parent scope"}
            }},
            {"recursive", {
                {"type", "boolean"},
                {"default", true},
                {"description", "Recursively scan directories"}
            }},
            {"file_patterns", {
                {"type", "array"},
                {"items", {{"type", "string"}}},
                {"default", json::array({"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"})},
                {"description", "File patterns to include"}
            }}
        }},
        {"required", json::array({"symbol"})}
    };

    return info;
}

json FindReferencesTool::execute(const json& args) {
    // Validate required parameter
    if (!args.contains("symbol")) {
        json error_result;
        error_result["error"] = "Missing required parameter: symbol";
        return error_result;
    }

    std::string symbol = args["symbol"].get<std::string>();
    [[maybe_unused]] bool include_context = args.value("include_context", true);
    bool recursive = args.value("recursive", true);

    std::vector<std::string> file_patterns =
        args.value("file_patterns", std::vector<std::string>{
            "*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"
        });

    // Determine filepath scope
    std::vector<std::string> input_paths;
    if (args.contains("filepath")) {
        if (args["filepath"].is_string()) {
            input_paths.push_back(args["filepath"].get<std::string>());
        } else if (args["filepath"].is_array()) {
            input_paths = args["filepath"].get<std::vector<std::string>>();
        } else {
            json error_result;
            error_result["error"] = "filepath must be a string or array of strings";
            return error_result;
        }
    } else {
        // No filepath specified - use current directory
        input_paths.push_back(".");
    }

    // Resolve file paths
    auto resolved = PathResolver::resolve_paths(input_paths, recursive, file_patterns);

    if (resolved.empty()) {
        json error_result;
        error_result["error"] = "No files found matching the specified paths";
        return error_result;
    }

    spdlog::debug("FindReferencesTool: searching for '{}' in {} files", symbol, resolved.size());

    // Search for references in all files
    std::vector<Reference> all_references;
    int processed_files = 0;
    int failed_files = 0;

    for (const auto& filepath : resolved) {
        Language lang = LanguageUtils::detect_from_extension(filepath);

        if (lang == Language::UNKNOWN) {
            failed_files++;
            continue;
        }

        try {
            auto refs = find_in_file(filepath.string(), symbol, lang);
            all_references.insert(all_references.end(), refs.begin(), refs.end());
            processed_files++;
        } catch (const std::exception& e) {
            spdlog::warn("FindReferencesTool: failed to process {}: {}", filepath.string(), e.what());
            failed_files++;
        }
    }

    // Build result JSON
    json result;
    result["symbol"] = symbol;
    result["total_references"] = all_references.size();
    result["files_searched"] = resolved.size();
    result["files_processed"] = processed_files;
    result["files_failed"] = failed_files;

    json references_array = json::array();
    for (const auto& ref : all_references) {
        references_array.push_back(reference_to_json(ref));
    }
    result["references"] = references_array;
    result["success"] = true;

    return result;
}

std::vector<FindReferencesTool::Reference> FindReferencesTool::find_in_file(
    const std::string& filepath,
    const std::string& symbol,
    Language language
) {
    std::vector<Reference> references;

    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::warn("FindReferencesTool: cannot open file {}", filepath);
        return references;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Quick text search first (optimization)
    if (source.find(symbol) == std::string::npos) {
        // Symbol not found in file at all
        return references;
    }

    // Parse with tree-sitter
    TreeSitterParser parser(language);
    auto parse_result = parser.parse_string(source);

    if (!parse_result) {
        spdlog::warn("FindReferencesTool: parse failed for {}", filepath);
        return references;
    }

    TSTreeCursor cursor = ts_tree_cursor_new(ts_tree_root_node(parse_result->get()));

    // Traverse AST looking for identifier nodes
    bool has_next = true;
    while (has_next) {
        TSNode node = ts_tree_cursor_current_node(&cursor);
        std::string node_type = ts_node_type(node);

        // Check if this is an identifier or type_identifier node
        if (node_type == "identifier" || node_type == "type_identifier" || node_type == "field_identifier") {
            if (node_matches_symbol(node, symbol, source)) {
                // Found a match - create reference
                Reference ref;
                ref.filepath = filepath;

                TSPoint start = ts_node_start_point(node);
                ref.line = start.row + 1;
                ref.column = start.column + 1;

                ref.type = classify_reference(node, source, language);
                ref.context = extract_context(node, source, 0);
                ref.parent_scope = find_parent_scope(node, source);
                ref.node_type = node_type;

                references.push_back(ref);
            }
        }

        // Navigate tree
        if (ts_tree_cursor_goto_first_child(&cursor)) {
            continue;
        }

        while (!ts_tree_cursor_goto_next_sibling(&cursor)) {
            if (!ts_tree_cursor_goto_parent(&cursor)) {
                has_next = false;
                break;
            }
        }
    }

    ts_tree_cursor_delete(&cursor);

    return references;
}

bool FindReferencesTool::node_matches_symbol(
    TSNode node,
    const std::string& symbol,
    std::string_view source
) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);

    if (end > source.size()) {
        return false;
    }

    std::string node_text = std::string(source.substr(start, end - start));
    return node_text == symbol;
}

FindReferencesTool::ReferenceType FindReferencesTool::classify_reference(
    TSNode node,
    [[maybe_unused]] std::string_view source,
    [[maybe_unused]] Language language
) {
    TSNode parent = ts_node_parent(node);

    if (ts_node_is_null(parent)) {
        return ReferenceType::UNKNOWN;
    }

    std::string parent_type = ts_node_type(parent);

    // Function call: parent is call_expression
    if (parent_type == "call_expression") {
        TSNode func_node = ts_node_child_by_field_name(parent, "function", 8);
        if (!ts_node_is_null(func_node) && ts_node_eq(func_node, node)) {
            return ReferenceType::CALL;
        }
    }

    // Declaration: parent is declarator or parameter_declaration
    if (parent_type == "declarator" || parent_type == "parameter_declaration" ||
        parent_type == "variable_declarator" || parent_type == "init_declarator") {
        return ReferenceType::DECLARATION;
    }

    // Definition: parent is function_definition or class_specifier
    TSNode grandparent = ts_node_parent(parent);
    if (!ts_node_is_null(grandparent)) {
        std::string grandparent_type = ts_node_type(grandparent);
        if (grandparent_type == "function_definition" || grandparent_type == "class_specifier") {
            return ReferenceType::DEFINITION;
        }
    }

    // Member access: parent is field_expression or qualified_identifier
    if (parent_type == "field_expression" || parent_type == "qualified_identifier" ||
        parent_type == "attribute") {
        return ReferenceType::MEMBER_ACCESS;
    }

    // Type usage: parent is type-related node
    if (parent_type == "type_identifier" || parent_type == "sized_type_specifier" ||
        parent_type == "type_descriptor" || parent_type == "class_type") {
        return ReferenceType::TYPE_USAGE;
    }

    return ReferenceType::UNKNOWN;
}

std::string FindReferencesTool::extract_context(
    TSNode node,
    std::string_view source,
    [[maybe_unused]] int context_lines
) {
    // For now, extract the single line containing the node
    TSPoint start_point = ts_node_start_point(node);

    // Find line start
    size_t line_start = 0;
    int current_line = 0;
    for (size_t i = 0; i < source.size() && current_line < static_cast<int>(start_point.row); i++) {
        if (source[i] == '\n') {
            current_line++;
            line_start = i + 1;
        }
    }

    // Find line end
    size_t line_end = line_start;
    while (line_end < source.size() && source[line_end] != '\n') {
        line_end++;
    }

    std::string line = std::string(source.substr(line_start, line_end - line_start));

    // Trim whitespace
    line.erase(0, line.find_first_not_of(" \t"));
    line.erase(line.find_last_not_of(" \t") + 1);

    return line;
}

std::string FindReferencesTool::find_parent_scope(
    TSNode node,
    std::string_view source
) {
    TSNode current = node;

    while (!ts_node_is_null(current)) {
        current = ts_node_parent(current);

        if (ts_node_is_null(current)) {
            break;
        }

        std::string node_type = ts_node_type(current);

        // Found parent function
        if (node_type == "function_definition" || node_type == "function_declaration") {
            TSNode declarator = ts_node_child_by_field_name(current, "declarator", 10);
            if (!ts_node_is_null(declarator)) {
                // Get function name from declarator
                TSNode name_node = ts_node_child_by_field_name(declarator, "declarator", 10);
                if (ts_node_is_null(name_node)) {
                    name_node = declarator;
                }

                // Find identifier in declarator tree
                uint32_t child_count = ts_node_child_count(name_node);
                for (uint32_t i = 0; i < child_count; i++) {
                    TSNode child = ts_node_child(name_node, i);
                    std::string child_type = ts_node_type(child);
                    if (child_type == "identifier" || child_type == "field_identifier") {
                        uint32_t start = ts_node_start_byte(child);
                        uint32_t end = ts_node_end_byte(child);
                        return std::string(source.substr(start, end - start));
                    }
                }
            }
        }

        // Found parent class
        if (node_type == "class_specifier" || node_type == "struct_specifier") {
            TSNode name_node = ts_node_child_by_field_name(current, "name", 4);
            if (!ts_node_is_null(name_node)) {
                uint32_t start = ts_node_start_byte(name_node);
                uint32_t end = ts_node_end_byte(name_node);
                return std::string(source.substr(start, end - start));
            }
        }

        // Python: function_definition or class_definition
        if (node_type == "function_definition" || node_type == "class_definition") {
            TSNode name_node = ts_node_child_by_field_name(current, "name", 4);
            if (!ts_node_is_null(name_node)) {
                uint32_t start = ts_node_start_byte(name_node);
                uint32_t end = ts_node_end_byte(name_node);
                return std::string(source.substr(start, end - start));
            }
        }
    }

    return "";  // Global scope
}

std::string FindReferencesTool::reference_type_to_string(ReferenceType type) {
    switch (type) {
        case ReferenceType::DECLARATION: return "declaration";
        case ReferenceType::DEFINITION: return "definition";
        case ReferenceType::CALL: return "call";
        case ReferenceType::MEMBER_ACCESS: return "member_access";
        case ReferenceType::TYPE_USAGE: return "type_usage";
        case ReferenceType::UNKNOWN: return "unknown";
    }
    return "unknown";
}

json FindReferencesTool::reference_to_json(const Reference& ref) {
    json j;
    j["filepath"] = ref.filepath;
    j["line"] = ref.line;
    j["column"] = ref.column;
    j["type"] = reference_type_to_string(ref.type);
    j["context"] = ref.context;

    if (!ref.parent_scope.empty()) {
        j["parent_scope"] = ref.parent_scope;
    }

    j["node_type"] = ref.node_type;

    return j;
}

} // namespace ts_mcp
