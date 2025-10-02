# MCP API Reference

Quick reference for all available MCP tools.

## Available Tools

### 1. parse_file
Parse C++/Python files and return basic metadata (class/function counts, errors, language).

**Input:** `filepath` (string/array/directory), `recursive` (bool), `file_patterns` (array)
**Output:** `{class_count, function_count, include_count, has_errors, language, success}`

---

### 2. find_classes
Extract all class declarations with line numbers.

**Input:** `filepath` (string/array/directory), `recursive` (bool), `file_patterns` (array)
**Output:** `{classes: [{name, line, column}], success}`

---

### 3. find_functions
Extract all function definitions with line numbers.

**Input:** `filepath` (string/array/directory), `recursive` (bool), `file_patterns` (array)
**Output:** `{functions: [{name, line, column}], success}`

---

### 4. execute_query
Run custom tree-sitter S-expression queries on code.

**Input:** `filepath` (string/array/directory), `query` (S-expression), `recursive` (bool), `file_patterns` (array)
**Output:** `{matches: [{capture_name, text, line, column}], success}`

---

### 5. extract_interface
Extract function signatures and class interfaces without implementation bodies (reduces AI context).

**Input:** `filepath` (string/array/directory), `output_format` (json/header/markdown), `include_private` (bool), `include_comments` (bool), `recursive` (bool), `file_patterns` (array)
**Output:** `{functions: [{signature, line, comment}], classes: [{name, methods, members}], success}`

---

### 6. find_references
Find all references to a symbol with AST-based classification (call, declaration, definition).

**Input:** `symbol` (string), `filepath` (string/array/directory), `reference_types` (array), `include_context` (bool), `recursive` (bool), `file_patterns` (array)
**Output:** `{symbol, total_references, references: [{filepath, line, column, type, context, parent_scope}], success}`

---

### 7. get_file_summary
Enhanced file analysis with cyclomatic complexity, TODO/FIXME extraction, full signatures, and code metrics.

**Input:** `filepath` (string/array/directory), `include_complexity` (bool), `include_comments` (bool), `include_docstrings` (bool), `recursive` (bool), `file_patterns` (array)
**Output:** `{metrics: {total_lines, code_lines, comment_lines, blank_lines}, functions: [{name, return_type, parameters, line, complexity, docstring}], classes: [{name, line}], imports: [{path, line, is_system}], comment_markers: [{type, text, line}], success}`

---

### 8. get_class_hierarchy
Analyze C++ class inheritance hierarchies with virtual methods and OOP structure.

**Input:** `filepath` (string/array/directory), `class_name` (string, optional), `show_methods` (bool), `show_virtual_only` (bool), `max_depth` (int), `recursive` (bool), `file_patterns` (array)
**Output:** `{classes: [{name, line, file, base_classes, is_abstract, virtual_methods: [{name, signature, line, is_pure_virtual, is_override, is_final, access}]}], hierarchy: {class_name: {children, parents, is_abstract}}, success}`

---

### 9. get_dependency_graph
Build #include dependency graphs with cycle detection and topological sorting.

**Input:** `filepath` (string/array/directory), `show_system_includes` (bool), `detect_cycles` (bool), `max_depth` (int), `output_format` (json/mermaid/dot), `recursive` (bool), `file_patterns` (array)
**Output:** `{nodes: [{file, includes, included_by, is_system, layer}], edges: [{from, to, is_system, line}], cycles: [[files]], layers: {layer_num: [files]}, success}` OR `{format: "mermaid", content: string}` OR `{format: "dot", content: string}`

---

### 10. get_symbol_context
Get comprehensive context for a symbol (function/class/method) including its definition and direct dependencies.

**Input:** `symbol_name` (string), `filepath` (string), `include_dependencies` (bool, default: true), `max_dependencies` (int, default: 10)
**Output:** `{symbol: {name, type, filepath, start_line, end_line, signature, full_code, parent_class?}, dependencies?: [{name, type, filepath, start_line, end_line, signature}], required_includes?: [includes], used_symbols_count, dependencies_found}`

---

## Detailed Examples

### tools/list

Get list of all available MCP tools with their schemas.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list",
  "params": {}
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "tools": [
      {
        "name": "parse_file",
        "description": "Parse C++/Python file(s) and return metadata",
        "inputSchema": {
          "type": "object",
          "properties": {
            "filepath": {"type": ["string", "array"]},
            "recursive": {"type": "boolean"},
            "file_patterns": {"type": "array"}
          },
          "required": ["filepath"]
        }
      },
      {
        "name": "find_classes",
        "description": "Find all class declarations with locations",
        "inputSchema": {"..."}
      },
      {
        "name": "find_functions",
        "description": "Find all function definitions with locations",
        "inputSchema": {"..."}
      },
      {
        "name": "execute_query",
        "description": "Execute custom tree-sitter S-expression query",
        "inputSchema": {"..."}
      },
      {
        "name": "extract_interface",
        "description": "Extract function signatures and class interfaces without implementation bodies",
        "inputSchema": {"..."}
      },
      {
        "name": "find_references",
        "description": "Find all references to a symbol with AST-based classification",
        "inputSchema": {"..."}
      },
      {
        "name": "get_file_summary",
        "description": "Enhanced file analysis with complexity, TODO extraction, and metrics",
        "inputSchema": {"..."}
      }
    ]
  }
}
```

---

### parse_file

#### Single File
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": "src/main.cpp"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "class_count": 2,
    "function_count": 5,
    "include_count": 3,
    "has_errors": false,
    "language": "cpp",
    "success": true
  }
}
```

#### Multiple Files
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": ["src/main.cpp", "src/utils.cpp"]
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "total_files": 2,
    "processed_files": 2,
    "failed_files": 0,
    "results": [
      {
        "filepath": "src/main.cpp",
        "class_count": 1,
        "function_count": 3,
        "success": true
      },
      {
        "filepath": "src/utils.cpp",
        "class_count": 0,
        "function_count": 5,
        "success": true
      }
    ],
    "success": true
  }
}
```

#### Directory (Recursive)
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": "src/",
      "recursive": true,
      "file_patterns": ["*.cpp", "*.hpp"]
    }
  }
}
```

---

### find_classes

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "tools/call",
  "params": {
    "name": "find_classes",
    "arguments": {
      "filepath": "src/MyClass.cpp"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "result": {
    "classes": [
      {
        "name": "MyClass",
        "line": 10,
        "column": 7
      },
      {
        "name": "HelperClass",
        "line": 42,
        "column": 7
      }
    ],
    "success": true
  }
}
```

---

### find_functions

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "method": "tools/call",
  "params": {
    "name": "find_functions",
    "arguments": {
      "filepath": "src/utils.cpp"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "result": {
    "functions": [
      {
        "name": "calculate",
        "line": 15,
        "column": 5
      },
      {
        "name": "process",
        "line": 30,
        "column": 5
      }
    ],
    "success": true
  }
}
```

---

### execute_query

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "src/Base.cpp",
      "query": "(class_specifier name: (type_identifier) @class_name)"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "matches": [
      {
        "capture_name": "class_name",
        "text": "Base",
        "line": 5,
        "column": 7
      }
    ],
    "success": true
  }
}
```

---

### extract_interface

#### JSON Format (C++)
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 8,
  "method": "tools/call",
  "params": {
    "name": "extract_interface",
    "arguments": {
      "filepath": "src/MyClass.cpp",
      "output_format": "json",
      "include_private": false,
      "include_comments": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 8,
  "result": {
    "filepath": "src/MyClass.cpp",
    "language": "cpp",
    "functions": [
      {
        "signature": "int calculate(int x, int y)",
        "line": 42,
        "comment": "Calculate sum of two integers"
      },
      {
        "signature": "void process()",
        "line": 55
      }
    ],
    "classes": [
      {
        "name": "MyClass",
        "line": 10,
        "methods": [
          {
            "signature": "void doWork()",
            "line": 15,
            "access": "public"
          }
        ],
        "members": [
          {
            "name": "value_",
            "type": "int",
            "line": 20,
            "access": "private"
          }
        ]
      }
    ],
    "success": true
  }
}
```

#### Header Format (C++)
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 9,
  "method": "tools/call",
  "params": {
    "name": "extract_interface",
    "arguments": {
      "filepath": "src/MyClass.cpp",
      "output_format": "header",
      "include_private": false
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 9,
  "result": {
    "filepath": "src/MyClass.cpp",
    "format": "header",
    "content": "#pragma once\n\nclass MyClass {\npublic:\n    void doWork();\n    int calculate(int x, int y);\n};\n\nvoid process();\n",
    "success": true
  }
}
```

#### Markdown Format
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 10,
  "method": "tools/call",
  "params": {
    "name": "extract_interface",
    "arguments": {
      "filepath": "src/MyClass.cpp",
      "output_format": "markdown"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 10,
  "result": {
    "filepath": "src/MyClass.cpp",
    "format": "markdown",
    "content": "# API Reference: src/MyClass.cpp\n\n## Classes\n\n### MyClass\n\n**Public Methods:**\n- `void doWork()`\n- `int calculate(int x, int y)`\n\n## Functions\n\n- `void process()`\n",
    "success": true
  }
}
```

---

### find_references

#### Find Class References (C++)
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 11,
  "method": "tools/call",
  "params": {
    "name": "find_references",
    "arguments": {
      "symbol": "MyClass",
      "filepath": "src/",
      "recursive": true,
      "include_context": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 11,
  "result": {
    "symbol": "MyClass",
    "total_references": 5,
    "files_searched": 12,
    "files_processed": 12,
    "files_failed": 0,
    "references": [
      {
        "filepath": "src/main.cpp",
        "line": 10,
        "column": 5,
        "type": "definition",
        "context": "class MyClass {",
        "parent_scope": "",
        "node_type": "type_identifier"
      },
      {
        "filepath": "src/main.cpp",
        "line": 42,
        "column": 5,
        "type": "call",
        "context": "MyClass obj;",
        "parent_scope": "main",
        "node_type": "identifier"
      },
      {
        "filepath": "src/utils.cpp",
        "line": 15,
        "column": 20,
        "type": "type_usage",
        "context": "std::unique_ptr<MyClass> ptr;",
        "parent_scope": "createInstance",
        "node_type": "type_identifier"
      }
    ],
    "success": true
  }
}
```

#### Filter by Reference Type
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 12,
  "method": "tools/call",
  "params": {
    "name": "find_references",
    "arguments": {
      "symbol": "calculate",
      "filepath": "src/",
      "reference_types": ["call"],
      "include_context": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 12,
  "result": {
    "symbol": "calculate",
    "total_references": 3,
    "references": [
      {
        "filepath": "src/main.cpp",
        "line": 50,
        "column": 15,
        "type": "call",
        "context": "int result = calculate(10, 20);",
        "parent_scope": "main",
        "node_type": "identifier"
      }
    ],
    "success": true
  }
}
```

---

### get_file_summary

#### Single File with All Features
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 13,
  "method": "tools/call",
  "params": {
    "name": "get_file_summary",
    "arguments": {
      "filepath": "src/Calculator.cpp",
      "include_complexity": true,
      "include_comments": true,
      "include_docstrings": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 13,
  "result": {
    "filepath": "src/Calculator.cpp",
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
        "name": "add",
        "return_type": "int",
        "line": 20,
        "complexity": 1,
        "parameters": [
          {"type": "int", "name": "a"},
          {"type": "int", "name": "b"}
        ],
        "docstring": "Add two integers and return the result",
        "is_virtual": false,
        "is_static": false
      },
      {
        "name": "divide",
        "return_type": "double",
        "line": 45,
        "complexity": 3,
        "parameters": [
          {"type": "double", "name": "numerator"},
          {"type": "double", "name": "denominator"}
        ],
        "docstring": "Divide two numbers with error checking",
        "is_virtual": false,
        "is_static": false
      }
    ],
    "function_count": 2,
    "classes": [
      {
        "name": "Calculator",
        "line": 10
      }
    ],
    "class_count": 1,
    "imports": [
      {
        "path": "iostream",
        "line": 1,
        "is_system": true
      },
      {
        "path": "Calculator.hpp",
        "line": 2,
        "is_system": false
      }
    ],
    "comment_markers": [
      {
        "type": "TODO",
        "text": "Add overflow checking",
        "line": 25,
        "context": "// TODO: Add overflow checking"
      },
      {
        "type": "FIXME",
        "text": "Division by zero handling is incomplete",
        "line": 50,
        "context": "// FIXME: Division by zero handling is incomplete"
      }
    ]
  }
}
```

#### Python File
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 14,
  "method": "tools/call",
  "params": {
    "name": "get_file_summary",
    "arguments": {
      "filepath": "utils.py",
      "include_complexity": true,
      "include_docstrings": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 14,
  "result": {
    "filepath": "utils.py",
    "language": "python",
    "success": true,
    "metrics": {
      "total_lines": 80,
      "code_lines": 55,
      "comment_lines": 15,
      "blank_lines": 10
    },
    "functions": [
      {
        "name": "process_data",
        "return_type": "",
        "line": 10,
        "complexity": 4,
        "parameters": [
          {"type": "", "name": "data"},
          {"type": "", "name": "options"}
        ],
        "docstring": "Process data with given options\n\nArgs:\n    data: Input data\n    options: Processing options",
        "is_async": false
      },
      {
        "name": "fetch_async",
        "return_type": "",
        "line": 45,
        "complexity": 2,
        "parameters": [
          {"type": "", "name": "url"}
        ],
        "docstring": "Fetch data asynchronously from URL",
        "is_async": true
      }
    ],
    "function_count": 2,
    "classes": [],
    "class_count": 0,
    "imports": [
      {
        "path": "asyncio",
        "line": 1,
        "module": "asyncio"
      }
    ],
    "comment_markers": []
  }
}
```

#### Multiple Files
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 15,
  "method": "tools/call",
  "params": {
    "name": "get_file_summary",
    "arguments": {
      "filepath": ["src/main.cpp", "src/utils.cpp"],
      "include_complexity": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 15,
  "result": {
    "total_files": 2,
    "processed_files": 2,
    "failed_files": 0,
    "results": [
      {
        "filepath": "src/main.cpp",
        "language": "cpp",
        "metrics": {"total_lines": 100, "code_lines": 75, "comment_lines": 15, "blank_lines": 10},
        "functions": [...],
        "classes": [...],
        "success": true
      },
      {
        "filepath": "src/utils.cpp",
        "language": "cpp",
        "metrics": {"total_lines": 50, "code_lines": 35, "comment_lines": 10, "blank_lines": 5},
        "functions": [...],
        "classes": [...],
        "success": true
      }
    ],
    "success": true
  }
}
```

---

## Error Responses

### File Not Found
```json
{
  "jsonrpc": "2.0",
  "id": 100,
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": "Failed to read file: src/nonexistent.cpp"
  }
}
```

### Invalid Query Syntax
```json
{
  "jsonrpc": "2.0",
  "id": 101,
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": "Failed to compile query: syntax error at line 1"
  }
}
```

### Missing Required Parameter
```json
{
  "jsonrpc": "2.0",
  "id": 102,
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": "Missing required parameter: filepath"
  }
}
```

---

### get_class_hierarchy

#### Analyze Full Hierarchy
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 16,
  "method": "tools/call",
  "params": {
    "name": "get_class_hierarchy",
    "arguments": {
      "filepath": "src/",
      "show_methods": true,
      "recursive": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 16,
  "result": {
    "total_files": 15,
    "files_processed": 15,
    "files_failed": 0,
    "total_classes": 8,
    "classes": [
      {
        "name": "Base",
        "line": 10,
        "file": "src/base.hpp",
        "base_classes": [],
        "is_abstract": true,
        "virtual_methods": [
          {
            "name": "process",
            "signature": "virtual void process() = 0",
            "line": 15,
            "is_pure_virtual": true,
            "is_override": false,
            "is_final": false,
            "access": "public"
          }
        ]
      },
      {
        "name": "Derived",
        "line": 25,
        "file": "src/derived.hpp",
        "base_classes": ["Base"],
        "is_abstract": false,
        "virtual_methods": [
          {
            "name": "process",
            "signature": "void process() override",
            "line": 30,
            "is_pure_virtual": false,
            "is_override": true,
            "is_final": false,
            "access": "public"
          }
        ]
      }
    ],
    "hierarchy": {
      "Base": {
        "children": ["Derived"],
        "parents": [],
        "is_abstract": true
      },
      "Derived": {
        "children": [],
        "parents": ["Base"],
        "is_abstract": false
      }
    },
    "success": true
  }
}
```

#### Focus on Specific Class
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 17,
  "method": "tools/call",
  "params": {
    "name": "get_class_hierarchy",
    "arguments": {
      "filepath": "src/",
      "class_name": "Base",
      "max_depth": 2
    }
  }
}
```

---

### get_dependency_graph

#### JSON Format with Cycle Detection
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 18,
  "method": "tools/call",
  "params": {
    "name": "get_dependency_graph",
    "arguments": {
      "filepath": "src/",
      "show_system_includes": false,
      "detect_cycles": true,
      "output_format": "json"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 18,
  "result": {
    "total_files": 20,
    "files_failed": 0,
    "nodes": [
      {
        "file": "main.cpp",
        "includes": ["utils.hpp", "config.hpp"],
        "included_by": [],
        "is_system": false,
        "layer": 1
      },
      {
        "file": "utils.hpp",
        "includes": ["types.hpp"],
        "included_by": ["main.cpp", "processor.cpp"],
        "is_system": false,
        "layer": 0
      }
    ],
    "edges": [
      {"from": "main.cpp", "to": "utils.hpp", "is_system": false, "line": 3},
      {"from": "main.cpp", "to": "config.hpp", "is_system": false, "line": 4}
    ],
    "cycles": [
      ["a.hpp", "b.hpp", "c.hpp", "a.hpp"]
    ],
    "layers": {
      "0": ["types.hpp", "config.hpp"],
      "1": ["utils.hpp"],
      "2": ["main.cpp"]
    },
    "success": true
  }
}
```

#### Mermaid Diagram Format
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "method": "tools/call",
  "params": {
    "name": "get_dependency_graph",
    "arguments": {
      "filepath": ["src/main.cpp", "src/utils.cpp"],
      "output_format": "mermaid"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "result": {
    "format": "mermaid",
    "content": "graph TD\n    N0[\"main.cpp\"]\n    N1[\"utils.hpp\"]\n    N2[\"config.hpp\"]\n    N0 --> N1\n    N0 --> N2\n    N1 --> N2\n",
    "total_files": 2,
    "total_dependencies": 3,
    "cycles_found": 0,
    "success": true
  }
}
```

#### DOT Format for Graphviz
**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 20,
  "method": "tools/call",
  "params": {
    "name": "get_dependency_graph",
    "arguments": {
      "filepath": "src/",
      "output_format": "dot",
      "show_system_includes": true
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 20,
  "result": {
    "format": "dot",
    "content": "digraph dependencies {\n    rankdir=LR;\n    node [shape=box];\n\n    N0 [label=\"main.cpp\"];\n    N1 [label=\"utils.hpp\"];\n    N0 -> N1;\n}\n",
    "success": true
  }
}
```

---

### get_symbol_context

Get comprehensive context for a symbol including its definition and direct dependencies.

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 21,
  "method": "tools/call",
  "params": {
    "name": "get_symbol_context",
    "arguments": {
      "symbol_name": "Database::connect",
      "filepath": "src/database.cpp",
      "include_dependencies": true,
      "max_dependencies": 10
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 21,
  "result": {
    "symbol": {
      "name": "Database::connect",
      "type": "method",
      "filepath": "src/database.cpp",
      "start_line": 45,
      "end_line": 52,
      "signature": "bool Database::connect(const std::string& host);",
      "full_code": "bool Database::connect(const std::string& host) {\n    conn_ = new Connection(host);\n    return conn_->isValid();\n}",
      "parent_class": "Database"
    },
    "dependencies": [
      {
        "name": "Connection",
        "type": "class",
        "filepath": "src/database.cpp",
        "start_line": 10,
        "end_line": 15,
        "signature": "class Connection;"
      }
    ],
    "required_includes": [
      "#include <string>",
      "#include \"connection.hpp\""
    ],
    "used_symbols_count": 3,
    "dependencies_found": 1
  }
}
```

---

## Quick Reference

| Tool | Primary Use Case | Key Output |
|------|------------------|------------|
| `parse_file` | Quick metadata check | Class/function counts, syntax errors |
| `find_classes` | List all classes | Class names with locations |
| `find_functions` | List all functions | Function names with locations |
| `execute_query` | Custom AST queries | S-expression query matches |
| `extract_interface` | Generate API docs | Signatures without bodies (JSON/header/markdown) |
| `find_references` | Symbol usage analysis | All references with type classification |
| `get_file_summary` | Comprehensive analysis | Complexity, TODOs, metrics, full signatures |
| `get_class_hierarchy` | OOP structure analysis | Inheritance trees, virtual methods, abstract classes |
| `get_dependency_graph` | Architecture visualization | Include graphs, cycles, build order, Mermaid/DOT diagrams |
| `get_symbol_context` | Context extraction for AI | Symbol definition + dependencies for minimal complete code |

---

## Common Use Cases

1. **Code Quality Check**: Use `get_file_summary` with `include_complexity=true` and `include_comments=true` to find complex functions and maintenance tasks
2. **API Documentation**: Use `extract_interface` with `output_format=markdown` to generate API docs
3. **Refactoring Support**: Use `find_references` to safely rename symbols
4. **Architecture Analysis**: Use `find_classes` + `find_functions` across directories to understand codebase structure
5. **Maintenance Planning**: Use `get_file_summary` with `include_comments=true` to extract all TODO/FIXME markers
