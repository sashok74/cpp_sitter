#include "core/TreeSitterParser.hpp"
#include "core/Language.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Tree-sitter C API
extern "C" {
    #include <tree_sitter/api.h>
}

namespace ts_mcp {

// ============================================================================
// Tree implementation
// ============================================================================

Tree::Tree(TSTree* tree) : tree_(tree) {
    if (!tree_) {
        throw std::invalid_argument("Cannot create Tree with nullptr");
    }
}

Tree::~Tree() {
    if (tree_) {
        ts_tree_delete(tree_);
    }
}

Tree::Tree(Tree&& other) noexcept : tree_(other.tree_) {
    other.tree_ = nullptr;
}

Tree& Tree::operator=(Tree&& other) noexcept {
    if (this != &other) {
        if (tree_) {
            ts_tree_delete(tree_);
        }
        tree_ = other.tree_;
        other.tree_ = nullptr;
    }
    return *this;
}

TSNode Tree::root_node() const {
    return ts_tree_root_node(tree_);
}

bool Tree::has_error() const {
    TSNode root = root_node();
    return ts_node_has_error(root);
}

// ============================================================================
// TreeSitterParser implementation
// ============================================================================

TreeSitterParser::TreeSitterParser(Language lang)
    : parser_(nullptr), last_source_(), language_(lang) {
    parser_ = ts_parser_new();
    if (!parser_) {
        throw std::runtime_error("Failed to create TSParser");
    }

    // Get language grammar
    const TSLanguage* ts_lang = LanguageUtils::get_ts_language(lang);
    if (!ts_lang) {
        ts_parser_delete(parser_);
        throw std::runtime_error("Unsupported language: " +
                                std::string(LanguageUtils::to_string(lang)));
    }

    // Set language for parser
    if (!ts_parser_set_language(parser_, ts_lang)) {
        ts_parser_delete(parser_);
        throw std::runtime_error("Failed to set language for parser: " +
                                std::string(LanguageUtils::to_string(lang)));
    }

    spdlog::debug("TreeSitterParser created successfully for language: {}",
                  LanguageUtils::to_string(lang));
}

TreeSitterParser::~TreeSitterParser() {
    if (parser_) {
        ts_parser_delete(parser_);
    }
}

TreeSitterParser::TreeSitterParser(TreeSitterParser&& other) noexcept
    : parser_(other.parser_),
      last_source_(std::move(other.last_source_)),
      language_(other.language_) {
    other.parser_ = nullptr;
}

TreeSitterParser& TreeSitterParser::operator=(TreeSitterParser&& other) noexcept {
    if (this != &other) {
        if (parser_) {
            ts_parser_delete(parser_);
        }
        parser_ = other.parser_;
        last_source_ = std::move(other.last_source_);
        language_ = other.language_;
        other.parser_ = nullptr;
    }
    return *this;
}

std::unique_ptr<Tree> TreeSitterParser::parse_string(std::string_view source) {
    // Cache source for node_text operations
    last_source_ = std::string(source);

    spdlog::debug("Parsing string of length {}", source.size());

    TSTree* raw_tree = ts_parser_parse_string(
        parser_,
        nullptr,  // old_tree
        source.data(),
        static_cast<uint32_t>(source.size())
    );

    if (!raw_tree) {
        spdlog::error("Failed to parse source code");
        return nullptr;
    }

    auto tree = std::make_unique<Tree>(raw_tree);

    if (tree->has_error()) {
        spdlog::warn("Parse completed with syntax errors");
    } else {
        spdlog::debug("Parse completed successfully");
    }

    return tree;
}

std::unique_ptr<Tree> TreeSitterParser::parse_file(const std::filesystem::path& filepath) {
    // Read file contents
    std::ifstream file(filepath);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    spdlog::debug("Parsing file: {} ({} bytes)", filepath.string(), source.size());

    return parse_string(source);
}

std::unique_ptr<Tree> TreeSitterParser::parse_incremental(
    const Tree& old_tree,
    std::string_view new_source,
    const TSInputEdit& edit
) {
    // Apply edit to old tree
    ts_tree_edit(old_tree.get(), &edit);

    // Cache new source
    last_source_ = std::string(new_source);

    spdlog::debug("Performing incremental parse");

    TSTree* raw_tree = ts_parser_parse_string(
        parser_,
        old_tree.get(),
        new_source.data(),
        static_cast<uint32_t>(new_source.size())
    );

    if (!raw_tree) {
        spdlog::error("Failed to perform incremental parse");
        return nullptr;
    }

    auto tree = std::make_unique<Tree>(raw_tree);

    if (tree->has_error()) {
        spdlog::warn("Incremental parse completed with syntax errors");
    } else {
        spdlog::debug("Incremental parse completed successfully");
    }

    return tree;
}

std::string TreeSitterParser::node_text(TSNode node, std::string_view source) const {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);

    if (start >= source.size() || end > source.size() || start >= end) {
        spdlog::warn("Invalid node byte range: [{}, {})", start, end);
        return "";
    }

    return std::string(source.substr(start, end - start));
}

} // namespace ts_mcp
