#pragma once

#include "core/ASTAnalyzer.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>

namespace ts_mcp {

/**
 * @brief MCP tool for finding all function definitions in a C++ file
 *
 * Returns list of functions with their names and line numbers.
 */
class FindFunctionsTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit FindFunctionsTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with "filepath" parameter
     * @return JSON array of functions with names and locations
     */
    json execute(const json& args);

private:
    std::shared_ptr<ASTAnalyzer> analyzer_;
};

} // namespace ts_mcp
