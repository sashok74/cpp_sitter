# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **cpp-treesitter-mcp** (C++ Tree-sitter MCP Server) - a high-performance MCP (Model Context Protocol) server written in C++20 that provides deep C++ code analysis capabilities using tree-sitter. It integrates with Claude Code CLI as a specialized sub-agent for static code analysis.

**Key Technologies:**
- C++20 (requires GCC 11+, Clang 14+, or MSVC 19.30+)
- Tree-sitter 0.22.6 with C++ grammar
- CMake 3.20+ build system
- Conan 2.x package manager
- MCP Protocol (stdio and optional SSE transports)

## Development Commands

### Initial Setup
```bash
# Detect Conan profile (first time only)
conan profile detect --force

# Install dependencies
conan install . --output-folder=build --build=missing

# Configure with CMake
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug

# Build project
cmake --build .
```

### Testing
```bash
# Run all tests
cd build
ctest --output-on-failure

# Run tests with verbose output
ctest -V

# Run specific test suite
./core_tests                    # Core tree-sitter tests
./mcp_tests                     # MCP protocol tests
./tools_tests                   # MCP tools tests

# Run integration tests
./tests/integration/EndToEnd_test
./tests/integration/test_claude_integration.sh
```

### Building Components
```bash
# Build only core library
cmake --build build --target ts_mcp_core

# Build stdio server
cmake --build build --target mcp_stdio_server

# Build SSE server (if enabled)
cmake --build build --target mcp_sse_server

# Parallel build
cmake --build build -j$(nproc)
```

### Manual Testing of MCP Server
```bash
# Test stdio transport directly
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | ./build/mcp_stdio_server

# Test with log level
./build/mcp_stdio_server --log-level debug < test_request.json

# Check available options
./build/mcp_stdio_server --help
```

### Installation
```bash
# Install to system (requires sudo)
cd build
sudo cmake --install .

# Setup Claude Code integration
./install_claude_agent.sh

# Verify installation
which mcp_stdio_server
ls ~/.claude/agents/ts-strategist.json
```

## Architecture Overview

### Three-Layer Architecture

1. **Core Layer** (`src/core/`)
   - `TreeSitterParser`: RAII wrapper around tree-sitter C API for parsing C++ code
   - `QueryEngine`: Executes S-expression queries on AST, provides predefined queries
   - `ASTAnalyzer`: High-level API with file caching and JSON result serialization

2. **MCP Protocol Layer** (`src/mcp/`)
   - `MCPServer`: Main server implementing JSON-RPC 2.0 protocol
   - `ITransport`: Abstract interface for different transport mechanisms
   - `StdioTransport`: Standard input/output transport for Claude Code CLI integration
   - `SSETransport`: Optional HTTP/SSE transport for web-based clients

3. **Tools Layer** (`src/tools/`)
   - `ParseFileTool`: Parse C++ file and return metadata (classes, functions, errors)
   - `FindClassesTool`: Extract all class declarations with locations
   - `FindFunctionsTool`: Extract all function definitions
   - `ExecuteQueryTool`: Run arbitrary tree-sitter queries

### Key Design Patterns

#### RAII Resource Management
All tree-sitter resources (TSParser, TSTree, TSQuery) are managed through RAII wrappers:
```cpp
class TreeSitterParser {
private:
    TSParser* parser_;  // Automatically freed in destructor
    std::string last_source_;  // Cached for node_text operations
};
```

#### Type-Safe Tool Registration
Each tool provides static metadata and instance execution:
```cpp
class ParseFileTool {
public:
    static ToolInfo get_info();      // Returns JSON schema
    json execute(const json& args);   // Executes with validation
};
```

#### Caching Strategy
`ASTAnalyzer` implements file-level caching with mtime validation:
```cpp
struct CachedFile {
    std::unique_ptr<Tree> tree;
    std::string source;
    std::filesystem::file_time_type mtime;
};
```

### Tree-sitter Integration Details

**C API Usage with RAII:**
- Use `tree_sitter/api.h` directly (not C++ bindings)
- Wrap all `ts_*` resources in RAII classes with unique_ptr deleters
- Never use raw `new`/`delete` - always use smart pointers

**Query Syntax:**
Tree-sitter uses S-expression queries. Example for finding classes:
```scheme
(class_specifier
  name: (type_identifier) @class_name)
```

**Predefined Queries:**
See `QueryEngine::PredefinedQueries` for commonly used patterns:
- `ALL_CLASSES`: Find all class declarations
- `ALL_FUNCTIONS`: Find function definitions
- `VIRTUAL_FUNCTIONS`: Find virtual/override methods
- `INCLUDES`: Extract #include directives

### MCP Protocol Implementation

**JSON-RPC 2.0 Message Format:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {"filepath": "src/main.cpp"}
  }
}
```

**Supported Methods:**
- `tools/list`: Return available tools with JSON schemas
- `tools/call`: Execute a tool with arguments

**Transport Layer:**
- Stdio: One JSON message per line, reads from stdin, writes to stdout
- SSE: HTTP endpoints at `/sse` (stream) and `/messages` (requests)

## Build System Details

### CMake Target Structure
```
ts_mcp_core (library)
├── Depends: nlohmann_json, spdlog, tree-sitter
│
ts_mcp_protocol (library)
├── Depends: ts_mcp_core
│
ts_mcp_tools (library)
├── Depends: ts_mcp_core, ts_mcp_protocol
│
mcp_stdio_server (executable)
├── Depends: ts_mcp_tools, CLI11
│
mcp_sse_server (executable, optional)
├── Depends: ts_mcp_tools, CLI11, cpp-httplib
```

### Build Options
```cmake
-DBUILD_TESTS=ON/OFF           # Enable/disable unit tests (default: ON)
-DBUILD_STDIO_SERVER=ON/OFF    # Build stdio server (default: ON)
-DBUILD_SSE_SERVER=ON/OFF      # Build SSE server (default: ON)
-DENABLE_COVERAGE=ON/OFF       # Code coverage instrumentation (default: OFF)
```

### Compiler Requirements
- **Must support:** C++20 concepts, ranges
- **Optional:** std::format (falls back to fmt library)
- **Warning level:** `-Wall -Wextra -Wpedantic -Werror` (all warnings are errors)

## Code Style Guidelines

### Naming Conventions
- Classes: `PascalCase` (TreeSitterParser, QueryEngine)
- Functions/methods: `snake_case` (parse_string, node_text)
- Variables: `snake_case` (file_path, raw_tree)
- Constants: `UPPER_SNAKE_CASE` (ALL_CLASSES, MAX_CACHE_SIZE)
- Private members: `trailing_underscore_` (parser_, cache_)

### Error Handling
- **Initialization errors:** Throw exceptions
- **Expected failures:** Return `std::optional<T>` or `nullptr`
- **All errors:** Log via spdlog before returning/throwing
- **Never:** Silently swallow exceptions

### Memory Management
- **Always:** Use `std::unique_ptr`, `std::shared_ptr`
- **Never:** Use raw `new`/`delete`
- **Tree-sitter resources:** Wrap in RAII with custom deleters
- **Move semantics:** Support for all RAII wrappers, delete copy constructors

## Testing Requirements

### Coverage Target
- Minimum: 80% code coverage
- Run: `cmake --build build --target coverage` (if ENABLE_COVERAGE=ON)

### Test Organization
- `tests/core/`: Unit tests for TreeSitterParser, QueryEngine, ASTAnalyzer
- `tests/mcp/`: Unit tests for MCPServer and transports (uses MockTransport)
- `tests/tools/`: Unit tests for individual MCP tools
- `tests/integration/`: End-to-end tests with real server instances
- `tests/fixtures/`: Sample C++ files for testing (simple_class.cpp, template_class.cpp, etc.)

### Critical Test Scenarios
1. **Syntax errors:** Parser must handle invalid C++ gracefully
2. **Complex queries:** Template classes, nested namespaces, virtual inheritance
3. **Cache invalidation:** mtime changes must trigger re-parsing
4. **Incremental parsing:** Edits should use `ts_parser_parse_incremental`
5. **NULL handling:** Optional values must be properly propagated

## Claude Code Integration

### Sub-agent Configuration
The project installs a specialized sub-agent at `~/.claude/agents/ts-strategist.json` that can be invoked:
```bash
claude @ts-strategist "analyze src/core/TreeSitterParser.cpp"
claude @ts-strategist "find all virtual functions in src/"
```

### MCP Server Registration
Server is registered in `~/.config/claude/claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "cpp-treesitter": {
      "command": "/usr/local/bin/mcp_stdio_server",
      "args": ["--log-level", "info"]
    }
  }
}
```

### Available Tools
- `parse_file`: Get metadata (class count, function count, error status)
- `find_classes`: List all classes with line numbers
- `find_functions`: List all functions with signatures
- `execute_query`: Run custom tree-sitter S-expression query

## Performance Considerations

### Target Metrics
- Parse time: <100ms for files <10KB
- Query execution: <50ms per query
- Startup time: <500ms
- Memory: <100MB for 1000 file cache

### Optimization Strategies
- **File caching:** Avoid re-parsing unchanged files (check mtime)
- **Incremental parsing:** Use `TSInputEdit` for small changes
- **Query compilation:** Cache compiled TSQuery objects
- **Zero-copy:** Use `std::string_view` where possible

## Troubleshooting

### tree-sitter Not Found
Tree-sitter may not be in Conan Center. Solutions:
1. Use CMake FetchContent to download from GitHub
2. Build tree-sitter manually and set CMAKE_PREFIX_PATH
3. Create custom Conan recipe

### Tests Can't Find Fixtures
Ensure `tests/CMakeLists.txt` includes:
```cmake
file(COPY ${CMAKE_SOURCE_DIR}/tests/fixtures
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
```

### Memory Leaks
Run with valgrind:
```bash
valgrind --leak-check=full ./build/mcp_stdio_server < test_input.json
```

Common causes:
- Forgot to call `ts_tree_delete()` (use RAII wrapper)
- Forgot to call `ts_query_delete()` (use RAII wrapper)
- Circular `shared_ptr` references (use `weak_ptr`)
