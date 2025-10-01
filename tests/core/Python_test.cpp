#include <gtest/gtest.h>
#include "core/TreeSitterParser.hpp"
#include "core/QueryEngine.hpp"
#include "core/ASTAnalyzer.hpp"
#include "core/Language.hpp"
#include <filesystem>

using namespace ts_mcp;

// Test 1: Parse simple Python class
TEST(PythonTest, ParseSimpleClass) {
    TreeSitterParser parser(Language::PYTHON);
    std::filesystem::path fixture_path = "../fixtures/simple_class.py";

    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Fixture file should exist at " << fixture_path;

    auto tree = parser.parse_file(fixture_path);
    ASSERT_NE(tree, nullptr) << "Should successfully parse Python file";
    EXPECT_FALSE(tree->has_error()) << "Parse tree should not have errors";
}

// Test 2: Find Python classes
TEST(PythonTest, FindClasses) {
    TreeSitterParser parser(Language::PYTHON);
    QueryEngine engine;
    std::filesystem::path fixture_path = "../fixtures/simple_class.py";

    auto tree = parser.parse_file(fixture_path);
    ASSERT_NE(tree, nullptr);

    auto query_str = QueryEngine::get_predefined_query(QueryType::CLASSES, Language::PYTHON);
    ASSERT_TRUE(query_str.has_value()) << "Python should support CLASSES query";

    auto query = engine.compile_query(*query_str, Language::PYTHON);
    ASSERT_NE(query, nullptr) << "Query should compile successfully";

    auto matches = engine.execute(*tree, *query, parser.last_source());

    // simple_class.py has Calculator and ScientificCalculator
    EXPECT_GE(matches.size(), 2u) << "Should find at least 2 classes";

    // Check for Calculator class
    bool found_calculator = false;
    for (const auto& match : matches) {
        if (match.text == "Calculator") {
            found_calculator = true;
            EXPECT_EQ(match.capture_name, "class_name");
            break;
        }
    }
    EXPECT_TRUE(found_calculator) << "Should find Calculator class";
}

// Test 3: Find Python functions
TEST(PythonTest, FindFunctions) {
    TreeSitterParser parser(Language::PYTHON);
    QueryEngine engine;
    std::filesystem::path fixture_path = "../fixtures/simple_class.py";

    auto tree = parser.parse_file(fixture_path);
    ASSERT_NE(tree, nullptr);

    auto query_str = QueryEngine::get_predefined_query(QueryType::FUNCTIONS, Language::PYTHON);
    ASSERT_TRUE(query_str.has_value()) << "Python should support FUNCTIONS query";

    auto query = engine.compile_query(*query_str, Language::PYTHON);
    ASSERT_NE(query, nullptr);

    auto matches = engine.execute(*tree, *query, parser.last_source());

    // Should find __init__, add, subtract, multiply, divide, power, square_root
    EXPECT_GE(matches.size(), 7u) << "Should find at least 7 functions";
}

// Test 4: Find Python decorators
TEST(PythonTest, FindDecorators) {
    TreeSitterParser parser(Language::PYTHON);
    QueryEngine engine;
    std::filesystem::path fixture_path = "../fixtures/with_decorators.py";

    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Decorator fixture should exist";

    auto tree = parser.parse_file(fixture_path);
    ASSERT_NE(tree, nullptr);

    auto query_str = QueryEngine::get_predefined_query(QueryType::DECORATORS, Language::PYTHON);
    ASSERT_TRUE(query_str.has_value()) << "Python should support DECORATORS query";

    auto query = engine.compile_query(*query_str, Language::PYTHON);
    ASSERT_NE(query, nullptr);

    auto matches = engine.execute(*tree, *query, parser.last_source());

    // with_decorators.py has @timer, @wraps, @retry, @staticmethod, @classmethod, @property
    EXPECT_GE(matches.size(), 6u) << "Should find at least 6 decorators";
}

// Test 5: Find async functions
TEST(PythonTest, FindAsyncFunctions) {
    TreeSitterParser parser(Language::PYTHON);
    QueryEngine engine;
    std::filesystem::path fixture_path = "../fixtures/async_example.py";

    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Async fixture should exist";

    auto tree = parser.parse_file(fixture_path);
    ASSERT_NE(tree, nullptr);

    auto query_str = QueryEngine::get_predefined_query(QueryType::ASYNC_FUNCTIONS, Language::PYTHON);
    ASSERT_TRUE(query_str.has_value()) << "Python should support ASYNC_FUNCTIONS query";

    auto query = engine.compile_query(*query_str, Language::PYTHON);
    ASSERT_NE(query, nullptr);

    auto matches = engine.execute(*tree, *query, parser.last_source());

    // async_example.py has multiple async functions
    EXPECT_GE(matches.size(), 5u) << "Should find at least 5 async functions";
}

// Test 6: Find Python imports
TEST(PythonTest, FindImports) {
    TreeSitterParser parser(Language::PYTHON);
    QueryEngine engine;
    std::filesystem::path fixture_path = "../fixtures/with_imports.py";

    ASSERT_TRUE(std::filesystem::exists(fixture_path))
        << "Imports fixture should exist";

    auto tree = parser.parse_file(fixture_path);
    ASSERT_NE(tree, nullptr);

    auto query_str = QueryEngine::get_predefined_query(QueryType::INCLUDES, Language::PYTHON);
    ASSERT_TRUE(query_str.has_value()) << "Python should support INCLUDES query";

    auto query = engine.compile_query(*query_str, Language::PYTHON);
    ASSERT_NE(query, nullptr);

    auto matches = engine.execute(*tree, *query, parser.last_source());

    // with_imports.py has multiple import statements
    EXPECT_GE(matches.size(), 6u) << "Should find at least 6 import statements";
}

// Test 7: Language auto-detection
TEST(PythonTest, LanguageAutoDetection) {
    ASTAnalyzer analyzer;
    std::filesystem::path py_file = "../fixtures/simple_class.py";

    auto result = analyzer.analyze_file(py_file);

    ASSERT_TRUE(result["success"].get<bool>()) << "Should successfully analyze Python file";
    EXPECT_EQ(result["language"].get<std::string>(), "python")
        << "Should auto-detect Python language";
    EXPECT_GE(result["class_count"].get<int>(), 2)
        << "Should find at least 2 classes";
    EXPECT_GE(result["function_count"].get<int>(), 7)
        << "Should find at least 7 functions";
}

// Test 8: C++ vs Python language distinction
TEST(PythonTest, LanguageDistinction) {
    TreeSitterParser cpp_parser(Language::CPP);
    TreeSitterParser py_parser(Language::PYTHON);

    EXPECT_EQ(cpp_parser.language(), Language::CPP);
    EXPECT_EQ(py_parser.language(), Language::PYTHON);
}

// Test 9: Query type not supported for language
TEST(PythonTest, UnsupportedQueryType) {
    // Virtual functions are C++ specific
    auto query_str = QueryEngine::get_predefined_query(QueryType::VIRTUAL_FUNCTIONS, Language::PYTHON);
    EXPECT_FALSE(query_str.has_value())
        << "Python should not support VIRTUAL_FUNCTIONS query";

    // Decorators are Python specific
    auto cpp_decorator_query = QueryEngine::get_predefined_query(QueryType::DECORATORS, Language::CPP);
    EXPECT_FALSE(cpp_decorator_query.has_value())
        << "C++ should not support DECORATORS query";
}
