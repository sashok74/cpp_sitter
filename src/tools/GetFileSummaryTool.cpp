#include "tools/GetFileSummaryTool.hpp"
#include "core/PathResolver.hpp"
#include "core/TreeSitterParser.hpp"
#include "core/Language.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace ts_mcp {

GetFileSummaryTool::GetFileSummaryTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(analyzer)
    , query_engine_() {
}

ToolInfo GetFileSummaryTool::get_info() {
    ToolInfo info;
    info.name = "get_file_summary";
    info.description = "Get comprehensive file summary with metrics, complexity, TODO markers, and detailed signatures";

    info.input_schema = {
        {"type", "object"},
        {"properties", {
            {"filepath", {
                {"oneOf", json::array({
                    {{"type", "string"}, {"description", "Single file path"}},
                    {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Multiple file paths"}}
                })}
            }},
            {"include_complexity", {
                {"type", "boolean"},
                {"default", true},
                {"description", "Calculate cyclomatic complexity per function"}
            }},
            {"include_comments", {
                {"type", "boolean"},
                {"default", true},
                {"description", "Extract TODO/FIXME/HACK markers from comments"}
            }},
            {"include_docstrings", {
                {"type", "boolean"},
                {"default", true},
                {"description", "Extract documentation comments"}
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
        {"required", json::array({"filepath"})}
    };

    return info;
}

json GetFileSummaryTool::execute(const json& args) {
    if (!args.contains("filepath")) {
        json error_result;
        error_result["error"] = "Missing required parameter: filepath";
        return error_result;
    }

    bool include_complexity = args.value("include_complexity", true);
    bool include_comments = args.value("include_comments", true);
    bool include_docstrings = args.value("include_docstrings", true);
    bool recursive = args.value("recursive", true);

    std::vector<std::string> file_patterns =
        args.value("file_patterns", std::vector<std::string>{
            "*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"
        });

    // Determine filepath type
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

    // Resolve paths
    auto resolved = PathResolver::resolve_paths(input_paths, recursive, file_patterns);

    if (resolved.empty()) {
        json error_result;
        error_result["error"] = "No files found matching the specified paths";
        return error_result;
    }

    spdlog::debug("GetFileSummaryTool: processing {} files", resolved.size());

    // Process single file
    if (resolved.size() == 1) {
        const auto& filepath = resolved[0];
        Language lang = LanguageUtils::detect_from_extension(filepath);

        if (lang == Language::UNKNOWN) {
            json error_result;
            error_result["error"] = "Unsupported file type";
            error_result["filepath"] = filepath.string();
            return error_result;
        }

        try {
            return summarize_file(filepath.string(), lang, include_complexity,
                                 include_comments, include_docstrings);
        } catch (const std::exception& e) {
            json error_result;
            error_result["error"] = e.what();
            error_result["filepath"] = filepath.string();
            error_result["success"] = false;
            return error_result;
        }
    }

    // Process multiple files
    json results = json::array();
    int success_count = 0;
    int failed_count = 0;

    for (const auto& filepath : resolved) {
        Language lang = LanguageUtils::detect_from_extension(filepath);

        if (lang == Language::UNKNOWN) {
            json error_item;
            error_item["filepath"] = filepath.string();
            error_item["error"] = "Unsupported file type";
            error_item["success"] = false;
            results.push_back(error_item);
            failed_count++;
            continue;
        }

        try {
            json summary = summarize_file(filepath.string(), lang, include_complexity,
                                         include_comments, include_docstrings);
            results.push_back(summary);
            success_count++;
        } catch (const std::exception& e) {
            json error_item;
            error_item["filepath"] = filepath.string();
            error_item["error"] = e.what();
            error_item["success"] = false;
            results.push_back(error_item);
            failed_count++;
        }
    }

    json final_result;
    final_result["total_files"] = resolved.size();
    final_result["processed_files"] = success_count;
    final_result["failed_files"] = failed_count;
    final_result["results"] = results;
    final_result["success"] = true;

    return final_result;
}

json GetFileSummaryTool::summarize_file(
    const std::string& filepath,
    Language language,
    bool include_complexity,
    bool include_comments,
    bool include_docstrings
) {
    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse with tree-sitter
    TreeSitterParser parser(language);
    auto parse_result = parser.parse_string(source);

    if (!parse_result) {
        throw std::runtime_error("Parse failed for file: " + filepath);
    }

    TSNode root = ts_tree_root_node(parse_result->get());

    // Initialize result
    json result;
    result["filepath"] = filepath;
    result["language"] = LanguageUtils::to_string(language);
    result["success"] = true;

    // Basic metrics
    auto metrics = calculate_metrics(source);
    result["metrics"] = {
        {"total_lines", metrics["loc"]},
        {"code_lines", metrics["sloc"]},
        {"comment_lines", metrics["comment_lines"]},
        {"blank_lines", metrics["blank_lines"]}
    };

    // Extract functions
    auto func_query = query_engine_.compile_query(
        QueryEngine::get_predefined_query(QueryType::FUNCTIONS, language).value_or(""),
        language
    );

    json functions = json::array();
    if (func_query) {
        auto matches = query_engine_.execute(*parse_result, *func_query, source);
        for (const auto& match : matches) {
            TSNode func_node = match.node;
            // Find parent function_definition
            TSNode parent = ts_node_parent(func_node);
            while (!ts_node_is_null(parent)) {
                std::string type = ts_node_type(parent);
                if (type == "function_definition" || type == "method_definition") {
                    func_node = parent;
                    break;
                }
                parent = ts_node_parent(parent);
            }

            FunctionSignature sig = extract_function_signature(
                func_node, source, language, include_docstrings
            );

            json func_json;
            func_json["name"] = sig.name;
            func_json["return_type"] = sig.return_type;
            func_json["line"] = sig.line;

            if (include_complexity) {
                sig.complexity = calculate_complexity(func_node, source);
                func_json["complexity"] = sig.complexity;
            }

            if (!sig.parameters.empty()) {
                json params = json::array();
                for (const auto& [type, name] : sig.parameters) {
                    json param;
                    param["type"] = type;
                    param["name"] = name;
                    params.push_back(param);
                }
                func_json["parameters"] = params;
            }

            if (include_docstrings && !sig.docstring.empty()) {
                func_json["docstring"] = sig.docstring;
            }

            if (sig.is_virtual) func_json["is_virtual"] = true;
            if (sig.is_static) func_json["is_static"] = true;
            if (sig.is_async) func_json["is_async"] = true;

            functions.push_back(func_json);
        }
    }
    result["functions"] = functions;
    result["function_count"] = functions.size();

    // Extract classes
    auto class_query = query_engine_.compile_query(
        QueryEngine::get_predefined_query(QueryType::CLASSES, language).value_or(""),
        language
    );

    json classes = json::array();
    if (class_query) {
        auto matches = query_engine_.execute(*parse_result, *class_query, source);
        for (const auto& match : matches) {
            json class_info;
            class_info["name"] = match.text;
            class_info["line"] = match.line;
            classes.push_back(class_info);
        }
    }
    result["classes"] = classes;
    result["class_count"] = classes.size();

    // Extract imports/includes
    auto imports = extract_imports(root, source, language);
    json imports_json = json::array();
    for (const auto& import : imports) {
        json imp;
        imp["path"] = import.path;
        imp["line"] = import.line;
        if (language == Language::CPP) {
            imp["is_system"] = import.is_system;
        } else if (language == Language::PYTHON) {
            if (!import.module.empty()) {
                imp["module"] = import.module;
            }
        }
        imports_json.push_back(imp);
    }
    result["imports"] = imports_json;
    result["import_count"] = imports.size();

    // Extract comment markers
    if (include_comments) {
        auto markers = extract_comment_markers(source, language);
        json markers_json = json::array();
        for (const auto& marker : markers) {
            json m;
            m["type"] = marker_to_string(marker.type);
            m["text"] = marker.text;
            m["line"] = marker.line;
            if (!marker.context.empty()) {
                m["context"] = marker.context;
            }
            markers_json.push_back(m);
        }
        result["comment_markers"] = markers_json;
        result["marker_count"] = markers.size();
    }

    // Calculate average complexity
    if (include_complexity && !functions.empty()) {
        int total_complexity = 0;
        for (const auto& func : functions) {
            if (func.contains("complexity")) {
                total_complexity += func["complexity"].get<int>();
            }
        }
        result["average_complexity"] = functions.size() > 0
            ? static_cast<double>(total_complexity) / functions.size()
            : 0.0;
    }

    return result;
}

int GetFileSummaryTool::calculate_complexity(TSNode node, [[maybe_unused]] std::string_view source) {
    int complexity = 1;  // Base complexity

    // Count decision points
    TSTreeCursor cursor = ts_tree_cursor_new(node);
    bool has_next = true;

    while (has_next) {
        TSNode current = ts_tree_cursor_current_node(&cursor);
        std::string node_type = ts_node_type(current);

        // Branch points increase complexity
        if (node_type == "if_statement" ||
            node_type == "for_statement" ||
            node_type == "while_statement" ||
            node_type == "do_statement" ||
            node_type == "case_statement" ||
            node_type == "catch_clause" ||
            node_type == "conditional_expression" ||  // ternary operator
            node_type == "logical_and" ||  // && operator
            node_type == "logical_or") {   // || operator
            complexity++;
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
            // Stop if we're back at the original node
            if (ts_node_eq(ts_tree_cursor_current_node(&cursor), node)) {
                has_next = false;
                break;
            }
        }
    }

    ts_tree_cursor_delete(&cursor);
    return complexity;
}

GetFileSummaryTool::FunctionSignature GetFileSummaryTool::extract_function_signature(
    TSNode node,
    std::string_view source,
    Language language,
    bool include_docstring
) {
    FunctionSignature sig;
    sig.complexity = 0;
    sig.is_virtual = false;
    sig.is_static = false;
    sig.is_async = false;

    TSPoint start_point = ts_node_start_point(node);
    sig.line = start_point.row + 1;

    if (language == Language::CPP) {
        // Check for virtual/static specifiers
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            std::string child_type = ts_node_type(child);

            if (child_type == "virtual_function_specifier") {
                sig.is_virtual = true;
            } else if (child_type == "storage_class_specifier") {
                uint32_t s = ts_node_start_byte(child);
                uint32_t e = ts_node_end_byte(child);
                if (source.substr(s, e - s) == "static") {
                    sig.is_static = true;
                }
            }
        }

        // Get function declarator
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
        if (!ts_node_is_null(declarator)) {
            // Find function_declarator
            TSNode func_decl = declarator;
            while (!ts_node_is_null(func_decl) &&
                   std::string(ts_node_type(func_decl)) != "function_declarator") {
                func_decl = ts_node_child(func_decl, 0);
            }

            if (!ts_node_is_null(func_decl)) {
                // Get function name
                TSNode decl_child = ts_node_child(func_decl, 0);
                if (!ts_node_is_null(decl_child)) {
                    uint32_t s = ts_node_start_byte(decl_child);
                    uint32_t e = ts_node_end_byte(decl_child);
                    sig.name = std::string(source.substr(s, e - s));
                }

                // Get parameters
                TSNode params = ts_node_child_by_field_name(func_decl, "parameters", 10);
                if (!ts_node_is_null(params)) {
                    uint32_t param_count = ts_node_named_child_count(params);
                    for (uint32_t i = 0; i < param_count; i++) {
                        TSNode param = ts_node_named_child(params, i);
                        if (std::string(ts_node_type(param)) == "parameter_declaration") {
                            // Extract type and name
                            std::string param_type, param_name;

                            TSNode type_node = ts_node_child_by_field_name(param, "type", 4);
                            if (!ts_node_is_null(type_node)) {
                                uint32_t s = ts_node_start_byte(type_node);
                                uint32_t e = ts_node_end_byte(type_node);
                                param_type = std::string(source.substr(s, e - s));
                            }

                            TSNode decl_node = ts_node_child_by_field_name(param, "declarator", 10);
                            if (!ts_node_is_null(decl_node)) {
                                uint32_t s = ts_node_start_byte(decl_node);
                                uint32_t e = ts_node_end_byte(decl_node);
                                param_name = std::string(source.substr(s, e - s));
                            }

                            sig.parameters.push_back({param_type, param_name});
                        }
                    }
                }
            }
        }

        // Get return type
        TSNode type_node = ts_node_child_by_field_name(node, "type", 4);
        if (!ts_node_is_null(type_node)) {
            uint32_t s = ts_node_start_byte(type_node);
            uint32_t e = ts_node_end_byte(type_node);
            sig.return_type = std::string(source.substr(s, e - s));
        }

    } else if (language == Language::PYTHON) {
        // Check for async
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            uint32_t s = ts_node_start_byte(child);
            uint32_t e = ts_node_end_byte(child);
            if (source.substr(s, e - s) == "async") {
                sig.is_async = true;
                break;
            }
        }

        // Get function name
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node)) {
            uint32_t s = ts_node_start_byte(name_node);
            uint32_t e = ts_node_end_byte(name_node);
            sig.name = std::string(source.substr(s, e - s));
        }

        // Get parameters
        TSNode params = ts_node_child_by_field_name(node, "parameters", 10);
        if (!ts_node_is_null(params)) {
            uint32_t param_count = ts_node_named_child_count(params);
            for (uint32_t i = 0; i < param_count; i++) {
                TSNode param = ts_node_named_child(params, i);
                std::string param_type_str = ts_node_type(param);

                if (param_type_str == "identifier") {
                    uint32_t s = ts_node_start_byte(param);
                    uint32_t e = ts_node_end_byte(param);
                    std::string param_name = std::string(source.substr(s, e - s));
                    sig.parameters.push_back({"", param_name});
                } else if (param_type_str == "typed_parameter") {
                    std::string param_name, param_type;
                    uint32_t s = ts_node_start_byte(param);
                    uint32_t e = ts_node_end_byte(param);
                    std::string full_param = std::string(source.substr(s, e - s));

                    // Parse "name: type"
                    size_t colon = full_param.find(':');
                    if (colon != std::string::npos) {
                        param_name = full_param.substr(0, colon);
                        param_type = full_param.substr(colon + 1);
                        // Trim whitespace
                        param_name.erase(0, param_name.find_first_not_of(" \t"));
                        param_name.erase(param_name.find_last_not_of(" \t") + 1);
                        param_type.erase(0, param_type.find_first_not_of(" \t"));
                        param_type.erase(param_type.find_last_not_of(" \t") + 1);
                    }
                    sig.parameters.push_back({param_type, param_name});
                }
            }
        }

        sig.return_type = "";  // Python - could parse type hints
    }

    // Get docstring
    if (include_docstring) {
        sig.docstring = get_docstring(node, source, language);
    }

    return sig;
}

GetFileSummaryTool::MemberVariable GetFileSummaryTool::extract_member_variable(
    TSNode node,
    std::string_view source,
    [[maybe_unused]] Language language
) {
    MemberVariable var;

    TSPoint start_point = ts_node_start_point(node);
    var.line = start_point.row + 1;

    // Extract type and name (simplified)
    uint32_t s = ts_node_start_byte(node);
    uint32_t e = ts_node_end_byte(node);
    std::string full_decl = std::string(source.substr(s, e - s));

    var.name = full_decl;  // Simplified
    var.type = "";
    var.access = "unknown";

    return var;
}

std::vector<GetFileSummaryTool::CommentMarkerInfo> GetFileSummaryTool::extract_comment_markers(
    std::string_view source,
    [[maybe_unused]] Language language
) {
    std::vector<CommentMarkerInfo> markers;

    // Regex patterns for markers
    std::regex todo_pattern(R"((TODO|FIXME|HACK|NOTE|WARNING|BUG|OPTIMIZE)[:\s]+(.+))",
                           std::regex::icase);

    std::istringstream stream{std::string(source)};
    std::string line;
    int line_num = 1;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, todo_pattern)) {
            CommentMarkerInfo marker;
            marker.type = detect_marker_type(match[1].str());
            marker.text = match[2].str();
            marker.line = line_num;

            // Trim text
            marker.text.erase(0, marker.text.find_first_not_of(" \t"));
            marker.text.erase(marker.text.find_last_not_of(" \t\r\n") + 1);

            marker.context = line;
            markers.push_back(marker);
        }
        line_num++;
    }

    return markers;
}

std::vector<GetFileSummaryTool::ImportInfo> GetFileSummaryTool::extract_imports(
    TSNode root,
    std::string_view source,
    Language language
) {
    std::vector<ImportInfo> imports;

    if (language == Language::CPP) {
        // Find #include directives
        TSTreeCursor cursor = ts_tree_cursor_new(root);
        bool has_next = true;

        while (has_next) {
            TSNode current = ts_tree_cursor_current_node(&cursor);
            std::string node_type = ts_node_type(current);

            if (node_type == "preproc_include") {
                ImportInfo info;
                TSPoint start_point = ts_node_start_point(current);
                info.line = start_point.row + 1;

                // Get path
                TSNode path_node = ts_node_child_by_field_name(current, "path", 4);
                if (!ts_node_is_null(path_node)) {
                    uint32_t s = ts_node_start_byte(path_node);
                    uint32_t e = ts_node_end_byte(path_node);
                    std::string path = std::string(source.substr(s, e - s));

                    info.is_system = (path[0] == '<');
                    // Remove quotes/brackets
                    if (path.size() >= 2) {
                        path = path.substr(1, path.size() - 2);
                    }
                    info.path = path;
                    imports.push_back(info);
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

    } else if (language == Language::PYTHON) {
        // Find import statements
        TSTreeCursor cursor = ts_tree_cursor_new(root);
        bool has_next = true;

        while (has_next) {
            TSNode current = ts_tree_cursor_current_node(&cursor);
            std::string node_type = ts_node_type(current);

            if (node_type == "import_statement" || node_type == "import_from_statement") {
                ImportInfo info;
                TSPoint start_point = ts_node_start_point(current);
                info.line = start_point.row + 1;
                info.is_system = false;

                uint32_t s = ts_node_start_byte(current);
                uint32_t e = ts_node_end_byte(current);
                info.path = std::string(source.substr(s, e - s));
                info.module = info.path;  // Simplified

                imports.push_back(info);
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
    }

    return imports;
}

std::string GetFileSummaryTool::get_docstring(
    TSNode node,
    std::string_view source,
    Language language
) {
    if (language == Language::PYTHON) {
        // Look for string literal as first statement in body
        TSNode body = ts_node_child_by_field_name(node, "body", 4);
        if (!ts_node_is_null(body)) {
            uint32_t child_count = ts_node_named_child_count(body);
            if (child_count > 0) {
                TSNode first_stmt = ts_node_named_child(body, 0);
                if (std::string(ts_node_type(first_stmt)) == "expression_statement") {
                    TSNode expr = ts_node_named_child(first_stmt, 0);
                    if (std::string(ts_node_type(expr)) == "string") {
                        uint32_t s = ts_node_start_byte(expr);
                        uint32_t e = ts_node_end_byte(expr);
                        std::string docstring = std::string(source.substr(s, e - s));

                        // Remove triple quotes
                        if (docstring.size() >= 6 &&
                            (docstring.substr(0, 3) == "\"\"\"" || docstring.substr(0, 3) == "'''")) {
                            docstring = docstring.substr(3, docstring.size() - 6);
                        }

                        return docstring;
                    }
                }
            }
        }
    } else if (language == Language::CPP) {
        // Look for comment before function
        TSNode prev = ts_node_prev_sibling(node);
        if (!ts_node_is_null(prev) && std::string(ts_node_type(prev)) == "comment") {
            uint32_t s = ts_node_start_byte(prev);
            uint32_t e = ts_node_end_byte(prev);
            std::string comment = std::string(source.substr(s, e - s));

            // Remove // or /* */
            if (comment.size() >= 2 && comment.substr(0, 2) == "//") {
                comment = comment.substr(2);
            } else if (comment.size() >= 4 && comment.substr(0, 2) == "/*") {
                comment = comment.substr(2, comment.size() - 4);
            }

            // Trim
            comment.erase(0, comment.find_first_not_of(" \t"));
            comment.erase(comment.find_last_not_of(" \t\r\n") + 1);

            return comment;
        }
    }

    return "";
}

std::map<std::string, int> GetFileSummaryTool::calculate_metrics(std::string_view source) {
    std::map<std::string, int> metrics;

    int total_lines = 0;
    int code_lines = 0;
    int comment_lines = 0;
    int blank_lines = 0;

    std::istringstream stream{std::string(source)};
    std::string line;
    bool in_block_comment = false;

    while (std::getline(stream, line)) {
        total_lines++;

        // Trim whitespace
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        if (trimmed.empty()) {
            blank_lines++;
        } else if (in_block_comment) {
            comment_lines++;
            if (trimmed.find("*/") != std::string::npos) {
                in_block_comment = false;
            }
        } else if (trimmed.substr(0, 2) == "/*") {
            comment_lines++;
            in_block_comment = true;
            if (trimmed.find("*/") != std::string::npos) {
                in_block_comment = false;
            }
        } else if (trimmed.substr(0, 2) == "//" || trimmed[0] == '#') {
            comment_lines++;
        } else {
            code_lines++;
        }
    }

    metrics["loc"] = total_lines;
    metrics["sloc"] = code_lines;
    metrics["comment_lines"] = comment_lines;
    metrics["blank_lines"] = blank_lines;

    return metrics;
}

std::string GetFileSummaryTool::marker_to_string(CommentMarker marker) {
    switch (marker) {
        case CommentMarker::TODO: return "TODO";
        case CommentMarker::FIXME: return "FIXME";
        case CommentMarker::HACK: return "HACK";
        case CommentMarker::NOTE: return "NOTE";
        case CommentMarker::WARNING: return "WARNING";
        case CommentMarker::BUG: return "BUG";
        case CommentMarker::OPTIMIZE: return "OPTIMIZE";
    }
    return "UNKNOWN";
}

GetFileSummaryTool::CommentMarker GetFileSummaryTool::detect_marker_type(std::string_view text) {
    std::string upper;
    for (char c : text) {
        upper += std::toupper(c);
    }

    if (upper == "TODO") return CommentMarker::TODO;
    if (upper == "FIXME") return CommentMarker::FIXME;
    if (upper == "HACK") return CommentMarker::HACK;
    if (upper == "NOTE") return CommentMarker::NOTE;
    if (upper == "WARNING") return CommentMarker::WARNING;
    if (upper == "BUG") return CommentMarker::BUG;
    if (upper == "OPTIMIZE") return CommentMarker::OPTIMIZE;

    return CommentMarker::TODO;
}

} // namespace ts_mcp
