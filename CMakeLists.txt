cmake_minimum_required(VERSION 3.20)
project(tree-sitter-mcp VERSION 1.0.0 LANGUAGES C CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Conan integration
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_STDIO_SERVER "Build stdio MCP server" ON)
option(BUILD_SSE_SERVER "Build SSE MCP server" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Find dependencies through Conan
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)

if(BUILD_SSE_SERVER)
    find_package(httplib REQUIRED)
endif()

if(BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
endif()

# Tree-sitter via FetchContent
include(FetchContent)

FetchContent_Declare(
    tree-sitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG v0.22.6
)

FetchContent_Declare(
    tree-sitter-cpp
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
    GIT_TAG v0.22.0
)

FetchContent_MakeAvailable(tree-sitter tree-sitter-cpp)

# Build tree-sitter library manually (no CMakeLists.txt in upstream)
file(GLOB TREE_SITTER_SOURCES
    "${tree-sitter_SOURCE_DIR}/lib/src/*.c"
)
list(FILTER TREE_SITTER_SOURCES EXCLUDE REGEX "lib.c$")

add_library(tree-sitter STATIC ${TREE_SITTER_SOURCES})
target_include_directories(tree-sitter
    PUBLIC
        ${tree-sitter_SOURCE_DIR}/lib/include
    PRIVATE
        ${tree-sitter_SOURCE_DIR}/lib/src
        ${tree-sitter_SOURCE_DIR}/lib/src/wasm
)
target_compile_definitions(tree-sitter PRIVATE _POSIX_C_SOURCE=200112L)
set_target_properties(tree-sitter PROPERTIES
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
)

# Build tree-sitter-cpp grammar
add_library(tree-sitter-cpp STATIC
    ${tree-sitter-cpp_SOURCE_DIR}/src/parser.c
    ${tree-sitter-cpp_SOURCE_DIR}/src/scanner.c
)
target_include_directories(tree-sitter-cpp
    PUBLIC
        ${tree-sitter-cpp_SOURCE_DIR}/src
)
target_link_libraries(tree-sitter-cpp PUBLIC tree-sitter)
set_target_properties(tree-sitter-cpp PROPERTIES
    C_STANDARD 11
    POSITION_INDEPENDENT_CODE ON
)

# Create alias for easier linking
add_library(tree-sitter::tree-sitter ALIAS tree-sitter)
add_library(tree-sitter::cpp ALIAS tree-sitter-cpp)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Coverage
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

# Subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Configure install script
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/install_claude_agent.sh.in
    ${CMAKE_BINARY_DIR}/scripts/install_claude_agent.sh
    @ONLY
)

# Install scripts
install(PROGRAMS ${CMAKE_BINARY_DIR}/scripts/install_claude_agent.sh
    DESTINATION share/tree-sitter-mcp
)

# Installation (will be enabled after targets are created)
# if(BUILD_STDIO_SERVER AND TARGET mcp_stdio_server)
#     install(TARGETS mcp_stdio_server
#         RUNTIME DESTINATION bin
#     )
# endif()
#
# if(BUILD_SSE_SERVER AND TARGET mcp_sse_server)
#     install(TARGETS mcp_sse_server
#         RUNTIME DESTINATION bin
#     )
# endif()
#
# install(DIRECTORY claude/
#     DESTINATION share/tree-sitter-mcp/claude
# )
#
# # Configure installation script
# configure_file(
#     ${CMAKE_SOURCE_DIR}/scripts/install_claude_agent.sh.in
#     ${CMAKE_BINARY_DIR}/install_claude_agent.sh
#     @ONLY
# )
#
# install(PROGRAMS ${CMAKE_BINARY_DIR}/install_claude_agent.sh
#     DESTINATION bin
# )
