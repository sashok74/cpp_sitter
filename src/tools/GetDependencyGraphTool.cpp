#include "tools/GetDependencyGraphTool.hpp"
#include "core/TreeSitterParser.hpp"
#include "core/Language.hpp"
#include "core/PathResolver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <queue>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ts_mcp {

GetDependencyGraphTool::GetDependencyGraphTool(std::shared_ptr<ASTAnalyzer> analyzer)
    : analyzer_(std::move(analyzer)), query_engine_() {
    spdlog::debug("GetDependencyGraphTool initialized");
}

ToolInfo GetDependencyGraphTool::get_info() {
    return ToolInfo{
        "get_dependency_graph",
        "Analyze #include dependency graphs with cycle detection",
        {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"type", json::array({"string", "array"})},
                    {"description", "File path, array of paths, or directory"}
                }},
                {"show_system_includes", {
                    {"type", "boolean"},
                    {"description", "Include system headers (<>) in graph (default: false)"}
                }},
                {"detect_cycles", {
                    {"type", "boolean"},
                    {"description", "Detect circular dependencies (default: true)"}
                }},
                {"max_depth", {
                    {"type", "integer"},
                    {"description", "Maximum dependency depth, -1 for unlimited (default: -1)"}
                }},
                {"output_format", {
                    {"type", "string"},
                    {"enum", json::array({"json", "mermaid", "dot"})},
                    {"description", "Output format (default: json)"}
                }},
                {"recursive", {
                    {"type", "boolean"},
                    {"description", "Scan directories recursively (default: true)"}
                }},
                {"file_patterns", {
                    {"type", "array"},
                    {"items", {{"type", "string"}}},
                    {"description", "File patterns for filtering (default: [\"*.cpp\", \"*.hpp\", \"*.h\", \"*.py\"])"}
                }}
            }},
            {"required", json::array({"filepath"})}
        }
    };
}

json GetDependencyGraphTool::execute(const json& args) {
    try {
        // Extract parameters
        if (!args.contains("filepath")) {
            json error = {{"error", "Missing required parameter: filepath"}};
            return error;
        }

        bool show_system = args.value("show_system_includes", false);
        bool detect_cycles_flag = args.value("detect_cycles", true);
        int max_depth = args.value("max_depth", -1);
        std::string output_format = args.value("output_format", "json");
        bool recursive = args.value("recursive", true);

        json file_patterns_json = args.value("file_patterns",
            json::array({"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"}));
        std::vector<std::string> file_patterns;
        for (const auto& pattern : file_patterns_json) {
            file_patterns.push_back(pattern.get<std::string>());
        }

        // Resolve paths
        std::vector<std::string> input_paths;

        if (args["filepath"].is_string()) {
            input_paths.push_back(args["filepath"].get<std::string>());
        } else if (args["filepath"].is_array()) {
            for (const auto& path_json : args["filepath"]) {
                input_paths.push_back(path_json.get<std::string>());
            }
        }

        std::vector<std::filesystem::path> resolved =
            PathResolver::resolve_paths(input_paths, recursive, file_patterns);

        if (resolved.empty()) {
            json error = {
                {"error", "Failed to resolve any files from filepath"},
                {"success", false}
            };
            return error;
        }

        // Extract all dependencies
        std::vector<DependencyEdge> all_edges;
        int files_processed = 0;
        int files_failed = 0;

        for (const auto& filepath : resolved) {
            Language lang = LanguageUtils::detect_from_extension(filepath);

            try {
                auto edges = extract_includes(filepath.string(), lang);
                all_edges.insert(all_edges.end(), edges.begin(), edges.end());
                files_processed++;
            } catch (const std::exception& e) {
                spdlog::warn("Failed to extract includes from {}: {}", filepath.string(), e.what());
                files_failed++;
            }
        }

        // Build graph
        auto graph = build_graph(all_edges, show_system);

        // Filter by depth if specified
        if (max_depth >= 0) {
            std::vector<std::string> root_files;
            for (const auto& path : resolved) {
                root_files.push_back(normalize_path(path.string()));
            }
            graph = filter_by_depth(graph, root_files, max_depth);
        }

        // Detect cycles
        std::vector<std::vector<std::string>> cycles;
        if (detect_cycles_flag) {
            cycles = detect_cycles(graph);
        }

        // Compute layers
        auto layers = compute_layers(graph);

        // Format output
        if (output_format == "mermaid") {
            json result;
            result["format"] = "mermaid";
            result["content"] = graph_to_mermaid(graph, all_edges, cycles);
            result["total_files"] = files_processed;
            result["total_dependencies"] = all_edges.size();
            result["cycles_found"] = cycles.size();
            result["success"] = true;
            return result;
        } else if (output_format == "dot") {
            json result;
            result["format"] = "dot";
            result["content"] = graph_to_dot(graph, all_edges, cycles);
            result["total_files"] = files_processed;
            result["total_dependencies"] = all_edges.size();
            result["cycles_found"] = cycles.size();
            result["success"] = true;
            return result;
        } else {
            // JSON format
            json result = graph_to_json(graph, all_edges, cycles, layers);
            result["total_files"] = files_processed;
            result["files_failed"] = files_failed;
            result["success"] = true;
            return result;
        }

    } catch (const std::exception& e) {
        spdlog::error("GetDependencyGraphTool error: {}", e.what());
        json error = {
            {"error", std::string("Internal error: ") + e.what()},
            {"success", false}
        };
        return error;
    }
}

std::vector<GetDependencyGraphTool::DependencyEdge>
GetDependencyGraphTool::extract_includes(
    const std::string& filepath,
    Language language
) {
    std::vector<DependencyEdge> edges;

    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::warn("Cannot open file {}", filepath);
        return edges;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Parse file
    TreeSitterParser parser(language);
    auto parse_result = parser.parse_string(source);

    if (!parse_result) {
        spdlog::warn("Failed to parse {}", filepath);
        return edges;
    }

    std::string normalized_from = normalize_path(filepath);

    if (language == Language::CPP) {
        // C++ #include directives
        std::string include_query = R"(
            (preproc_include
                path: (_) @include_path
            )
        )";

        auto query = query_engine_.compile_query(include_query, language);
        if (!query) {
            spdlog::warn("Failed to compile include query");
            return edges;
        }

        auto matches = query_engine_.execute(*parse_result, *query, source);

        for (const auto& match : matches) {
            if (match.capture_name == "include_path") {
                std::string path = match.text;

                // Determine if system or user include
                bool is_system = (path.front() == '<');

                // Remove quotes or angle brackets
                if (!path.empty() && (path.front() == '"' || path.front() == '<')) {
                    path = path.substr(1, path.length() - 2);
                }

                DependencyEdge edge;
                edge.from = normalized_from;
                edge.to = path;
                edge.is_system = is_system;
                edge.line = match.line;

                edges.push_back(edge);
            }
        }

    } else if (language == Language::PYTHON) {
        // Python import statements
        std::string import_query = R"(
            (import_statement) @import
        )";

        auto query = query_engine_.compile_query(import_query, language);
        if (!query) {
            return edges;
        }

        auto matches = query_engine_.execute(*parse_result, *query, source);

        for (const auto& match : matches) {
            // Extract module name from import statement
            std::string import_text = match.text;

            // Simple parsing: "import module" or "from module import ..."
            size_t import_pos = import_text.find("import");
            if (import_pos != std::string::npos) {
                std::string module_part = import_text.substr(import_pos + 6);

                // Trim whitespace
                module_part.erase(0, module_part.find_first_not_of(" \t"));
                size_t space_pos = module_part.find_first_of(" \t,");
                if (space_pos != std::string::npos) {
                    module_part = module_part.substr(0, space_pos);
                }

                if (!module_part.empty()) {
                    DependencyEdge edge;
                    edge.from = normalized_from;
                    edge.to = module_part;
                    edge.is_system = false;  // Python imports are not "system" in same sense
                    edge.line = match.line;

                    edges.push_back(edge);
                }
            }
        }
    }

    return edges;
}

std::map<std::string, GetDependencyGraphTool::FileNode>
GetDependencyGraphTool::build_graph(
    const std::vector<DependencyEdge>& edges,
    bool show_system
) {
    std::map<std::string, FileNode> graph;

    for (const auto& edge : edges) {
        // Skip system includes if not requested
        if (edge.is_system && !show_system) {
            continue;
        }

        // Ensure nodes exist
        if (graph.find(edge.from) == graph.end()) {
            FileNode node;
            node.filepath = edge.from;
            node.is_system = false;
            node.layer = -1;
            graph[edge.from] = node;
        }

        if (graph.find(edge.to) == graph.end()) {
            FileNode node;
            node.filepath = edge.to;
            node.is_system = edge.is_system;
            node.layer = -1;
            graph[edge.to] = node;
        }

        // Add edge
        graph[edge.from].includes.push_back(edge.to);
        graph[edge.to].included_by.push_back(edge.from);
    }

    return graph;
}

std::vector<std::vector<std::string>>
GetDependencyGraphTool::detect_cycles(
    const std::map<std::string, FileNode>& graph
) {
    std::vector<std::vector<std::string>> sccs;
    std::map<std::string, int> indices;
    std::map<std::string, int> lowlinks;
    std::set<std::string> on_stack;
    std::vector<std::string> stack;
    int index = 0;

    for (const auto& [node, _] : graph) {
        if (indices.find(node) == indices.end()) {
            tarjan_scc(node, graph, index, stack, indices, lowlinks, on_stack, sccs);
        }
    }

    // Filter out single-node SCCs (not cycles)
    std::vector<std::vector<std::string>> cycles;
    for (const auto& scc : sccs) {
        if (scc.size() > 1) {
            cycles.push_back(scc);
        }
    }

    return cycles;
}

void GetDependencyGraphTool::tarjan_scc(
    const std::string& node,
    const std::map<std::string, FileNode>& graph,
    int& index,
    std::vector<std::string>& stack,
    std::map<std::string, int>& indices,
    std::map<std::string, int>& lowlinks,
    std::set<std::string>& on_stack,
    std::vector<std::vector<std::string>>& sccs
) {
    indices[node] = index;
    lowlinks[node] = index;
    index++;
    stack.push_back(node);
    on_stack.insert(node);

    // Consider successors
    if (graph.find(node) != graph.end()) {
        for (const auto& successor : graph.at(node).includes) {
            if (indices.find(successor) == indices.end()) {
                // Successor has not yet been visited; recurse
                tarjan_scc(successor, graph, index, stack, indices, lowlinks, on_stack, sccs);
                lowlinks[node] = std::min(lowlinks[node], lowlinks[successor]);
            } else if (on_stack.find(successor) != on_stack.end()) {
                // Successor is on stack and hence in the current SCC
                lowlinks[node] = std::min(lowlinks[node], indices[successor]);
            }
        }
    }

    // If node is a root node, pop the stack and generate an SCC
    if (lowlinks[node] == indices[node]) {
        std::vector<std::string> scc;
        std::string w;
        do {
            w = stack.back();
            stack.pop_back();
            on_stack.erase(w);
            scc.push_back(w);
        } while (w != node);

        sccs.push_back(scc);
    }
}

std::map<int, std::vector<std::string>>
GetDependencyGraphTool::compute_layers(
    const std::map<std::string, FileNode>& graph
) {
    std::map<int, std::vector<std::string>> layers;
    std::map<std::string, int> node_layers;

    // Find nodes with no dependencies (layer 0)
    std::queue<std::string> queue;
    std::map<std::string, int> in_degree;

    for (const auto& [node, info] : graph) {
        in_degree[node] = info.included_by.size();
        if (in_degree[node] == 0) {
            queue.push(node);
            node_layers[node] = 0;
        }
    }

    // Topological sort with layer assignment
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        int current_layer = node_layers[current];
        layers[current_layer].push_back(current);

        if (graph.find(current) != graph.end()) {
            for (const auto& dependent : graph.at(current).includes) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    queue.push(dependent);
                    node_layers[dependent] = current_layer + 1;
                }
            }
        }
    }

    return layers;
}

std::map<std::string, GetDependencyGraphTool::FileNode>
GetDependencyGraphTool::filter_by_depth(
    const std::map<std::string, FileNode>& graph,
    const std::vector<std::string>& root_files,
    int max_depth
) {
    std::map<std::string, FileNode> filtered;
    std::queue<std::pair<std::string, int>> queue;
    std::set<std::string> visited;

    // Start from root files
    for (const auto& root : root_files) {
        if (graph.find(root) != graph.end()) {
            queue.push({root, 0});
            visited.insert(root);
        }
    }

    while (!queue.empty()) {
        auto [current, depth] = queue.front();
        queue.pop();

        // Add to filtered graph
        if (graph.find(current) != graph.end()) {
            filtered[current] = graph.at(current);
        }

        // Check depth limit
        if (max_depth >= 0 && depth >= max_depth) {
            continue;
        }

        // Add dependencies
        if (graph.find(current) != graph.end()) {
            for (const auto& dep : graph.at(current).includes) {
                if (visited.find(dep) == visited.end()) {
                    queue.push({dep, depth + 1});
                    visited.insert(dep);
                }
            }
        }
    }

    return filtered;
}

json GetDependencyGraphTool::graph_to_json(
    const std::map<std::string, FileNode>& graph,
    const std::vector<DependencyEdge>& edges,
    const std::vector<std::vector<std::string>>& cycles,
    const std::map<int, std::vector<std::string>>& layers
) {
    json result;

    // Nodes
    json nodes = json::array();
    for (const auto& [filepath, node] : graph) {
        json n;
        n["file"] = node.filepath;
        n["includes"] = node.includes;
        n["included_by"] = node.included_by;
        n["is_system"] = node.is_system;

        // Find layer
        for (const auto& [layer_num, files] : layers) {
            if (std::find(files.begin(), files.end(), filepath) != files.end()) {
                n["layer"] = layer_num;
                break;
            }
        }

        nodes.push_back(n);
    }
    result["nodes"] = nodes;

    // Edges
    json edges_json = json::array();
    for (const auto& edge : edges) {
        json e;
        e["from"] = edge.from;
        e["to"] = edge.to;
        e["is_system"] = edge.is_system;
        e["line"] = edge.line;
        edges_json.push_back(e);
    }
    result["edges"] = edges_json;

    // Cycles
    json cycles_json = json::array();
    for (const auto& cycle : cycles) {
        cycles_json.push_back(cycle);
    }
    result["cycles"] = cycles_json;

    // Layers
    json layers_json;
    for (const auto& [layer_num, files] : layers) {
        layers_json[std::to_string(layer_num)] = files;
    }
    result["layers"] = layers_json;

    return result;
}

std::string GetDependencyGraphTool::graph_to_mermaid(
    const std::map<std::string, FileNode>& graph,
    const std::vector<DependencyEdge>& edges,
    const std::vector<std::vector<std::string>>& cycles
) {
    std::stringstream ss;
    ss << "graph TD\n";

    // Nodes
    std::map<std::string, std::string> node_ids;
    int id_counter = 0;
    for (const auto& [filepath, node] : graph) {
        std::string node_id = "N" + std::to_string(id_counter++);
        node_ids[filepath] = node_id;

        // Extract filename for display
        std::filesystem::path p(filepath);
        std::string display_name = p.filename().string();

        ss << "    " << node_id << "[\"" << display_name << "\"]\n";
    }

    // Edges
    std::set<std::string> cycle_edges;
    for (const auto& cycle : cycles) {
        for (size_t i = 0; i < cycle.size(); i++) {
            std::string from = cycle[i];
            std::string to = cycle[(i + 1) % cycle.size()];
            cycle_edges.insert(from + "->" + to);
        }
    }

    for (const auto& edge : edges) {
        if (node_ids.find(edge.from) != node_ids.end() &&
            node_ids.find(edge.to) != node_ids.end()) {

            std::string edge_key = edge.from + "->" + edge.to;
            bool is_cycle_edge = cycle_edges.find(edge_key) != cycle_edges.end();

            if (is_cycle_edge) {
                ss << "    " << node_ids[edge.from] << " -.->|cycle| " << node_ids[edge.to] << "\n";
            } else {
                ss << "    " << node_ids[edge.from] << " --> " << node_ids[edge.to] << "\n";
            }
        }
    }

    // Highlight cycles
    if (!cycles.empty()) {
        ss << "\n    classDef cycleNode fill:#f96\n";
        for (const auto& cycle : cycles) {
            for (const auto& node : cycle) {
                if (node_ids.find(node) != node_ids.end()) {
                    ss << "    class " << node_ids[node] << " cycleNode\n";
                }
            }
        }
    }

    return ss.str();
}

std::string GetDependencyGraphTool::graph_to_dot(
    const std::map<std::string, FileNode>& graph,
    const std::vector<DependencyEdge>& edges,
    const std::vector<std::vector<std::string>>& cycles
) {
    std::stringstream ss;
    ss << "digraph dependencies {\n";
    ss << "    rankdir=LR;\n";
    ss << "    node [shape=box];\n\n";

    // Nodes
    std::map<std::string, std::string> node_ids;
    int id_counter = 0;
    for (const auto& [filepath, node] : graph) {
        std::string node_id = "N" + std::to_string(id_counter++);
        node_ids[filepath] = node_id;

        std::filesystem::path p(filepath);
        std::string display_name = p.filename().string();

        ss << "    " << node_id << " [label=\"" << display_name << "\"];\n";
    }

    ss << "\n";

    // Edges
    std::set<std::string> cycle_edges;
    for (const auto& cycle : cycles) {
        for (size_t i = 0; i < cycle.size(); i++) {
            std::string from = cycle[i];
            std::string to = cycle[(i + 1) % cycle.size()];
            cycle_edges.insert(from + "->" + to);
        }
    }

    for (const auto& edge : edges) {
        if (node_ids.find(edge.from) != node_ids.end() &&
            node_ids.find(edge.to) != node_ids.end()) {

            std::string edge_key = edge.from + "->" + edge.to;
            bool is_cycle_edge = cycle_edges.find(edge_key) != cycle_edges.end();

            ss << "    " << node_ids[edge.from] << " -> " << node_ids[edge.to];
            if (is_cycle_edge) {
                ss << " [color=red, penwidth=2.0, label=\"cycle\"]";
            }
            ss << ";\n";
        }
    }

    ss << "}\n";
    return ss.str();
}

std::string GetDependencyGraphTool::normalize_path(const std::string& filepath) {
    std::filesystem::path p(filepath);

    // Try to make relative to current directory
    try {
        auto current = std::filesystem::current_path();
        auto relative = std::filesystem::relative(p, current);
        if (!relative.empty() && relative.string().find("..") == std::string::npos) {
            return relative.string();
        }
    } catch (...) {
        // Fall through to absolute path
    }

    // Return filename if path normalization fails
    return p.filename().string();
}

} // namespace ts_mcp
