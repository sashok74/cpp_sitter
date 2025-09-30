#pragma once

#include "core/ASTAnalyzer.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>

namespace ts_mcp {

/**
 * @brief MCP tool for parsing C++ files and returning metadata
 *
 * Returns statistics about the file: number of classes, functions,
 * syntax errors, and overall parsing success status.
 */
class ParseFileTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit ParseFileTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with "filepath" parameter
     * @return JSON result with file metadata or error
     */
    json execute(const json& args);

private:
    std::shared_ptr<ASTAnalyzer> analyzer_;
};

} // namespace ts_mcp
