#pragma once

#include "core/ASTAnalyzer.hpp"
#include "mcp/MCPServer.hpp"
#include <memory>

namespace ts_mcp {

/**
 * @brief MCP tool for finding all class declarations in a C++ file
 *
 * Returns list of classes with their names and line numbers.
 */
class FindClassesTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit FindClassesTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with "filepath" parameter
     * @return JSON array of classes with names and locations
     */
    json execute(const json& args);

private:
    std::shared_ptr<ASTAnalyzer> analyzer_;
};

} // namespace ts_mcp
