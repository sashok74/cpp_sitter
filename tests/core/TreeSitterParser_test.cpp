#include <gtest/gtest.h>
#include "core/TreeSitterParser.hpp"
#include <filesystem>

extern "C" {
    #include <tree_sitter/api.h>
}

using namespace ts_mcp;

// Test 1: ParseSimpleString - parse "int main() { return 0; }"
TEST(TreeSitterParserTest, ParseSimpleString) {
    TreeSitterParser parser;
    std::string source = "int main() { return 0; }";

    auto tree = parser.parse_string(source);

    ASSERT_NE(tree, nullptr) << "Parser should return a valid tree";
    EXPECT_FALSE(tree->has_error()) << "Simple code should parse without errors";

    TSNode root = tree->root_node();
    EXPECT_TRUE(!ts_node_is_null(root)) << "Root node should not be null";
}

// Test 2: ParseSimpleClass - parse class with methods
TEST(TreeSitterParserTest, ParseSimpleClass) {
    TreeSitterParser parser;
    std::string source = R"(
class Calculator {
public:
    int add(int a, int b) {
        return a + b;
    }
};
)";

    auto tree = parser.parse_string(source);

    ASSERT_NE(tree, nullptr) << "Parser should return a valid tree";
    EXPECT_FALSE(tree->has_error()) << "Class definition should parse without errors";

    // Verify root node exists and has children
    TSNode root = tree->root_node();
    EXPECT_TRUE(!ts_node_is_null(root)) << "Root node should not be null";
    EXPECT_GT(ts_node_child_count(root), 0u) << "Root should have child nodes";
}

// Test 3: ParseFile - parse from fixtures/simple_class.cpp
TEST(TreeSitterParserTest, ParseFile) {
    TreeSitterParser parser;
    std::filesystem::path fixture_path = "../fixtures/simple_class.cpp";

    // Check if file exists
    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Fixture file should exist at " << fixture_path;

    auto tree = parser.parse_file(fixture_path);

    ASSERT_NE(tree, nullptr) << "Parser should return a valid tree for file";
    EXPECT_FALSE(tree->has_error()) << "simple_class.cpp should parse without errors";

    // Verify source was cached
    EXPECT_FALSE(parser.last_source().empty()) << "Source should be cached after parsing";
}

// Test 4: ParseWithSyntaxError - parse fixtures/syntax_error.cpp, verify has_error()
TEST(TreeSitterParserTest, ParseWithSyntaxError) {
    TreeSitterParser parser;
    std::filesystem::path fixture_path = "../fixtures/syntax_error.cpp";

    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Fixture file should exist at " << fixture_path;

    auto tree = parser.parse_file(fixture_path);

    ASSERT_NE(tree, nullptr) << "Parser should return a tree even with syntax errors";
    EXPECT_TRUE(tree->has_error()) << "syntax_error.cpp should have syntax errors";
}

// Test 5: NodeText - extract text from parsed nodes
TEST(TreeSitterParserTest, NodeText) {
    TreeSitterParser parser;
    std::string source = "int x = 42;";

    auto tree = parser.parse_string(source);
    ASSERT_NE(tree, nullptr);

    TSNode root = tree->root_node();
    ASSERT_FALSE(ts_node_is_null(root));

    // Get the first child (should be a declaration)
    if (ts_node_child_count(root) > 0) {
        TSNode first_child = ts_node_child(root, 0);
        std::string node_text = parser.node_text(first_child, source);

        EXPECT_FALSE(node_text.empty()) << "Node text should not be empty";
        EXPECT_NE(node_text.find("int"), std::string::npos)
            << "Node text should contain 'int'";
    }
}

// Test 6: IncrementalParsing - test parse_incremental (simple edit)
TEST(TreeSitterParserTest, IncrementalParsing) {
    TreeSitterParser parser;
    std::string old_source = "int x = 10;";

    auto old_tree = parser.parse_string(old_source);
    ASSERT_NE(old_tree, nullptr);
    ASSERT_FALSE(old_tree->has_error());

    // Create a simple edit: change "10" to "20"
    std::string new_source = "int x = 20;";

    // Define the edit: position 8, delete 2 chars ("10"), insert 2 chars ("20")
    TSInputEdit edit;
    edit.start_byte = 8;
    edit.old_end_byte = 10;
    edit.new_end_byte = 10;
    edit.start_point = {0, 8};
    edit.old_end_point = {0, 10};
    edit.new_end_point = {0, 10};

    auto new_tree = parser.parse_incremental(*old_tree, new_source, edit);

    ASSERT_NE(new_tree, nullptr) << "Incremental parse should return a valid tree";
    EXPECT_FALSE(new_tree->has_error()) << "Incremental parse should succeed without errors";

    // Verify the new source was cached
    EXPECT_EQ(parser.last_source(), new_source) << "New source should be cached";
}
