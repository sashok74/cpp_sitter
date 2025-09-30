#pragma once

#include "core/ASTAnalyzer.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>

namespace ts_mcp {

/**
 * @brief MCP tool for executing custom tree-sitter queries
 *
 * Allows executing arbitrary S-expression queries on C++ files.
 */
class ExecuteQueryTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit ExecuteQueryTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with "filepath" and "query" parameters
     * @return JSON array of query matches
     */
    json execute(const json& args);

private:
    std::shared_ptr<ASTAnalyzer> analyzer_;
};

} // namespace ts_mcp
