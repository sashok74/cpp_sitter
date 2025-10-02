#pragma once

#include "core/ASTAnalyzer.hpp"
#include "core/QueryEngine.hpp"
#include "core/Language.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace ts_mcp {

/**
 * @brief Enhanced MCP tool for comprehensive file analysis
 *
 * Extends parse_file with detailed metrics and summaries:
 * - Cyclomatic complexity per function
 * - TODO/FIXME/HACK extraction from comments
 * - Full function signatures with parameter types
 * - Member variables with types
 * - Docstring/comment extraction
 * - Import/include analysis
 * - Code metrics (LOC, branch count)
 *
 * Useful for:
 * - Quick file overview without reading full code
 * - Complexity assessment
 * - Maintenance task discovery
 * - API understanding
 */
class GetFileSummaryTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit GetFileSummaryTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with parameters
     * @return JSON result with enhanced summary or error
     */
    json execute(const json& args);

private:
    /**
     * @brief Comment marker types
     */
    enum class CommentMarker {
        TODO,
        FIXME,
        HACK,
        NOTE,
        WARNING,
        BUG,
        OPTIMIZE
    };

    /**
     * @brief Function signature information
     */
    struct FunctionSignature {
        std::string name;
        std::string return_type;
        std::vector<std::pair<std::string, std::string>> parameters; // (type, name)
        int line;
        int complexity;  // Cyclomatic complexity
        std::string docstring;
        bool is_virtual;
        bool is_static;
        bool is_async;  // Python
    };

    /**
     * @brief Member variable information
     */
    struct MemberVariable {
        std::string name;
        std::string type;
        int line;
        std::string access;  // public/private/protected
        std::string docstring;
    };

    /**
     * @brief Comment marker instance
     */
    struct CommentMarkerInfo {
        CommentMarker type;
        std::string text;
        int line;
        std::string context;
    };

    /**
     * @brief Import/include information
     */
    struct ImportInfo {
        std::string path;
        int line;
        bool is_system;  // <> vs ""
        std::string module;  // Python module name
    };

    /**
     * @brief Generate enhanced summary for a single file
     * @param filepath Path to file
     * @param language Programming language
     * @param include_complexity Calculate cyclomatic complexity
     * @param include_comments Extract TODO/FIXME markers
     * @param include_docstrings Extract documentation
     * @return JSON with enhanced summary
     */
    json summarize_file(
        const std::string& filepath,
        Language language,
        bool include_complexity,
        bool include_comments,
        bool include_docstrings
    );

    /**
     * @brief Calculate cyclomatic complexity for a function
     * @param node Function definition node
     * @param source Source code
     * @return Complexity score
     */
    int calculate_complexity(TSNode node, std::string_view source);

    /**
     * @brief Extract function signature with full details
     * @param node Function node
     * @param source Source code
     * @param language Programming language
     * @param include_docstring Include documentation
     * @return Function signature info
     */
    FunctionSignature extract_function_signature(
        TSNode node,
        std::string_view source,
        Language language,
        bool include_docstring
    );

    /**
     * @brief Extract member variable information
     * @param node Member declaration node
     * @param source Source code
     * @param language Programming language
     * @return Member variable info
     */
    MemberVariable extract_member_variable(
        TSNode node,
        std::string_view source,
        Language language
    );

    /**
     * @brief Extract all comment markers (TODO, FIXME, etc.)
     * @param source Source code
     * @param language Programming language
     * @return Vector of comment markers
     */
    std::vector<CommentMarkerInfo> extract_comment_markers(
        std::string_view source,
        Language language
    );

    /**
     * @brief Extract imports/includes
     * @param node Root node
     * @param source Source code
     * @param language Programming language
     * @return Vector of imports
     */
    std::vector<ImportInfo> extract_imports(
        TSNode node,
        std::string_view source,
        Language language
    );

    /**
     * @brief Get docstring/comment before node
     * @param node Target node
     * @param source Source code
     * @param language Programming language
     * @return Docstring text or empty
     */
    std::string get_docstring(
        TSNode node,
        std::string_view source,
        Language language
    );

    /**
     * @brief Calculate code metrics
     * @param source Source code
     * @return Map with metrics (loc, sloc, comment_lines)
     */
    std::map<std::string, int> calculate_metrics(std::string_view source);

    /**
     * @brief Convert CommentMarker to string
     */
    static std::string marker_to_string(CommentMarker marker);

    /**
     * @brief Detect comment marker type from text
     */
    static CommentMarker detect_marker_type(std::string_view text);

    std::shared_ptr<ASTAnalyzer> analyzer_;
    QueryEngine query_engine_;
};

} // namespace ts_mcp
