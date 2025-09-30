#include "core/QueryEngine.hpp"
#include <spdlog/spdlog.h>

// Tree-sitter C API
extern "C" {
    #include <tree_sitter/api.h>
    TSLanguage* tree_sitter_cpp();
}

namespace ts_mcp {

// ============================================================================
// Query implementation
// ============================================================================

Query::Query(TSQuery* query) : query_(query) {
    if (!query_) {
        throw std::invalid_argument("Cannot create Query with nullptr");
    }
}

Query::~Query() {
    if (query_) {
        ts_query_delete(query_);
    }
}

Query::Query(Query&& other) noexcept : query_(other.query_) {
    other.query_ = nullptr;
}

Query& Query::operator=(Query&& other) noexcept {
    if (this != &other) {
        if (query_) {
            ts_query_delete(query_);
        }
        query_ = other.query_;
        other.query_ = nullptr;
    }
    return *this;
}

uint32_t Query::pattern_count() const {
    return ts_query_pattern_count(query_);
}

uint32_t Query::capture_count() const {
    return ts_query_capture_count(query_);
}

std::string Query::capture_name(uint32_t index) const {
    uint32_t length;
    const char* name = ts_query_capture_name_for_id(query_, index, &length);
    return std::string(name, length);
}

// ============================================================================
// QueryEngine implementation
// ============================================================================

std::unique_ptr<Query> QueryEngine::compile_query(std::string_view query_string) {
    TSLanguage* language = tree_sitter_cpp();

    uint32_t error_offset;
    TSQueryError error_type;

    TSQuery* raw_query = ts_query_new(
        language,
        query_string.data(),
        static_cast<uint32_t>(query_string.size()),
        &error_offset,
        &error_type
    );

    if (!raw_query) {
        spdlog::error("Failed to compile query at offset {}: error type {}",
                     error_offset, static_cast<int>(error_type));
        return nullptr;
    }

    spdlog::debug("Query compiled successfully with {} patterns",
                 ts_query_pattern_count(raw_query));

    return std::make_unique<Query>(raw_query);
}

std::vector<QueryMatch> QueryEngine::execute(
    const Tree& tree,
    const Query& query,
    std::string_view source
) {
    std::vector<QueryMatch> results;

    // Create query cursor
    TSQueryCursor* cursor = ts_query_cursor_new();
    if (!cursor) {
        spdlog::error("Failed to create query cursor");
        return results;
    }

    // Execute query
    ts_query_cursor_exec(cursor, query.get(), tree.root_node());

    // Iterate through matches
    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        // Process each capture in the match
        for (uint32_t i = 0; i < match.capture_count; i++) {
            TSQueryCapture capture = match.captures[i];
            TSNode node = capture.node;

            QueryMatch result;
            result.node = node;
            result.capture_name = query.capture_name(capture.index);

            // Get position
            get_node_position(node, result.line, result.column);

            // Extract text
            uint32_t start = ts_node_start_byte(node);
            uint32_t end = ts_node_end_byte(node);

            if (start < source.size() && end <= source.size() && start < end) {
                result.text = std::string(source.substr(start, end - start));
            } else {
                spdlog::warn("Invalid node byte range: [{}, {})", start, end);
                result.text = "";
            }

            results.push_back(std::move(result));
        }
    }

    ts_query_cursor_delete(cursor);

    spdlog::debug("Query executed with {} matches", results.size());

    return results;
}

void QueryEngine::get_node_position(TSNode node, uint32_t& line, uint32_t& column) const {
    TSPoint start_point = ts_node_start_point(node);
    line = start_point.row;
    column = start_point.column;
}

} // namespace ts_mcp
