#pragma once

#include "core/ASTAnalyzer.hpp"
#include "core/QueryEngine.hpp"
#include "core/Language.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace ts_mcp {

/**
 * @brief MCP tool for analyzing C++ class hierarchies
 *
 * Provides comprehensive class inheritance analysis:
 * - Parent-child relationships (single and multiple inheritance)
 * - Virtual method detection (virtual/override/final/pure virtual)
 * - Abstract class identification
 * - Full hierarchy tree construction
 * - Method override tracking
 *
 * Useful for:
 * - Understanding OOP structure
 * - Finding polymorphic interfaces
 * - Identifying inheritance chains
 * - Refactoring planning
 */
class GetClassHierarchyTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit GetClassHierarchyTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with parameters
     * @return JSON result with hierarchy or error
     */
    json execute(const json& args);

private:
    /**
     * @brief Information about a virtual method
     */
    struct VirtualMethod {
        std::string name;
        std::string signature;
        int line;
        bool is_pure_virtual;
        bool is_override;
        bool is_final;
        std::string access;  // public/private/protected
    };

    /**
     * @brief Information about a class
     */
    struct ClassInfo {
        std::string name;
        int line;
        std::string filepath;
        std::vector<std::string> base_classes;
        std::vector<VirtualMethod> virtual_methods;
        bool is_abstract;
        std::set<std::string> children;  // Derived classes
    };

    /**
     * @brief Analyze class hierarchy in a single file
     * @param filepath Path to file
     * @param class_name Optional: specific class to focus on
     * @param show_methods Include method information
     * @param show_virtual_only Only show virtual methods
     * @param language Programming language
     * @return Map of class name -> ClassInfo
     */
    std::map<std::string, ClassInfo> analyze_file(
        const std::string& filepath,
        const std::string& class_name,
        bool show_methods,
        bool show_virtual_only,
        Language language
    );

    /**
     * @brief Extract base classes from class definition
     * @param node Class specifier node
     * @param source Source code
     * @return Vector of base class names
     */
    std::vector<std::string> extract_base_classes(
        TSNode node,
        std::string_view source
    );

    /**
     * @brief Extract virtual methods from class body
     * @param class_node Class specifier node
     * @param source Source code
     * @return Vector of virtual methods
     */
    std::vector<VirtualMethod> extract_virtual_methods(
        TSNode class_node,
        std::string_view source
    );

    /**
     * @brief Check if method is pure virtual
     * @param method_node Function definition node
     * @return True if pure virtual
     */
    bool is_pure_virtual(TSNode method_node);

    /**
     * @brief Check if method has override specifier
     * @param method_node Function definition node
     * @return True if override
     */
    bool is_override(TSNode method_node);

    /**
     * @brief Check if method has final specifier
     * @param method_node Function definition node
     * @return True if final
     */
    bool is_final(TSNode method_node);

    /**
     * @brief Get method access level
     * @param method_node Function definition node
     * @param class_node Parent class node
     * @param source Source code
     * @return Access level (public/private/protected)
     */
    std::string get_access_level(
        TSNode method_node,
        TSNode class_node,
        std::string_view source
    );

    /**
     * @brief Build parent-child relationships from class map
     * @param classes Map of all classes
     * @return Hierarchy tree (class name -> children set)
     */
    std::map<std::string, std::set<std::string>> build_hierarchy_tree(
        const std::map<std::string, ClassInfo>& classes
    );

    /**
     * @brief Filter hierarchy by specific class and max depth
     * @param classes All classes
     * @param hierarchy Full hierarchy tree
     * @param root_class Root class to start from
     * @param max_depth Maximum depth (-1 = unlimited)
     * @return Filtered class map
     */
    std::map<std::string, ClassInfo> filter_hierarchy(
        const std::map<std::string, ClassInfo>& classes,
        const std::map<std::string, std::set<std::string>>& hierarchy,
        const std::string& root_class,
        int max_depth
    );

    /**
     * @brief Convert ClassInfo to JSON
     * @param info Class information
     * @param show_methods Include methods in output
     * @return JSON representation
     */
    json class_info_to_json(
        const ClassInfo& info,
        bool show_methods
    );

    /**
     * @brief Convert hierarchy tree to JSON
     * @param hierarchy Tree structure
     * @param classes Class information map
     * @return JSON representation
     */
    json hierarchy_to_json(
        const std::map<std::string, std::set<std::string>>& hierarchy,
        const std::map<std::string, ClassInfo>& classes
    );

    std::shared_ptr<ASTAnalyzer> analyzer_;
    QueryEngine query_engine_;
};

} // namespace ts_mcp
