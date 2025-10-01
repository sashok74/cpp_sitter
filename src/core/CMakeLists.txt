# Core library for tree-sitter parsing and analysis

add_library(ts_mcp_core STATIC
    TreeSitterParser.cpp
    QueryEngine.cpp
    ASTAnalyzer.cpp
    PathResolver.cpp
    Language.cpp
)

target_include_directories(ts_mcp_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ts_mcp_core
    PUBLIC
        tree-sitter::tree-sitter
        tree-sitter::cpp
        tree-sitter::python
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_compile_features(ts_mcp_core PUBLIC cxx_std_20)

# Add additional warnings
if(MSVC)
    target_compile_options(ts_mcp_core PRIVATE /W4 /WX)
else()
    target_compile_options(ts_mcp_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
