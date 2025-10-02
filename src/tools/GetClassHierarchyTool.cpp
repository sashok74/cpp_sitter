#include "tools/GetClassHierarchyTool.hpp"
#include "core/TreeSitterParser.hpp"
#include "core/Language.hpp"
#include "core/PathResolver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <queue>
#include <fstream>
#include <sstream>

namespace ts_mcp {

GetClassHierarchyTool::GetClassHierarchyTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)), query_engine_() {
    spdlog::debug("GetClassHierarchyTool initialized");
}

ToolInfo GetClassHierarchyTool::get_info() {
    return ToolInfo{
        "get_class_hierarchy",
        "Analyze C++ class inheritance hierarchies with virtual methods",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"type", json::array({"string", "array"})},
                    {"description", "File path, array of paths, or directory"}
                }},
                {"class_name", {
                    {"type", "string"},
                    {"description", "Optional: Focus on specific class hierarchy"}
                }},
                {"show_methods", {
                    {"type", "boolean"},
                    {"description", "Include method information (default: true)"}
                }},
                {"show_virtual_only", {
                    {"type", "boolean"},
                    {"description", "Only show virtual methods (default: false)"}
                }},
                {"max_depth", {
                    {"type", "integer"},
                    {"description", "Maximum hierarchy depth, -1 for unlimited (default: -1)"}
                }},
                {"recursive", {
                    {"type", "boolean"},
                    {"description", "Scan directories recursively (default: true)"}
                }},
                {"file_patterns", {
                    {"type", "array"},
                    {"items", {{"type", "string"}}},
                    {"description", "File patterns for filtering (default: [\"*.cpp\", \"*.hpp\", \"*.h\"])"}
                }}
            }},
            {"required", json::array({"filepath"})}
        }
    };
}

json GetClassHierarchyTool::execute(const json& args) {
    try {
        // Extract parameters
        if (!args.contains("filepath")) {
            json error = {{"error", "Missing required parameter: filepath"}};
            return error;
        }

        std::string class_name = args.value("class_name", "");
        bool show_methods = args.value("show_methods", true);
        bool show_virtual_only = args.value("show_virtual_only", false);
        int max_depth = args.value("max_depth", -1);
        bool recursive = args.value("recursive", true);

        json file_patterns_json = args.value("file_patterns",
            json::array({"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx"}));
        std::vector<std::string> file_patterns;
        for (const auto& pattern : file_patterns_json) {
            file_patterns.push_back(pattern.get<std::string>());
        }

        // Resolve paths
        std::vector<std::string> input_paths;

        if (args["filepath"].is_string()) {
            input_paths.push_back(args["filepath"].get<std::string>());
        } else if (args["filepath"].is_array()) {
            for (const auto& path_json : args["filepath"]) {
                input_paths.push_back(path_json.get<std::string>());
            }
        }

        std::vector<std::filesystem::path> resolved =
            PathResolver::resolve_paths(input_paths, recursive, file_patterns);

        if (resolved.empty()) {
            json error = {
                {"error", "Failed to resolve any files from filepath"},
                {"success", false}
            };
            return error;
        }

        // Analyze all files and collect classes
        std::map<std::string, ClassInfo> all_classes;
        int files_processed = 0;
        int files_failed = 0;

        for (const auto& filepath : resolved) {
            Language lang = LanguageUtils::detect_from_extension(filepath);

            // Only C++ supported for class hierarchy
            if (lang != Language::CPP) {
                continue;
            }

            try {
                auto file_classes = analyze_file(
                    filepath.string(),
                    "",  // Don't filter by class yet
                    show_methods,
                    show_virtual_only,
                    lang
                );

                // Merge into all_classes
                for (auto& [name, info] : file_classes) {
                    if (all_classes.find(name) == all_classes.end()) {
                        all_classes[name] = std::move(info);
                    } else {
                        // Merge information (same class in different files)
                        auto& existing = all_classes[name];
                        for (const auto& base : info.base_classes) {
                            if (std::find(existing.base_classes.begin(),
                                        existing.base_classes.end(),
                                        base) == existing.base_classes.end()) {
                                existing.base_classes.push_back(base);
                            }
                        }
                        existing.virtual_methods.insert(
                            existing.virtual_methods.end(),
                            info.virtual_methods.begin(),
                            info.virtual_methods.end()
                        );
                    }
                }

                files_processed++;
            } catch (const std::exception& e) {
                spdlog::warn("Failed to analyze {}: {}", filepath.string(), e.what());
                files_failed++;
            }
        }

        // Build hierarchy tree
        auto hierarchy = build_hierarchy_tree(all_classes);

        // Filter by class name if specified
        if (!class_name.empty()) {
            if (all_classes.find(class_name) == all_classes.end()) {
                json error = {
                    {"error", "Class not found: " + class_name},
                    {"success", false}
                };
                return error;
            }
            all_classes = filter_hierarchy(all_classes, hierarchy, class_name, max_depth);
            hierarchy = build_hierarchy_tree(all_classes);
        }

        // Build result
        json result;
        result["total_files"] = resolved.size();
        result["files_processed"] = files_processed;
        result["files_failed"] = files_failed;
        result["total_classes"] = all_classes.size();

        // Classes array
        json classes_array = json::array();
        for (const auto& [name, info] : all_classes) {
            classes_array.push_back(class_info_to_json(info, show_methods));
        }
        result["classes"] = classes_array;

        // Hierarchy tree
        result["hierarchy"] = hierarchy_to_json(hierarchy, all_classes);
        result["success"] = true;

        return result;

    } catch (const std::exception& e) {
        spdlog::error("GetClassHierarchyTool error: {}", e.what());
        json error = {
            {"error", std::string("Internal error: ") + e.what()},
            {"success", false}
        };
        return error;
    }
}

std::map<std::string, GetClassHierarchyTool::ClassInfo>
GetClassHierarchyTool::analyze_file(
    const std::string& filepath,
    [[maybe_unused]] const std::string& class_name,
    bool show_methods,
    [[maybe_unused]] bool show_virtual_only,
    Language language
) {
    std::map<std::string, ClassInfo> classes;

    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::warn("Cannot open file {}", filepath);
        return classes;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse file
    TreeSitterParser parser(language);
    auto parse_result = parser.parse_string(source);

    if (!parse_result) {
        spdlog::warn("Failed to parse {}", filepath);
        return classes;
    }

    [[maybe_unused]] TSNode root = ts_tree_root_node(parse_result->get());

    // Query for classes with base classes
    std::string class_query = R"(
        (class_specifier
            name: (type_identifier) @class_name
            (base_class_clause)? @base_clause
        )
    )";

    auto query = query_engine_.compile_query(class_query, language);
    if (!query) {
        spdlog::warn("Failed to compile class query");
        return classes;
    }

    // Execute query
    auto matches = query_engine_.execute(*parse_result, *query, source);

    for (const auto& match : matches) {
        if (match.capture_name != "class_name") {
            continue;
        }

        ClassInfo info;
        info.name = match.text;
        info.line = match.line;
        info.filepath = filepath;

        // Find the full class node
        TSNode class_node = match.node;
        TSNode parent = ts_node_parent(class_node);
        while (!ts_node_is_null(parent)) {
            std::string type = ts_node_type(parent);
            if (type == "class_specifier") {
                class_node = parent;
                break;
            }
            parent = ts_node_parent(parent);
        }

        // Extract base classes
        info.base_classes = extract_base_classes(class_node, source);

        // Extract virtual methods if requested
        if (show_methods) {
            info.virtual_methods = extract_virtual_methods(class_node, source);
        }

        // Check if abstract (has pure virtual methods)
        info.is_abstract = std::any_of(
            info.virtual_methods.begin(),
            info.virtual_methods.end(),
            [](const VirtualMethod& m) { return m.is_pure_virtual; }
        );

        classes[info.name] = info;
    }

    return classes;
}

std::vector<std::string> GetClassHierarchyTool::extract_base_classes(
    TSNode node,
    std::string_view source
) {
    std::vector<std::string> bases;

    // Find base_class_clause child
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(node, i);
        std::string type = ts_node_type(child);

        if (type == "base_class_clause") {
            // Iterate through base class specifiers
            uint32_t base_count = ts_node_child_count(child);
            for (uint32_t j = 0; j < base_count; j++) {
                TSNode base_child = ts_node_child(child, j);
                std::string base_type = ts_node_type(base_child);

                if (base_type == "base_class_specifier") {
                    // Extract the type (could be nested)
                    uint32_t spec_count = ts_node_child_count(base_child);
                    for (uint32_t k = 0; k < spec_count; k++) {
                        TSNode spec_child = ts_node_child(base_child, k);
                        std::string spec_child_type = ts_node_type(spec_child);

                        if (spec_child_type == "type_identifier" ||
                            spec_child_type == "qualified_identifier" ||
                            spec_child_type == "template_type") {

                            uint32_t start = ts_node_start_byte(spec_child);
                            uint32_t end = ts_node_end_byte(spec_child);
                            std::string base_name(source.substr(start, end - start));
                            bases.push_back(base_name);
                            break;
                        }
                    }
                }
            }
            break;
        }
    }

    return bases;
}

std::vector<GetClassHierarchyTool::VirtualMethod>
GetClassHierarchyTool::extract_virtual_methods(
    TSNode class_node,
    std::string_view source
) {
    std::vector<VirtualMethod> methods;

    // Find field_declaration_list (class body)
    uint32_t count = ts_node_child_count(class_node);
    TSNode body_node = {};  // Zero-initialized TSNode
    bool found_body = false;

    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(class_node, i);
        std::string type = ts_node_type(child);

        if (type == "field_declaration_list") {
            body_node = child;
            found_body = true;
            break;
        }
    }

    if (!found_body || ts_node_is_null(body_node)) {
        return methods;
    }

    // Track current access level
    std::string current_access = "private";  // Default for class

    // Iterate through class members
    uint32_t member_count = ts_node_child_count(body_node);
    for (uint32_t i = 0; i < member_count; i++) {
        TSNode member = ts_node_child(body_node, i);
        std::string member_type = ts_node_type(member);

        // Update access level
        if (member_type == "access_specifier") {
            uint32_t start = ts_node_start_byte(member);
            uint32_t end = ts_node_end_byte(member);
            std::string access_text(source.substr(start, end - start));
            if (access_text.find("public") != std::string::npos) {
                current_access = "public";
            } else if (access_text.find("private") != std::string::npos) {
                current_access = "private";
            } else if (access_text.find("protected") != std::string::npos) {
                current_access = "protected";
            }
            continue;
        }

        // Look for function definitions/declarations
        if (member_type == "function_definition" ||
            member_type == "field_declaration") {

            bool has_virtual = false;
            bool has_pure = false;
            bool has_override = false;
            bool has_final = false;

            // Check for virtual specifiers
            uint32_t func_count = ts_node_child_count(member);
            for (uint32_t j = 0; j < func_count; j++) {
                TSNode func_child = ts_node_child(member, j);
                std::string func_child_type = ts_node_type(func_child);

                if (func_child_type == "virtual_function_specifier" ||
                    func_child_type == "virtual") {
                    has_virtual = true;
                }
                if (func_child_type == "pure_virtual_clause") {
                    has_pure = true;
                    has_virtual = true;
                }
            }

            // Check for override/final in text (tree-sitter may not have specific nodes)
            uint32_t start = ts_node_start_byte(member);
            uint32_t end = ts_node_end_byte(member);
            std::string member_text(source.substr(start, end - start));

            if (member_text.find("override") != std::string::npos) {
                has_override = true;
                has_virtual = true;  // override implies virtual
            }
            if (member_text.find("final") != std::string::npos) {
                has_final = true;
                has_virtual = true;  // final implies virtual
            }

            if (!has_virtual) {
                continue;
            }

            // Extract method name and signature
            VirtualMethod method;
            method.is_pure_virtual = has_pure;
            method.is_override = has_override;
            method.is_final = has_final;
            method.access = current_access;
            method.line = ts_node_start_point(member).row + 1;

            // Find function declarator
            for (uint32_t j = 0; j < func_count; j++) {
                TSNode func_child = ts_node_child(member, j);
                std::string func_child_type = ts_node_type(func_child);

                if (func_child_type == "function_declarator") {
                    // Extract declarator text for signature
                    uint32_t decl_start = ts_node_start_byte(func_child);
                    uint32_t decl_end = ts_node_end_byte(func_child);
                    std::string declarator(source.substr(decl_start, decl_end - decl_start));

                    // Extract just function name
                    TSNode name_node = ts_node_child_by_field_name(func_child, "declarator", 10);
                    if (!ts_node_is_null(name_node)) {
                        uint32_t name_start = ts_node_start_byte(name_node);
                        uint32_t name_end = ts_node_end_byte(name_node);
                        method.name = std::string(source.substr(name_start, name_end - name_start));
                    } else {
                        // Try to extract from declarator
                        size_t paren_pos = declarator.find('(');
                        if (paren_pos != std::string::npos) {
                            method.name = declarator.substr(0, paren_pos);
                            // Trim whitespace
                            method.name.erase(0, method.name.find_first_not_of(" \t"));
                            method.name.erase(method.name.find_last_not_of(" \t") + 1);
                        }
                    }

                    // Build full signature
                    method.signature = declarator;
                    if (has_override) method.signature += " override";
                    if (has_final) method.signature += " final";
                    if (has_pure) method.signature += " = 0";

                    break;
                }
            }

            if (!method.name.empty()) {
                methods.push_back(method);
            }
        }
    }

    return methods;
}

bool GetClassHierarchyTool::is_pure_virtual(TSNode method_node) {
    uint32_t count = ts_node_child_count(method_node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(method_node, i);
        if (std::string(ts_node_type(child)) == "pure_virtual_clause") {
            return true;
        }
    }
    return false;
}

bool GetClassHierarchyTool::is_override([[maybe_unused]] TSNode method_node) {
    // Tree-sitter may not have explicit override node, check in text
    return false;  // Handled in extract_virtual_methods
}

bool GetClassHierarchyTool::is_final([[maybe_unused]] TSNode method_node) {
    // Tree-sitter may not have explicit final node, check in text
    return false;  // Handled in extract_virtual_methods
}

std::string GetClassHierarchyTool::get_access_level(
    [[maybe_unused]] TSNode method_node,
    [[maybe_unused]] TSNode class_node,
    [[maybe_unused]] std::string_view source
) {
    return "public";  // Handled in extract_virtual_methods
}

std::map<std::string, std::set<std::string>>
GetClassHierarchyTool::build_hierarchy_tree(
    const std::map<std::string, ClassInfo>& classes
) {
    std::map<std::string, std::set<std::string>> hierarchy;

    // Initialize all classes in hierarchy
    for (const auto& [name, info] : classes) {
        if (hierarchy.find(name) == hierarchy.end()) {
            hierarchy[name] = std::set<std::string>();
        }

        // Add this class as child of its bases
        for (const auto& base : info.base_classes) {
            hierarchy[base].insert(name);
            // Ensure base exists in hierarchy even if not in classes
            if (hierarchy.find(base) == hierarchy.end()) {
                hierarchy[base] = std::set<std::string>();
            }
        }
    }

    return hierarchy;
}

std::map<std::string, GetClassHierarchyTool::ClassInfo>
GetClassHierarchyTool::filter_hierarchy(
    const std::map<std::string, ClassInfo>& classes,
    const std::map<std::string, std::set<std::string>>& hierarchy,
    const std::string& root_class,
    int max_depth
) {
    std::map<std::string, ClassInfo> filtered;

    // BFS to traverse hierarchy up to max_depth
    std::queue<std::pair<std::string, int>> queue;
    std::set<std::string> visited;

    queue.push({root_class, 0});
    visited.insert(root_class);

    while (!queue.empty()) {
        auto [current, depth] = queue.front();
        queue.pop();

        // Add current class if it exists
        if (classes.find(current) != classes.end()) {
            filtered[current] = classes.at(current);
        }

        // Check depth limit
        if (max_depth >= 0 && depth >= max_depth) {
            continue;
        }

        // Add children
        if (hierarchy.find(current) != hierarchy.end()) {
            for (const auto& child : hierarchy.at(current)) {
                if (visited.find(child) == visited.end()) {
                    queue.push({child, depth + 1});
                    visited.insert(child);
                }
            }
        }

        // Add parents
        if (classes.find(current) != classes.end()) {
            for (const auto& base : classes.at(current).base_classes) {
                if (visited.find(base) == visited.end()) {
                    queue.push({base, depth + 1});
                    visited.insert(base);
                }
            }
        }
    }

    return filtered;
}

json GetClassHierarchyTool::class_info_to_json(
    const ClassInfo& info,
    bool show_methods
) {
    json result;
    result["name"] = info.name;
    result["line"] = info.line;
    result["file"] = info.filepath;
    result["base_classes"] = info.base_classes;
    result["is_abstract"] = info.is_abstract;

    if (show_methods) {
        json methods = json::array();
        for (const auto& method : info.virtual_methods) {
            json m;
            m["name"] = method.name;
            m["signature"] = method.signature;
            m["line"] = method.line;
            m["is_pure_virtual"] = method.is_pure_virtual;
            m["is_override"] = method.is_override;
            m["is_final"] = method.is_final;
            m["access"] = method.access;
            methods.push_back(m);
        }
        result["virtual_methods"] = methods;
    }

    return result;
}

json GetClassHierarchyTool::hierarchy_to_json(
    const std::map<std::string, std::set<std::string>>& hierarchy,
    const std::map<std::string, ClassInfo>& classes
) {
    json result;

    for (const auto& [class_name, children] : hierarchy) {
        json node;
        node["children"] = json::array();
        for (const auto& child : children) {
            node["children"].push_back(child);
        }

        // Find parents
        json parents = json::array();
        if (classes.find(class_name) != classes.end()) {
            for (const auto& base : classes.at(class_name).base_classes) {
                parents.push_back(base);
            }
        }
        node["parents"] = parents;

        // Is abstract?
        if (classes.find(class_name) != classes.end()) {
            node["is_abstract"] = classes.at(class_name).is_abstract;
        } else {
            node["is_abstract"] = false;
        }

        result[class_name] = node;
    }

    return result;
}

} // namespace ts_mcp
