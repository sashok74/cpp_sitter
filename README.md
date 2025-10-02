# tree-sitter-mcp

A high-performance MCP (Model Context Protocol) server written in C++20 that provides multi-language code analysis (C++ and Python) using tree-sitter. Designed to integrate seamlessly with Claude Code CLI as a specialized sub-agent for static code analysis.

## Features

- **Multi-Language Support**: C++ and Python parsing with automatic language detection
- **Tree-sitter Powered Parsing**: Robust code parsing with syntax error detection
- **MCP Protocol Support**: JSON-RPC 2.0 over stdio for Claude Code CLI integration
- **Ten Specialized Tools**:
  - `parse_file`: Get metadata (class/function counts, error status, language)
  - `find_classes`: Extract all class declarations with locations
  - `find_functions`: Extract all function definitions (including async functions for Python)
  - `execute_query`: Run custom tree-sitter S-expression queries
  - `extract_interface`: Extract function signatures and class interfaces without implementation bodies (reduces AI context size)
  - `find_references`: Find all references to a symbol with AST-based classification (call, declaration, definition)
  - `get_file_summary`: Enhanced file analysis with cyclomatic complexity, TODO/FIXME extraction, full function signatures, and code metrics
  - `get_class_hierarchy`: Analyze C++ class inheritance with virtual methods, abstract classes, and full hierarchy trees
  - `get_dependency_graph`: Build #include dependency graphs with cycle detection, topological sorting, and visualization (JSON/Mermaid/DOT)
  - `get_symbol_context`: Get comprehensive context for a symbol (function/class/method) including definition and direct dependencies
- **Language-Specific Queries**:
  - **C++**: classes, functions, virtual functions, includes, namespaces, structs, templates
  - **Python**: classes, functions, decorators, async functions, imports
- **Batch Processing**: Analyze multiple files or entire directories with recursive scanning
- **Smart Caching**: File-level caching with mtime validation for performance
- **Type-Safe Design**: Modern C++20 with RAII, smart pointers, and strong typing
- **Comprehensive Testing**: 55 unit/integration tests with 100% pass rate (46 C++ + 9 Python)

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

### 5. extract_interface

Extract function signatures and class interfaces without implementation bodies. Useful for reducing context size when sending code to AI models.

```json
{
  "name": "extract_interface",
  "arguments": {
    "filepath": "src/MyClass.cpp",
    "output_format": "json",
    "include_private": false,
    "include_comments": true
  }
}
```

**Parameters:**
- `filepath`: String or array of strings (file paths or directories)
- `output_format`: `"json"` (structured), `"header"` (.hpp format), or `"markdown"` (documentation) - default: `"json"`
- `include_private`: Include private class members - default: `false`
- `include_comments`: Include function comments/docstrings - default: `true`
- `recursive`: Recursively scan directories - default: `true`
- `file_patterns`: Array of glob patterns - default: `["*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"]`

**Returns (JSON format):**
- Single file: `{functions: [{signature, line, comment?}, ...], classes: [{name, line, methods: [...], members: [...]}], filepath, success}`
- Multiple files: `{total_files, processed_files, failed_files, results: [...]}`

**Returns (header format):**
- `{filepath, format: "header", content: "...", success}`

**Returns (markdown format):**
- `{filepath, format: "markdown", content: "...", success}`

### 6. find_references

Find all references to a symbol (function, class, variable) in codebase with intelligent AST-based validation.

```json
{
  "name": "find_references",
  "arguments": {
    "symbol": "MyClass",
    "filepath": "src/",
    "recursive": true
  }
}
```

**Parameters:**
- `symbol`: Symbol name to search for (required)
- `filepath`: String or array of strings (file paths or directories) - default: current directory
- `reference_types`: Array of types to filter: `["call", "declaration", "definition", "member_access", "type_usage", "all"]` - default: `["all"]`
- `include_context`: Include code context and parent scope - default: `true`
- `recursive`: Recursively scan directories - default: `true`
- `file_patterns`: Array of glob patterns - default: `["*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"]`

**Returns:**
```json
{
  "symbol": "MyClass",
  "total_references": 15,
  "files_searched": 42,
  "files_processed": 40,
  "files_failed": 2,
  "references": [
    {
      "filepath": "src/main.cpp",
      "line": 42,
      "column": 10,
      "type": "call",
      "context": "MyClass obj;",
      "parent_scope": "main",
      "node_type": "identifier"
    }
  ],
  "success": true
}
```

**Reference Types:**
- `call`: Function call or constructor usage
- `declaration`: Variable/parameter declaration
- `definition`: Function/class definition
- `member_access`: Class member access (obj.member)
- `type_usage`: Type in declaration (MyClass var;)

### 7. get_file_summary

Enhanced file analysis with comprehensive metrics and detailed code insights.

```json
{
  "name": "get_file_summary",
  "arguments": {
    "filepath": "src/MyClass.cpp",
    "include_complexity": true,
    "include_comments": true,
    "include_docstrings": true
  }
}
```

**Parameters:**
- `filepath`: String or array of strings (file paths or directories)
- `include_complexity`: Calculate cyclomatic complexity for functions - default: `true`
- `include_comments`: Extract TODO/FIXME/HACK markers - default: `true`
- `include_docstrings`: Include function documentation - default: `true`
- `recursive`: Recursively scan directories - default: `true`
- `file_patterns`: Array of glob patterns - default: `[\"*.cpp\", \"*.hpp\", \"*.h\", \"*.cc\", \"*.cxx\", \"*.py\"]`

**Returns (single file):**
```json
{
  "filepath": "src/MyClass.cpp",
  "language": "cpp",
  "success": true,
  "metrics": {
    "total_lines": 150,
    "code_lines": 95,
    "comment_lines": 30,
    "blank_lines": 25
  },
  "functions": [
    {
      "name": "calculate",
      "return_type": "int",
      "line": 42,
      "complexity": 5,
      "parameters": [
        {"type": "int", "name": "x"},
        {"type": "int", "name": "y"}
      ],
      "docstring": "Calculate sum of two integers",
      "is_virtual": false,
      "is_static": false
    }
  ],
  "function_count": 3,
  "classes": [
    {"name": "MyClass", "line": 10}
  ],
  "class_count": 1,
  "imports": [
    {"path": "iostream", "line": 1, "is_system": true},
    {"path": "MyClass.hpp", "line": 2, "is_system": false}
  ],
  "comment_markers": [
    {
      "type": "TODO",
      "text": "Refactor this method",
      "line": 55,
      "context": "// TODO: Refactor this method"
    }
  ]
}
```

**Returns (multiple files):**
- `{total_files, processed_files, failed_files, results: [...]}`

**Features:**
- **Cyclomatic Complexity**: Measures code complexity based on decision points (if, for, while, switch, logical operators)
- **Comment Marker Extraction**: Finds TODO, FIXME, HACK, NOTE, WARNING, BUG, OPTIMIZE markers
- **Full Function Signatures**: Extracts return types, parameter types and names
- **Code Metrics**: Lines of code (LOC), source lines (SLOC), comment lines, blank lines
- **Import Analysis**: Distinguishes between system (<>) and user ("") includes
- **Docstring Extraction**: C++ comment blocks and Python docstrings
- **Member Variables**: Type and access level information (C++)
- **Python Support**: Includes is_async flag for async functions

### 8. get_class_hierarchy

Analyze C++ class inheritance hierarchies with comprehensive OOP structure analysis.

```json
{
  "name": "get_class_hierarchy",
  "arguments": {
    "filepath": "src/",
    "class_name": "Base",
    "show_methods": true,
    "max_depth": 3
  }
}
```

**Parameters:**
- `filepath`: String or array of strings (file paths or directories)
- `class_name` (optional): Focus on specific class hierarchy
- `show_methods`: Include method information - default: `true`
- `show_virtual_only`: Only show virtual methods - default: `false`
- `max_depth`: Maximum hierarchy depth, -1 for unlimited - default: `-1`
- `recursive`, `file_patterns`: Standard parameters

**Returns:**
```json
{
  "classes": [
    {
      "name": "Derived",
      "line": 20,
      "file": "derived.hpp",
      "base_classes": ["Base1", "Base2"],
      "is_abstract": false,
      "virtual_methods": [
        {
          "name": "process",
          "signature": "void process() override",
          "line": 25,
          "is_pure_virtual": false,
          "is_override": true,
          "is_final": false,
          "access": "public"
        }
      ]
    }
  ],
  "hierarchy": {
    "Base1": {
      "children": ["Derived"],
      "parents": [],
      "is_abstract": true
    }
  },
  "success": true
}
```

**Features:**
- **Inheritance Analysis**: Single and multiple inheritance tracking
- **Virtual Methods**: Detects virtual, override, final, and pure virtual methods
- **Abstract Classes**: Identifies classes with pure virtual methods
- **Access Levels**: Tracks public/private/protected method visibility
- **Hierarchy Tree**: Parent-child relationships with full traversal

### 9. get_dependency_graph

Build and analyze #include dependency graphs with advanced graph algorithms.

```json
{
  "name": "get_dependency_graph",
  "arguments": {
    "filepath": "src/",
    "show_system_includes": false,
    "detect_cycles": true,
    "output_format": "json"
  }
}
```

**Parameters:**
- `filepath`: String or array of strings (file paths or directories)
- `show_system_includes`: Include system headers (<>) - default: `false`
- `detect_cycles`: Detect circular dependencies - default: `true`
- `max_depth`: Maximum dependency depth, -1 for unlimited - default: `-1`
- `output_format`: Output format (json/mermaid/dot) - default: `"json"`
- `recursive`, `file_patterns`: Standard parameters

**Returns (JSON format):**
```json
{
  "nodes": [
    {
      "file": "src/main.cpp",
      "includes": ["utils.hpp", "config.hpp"],
      "included_by": ["app.cpp"],
      "is_system": false,
      "layer": 1
    }
  ],
  "edges": [
    {"from": "main.cpp", "to": "utils.hpp", "is_system": false, "line": 5}
  ],
  "cycles": [
    ["a.hpp", "b.hpp", "c.hpp", "a.hpp"]
  ],
  "layers": {
    "0": ["utils.hpp", "config.hpp"],
    "1": ["main.cpp"]
  },
  "success": true
}
```

**Returns (Mermaid format):**
```json
{
  "format": "mermaid",
  "content": "graph TD\n    N0[\"main.cpp\"]\n    N1[\"utils.hpp\"]\n    N0 --> N1\n",
  "success": true
}
```

**Features:**
- **Cycle Detection**: Tarjan's strongly connected components algorithm
- **Topological Sorting**: Layer-based build order computation
- **Multiple Formats**: JSON (structured), Mermaid (diagrams), DOT (Graphviz)
- **System vs User**: Distinguishes <system> and "user" includes
- **Python Support**: import statement analysis

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

- **[Claude Code Setup](doc/INSTALL_CLAUDE_CODE.md)** - Connect to Claude Code (recommended)
- **[Quick Start (Russian)](doc/quick-start-ru.md)** - Полный гайд на русском языке
- **[Build Instructions](BUILD.md)** - Detailed build, CMake options, troubleshooting

### API Documentation

- **[MCP API Reference](doc/MCP_API_REFERENCE.md)** - Complete API with request/response examples
- **[MCP Server Architecture](doc/MCP_SERVER_ARCHITECTURE.md)** - Guide for building your own MCP server
- **[MCP Roadmap](doc/scenary_mcp.md)** - Future features and vision for AI-agent integration
- **[Roadmap Analysis](doc/ROADMAP_ANALYSIS.md)** - Detailed implementation plan and priorities
- **[Technical Specification](doc/tz.md)** - Architecture and implementation details

### Architecture Decision Records (ADR)

- **[ADR Index](docs/adr/INDEX.md)** - Master index of all architectural decisions and reasoning chains
- **[ADR Quick Start](docs/adr/QUICK_START.md)** - 5-minute guide to finding and creating ADRs
- **[ADR Structure](docs/adr/README.md)** - Overview of the ADR system and organization
- **[Claude Code Guide](docs/adr/CLAUDE_GUIDE.md)** - How AI agents should use the ADR system

**Key ADRs:**
- [Three-Layer Architecture](docs/adr/index/0001-project-architecture.md)
- [RAII for Tree-sitter](docs/adr/core/tree-sitter-parser/0001-use-raii-for-tree-sitter.md)
- [Cache Invalidation Strategy](docs/adr/core/ast-analyzer/reasoning-cache-invalidation-strategy.md) (Reasoning Chain)

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
