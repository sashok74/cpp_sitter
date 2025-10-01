#pragma once

#include "core/TreeSitterParser.hpp"
#include "core/Language.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

// Need full tree-sitter API for TSNode in QueryMatch
extern "C" {
    #include <tree_sitter/api.h>
}

// Forward declarations for types not used inline
extern "C" {
    struct TSQuery;
    struct TSQueryCursor;
    struct TSQueryMatch;
}

namespace ts_mcp {

/**
 * @brief Represents a single query match result
 */
struct QueryMatch {
    std::string capture_name;  // Name of the capture (e.g., "@class_name")
    TSNode node;               // The matched node
    uint32_t line;            // Line number (0-based)
    uint32_t column;          // Column number (0-based)
    std::string text;         // Text content of the matched node
};

/**
 * @brief RAII wrapper for TSQuery from tree-sitter
 *
 * Manages the lifetime of a compiled tree-sitter query.
 */
class Query {
public:
    explicit Query(TSQuery* query);
    ~Query();

    // Delete copy operations
    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    // Move operations
    Query(Query&& other) noexcept;
    Query& operator=(Query&& other) noexcept;

    /**
     * @brief Get the underlying TSQuery pointer
     */
    TSQuery* get() const { return query_; }

    /**
     * @brief Get the number of patterns in this query
     */
    uint32_t pattern_count() const;

    /**
     * @brief Get the number of captures in this query
     */
    uint32_t capture_count() const;

    /**
     * @brief Get the name of a capture by index
     */
    std::string capture_name(uint32_t index) const;

private:
    TSQuery* query_;
};

/**
 * @brief Types of predefined queries
 */
enum class QueryType {
    CLASSES,           // Find all class declarations
    FUNCTIONS,         // Find all function definitions
    VIRTUAL_FUNCTIONS, // Find virtual/override functions (C++ only)
    INCLUDES,          // Find include/import directives
    NAMESPACES,        // Find namespace declarations (C++ only)
    STRUCTS,           // Find struct declarations (C++ only)
    TEMPLATES,         // Find template declarations (C++ only)
    DECORATORS,        // Find decorators (Python only)
    ASYNC_FUNCTIONS    // Find async functions (Python only)
};

/**
 * @brief Engine for executing tree-sitter queries on syntax trees
 *
 * Provides methods to compile and execute S-expression queries on parsed code.
 * Supports multiple programming languages with language-specific query patterns.
 */
class QueryEngine {
public:
    /**
     * @brief Compile a tree-sitter query from S-expression syntax
     * @param query_string S-expression query string
     * @param lang Programming language (needed to get correct TSLanguage)
     * @return Unique pointer to compiled query, or nullptr on error
     */
    std::unique_ptr<Query> compile_query(std::string_view query_string, Language lang);

    /**
     * @brief Execute a query on a syntax tree
     * @param tree The parsed syntax tree
     * @param query The compiled query
     * @param source The source code (for extracting text)
     * @return Vector of query matches
     */
    std::vector<QueryMatch> execute(
        const Tree& tree,
        const Query& query,
        std::string_view source
    );

    /**
     * @brief Get predefined query string for a specific query type and language
     * @param type Query type
     * @param lang Programming language
     * @return Query string if available, empty optional if not supported for this language
     */
    static std::optional<std::string_view> get_predefined_query(QueryType type, Language lang);

    /**
     * @brief Predefined queries for common C++ code patterns
     * @deprecated Use get_predefined_query() with Language parameter instead
     */
    struct PredefinedQueries {
        // Find all class declarations
        static constexpr std::string_view ALL_CLASSES =
            "(class_specifier name: (type_identifier) @class_name)";

        // Find all function definitions
        static constexpr std::string_view ALL_FUNCTIONS =
            "(function_definition) @func_def";

        // Find virtual and override functions
        static constexpr std::string_view VIRTUAL_FUNCTIONS =
            "["
            "  (function_definition"
            "    (function_declarator)"
            "    [(virtual_specifier) (type_qualifier (virtual_specifier))]"
            "  ) @virtual_func"
            "]";

        // Find #include directives
        static constexpr std::string_view INCLUDES =
            "(preproc_include) @include";

        // Find all namespaces
        static constexpr std::string_view NAMESPACES =
            "(namespace_definition name: (namespace_identifier) @namespace_name)";

        // Find all struct declarations
        static constexpr std::string_view STRUCTS =
            "(struct_specifier name: (type_identifier) @struct_name)";

        // Find template declarations
        static constexpr std::string_view TEMPLATES =
            "(template_declaration) @template_decl";
    };

private:
    /**
     * @brief Extract line and column from a node
     */
    void get_node_position(TSNode node, uint32_t& line, uint32_t& column) const;
};

} // namespace ts_mcp
