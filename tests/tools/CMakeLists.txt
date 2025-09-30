# Tools Tests

add_executable(tools_tests
    Tools_test.cpp
)

target_include_directories(tools_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(tools_tests
    PRIVATE
        ts_mcp_tools
        GTest::gtest
        GTest::gtest_main
)

target_compile_features(tools_tests PRIVATE cxx_std_20)

# Add to CTest
add_test(NAME ToolsTests COMMAND tools_tests)
