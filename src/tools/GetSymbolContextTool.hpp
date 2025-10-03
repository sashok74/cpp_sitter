#pragma once

#include "mcp/MCPServer.hpp"
#include "core/ASTAnalyzer.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace ts_mcp {

/**
 * @brief MCP tool to get comprehensive context for a symbol (function/class/method)
 *
 * This tool implements a 4-stage pipeline:
 * 1. Localization - find symbol definition in file
 * 2. Extraction - get code and metadata
 * 3. Dependency Analysis - determine what the symbol uses
 * 4. Enrichment - add related context
 */
class GetSymbolContextTool {
public:
    explicit GetSymbolContextTool(std::shared_ptr<ASTAnalyzer> analyzer)
        : analyzer_(std::move(analyzer)) {}

    static ToolInfo get_info();
    nlohmann::json execute(const nlohmann::json& args);

private:
    // Stage 1: Localization - find symbol in file
    struct SymbolLocation {
        std::string name;
        std::string type;        // "function", "method", "class"
        std::string filepath;
        int start_line;
        int end_line;
        uint32_t start_byte;
        uint32_t end_byte;
    };

    std::optional<SymbolLocation> locate_symbol(
        const std::string& symbol_name,
        const std::string& filepath,
        Language language
    );

    // Stage 2: Extraction - get code and metadata
    struct SymbolDefinition {
        std::string name;
        std::string type;
        std::string filepath;
        int start_line;
        int end_line;
        std::string signature;   // without body
        std::string full_code;   // with body
        std::string parent_class; // if method
    };

    SymbolDefinition extract_definition(
        const SymbolLocation& location,
        const std::string& source
    );

    // Stage 3: Dependency Analysis - what does symbol use
    struct UsedSymbol {
        std::string name;
        std::string type;        // "type", "function", "variable"
        std::string context;     // where used
    };

    std::vector<UsedSymbol> analyze_dependencies(
        const SymbolDefinition& definition,
        const std::string& source,
        uint32_t start_byte,
        uint32_t end_byte
    );

    // Stage 4: Enrichment - add related context
    struct UsageExample {
        std::string filepath;
        int line;
        std::vector<std::string> context_lines;
        std::string parent_scope;
    };

    struct EnrichedContext {
        SymbolDefinition target;
        std::vector<SymbolDefinition> dependencies;
        std::vector<std::string> includes;
        std::vector<UsageExample> usage_examples;
    };

    EnrichedContext enrich_context(
        const SymbolDefinition& target,
        const std::vector<UsedSymbol>& used_symbols,
        const std::string& filepath,
        Language language,
        bool resolve_external_types = false,
        bool include_usage_examples = false,
        int context_lines = 3,
        const std::vector<std::string>& search_paths = {}
    );

    // Helper: extract signature without body
    std::string extract_signature(
        TSNode node,
        const std::string& source
    );

    // Helper: find symbol in current file
    std::optional<SymbolDefinition> find_symbol_in_file(
        const std::string& symbol_name,
        const std::string& filepath,
        Language language
    );

    // Helper: extract includes from file
    std::vector<std::string> extract_includes(
        const std::string& filepath,
        Language language
    );

    // Helper: get node text
    std::string get_node_text(TSNode node, const std::string& source);

    // Helper: get line number for position
    int get_line_number(const std::string& source, uint32_t byte_offset);

    // NEW: Cross-file type resolution
    std::optional<SymbolDefinition> find_in_search_paths(
        const std::string& symbol_name,
        const std::vector<std::string>& search_paths
    );

    // NEW: Read context lines around a specific line
    std::vector<std::string> read_context_lines(
        const std::string& filepath,
        int center_line,
        int context_size
    );

    // NEW: Find all usages of a symbol
    std::vector<UsageExample> find_usage_examples(
        const std::string& symbol_name,
        const std::string& base_path,
        int context_lines,
        int max_examples = 10
    );

    std::shared_ptr<ASTAnalyzer> analyzer_;
};

} // namespace ts_mcp
