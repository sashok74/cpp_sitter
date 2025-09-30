# Integration Tests

add_executable(integration_tests
    EndToEnd_test.cpp
)

target_include_directories(integration_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(integration_tests
    PRIVATE
        ts_mcp_tools
        GTest::gtest
        GTest::gtest_main
)

target_compile_features(integration_tests PRIVATE cxx_std_20)

# Add to CTest
add_test(NAME IntegrationTests COMMAND integration_tests)

# Add shell script test
add_test(
    NAME ClaudeIntegrationScript
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/integration/test_claude_integration.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
