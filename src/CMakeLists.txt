# Subdirectories
add_subdirectory(core)
add_subdirectory(mcp)
add_subdirectory(tools)

# MCP Stdio Server executable
add_executable(tree-sitter-mcp main_stdio.cpp)

target_link_libraries(tree-sitter-mcp
    PRIVATE
        ts_mcp_tools
        CLI11::CLI11
        spdlog::spdlog
)

target_compile_features(tree-sitter-mcp PRIVATE cxx_std_20)

# Installation
install(TARGETS tree-sitter-mcp
    RUNTIME DESTINATION bin
)
