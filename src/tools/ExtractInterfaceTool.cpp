#include "ExtractInterfaceTool.hpp"
#include "core/PathResolver.hpp"
#include "core/TreeSitterParser.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>

// Tree-sitter C API
extern "C" {
    #include <tree_sitter/api.h>
}

namespace ts_mcp {

ExtractInterfaceTool::ExtractInterfaceTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)) {
    if (!analyzer_) {
        throw std::invalid_argument("Analyzer cannot be null");
    }
}

ToolInfo ExtractInterfaceTool::get_info() {
    return {
        "extract_interface",
        "Extract function signatures and class interfaces without implementation bodies (reduces context size)",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"oneOf", json::array({
                        {{"type", "string"}, {"description", "Single file or directory path"}},
                        {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Multiple file or directory paths"}}
                    })}
                }},
                {"include_private", {
                    {"type", "boolean"},
                    {"default", false},
                    {"description", "Include private class members"}
                }},
                {"include_comments", {
                    {"type", "boolean"},
                    {"default", true},
                    {"description", "Include comments and docstrings"}
                }},
                {"output_format", {
                    {"type", "string"},
                    {"enum", json::array({"json", "header", "markdown"})},
                    {"default", "json"},
                    {"description", "Output format: json (structured), header (.hpp format), or markdown (documentation)"}
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
                    {"description", "File patterns to include (glob patterns)"}
                }}
            }},
            {"required", json::array({"filepath"})}
        }
    };
}

json ExtractInterfaceTool::execute(const json& args) {
    if (!args.contains("filepath")) {
        json error_result = {
            {"error", "Missing required parameter: filepath"}
        };
        return error_result;
    }

    bool include_private = args.value("include_private", false);
    bool include_comments = args.value("include_comments", true);
    std::string output_format = args.value("output_format", "json");
    bool recursive = args.value("recursive", true);

    std::vector<std::string> file_patterns =
        args.value("file_patterns", std::vector<std::string>{
            "*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"
        });

    // Determine filepath type and collect paths
    std::vector<std::string> input_paths;
    if (args["filepath"].is_string()) {
        input_paths.push_back(args["filepath"].get<std::string>());
    } else if (args["filepath"].is_array()) {
        input_paths = args["filepath"].get<std::vector<std::string>>();
    } else {
        json error_result;
        error_result["error"] = "filepath must be a string or array of strings";
        return error_result;
    }

    // Resolve file paths
    auto resolved = PathResolver::resolve_paths(input_paths, recursive, file_patterns);

    if (resolved.empty()) {
        json error_result = {
            {"error", "No files found matching the specified paths"}
        };
        return error_result;
    }

    spdlog::debug("ExtractInterfaceTool: processing {} files", resolved.size());

    // Process single file
    if (resolved.size() == 1) {
        const auto& filepath = resolved[0];
        Language lang = LanguageUtils::detect_from_extension(filepath);

        if (lang == Language::UNKNOWN) {
            json error_result = {
                {"error", "Unsupported file type"},
                {"filepath", filepath}
            };
            return error_result;
        }

        try {
            json interface_data = extract_from_file(filepath, include_private, include_comments, lang);

            // Format output
            if (output_format == "json") {
                return format_as_json(interface_data, filepath, lang);
            } else if (output_format == "header") {
                std::string header_text = format_as_header(interface_data, filepath, lang);
                json result = {
                    {"filepath", filepath},
                    {"format", "header"},
                    {"content", header_text},
                    {"success", true}
                };
                return result;
            } else if (output_format == "markdown") {
                std::string markdown_text = format_as_markdown(interface_data, filepath, lang);
                json result = {
                    {"filepath", filepath},
                    {"format", "markdown"},
                    {"content", markdown_text},
                    {"success", true}
                };
                return result;
            } else {
                json error_result = {
                    {"error", "Invalid output_format: " + output_format}
                };
                return error_result;
            }
        } catch (const std::exception& e) {
            json error_result = {
                {"error", e.what()},
                {"filepath", filepath},
                {"success", false}
            };
            return error_result;
        }
    }

    // Process multiple files (only JSON format supported)
    json results = json::array();
    int success_count = 0;
    int failed_count = 0;

    for (const auto& filepath : resolved) {
        Language lang = LanguageUtils::detect_from_extension(filepath);

        if (lang == Language::UNKNOWN) {
            json error_item = {
                {"filepath", filepath},
                {"error", "Unsupported file type"},
                {"success", false}
            };
            results.push_back(error_item);
            failed_count++;
            continue;
        }

        try {
            json interface_data = extract_from_file(filepath, include_private, include_comments, lang);
            json formatted = format_as_json(interface_data, filepath, lang);
            results.push_back(formatted);
            success_count++;
        } catch (const std::exception& e) {
            json error_item = {
                {"filepath", filepath},
                {"error", e.what()},
                {"success", false}
            };
            results.push_back(error_item);
            failed_count++;
        }
    }

    json final_result = {
        {"total_files", resolved.size()},
        {"processed_files", success_count},
        {"failed_files", failed_count},
        {"output_format", output_format},
        {"results", results}
    };
    return final_result;
}

json ExtractInterfaceTool::extract_from_file(
    const std::string& filepath,
    bool include_private,
    bool include_comments,
    Language language
) {
    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse file using TreeSitterParser directly
    TreeSitterParser parser(language);
    auto parse_result = parser.parse_string(source);
    if (!parse_result) {
        throw std::runtime_error("Failed to parse file: " + filepath);
    }

    json result = {
        {"classes", json::array()},
        {"functions", json::array()},
        {"namespaces", json::array()},  // C++ only
        {"language", LanguageUtils::to_string(language)}
    };

    // Extract functions
    auto func_query = query_engine_.compile_query(
        QueryEngine::get_predefined_query(QueryType::FUNCTIONS, language).value_or(""),
        language
    );

    if (func_query) {
        auto matches = query_engine_.execute(*parse_result, *func_query, source);
        for (const auto& match : matches) {
            json func_sig = extract_function_signature(
                match.node, source, include_comments, language
            );
            result["functions"].push_back(func_sig);
        }
    }

    // Extract classes
    auto class_query = query_engine_.compile_query(
        QueryEngine::get_predefined_query(QueryType::CLASSES, language).value_or(""),
        language
    );

    if (class_query) {
        auto matches = query_engine_.execute(*parse_result, *class_query, source);
        for (const auto& match : matches) {
            // Find the full class node (not just the name)
            TSNode class_node = ts_node_parent(match.node);
            json class_iface = extract_class_interface(
                class_node, source, include_private, include_comments, language
            );
            result["classes"].push_back(class_iface);
        }
    }

    // Extract namespaces (C++ only)
    if (language == Language::CPP) {
        auto ns_query = query_engine_.compile_query(
            QueryEngine::get_predefined_query(QueryType::NAMESPACES, language).value_or(""),
            language
        );

        if (ns_query) {
            auto matches = query_engine_.execute(*parse_result, *ns_query, source);
            for (const auto& match : matches) {
                json ns_info = {
                    {"name", match.text},
                    {"line", match.line}
                };
                result["namespaces"].push_back(ns_info);
            }
        }
    }

    return result;
}

json ExtractInterfaceTool::extract_function_signature(
    TSNode node,
    std::string_view source,
    bool include_comments,
    Language language
) {
    std::string signature = get_signature_text(node, source, language);
    TSPoint start = ts_node_start_point(node);

    json result = {
        {"signature", signature},
        {"line", start.row + 1},  // 1-based line numbers
        {"column", start.column}
    };

    if (include_comments) {
        std::string comment = get_preceding_comment(node, source, language);
        if (!comment.empty()) {
            result["comment"] = comment;
        }
    }

    // Extract decorators for Python
    if (language == Language::PYTHON) {
        auto decorators = extract_decorators(node, source);
        if (!decorators.empty()) {
            result["decorators"] = decorators;
        }
    }

    return result;
}

json ExtractInterfaceTool::extract_class_interface(
    TSNode node,
    std::string_view source,
    bool include_private,
    bool include_comments,
    Language language
) {
    json result;

    // Get class name
    uint32_t child_count = ts_node_child_count(node);
    std::string class_name;

    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* type = ts_node_type(child);

        if (std::string(type) == "type_identifier" || std::string(type) == "identifier") {
            uint32_t start = ts_node_start_byte(child);
            uint32_t end = ts_node_end_byte(child);
            class_name = std::string(source.substr(start, end - start));
            break;
        }
    }

    TSPoint start = ts_node_start_point(node);
    result["name"] = class_name;
    result["line"] = start.row + 1;
    result["methods"] = json::array();
    result["members"] = json::array();

    if (include_comments) {
        std::string comment = get_preceding_comment(node, source, language);
        if (!comment.empty()) {
            result["comment"] = comment;
        }
    }

    // Extract base classes (C++)
    if (language == Language::CPP) {
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (std::string(ts_node_type(child)) == "base_class_clause") {
                // Extract base class names
                json bases = json::array();
                uint32_t base_count = ts_node_child_count(child);
                for (uint32_t j = 0; j < base_count; j++) {
                    TSNode base_node = ts_node_child(child, j);
                    if (std::string(ts_node_type(base_node)) == "type_identifier") {
                        uint32_t s = ts_node_start_byte(base_node);
                        uint32_t e = ts_node_end_byte(base_node);
                        bases.push_back(std::string(source.substr(s, e - s)));
                    }
                }
                if (!bases.empty()) {
                    result["base_classes"] = bases;
                }
                break;
            }
        }
    }

    // Extract methods and members from class body
    TSNode body_node = ts_node_child_by_field_name(node, "body", 4);
    if (!ts_node_is_null(body_node)) {
        uint32_t member_count = ts_node_child_count(body_node);

        for (uint32_t i = 0; i < member_count; i++) {
            TSNode member = ts_node_child(body_node, i);
            std::string member_type = ts_node_type(member);

            // Check access specifier
            std::string access = get_access_specifier(member);
            if (!include_private && (access == "private")) {
                continue;  // Skip private members if not requested
            }

            // Extract method signatures
            if (member_type == "function_definition") {
                json method_sig = extract_function_signature(member, source, include_comments, language);
                if (!access.empty() && access != "unknown") {
                    method_sig["access"] = access;
                }
                result["methods"].push_back(method_sig);
            }
            // Extract member variables
            else if (member_type == "field_declaration") {
                uint32_t decl_start = ts_node_start_byte(member);
                uint32_t decl_end = ts_node_end_byte(member);
                std::string decl_text = std::string(source.substr(decl_start, decl_end - decl_start));

                // Remove trailing semicolon and whitespace
                while (!decl_text.empty() && (decl_text.back() == ';' || std::isspace(decl_text.back()))) {
                    decl_text.pop_back();
                }

                TSPoint member_pos = ts_node_start_point(member);
                json member_info = {
                    {"declaration", decl_text},
                    {"line", member_pos.row + 1}
                };

                if (!access.empty() && access != "unknown") {
                    member_info["access"] = access;
                }

                result["members"].push_back(member_info);
            }
        }
    }

    // Extract decorators for Python classes
    if (language == Language::PYTHON) {
        auto decorators = extract_decorators(node, source);
        if (!decorators.empty()) {
            result["decorators"] = decorators;
        }
    }

    return result;
}

std::string ExtractInterfaceTool::get_signature_text(
    TSNode node,
    std::string_view source,
    Language language
) {
    if (language == Language::CPP) {
        // For C++: extract everything except the function body (compound_statement)
        std::stringstream signature;
        uint32_t child_count = ts_node_child_count(node);

        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            std::string child_type = ts_node_type(child);

            // Skip the function body
            if (child_type == "compound_statement") {
                signature << ";";  // Add semicolon for declaration
                break;
            }

            uint32_t start = ts_node_start_byte(child);
            uint32_t end = ts_node_end_byte(child);
            signature << source.substr(start, end - start);

            // Add space between tokens if needed
            if (i < child_count - 1 && child_type != "(") {
                signature << " ";
            }
        }

        return signature.str();
    }
    else if (language == Language::PYTHON) {
        // For Python: extract def + name + parameters (no body)
        std::stringstream signature;
        uint32_t child_count = ts_node_child_count(node);

        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            std::string child_type = ts_node_type(child);

            // Include: "def", "async", name, parameters, return type
            if (child_type == "def" || child_type == "async" ||
                child_type == "identifier" || child_type == "parameters" ||
                child_type == "type") {

                uint32_t start = ts_node_start_byte(child);
                uint32_t end = ts_node_end_byte(child);
                signature << source.substr(start, end - start);

                if (child_type != "parameters") {
                    signature << " ";
                }
            }

            // Stop before the body (block or :)
            if (child_type == "block" || child_type == ":") {
                signature << ":";
                break;
            }
        }

        return signature.str();
    }

    // Fallback: return full node text
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    return std::string(source.substr(start, end - start));
}

std::string ExtractInterfaceTool::get_preceding_comment(
    TSNode node,
    std::string_view source,
    [[maybe_unused]] Language language
) {
    // Get previous sibling (might be a comment)
    TSNode prev_sibling = ts_node_prev_sibling(node);

    if (ts_node_is_null(prev_sibling)) {
        return "";
    }

    std::string sibling_type = ts_node_type(prev_sibling);

    // Check if it's a comment node
    if (sibling_type == "comment") {
        uint32_t start = ts_node_start_byte(prev_sibling);
        uint32_t end = ts_node_end_byte(prev_sibling);
        return std::string(source.substr(start, end - start));
    }

    return "";
}

json ExtractInterfaceTool::format_as_json(
    const json& interface_data,
    const std::string& filepath,
    Language language
) {
    json result = interface_data;
    result["filepath"] = filepath;
    result["success"] = true;

    // Calculate statistics
    result["total_functions"] = result["functions"].size();
    result["total_classes"] = result["classes"].size();

    if (language == Language::CPP) {
        result["total_namespaces"] = result["namespaces"].size();
    }

    return result;
}

std::string ExtractInterfaceTool::format_as_header(
    const json& interface_data,
    const std::string& filepath,
    Language language
) {
    std::stringstream header;

    // Header guard
    std::string guard_name = std::filesystem::path(filepath).filename().string();
    std::transform(guard_name.begin(), guard_name.end(), guard_name.begin(), ::toupper);
    std::replace(guard_name.begin(), guard_name.end(), '.', '_');

    header << "#pragma once\n\n";
    header << "// Extracted interface from: " << filepath << "\n";
    header << "// Language: " << LanguageUtils::to_string(language) << "\n\n";

    // Namespaces (C++)
    if (language == Language::CPP && interface_data.contains("namespaces")) {
        for (const auto& ns : interface_data["namespaces"]) {
            header << "namespace " << ns["name"].get<std::string>() << " {\n\n";
        }
    }

    // Classes
    if (interface_data.contains("classes")) {
        for (const auto& cls : interface_data["classes"]) {
            if (cls.contains("comment")) {
                header << cls["comment"].get<std::string>() << "\n";
            }

            header << "class " << cls["name"].get<std::string>();

            if (cls.contains("base_classes") && !cls["base_classes"].empty()) {
                header << " : ";
                for (size_t i = 0; i < cls["base_classes"].size(); i++) {
                    if (i > 0) header << ", ";
                    header << "public " << cls["base_classes"][i].get<std::string>();
                }
            }

            header << " {\n";

            // Group by access specifier
            std::string current_access = "";

            // Methods
            for (const auto& method : cls["methods"]) {
                std::string access = method.value("access", "public");
                if (access != current_access) {
                    header << access << ":\n";
                    current_access = access;
                }

                header << "    " << method["signature"].get<std::string>() << "\n";
            }

            // Members
            if (cls.contains("members")) {
                for (const auto& member : cls["members"]) {
                    std::string access = member.value("access", "private");
                    if (access != current_access) {
                        header << access << ":\n";
                        current_access = access;
                    }

                    header << "    " << member["declaration"].get<std::string>() << ";\n";
                }
            }

            header << "};\n\n";
        }
    }

    // Free functions
    if (interface_data.contains("functions")) {
        for (const auto& func : interface_data["functions"]) {
            if (func.contains("comment")) {
                header << func["comment"].get<std::string>() << "\n";
            }
            header << func["signature"].get<std::string>() << "\n\n";
        }
    }

    // Close namespaces
    if (language == Language::CPP && interface_data.contains("namespaces")) {
        for (size_t i = 0; i < interface_data["namespaces"].size(); i++) {
            header << "} // namespace " << interface_data["namespaces"][i]["name"].get<std::string>() << "\n";
        }
    }

    return header.str();
}

std::string ExtractInterfaceTool::format_as_markdown(
    const json& interface_data,
    const std::string& filepath,
    Language language
) {
    std::stringstream md;

    md << "# Interface: " << std::filesystem::path(filepath).filename().string() << "\n\n";
    md << "**Language:** " << LanguageUtils::to_string(language) << "  \n";
    md << "**File:** `" << filepath << "`\n\n";

    // Summary
    md << "## Summary\n\n";
    if (interface_data.contains("classes")) {
        md << "- **Classes:** " << interface_data["classes"].size() << "\n";
    }
    if (interface_data.contains("functions")) {
        md << "- **Functions:** " << interface_data["functions"].size() << "\n";
    }
    if (language == Language::CPP && interface_data.contains("namespaces")) {
        md << "- **Namespaces:** " << interface_data["namespaces"].size() << "\n";
    }
    md << "\n";

    // Namespaces
    if (language == Language::CPP && interface_data.contains("namespaces") &&
        !interface_data["namespaces"].empty()) {
        md << "## Namespaces\n\n";
        for (const auto& ns : interface_data["namespaces"]) {
            md << "- `" << ns["name"].get<std::string>() << "` (line " << ns["line"] << ")\n";
        }
        md << "\n";
    }

    // Classes
    if (interface_data.contains("classes") && !interface_data["classes"].empty()) {
        md << "## Classes\n\n";
        for (const auto& cls : interface_data["classes"]) {
            md << "### `" << cls["name"].get<std::string>() << "`\n\n";

            if (cls.contains("comment")) {
                md << cls["comment"].get<std::string>() << "\n\n";
            }

            md << "**Location:** Line " << cls["line"] << "\n\n";

            if (cls.contains("base_classes") && !cls["base_classes"].empty()) {
                md << "**Inherits from:** ";
                for (size_t i = 0; i < cls["base_classes"].size(); i++) {
                    if (i > 0) md << ", ";
                    md << "`" << cls["base_classes"][i].get<std::string>() << "`";
                }
                md << "\n\n";
            }

            // Methods
            if (cls.contains("methods") && !cls["methods"].empty()) {
                md << "**Methods:**\n\n";
                for (const auto& method : cls["methods"]) {
                    md << "- `" << method["signature"].get<std::string>() << "`";
                    if (method.contains("access")) {
                        md << " (" << method["access"].get<std::string>() << ")";
                    }
                    md << "\n";
                }
                md << "\n";
            }

            // Members
            if (cls.contains("members") && !cls["members"].empty()) {
                md << "**Members:**\n\n";
                for (const auto& member : cls["members"]) {
                    md << "- `" << member["declaration"].get<std::string>() << "`";
                    if (member.contains("access")) {
                        md << " (" << member["access"].get<std::string>() << ")";
                    }
                    md << "\n";
                }
                md << "\n";
            }
        }
    }

    // Free functions
    if (interface_data.contains("functions") && !interface_data["functions"].empty()) {
        md << "## Functions\n\n";
        for (const auto& func : interface_data["functions"]) {
            if (func.contains("comment")) {
                md << func["comment"].get<std::string>() << "\n\n";
            }
            md << "```" << LanguageUtils::to_string(language) << "\n";
            md << func["signature"].get<std::string>() << "\n";
            md << "```\n\n";

            md << "**Line:** " << func["line"] << "\n\n";
        }
    }

    return md.str();
}

std::string ExtractInterfaceTool::get_access_specifier(TSNode node) {
    // Look for previous sibling that might be an access specifier
    TSNode prev = node;

    // Walk backwards to find access specifier
    while (!ts_node_is_null(prev)) {
        prev = ts_node_prev_sibling(prev);
        if (ts_node_is_null(prev)) break;

        std::string type = ts_node_type(prev);
        if (type == "access_specifier") {
            // This is C++ access specifier node, get its text would be "public:", "private:", etc.
            // We need to check its child
            uint32_t child_count = ts_node_child_count(prev);
            if (child_count > 0) {
                TSNode child = ts_node_child(prev, 0);
                std::string spec_type = ts_node_type(child);
                if (spec_type == "public" || spec_type == "private" || spec_type == "protected") {
                    return spec_type;
                }
            }
        }

        // If we hit another member, stop searching
        if (type == "function_definition" || type == "field_declaration") {
            break;
        }
    }

    return "public";  // Default to public if not found (struct) or unknown
}

std::vector<std::string> ExtractInterfaceTool::extract_decorators(
    TSNode node,
    std::string_view source
) {
    std::vector<std::string> decorators;

    // Python decorators are previous siblings of function/class
    TSNode prev = node;

    while (!ts_node_is_null(prev)) {
        prev = ts_node_prev_sibling(prev);
        if (ts_node_is_null(prev)) break;

        std::string type = ts_node_type(prev);
        if (type == "decorator") {
            uint32_t start = ts_node_start_byte(prev);
            uint32_t end = ts_node_end_byte(prev);
            std::string decorator_text = std::string(source.substr(start, end - start));
            decorators.insert(decorators.begin(), decorator_text);  // Prepend to maintain order
        } else if (type != "comment") {
            // Stop if we hit a non-decorator, non-comment node
            break;
        }
    }

    return decorators;
}

} // namespace ts_mcp
