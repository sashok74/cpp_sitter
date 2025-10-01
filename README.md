# tree-sitter-mcp

A high-performance MCP (Model Context Protocol) server written in C++20 that provides multi-language code analysis (C++ and Python) using tree-sitter. Designed to integrate seamlessly with Claude Code CLI as a specialized sub-agent for static code analysis.

## Features

- **Multi-Language Support**: C++ and Python parsing with automatic language detection
- **Tree-sitter Powered Parsing**: Robust code parsing with syntax error detection
- **MCP Protocol Support**: JSON-RPC 2.0 over stdio for Claude Code CLI integration
- **Four Specialized Tools**:
  - `parse_file`: Get metadata (class/function counts, error status, language)
  - `find_classes`: Extract all class declarations with locations
  - `find_functions`: Extract all function definitions (including async functions for Python)
  - `execute_query`: Run custom tree-sitter S-expression queries
- **Language-Specific Queries**:
  - **C++**: classes, functions, virtual functions, includes, namespaces, structs, templates
  - **Python**: classes, functions, decorators, async functions, imports
- **Batch Processing**: Analyze multiple files or entire directories with recursive scanning
- **Smart Caching**: File-level caching with mtime validation for performance
- **Type-Safe Design**: Modern C++20 with RAII, smart pointers, and strong typing
- **Comprehensive Testing**: 42 unit/integration tests with 100% pass rate (33 C++ + 9 Python)

## Quick Start

### Prerequisites

- C++20 compiler (GCC 11+, Clang 14+, or MSVC 19.30+)
- CMake 3.20+
- Conan 2.x package manager
- Claude Code CLI (optional, for integration)

### Installation

```bash
# Clone and build
git clone https://github.com/yourusername/tree-sitter-mcp.git
cd tree-sitter-mcp

# Setup Conan profile (first time only)
conan profile detect --force

# Install dependencies
conan install . --output-folder=build --build=missing

# Configure and build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure

# Install (requires sudo)
sudo cmake --install .

# Configure Claude Code integration
bash /usr/local/share/tree-sitter-mcp/install_claude_agent.sh
```

### Quick Test

```bash
# Test the server directly
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level error

# Test with Claude Code (C++)
claude @ts-strategist "analyze src/core/TreeSitterParser.cpp"

# Test with Claude Code (Python)
claude @ts-strategist "analyze tests/fixtures/simple_class.py"
```

## Architecture

### Three-Layer Design

```
┌─────────────────────────────────────────────────────────┐
│  MCP Tools Layer (src/tools/)                           │
│  ├─ ParseFileTool                                       │
│  ├─ FindClassesTool                                     │
│  ├─ FindFunctionsTool                                   │
│  └─ ExecuteQueryTool                                    │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  MCP Protocol Layer (src/mcp/)                          │
│  ├─ MCPServer (JSON-RPC 2.0)                           │
│  ├─ ITransport (abstract)                              │
│  └─ StdioTransport (stdin/stdout)                      │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  Core Analysis Layer (src/core/)                        │
│  ├─ TreeSitterParser (RAII wrapper)                    │
│  ├─ QueryEngine (S-expression queries)                 │
│  └─ ASTAnalyzer (high-level API with caching)          │
└─────────────────────────────────────────────────────────┘
```

### Key Design Patterns

- **RAII Resource Management**: All tree-sitter resources wrapped in smart pointers
- **Type-Safe Tool Registration**: Static metadata + instance execution
- **File-Level Caching**: Avoid re-parsing unchanged files (mtime validation)
- **Zero-Copy where Possible**: `std::string_view` for node text extraction

## Available Tools

### 1. parse_file

Get high-level metadata about C++/Python file(s) or directory.

**Single file:**
```json
{
  "name": "parse_file",
  "arguments": {
    "filepath": "src/main.cpp"
  }
}
```

**Multiple files:**
```json
{
  "name": "parse_file",
  "arguments": {
    "filepath": ["src/main.cpp", "src/utils.py"]
  }
}
```

**Directory (recursive):**
```json
{
  "name": "parse_file",
  "arguments": {
    "filepath": "src/",
    "recursive": true,
    "file_patterns": ["*.cpp", "*.hpp", "*.py"]
  }
}
```

**Parameters:**
- `filepath`: String or array of strings (file paths or directories)
- `recursive`: Boolean, default `true` (scan directories recursively)
- `file_patterns`: Array of glob patterns, default `["*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"]`

**Returns:**
- Single file: `{class_count, function_count, include_count, has_errors, language, success}`
- Multiple files: `{total_files, processed_files, failed_files, results: [...]}`

### 2. find_classes

List all class declarations with line numbers. Supports single file, multiple files, or directories.

```json
{
  "name": "find_classes",
  "arguments": {
    "filepath": "src/MyClass.cpp"
  }
}
```

**Array/directory support:** Same as `parse_file` (supports arrays, `recursive`, `file_patterns`)

**Returns:**
- Single file: `{classes: [{name, line, column}, ...], success}`
- Multiple files: `{total_files, results: [{filepath, classes: [...]}]}`

### 3. find_functions

List all function definitions with line numbers. Supports single file, multiple files, or directories.

```json
{
  "name": "find_functions",
  "arguments": {
    "filepath": "src/utils.cpp"
  }
}
```

**Array/directory support:** Same as `parse_file` (supports arrays, `recursive`, `file_patterns`)

**Returns:**
- Single file: `{functions: [{name, line, column}, ...], success}`
- Multiple files: `{total_files, results: [{filepath, functions: [...]}]}`

### 4. execute_query

Run custom tree-sitter S-expression query. Supports single file, multiple files, or directories.

```json
{
  "name": "execute_query",
  "arguments": {
    "filepath": "src/Base.cpp",
    "query": "(class_specifier name: (type_identifier) @class_name)"
  }
}
```

**Array/directory support:** Same as `parse_file` (supports arrays, `recursive`, `file_patterns`)

**Returns:**
- Single file: `{matches: [{capture_name, text, line, column}, ...], success}`
- Multiple files: `{total_files, results: [{filepath, matches: [...]}]}`

## Usage Examples

### With Claude Code CLI

```bash
# Analyze C++ files
claude @ts-strategist "What classes are defined in src/core/TreeSitterParser.cpp?"
claude @ts-strategist "Find all virtual functions in src/core/ and src/mcp/"

# Analyze Python files
claude @ts-strategist "Find all async functions in tests/fixtures/async_example.py"
claude @ts-strategist "What decorators are used in tests/fixtures/with_decorators.py?"

# Mixed language analysis
claude @ts-strategist "Find all classes in src/ and tests/"
claude @ts-strategist "Are there any factory patterns in src/tools/?"
```

### Direct MCP Protocol

```bash
# Parse single file
echo '{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {"filepath": "test.cpp"}
  }
}' | tree-sitter-mcp --log-level error

# Parse multiple files
echo '{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": ["src/a.cpp", "src/b.cpp"]
    }
  }
}' | tree-sitter-mcp --log-level error

# Scan directory recursively
echo '{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "find_classes",
    "arguments": {
      "filepath": "src/",
      "recursive": true,
      "file_patterns": ["*.hpp"]
    }
  }
}' | tree-sitter-mcp --log-level error
```

### Tree-sitter Query Examples

**C++ Queries:**
```scheme
# Find all classes
(class_specifier name: (type_identifier) @class_name)

# Find virtual functions
(function_definition (virtual_function_specifier) declarator: (function_declarator) @func)

# Find template classes
(template_declaration (class_specifier) @template_class)

# Find inheritance
(base_class_clause (type_identifier) @base)

# Find includes
(preproc_include path: (_) @include_path)
```

**Python Queries:**
```scheme
# Find all classes
(class_definition name: (identifier) @class_name)

# Find async functions
(function_definition "async" @async_keyword name: (identifier) @async_func_name)

# Find decorators
(decorator) @decorator

# Find imports
(import_statement) @import
(import_from_statement) @import_from
```

## Documentation

### Installation Guides

- **[Claude Code CLI Setup](doc/INSTALL_CLAUDE_CLI.md)** - Connect to Claude CLI (`claude @ts-strategist`)
- **[Claude Code Desktop Setup](doc/INSTALL_CLAUDE_MCP.md)** - Connect to Claude Desktop app
- **[Quick Start (Russian)](doc/quick-start-ru.md)** - Полный гайд на русском языке
- **[Build Instructions](BUILD.md)** - Detailed build, CMake options, troubleshooting

### API Documentation

- **[MCP API Reference](doc/MCP_API_REFERENCE.md)** - Complete API with request/response examples
- **[Technical Specification](doc/tz.md)** - Architecture and implementation details

## Building from Source

See [BUILD.md](BUILD.md) for detailed build instructions, CMake options, and troubleshooting.

## Development

### Running Tests

```bash
cd build

# All tests
ctest --output-on-failure

# Specific test suite
./tests/core/core_tests
./tests/mcp/mcp_tests
./tests/tools/tools_tests
./tests/integration/integration_tests

# Integration script
bash ../tests/integration/test_claude_integration.sh
```

### Code Coverage

```bash
cmake .. -DENABLE_COVERAGE=ON
cmake --build .
ctest
cmake --build . --target coverage
```

### Project Structure

```
tree-sitter-mcp/
├── src/
│   ├── core/              # Tree-sitter parsing layer
│   ├── mcp/               # MCP protocol implementation
│   ├── tools/             # MCP tool implementations
│   └── main_stdio.cpp     # Server entry point
├── tests/
│   ├── core/              # Core unit tests
│   ├── mcp/               # Protocol unit tests
│   ├── tools/             # Tool unit tests
│   ├── integration/       # End-to-end tests
│   └── fixtures/          # Test C++ and Python files
├── claude/
│   └── agents/            # Sub-agent configurations
├── scripts/               # Installation scripts
└── doc/                   # Documentation

```

## Performance

Target metrics (achieved):
- Parse time: <100ms for files <10KB
- Query execution: <50ms per query
- Startup time: <500ms
- Memory: <100MB for 1000 file cache

## Troubleshooting

### Server not responding

```bash
# Check if server is executable
which tree-sitter-mcp

# Test manually
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp
```

### Claude Code not finding tools

```bash
# Check config
cat ~/.config/claude/claude_desktop_config.json

# Re-run install script
bash /usr/local/share/tree-sitter-mcp/install_claude_agent.sh
```

### Build errors

See [BUILD.md](BUILD.md) for detailed troubleshooting.

## Contributing

Contributions welcome! Please:
1. Follow existing code style (see `.clang-format`)
2. Add tests for new features
3. Update documentation
4. Ensure all tests pass: `ctest --output-on-failure`

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- [tree-sitter](https://github.com/tree-sitter/tree-sitter) for the parsing engine
- [tree-sitter-cpp](https://github.com/tree-sitter/tree-sitter-cpp) for C++ grammar
- [tree-sitter-python](https://github.com/tree-sitter/tree-sitter-python) for Python grammar
- [Anthropic](https://www.anthropic.com) for the Claude Code platform and MCP protocol
- [nlohmann/json](https://github.com/nlohmann/json) for JSON handling
- [gabime/spdlog](https://github.com/gabime/spdlog) for logging
- [CLIUtils/CLI11](https://github.com/CLIUtils/CLI11) for argument parsing
