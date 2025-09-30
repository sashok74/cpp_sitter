# Core component unit tests

add_executable(core_tests
    TreeSitterParser_test.cpp
    QueryEngine_test.cpp
    ASTAnalyzer_test.cpp
)

target_link_libraries(core_tests
    PRIVATE
        ts_mcp_core
        test_helpers
        GTest::gtest
        GTest::gtest_main
)

# Discover and register tests with CTest
gtest_discover_tests(core_tests)
