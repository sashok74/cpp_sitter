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
 * @brief MCP tool for analyzing #include dependency graphs
 *
 * Provides comprehensive dependency analysis:
 * - Include directive extraction (C++ #include, Python import)
 * - Directed graph construction
 * - Cycle detection (Tarjan's algorithm)
 * - Topological sorting for build order
 * - Layered architecture visualization
 * - System vs user include distinction
 *
 * Useful for:
 * - Understanding project architecture
 * - Finding circular dependencies
 * - Optimizing build order
 * - Identifying tightly coupled modules
 * - Planning refactoring
 */
class GetDependencyGraphTool {
public:
    /**
     * @brief Construct tool with analyzer reference
     * @param analyzer AST analyzer instance
     */
    explicit GetDependencyGraphTool(std::shared_ptr<ASTAnalyzer> analyzer);

    /**
     * @brief Get tool metadata and JSON schema
     * @return ToolInfo with name, description, and input schema
     */
    static ToolInfo get_info();

    /**
     * @brief Execute tool with arguments
     * @param args JSON object with parameters
     * @return JSON result with dependency graph or error
     */
    json execute(const json& args);

private:
    /**
     * @brief Information about a dependency edge
     */
    struct DependencyEdge {
        std::string from;
        std::string to;
        bool is_system;
        int line;
    };

    /**
     * @brief Information about a file node
     */
    struct FileNode {
        std::string filepath;
        std::vector<std::string> includes;  // Files this depends on
        std::vector<std::string> included_by;  // Files that depend on this
        bool is_system;
        int layer;  // Topological layer
    };

    /**
     * @brief Extract include directives from a file
     * @param filepath Path to file
     * @param language Programming language
     * @return Vector of dependency edges
     */
    std::vector<DependencyEdge> extract_includes(
        const std::string& filepath,
        Language language
    );

    /**
     * @brief Build dependency graph from edges
     * @param edges Vector of all edges
     * @param show_system Include system headers
     * @return Map of file -> FileNode
     */
    std::map<std::string, FileNode> build_graph(
        const std::vector<DependencyEdge>& edges,
        bool show_system
    );

    /**
     * @brief Detect cycles in dependency graph using Tarjan's algorithm
     * @param graph Dependency graph
     * @return Vector of cycles (each cycle is a vector of file paths)
     */
    std::vector<std::vector<std::string>> detect_cycles(
        const std::map<std::string, FileNode>& graph
    );

    /**
     * @brief Tarjan's SCC algorithm helper
     * @param node Current node
     * @param graph Dependency graph
     * @param index Current index
     * @param stack DFS stack
     * @param indices Node indices
     * @param lowlinks Lowlink values
     * @param on_stack Nodes on stack
     * @param sccs Strongly connected components (output)
     */
    void tarjan_scc(
        const std::string& node,
        const std::map<std::string, FileNode>& graph,
        int& index,
        std::vector<std::string>& stack,
        std::map<std::string, int>& indices,
        std::map<std::string, int>& lowlinks,
        std::set<std::string>& on_stack,
        std::vector<std::vector<std::string>>& sccs
    );

    /**
     * @brief Compute topological layers for build order
     * @param graph Dependency graph
     * @return Map of layer number -> files
     */
    std::map<int, std::vector<std::string>> compute_layers(
        const std::map<std::string, FileNode>& graph
    );

    /**
     * @brief Filter graph by max depth from root files
     * @param graph Full graph
     * @param root_files Starting files
     * @param max_depth Maximum depth (-1 = unlimited)
     * @return Filtered graph
     */
    std::map<std::string, FileNode> filter_by_depth(
        const std::map<std::string, FileNode>& graph,
        const std::vector<std::string>& root_files,
        int max_depth
    );

    /**
     * @brief Convert graph to JSON format
     * @param graph Dependency graph
     * @param edges All edges
     * @param cycles Detected cycles
     * @param layers Topological layers
     * @return JSON representation
     */
    json graph_to_json(
        const std::map<std::string, FileNode>& graph,
        const std::vector<DependencyEdge>& edges,
        const std::vector<std::vector<std::string>>& cycles,
        const std::map<int, std::vector<std::string>>& layers
    );

    /**
     * @brief Convert graph to Mermaid diagram format
     * @param graph Dependency graph
     * @param edges All edges
     * @param cycles Detected cycles
     * @return Mermaid markdown string
     */
    std::string graph_to_mermaid(
        const std::map<std::string, FileNode>& graph,
        const std::vector<DependencyEdge>& edges,
        const std::vector<std::vector<std::string>>& cycles
    );

    /**
     * @brief Convert graph to Graphviz DOT format
     * @param graph Dependency graph
     * @param edges All edges
     * @param cycles Detected cycles
     * @return DOT format string
     */
    std::string graph_to_dot(
        const std::map<std::string, FileNode>& graph,
        const std::vector<DependencyEdge>& edges,
        const std::vector<std::vector<std::string>>& cycles
    );

    /**
     * @brief Normalize file path for graph nodes
     * @param filepath Original file path
     * @return Normalized path (e.g., relative to project root)
     */
    std::string normalize_path(const std::string& filepath);

    std::shared_ptr<ASTAnalyzer> analyzer_;
    QueryEngine query_engine_;
};

} // namespace ts_mcp
