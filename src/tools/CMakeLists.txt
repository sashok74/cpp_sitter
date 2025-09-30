# MCP Tools Library

add_library(ts_mcp_tools
    ParseFileTool.cpp
    FindClassesTool.cpp
    FindFunctionsTool.cpp
    ExecuteQueryTool.cpp
)

target_include_directories(ts_mcp_tools
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_mcp_tools
    PUBLIC
        ts_mcp_core
        ts_mcp_protocol
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_compile_features(ts_mcp_tools PUBLIC cxx_std_20)
