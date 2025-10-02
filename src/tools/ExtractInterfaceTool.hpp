#pragma once

#include "core/ASTAnalyzer.hpp"
#include "core/QueryEngine.hpp"
#include "core/Language.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ts_mcp {

/**
 * @brief MCP tool for extracting interface (signatures only) from code files
 *
 * Extracts function signatures, class declarations, and type definitions
 * without implementation bodies. Useful for reducing context size for AI agents.
 *
 * Supports multiple output formats:
 * - JSON: Structured data
 * - header: Valid .hpp/.h file format
 * - markdown: Documentation format
 */
class ExtractInterfaceTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit ExtractInterfaceTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with parameters
     * @return JSON result with extracted interface or error
     */
    json execute(const json& args);

private:
    /**
     * @brief Extract interface from a single file
     * @param filepath Path to file
     * @param include_private Include private members
     * @param include_comments Include comments/docstrings
     * @param language Programming language
     * @return JSON with interface data
     */
    json extract_from_file(
        const std::string& filepath,
        bool include_private,
        bool include_comments,
        Language language
    );

    /**
     * @brief Extract function signature without body
     * @param node Function definition node
     * @param source Source code
     * @param include_comments Include function comments
     * @param language Programming language
     * @return JSON with function signature
     */
    json extract_function_signature(
        TSNode node,
        std::string_view source,
        bool include_comments,
        Language language
    );

    /**
     * @brief Extract class interface (methods without bodies)
     * @param node Class definition node
     * @param source Source code
     * @param include_private Include private members
     * @param include_comments Include comments
     * @param language Programming language
     * @return JSON with class interface
     */
    json extract_class_interface(
        TSNode node,
        std::string_view source,
        bool include_private,
        bool include_comments,
        Language language
    );

    /**
     * @brief Get node text excluding function body
     * @param node Function/method node
     * @param source Source code
     * @param language Programming language
     * @return Signature string
     */
    std::string get_signature_text(
        TSNode node,
        std::string_view source,
        Language language
    );

    /**
     * @brief Get comment before a node (if exists)
     * @param node Target node
     * @param source Source code
     * @param language Programming language
     * @return Comment text or empty string
     */
    std::string get_preceding_comment(
        TSNode node,
        std::string_view source,
        Language language
    );

    /**
     * @brief Format output as JSON structure
     * @param interface_data Extracted interface data
     * @param filepath Source file path
     * @param language Programming language
     * @return Formatted JSON
     */
    json format_as_json(
        const json& interface_data,
        const std::string& filepath,
        Language language
    );

    /**
     * @brief Format output as header file (.hpp/.h format)
     * @param interface_data Extracted interface data
     * @param filepath Source file path
     * @param language Programming language
     * @return Header file as string
     */
    std::string format_as_header(
        const json& interface_data,
        const std::string& filepath,
        Language language
    );

    /**
     * @brief Format output as markdown documentation
     * @param interface_data Extracted interface data
     * @param filepath Source file path
     * @param language Programming language
     * @return Markdown string
     */
    std::string format_as_markdown(
        const json& interface_data,
        const std::string& filepath,
        Language language
    );

    /**
     * @brief Check if node is in public/protected/private section
     * @param node Class member node
     * @return Access specifier ("public", "protected", "private", or "unknown")
     */
    std::string get_access_specifier(TSNode node);

    /**
     * @brief Extract decorators from Python function/class
     * @param node Function or class node
     * @param source Source code
     * @return Array of decorator strings
     */
    std::vector<std::string> extract_decorators(
        TSNode node,
        std::string_view source
    );

    std::shared_ptr<ASTAnalyzer> analyzer_;
    QueryEngine query_engine_;
};

} // namespace ts_mcp
