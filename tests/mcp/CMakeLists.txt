# MCP Server Tests

add_executable(mcp_tests
    MCPServer_test.cpp
    MockTransport.cpp
)

target_include_directories(mcp_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(mcp_tests
    PRIVATE
        ts_mcp_protocol
        GTest::gtest
        GTest::gtest_main
)

target_compile_features(mcp_tests PRIVATE cxx_std_20)

# Add to CTest
add_test(NAME MCPServerTests COMMAND mcp_tests)
