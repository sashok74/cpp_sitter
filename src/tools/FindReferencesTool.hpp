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
 * @brief MCP tool for finding all references to a symbol in codebase
 *
 * Performs intelligent symbol search:
 * 1. Fast text search (grep-like) for initial candidates
 * 2. Tree-sitter AST validation (filter false positives)
 * 3. Reference type classification (call, declaration, definition)
 * 4. Context extraction (parent function/class, surrounding code)
 *
 * Useful for:
 * - Understanding symbol usage patterns
 * - Impact analysis before refactoring
 * - Finding API usage examples
 */
class FindReferencesTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit FindReferencesTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with parameters
     * @return JSON result with references or error
     */
    json execute(const json& args);

private:
    /**
     * @brief Reference types for classification
     */
    enum class ReferenceType {
        DECLARATION,    // Variable/function declaration
        DEFINITION,     // Function/class definition
        CALL,          // Function call or constructor
        MEMBER_ACCESS, // Class member access (obj.member)
        TYPE_USAGE,    // Type in declaration (MyClass var;)
        UNKNOWN
    };

    /**
     * @brief Single reference information
     */
    struct Reference {
        std::string filepath;
        int line;
        int column;
        ReferenceType type;
        std::string context;        // Line of code containing reference
        std::string parent_scope;   // Parent function/class name
        std::string node_type;      // Tree-sitter node type
    };

    /**
     * @brief Find all references to symbol in a single file
     * @param filepath Path to file
     * @param symbol Symbol name to search
     * @param language Programming language
     * @return Vector of references
     */
    std::vector<Reference> find_in_file(
        const std::string& filepath,
        const std::string& symbol,
        Language language
    );

    /**
     * @brief Check if node contains symbol reference
     * @param node AST node
     * @param symbol Symbol name
     * @param source Source code
     * @return true if node matches symbol
     */
    bool node_matches_symbol(
        TSNode node,
        const std::string& symbol,
        std::string_view source
    );

    /**
     * @brief Classify reference type based on AST context
     * @param node Identifier node
     * @param source Source code
     * @param language Programming language
     * @return Reference type classification
     */
    ReferenceType classify_reference(
        TSNode node,
        std::string_view source,
        Language language
    );

    /**
     * @brief Extract context around reference
     * @param node Reference node
     * @param source Source code
     * @param context_lines Lines before/after (default 0 = same line)
     * @return Context string
     */
    std::string extract_context(
        TSNode node,
        std::string_view source,
        int context_lines = 0
    );

    /**
     * @brief Find parent scope (function/class) of node
     * @param node Child node
     * @param source Source code
     * @return Parent scope name or empty string
     */
    std::string find_parent_scope(
        TSNode node,
        std::string_view source
    );

    /**
     * @brief Convert ReferenceType to string
     */
    static std::string reference_type_to_string(ReferenceType type);

    /**
     * @brief Convert Reference struct to JSON
     */
    json reference_to_json(const Reference& ref);

    std::shared_ptr<ASTAnalyzer> analyzer_;
    QueryEngine query_engine_;
};

} // namespace ts_mcp
