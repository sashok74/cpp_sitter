#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <filesystem>
#include "core/Language.hpp"

// Forward declarations for tree-sitter C API
extern "C" {
    struct TSParser;
    struct TSTree;
    struct TSNode;
    struct TSInputEdit;
}

namespace ts_mcp {

/**
 * @brief RAII wrapper for TSTree from tree-sitter
 *
 * Manages the lifetime of a TSTree object, ensuring proper cleanup.
 */
class Tree {
public:
    explicit Tree(TSTree* tree);
    ~Tree();

    // Delete copy operations
    Tree(const Tree&) = delete;
    Tree& operator=(const Tree&) = delete;

    // Move operations
    Tree(Tree&& other) noexcept;
    Tree& operator=(Tree&& other) noexcept;

    /**
     * @brief Get the root node of the syntax tree
     */
    TSNode root_node() const;

    /**
     * @brief Check if the tree has any syntax errors
     */
    bool has_error() const;

    /**
     * @brief Get the underlying TSTree pointer
     */
    TSTree* get() const { return tree_; }

private:
    TSTree* tree_;
};

/**
 * @brief RAII parser for C++ source code using tree-sitter
 *
 * This class wraps the tree-sitter C API and provides a modern C++ interface
 * for parsing C++ code into Abstract Syntax Trees (AST).
 */
class TreeSitterParser {
public:
    /**
     * @brief Construct a new TreeSitterParser
     * @param lang Programming language to parse (default: CPP)
     * @throws std::runtime_error if parser creation fails or language not supported
     */
    explicit TreeSitterParser(Language lang = Language::CPP);

    /**
     * @brief Destroy the TreeSitterParser
     */
    ~TreeSitterParser();

    // Delete copy operations
    TreeSitterParser(const TreeSitterParser&) = delete;
    TreeSitterParser& operator=(const TreeSitterParser&) = delete;

    // Move operations
    TreeSitterParser(TreeSitterParser&& other) noexcept;
    TreeSitterParser& operator=(TreeSitterParser&& other) noexcept;

    /**
     * @brief Parse C++ source code from a string
     * @param source Source code to parse
     * @return Unique pointer to parsed tree, or nullptr on error
     */
    std::unique_ptr<Tree> parse_string(std::string_view source);

    /**
     * @brief Parse C++ source code from a file
     * @param filepath Path to the file to parse
     * @return Unique pointer to parsed tree, or nullptr on error
     * @throws std::runtime_error if file cannot be read
     */
    std::unique_ptr<Tree> parse_file(const std::filesystem::path& filepath);

    /**
     * @brief Perform incremental parsing on an existing tree
     * @param old_tree Previously parsed tree
     * @param new_source Updated source code
     * @param edit Edit information describing the change
     * @return Unique pointer to updated tree, or nullptr on error
     */
    std::unique_ptr<Tree> parse_incremental(
        const Tree& old_tree,
        std::string_view new_source,
        const TSInputEdit& edit
    );

    /**
     * @brief Extract text content of a syntax node
     * @param node The syntax node
     * @param source The source code string
     * @return Text content of the node
     */
    std::string node_text(TSNode node, std::string_view source) const;

    /**
     * @brief Get the last source code that was parsed
     * @return Reference to the cached source code
     */
    const std::string& last_source() const { return last_source_; }

    /**
     * @brief Get the language this parser is configured for
     * @return Language enum value
     */
    Language language() const { return language_; }

private:
    TSParser* parser_;
    std::string last_source_;
    Language language_;
};

} // namespace ts_mcp
