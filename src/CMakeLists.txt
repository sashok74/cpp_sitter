# Subdirectories
add_subdirectory(core)
add_subdirectory(mcp)
add_subdirectory(tools)

# MCP Stdio Server executable
add_executable(mcp_stdio_server main_stdio.cpp)

target_link_libraries(mcp_stdio_server
    PRIVATE
        ts_mcp_tools
        CLI11::CLI11
        spdlog::spdlog
)

target_compile_features(mcp_stdio_server PRIVATE cxx_std_20)

# Installation
install(TARGETS mcp_stdio_server
    RUNTIME DESTINATION bin
)
