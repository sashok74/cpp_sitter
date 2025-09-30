# MCP Protocol Library

add_library(ts_mcp_protocol
    MCPServer.cpp
    StdioTransport.cpp
)

target_include_directories(ts_mcp_protocol
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_mcp_protocol
    PUBLIC
        ts_mcp_core
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_compile_features(ts_mcp_protocol PUBLIC cxx_std_20)
