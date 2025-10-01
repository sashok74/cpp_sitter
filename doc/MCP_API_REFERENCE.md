# MCP API Reference

Quick reference for all available MCP tools with request/response examples.

## Table of Contents

1. [tools/list](#toolslist) - List available tools
2. [parse_file](#parse_file) - Parse and analyze files
3. [find_classes](#find_classes) - Extract class declarations
4. [find_functions](#find_functions) - Extract function definitions
5. [execute_query](#execute_query) - Run custom tree-sitter queries

---

## tools/list

Get list of all available MCP tools with their schemas.

### Request

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/list",
  "params": {}
}
```

### Response

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
            "filepath": {
              "type": ["string", "array"],
              "description": "File path, array of paths, or directory"
            },
            "recursive": {
              "type": "boolean",
              "description": "Scan directories recursively (default: true)"
            },
            "file_patterns": {
              "type": "array",
              "description": "Glob patterns for file filtering"
            }
          },
          "required": ["filepath"]
        }
      },
      {
        "name": "find_classes",
        "description": "Find all class declarations with locations",
        "inputSchema": { "..." }
      },
      {
        "name": "find_functions",
        "description": "Find all function definitions with locations",
        "inputSchema": { "..." }
      },
      {
        "name": "execute_query",
        "description": "Execute custom tree-sitter S-expression query",
        "inputSchema": { "..." }
      }
    ]
  }
}
```

---

## parse_file

Parse C++/Python file(s) and return high-level metadata (class count, function count, errors, language).

### Single File (C++)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": "src/core/TreeSitterParser.cpp"
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
    "content": [
      {
        "type": "text",
        "text": "{\"class_count\":1,\"function_count\":5,\"include_count\":3,\"has_errors\":false,\"language\":\"cpp\",\"success\":true}"
      }
    ]
  }
}
```

### Single File (Python)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": "tests/fixtures/simple_class.py"
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
    "content": [
      {
        "type": "text",
        "text": "{\"class_count\":2,\"function_count\":7,\"include_count\":1,\"has_errors\":false,\"language\":\"python\",\"success\":true}"
      }
    ]
  }
}
```

### Multiple Files

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": ["src/main.cpp", "tests/fixtures/simple_class.py"]
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"total_files\":2,\"processed_files\":2,\"failed_files\":0,\"results\":[{\"filepath\":\"src/main.cpp\",\"class_count\":0,\"function_count\":1,\"include_count\":4,\"has_errors\":false,\"language\":\"cpp\",\"success\":true},{\"filepath\":\"tests/fixtures/simple_class.py\",\"class_count\":2,\"function_count\":7,\"include_count\":1,\"has_errors\":false,\"language\":\"python\",\"success\":true}]}"
      }
    ]
  }
}
```

### Directory (Recursive)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": "src/core/",
      "recursive": true,
      "file_patterns": ["*.cpp", "*.hpp"]
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
    "content": [
      {
        "type": "text",
        "text": "{\"total_files\":8,\"processed_files\":8,\"failed_files\":0,\"results\":[{\"filepath\":\"src/core/ASTAnalyzer.cpp\",\"class_count\":0,\"function_count\":6,\"include_count\":2,\"has_errors\":false,\"language\":\"cpp\",\"success\":true},{\"filepath\":\"src/core/TreeSitterParser.cpp\",\"class_count\":1,\"function_count\":5,\"include_count\":3,\"has_errors\":false,\"language\":\"cpp\",\"success\":true}]}"
      }
    ]
  }
}
```

---

## find_classes

Extract all class declarations with line/column locations.

### Single File (C++)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "method": "tools/call",
  "params": {
    "name": "find_classes",
    "arguments": {
      "filepath": "tests/fixtures/template_class.cpp"
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
    "content": [
      {
        "type": "text",
        "text": "{\"classes\":[{\"name\":\"Container\",\"line\":4,\"column\":7},{\"name\":\"Stack\",\"line\":12,\"column\":7}],\"success\":true}"
      }
    ]
  }
}
```

### Single File (Python)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "tools/call",
  "params": {
    "name": "find_classes",
    "arguments": {
      "filepath": "tests/fixtures/simple_class.py"
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
    "content": [
      {
        "type": "text",
        "text": "{\"classes\":[{\"name\":\"Calculator\",\"line\":4,\"column\":6},{\"name\":\"ScientificCalculator\",\"line\":22,\"column\":6}],\"success\":true}"
      }
    ]
  }
}
```

### Multiple Files

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 8,
  "method": "tools/call",
  "params": {
    "name": "find_classes",
    "arguments": {
      "filepath": ["src/core/TreeSitterParser.hpp", "tests/fixtures/simple_class.py"]
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
    "content": [
      {
        "type": "text",
        "text": "{\"total_files\":2,\"results\":[{\"filepath\":\"src/core/TreeSitterParser.hpp\",\"classes\":[{\"name\":\"TreeSitterParser\",\"line\":10,\"column\":7}],\"success\":true},{\"filepath\":\"tests/fixtures/simple_class.py\",\"classes\":[{\"name\":\"Calculator\",\"line\":4,\"column\":6},{\"name\":\"ScientificCalculator\",\"line\":22,\"column\":6}],\"success\":true}]}"
      }
    ]
  }
}
```

### Directory (Python only)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 9,
  "method": "tools/call",
  "params": {
    "name": "find_classes",
    "arguments": {
      "filepath": "tests/fixtures/",
      "recursive": false,
      "file_patterns": ["*.py"]
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
    "content": [
      {
        "type": "text",
        "text": "{\"total_files\":4,\"results\":[{\"filepath\":\"tests/fixtures/async_example.py\",\"classes\":[{\"name\":\"AsyncDataFetcher\",\"line\":26,\"column\":6}],\"success\":true},{\"filepath\":\"tests/fixtures/simple_class.py\",\"classes\":[{\"name\":\"Calculator\",\"line\":4,\"column\":6},{\"name\":\"ScientificCalculator\",\"line\":22,\"column\":6}],\"success\":true},{\"filepath\":\"tests/fixtures/with_decorators.py\",\"classes\":[{\"name\":\"MyClass\",\"line\":30,\"column\":6}],\"success\":true},{\"filepath\":\"tests/fixtures/with_imports.py\",\"classes\":[],\"success\":true}]}"
      }
    ]
  }
}
```

---

## find_functions

Extract all function definitions with line/column locations.

### Single File (C++)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 10,
  "method": "tools/call",
  "params": {
    "name": "find_functions",
    "arguments": {
      "filepath": "src/core/TreeSitterParser.cpp"
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
    "content": [
      {
        "type": "text",
        "text": "{\"functions\":[{\"name\":\"TreeSitterParser\",\"line\":8,\"column\":0},{\"name\":\"~TreeSitterParser\",\"line\":21,\"column\":0},{\"name\":\"parse_string\",\"line\":27,\"column\":13},{\"name\":\"parse_file\",\"line\":37,\"column\":13},{\"name\":\"node_text\",\"line\":54,\"column\":17}],\"success\":true}"
      }
    ]
  }
}
```

### Single File (Python with async)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 11,
  "method": "tools/call",
  "params": {
    "name": "find_functions",
    "arguments": {
      "filepath": "tests/fixtures/async_example.py"
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
    "content": [
      {
        "type": "text",
        "text": "{\"functions\":[{\"name\":\"fetch_data\",\"line\":7,\"column\":10},{\"name\":\"process_item\",\"line\":13,\"column\":10},{\"name\":\"batch_process\",\"line\":19,\"column\":10},{\"name\":\"__init__\",\"line\":29,\"column\":4},{\"name\":\"get\",\"line\":34,\"column\":10},{\"name\":\"get_multiple\",\"line\":44,\"column\":10},{\"name\":\"__aenter__\",\"line\":49,\"column\":10},{\"name\":\"__aexit__\",\"line\":53,\"column\":10},{\"name\":\"main\",\"line\":58,\"column\":10}],\"success\":true}"
      }
    ]
  }
}
```

### Directory (Recursive)

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 12,
  "method": "tools/call",
  "params": {
    "name": "find_functions",
    "arguments": {
      "filepath": "src/mcp/",
      "recursive": true,
      "file_patterns": ["*.cpp"]
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
    "content": [
      {
        "type": "text",
        "text": "{\"total_files\":3,\"results\":[{\"filepath\":\"src/mcp/MCPServer.cpp\",\"functions\":[{\"name\":\"MCPServer\",\"line\":10,\"column\":0},{\"name\":\"register_tool\",\"line\":18,\"column\":5},{\"name\":\"handle_request\",\"line\":25,\"column\":5}],\"success\":true}]}"
      }
    ]
  }
}
```

---

## execute_query

Run custom tree-sitter S-expression query on file(s).

### C++ - Find Virtual Functions

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 13,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "tests/fixtures/virtual_functions.cpp",
      "query": "(function_definition (virtual_function_specifier) declarator: (function_declarator declarator: (field_identifier) @virtual_func))"
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
    "content": [
      {
        "type": "text",
        "text": "{\"matches\":[{\"capture_name\":\"virtual_func\",\"text\":\"draw\",\"line\":5,\"column\":17},{\"capture_name\":\"virtual_func\",\"text\":\"area\",\"line\":6,\"column\":18}],\"success\":true}"
      }
    ]
  }
}
```

### C++ - Find Template Classes

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 14,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "tests/fixtures/template_class.cpp",
      "query": "(template_declaration (class_specifier name: (type_identifier) @template_class))"
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
    "content": [
      {
        "type": "text",
        "text": "{\"matches\":[{\"capture_name\":\"template_class\",\"text\":\"Container\",\"line\":4,\"column\":7},{\"capture_name\":\"template_class\",\"text\":\"Stack\",\"line\":12,\"column\":7}],\"success\":true}"
      }
    ]
  }
}
```

### C++ - Find Includes

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 15,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "src/core/TreeSitterParser.cpp",
      "query": "(preproc_include path: (_) @include_path)"
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
    "content": [
      {
        "type": "text",
        "text": "{\"matches\":[{\"capture_name\":\"include_path\",\"text\":\"\\\"core/TreeSitterParser.hpp\\\"\",\"line\":1,\"column\":9},{\"capture_name\":\"include_path\",\"text\":\"<spdlog/spdlog.h>\",\"line\":2,\"column\":9},{\"capture_name\":\"include_path\",\"text\":\"<fstream>\",\"line\":3,\"column\":9}],\"success\":true}"
      }
    ]
  }
}
```

### Python - Find Decorators

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 16,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "tests/fixtures/with_decorators.py",
      "query": "(decorator) @decorator"
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
    "content": [
      {
        "type": "text",
        "text": "{\"matches\":[{\"capture_name\":\"decorator\",\"text\":\"@timer\",\"line\":4,\"column\":0},{\"capture_name\":\"decorator\",\"text\":\"@wraps(func)\",\"line\":7,\"column\":4},{\"capture_name\":\"decorator\",\"text\":\"@retry(max_attempts=3)\",\"line\":13,\"column\":0},{\"capture_name\":\"decorator\",\"text\":\"@staticmethod\",\"line\":33,\"column\":4},{\"capture_name\":\"decorator\",\"text\":\"@classmethod\",\"line\":38,\"column\":4},{\"capture_name\":\"decorator\",\"text\":\"@property\",\"line\":44,\"column\":4}],\"success\":true}"
      }
    ]
  }
}
```

### Python - Find Async Functions

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 17,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "tests/fixtures/async_example.py",
      "query": "(function_definition \"async\" @async_keyword name: (identifier) @async_func_name)"
    }
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 17,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"matches\":[{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":7,\"column\":0},{\"capture_name\":\"async_func_name\",\"text\":\"fetch_data\",\"line\":7,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":13,\"column\":0},{\"capture_name\":\"async_func_name\",\"text\":\"process_item\",\"line\":13,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":19,\"column\":0},{\"capture_name\":\"async_func_name\",\"text\":\"batch_process\",\"line\":19,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":34,\"column\":4},{\"capture_name\":\"async_func_name\",\"text\":\"get\",\"line\":34,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":44,\"column\":4},{\"capture_name\":\"async_func_name\",\"text\":\"get_multiple\",\"line\":44,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":49,\"column\":4},{\"capture_name\":\"async_func_name\",\"text\":\"__aenter__\",\"line\":49,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":53,\"column\":4},{\"capture_name\":\"async_func_name\",\"text\":\"__aexit__\",\"line\":53,\"column\":10},{\"capture_name\":\"async_keyword\",\"text\":\"async\",\"line\":58,\"column\":0},{\"capture_name\":\"async_func_name\",\"text\":\"main\",\"line\":58,\"column\":10}],\"success\":true}"
      }
    ]
  }
}
```

### Python - Find Imports

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 18,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "tests/fixtures/with_imports.py",
      "query": "[(import_statement) @import (import_from_statement) @import_from]"
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
    "content": [
      {
        "type": "text",
        "text": "{\"matches\":[{\"capture_name\":\"import\",\"text\":\"import os\",\"line\":3,\"column\":0},{\"capture_name\":\"import\",\"text\":\"import sys\",\"line\":4,\"column\":0},{\"capture_name\":\"import_from\",\"text\":\"from pathlib import Path\",\"line\":5,\"column\":0},{\"capture_name\":\"import_from\",\"text\":\"from typing import List, Dict, Optional, Union\",\"line\":6,\"column\":0},{\"capture_name\":\"import_from\",\"text\":\"from collections import defaultdict, Counter\",\"line\":7,\"column\":0},{\"capture_name\":\"import\",\"text\":\"import json as js\",\"line\":8,\"column\":0},{\"capture_name\":\"import_from\",\"text\":\"from datetime import datetime, timedelta\",\"line\":9,\"column\":0}],\"success\":true}"
      }
    ]
  }
}
```

### Multiple Files with Custom Query

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": ["tests/fixtures/simple_class.py", "tests/fixtures/async_example.py"],
      "query": "(class_definition name: (identifier) @class_name)"
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
    "content": [
      {
        "type": "text",
        "text": "{\"total_files\":2,\"results\":[{\"filepath\":\"tests/fixtures/simple_class.py\",\"matches\":[{\"capture_name\":\"class_name\",\"text\":\"Calculator\",\"line\":4,\"column\":6},{\"capture_name\":\"class_name\",\"text\":\"ScientificCalculator\",\"line\":22,\"column\":6}],\"success\":true},{\"filepath\":\"tests/fixtures/async_example.py\",\"matches\":[{\"capture_name\":\"class_name\",\"text\":\"AsyncDataFetcher\",\"line\":26,\"column\":6}],\"success\":true}]}"
      }
    ]
  }
}
```

---

## Error Responses

### File Not Found

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 20,
  "method": "tools/call",
  "params": {
    "name": "parse_file",
    "arguments": {
      "filepath": "nonexistent.cpp"
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
    "content": [
      {
        "type": "text",
        "text": "{\"success\":false,\"error\":\"Failed to open file: nonexistent.cpp\"}"
      }
    ],
    "isError": true
  }
}
```

### Invalid Query Syntax

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 21,
  "method": "tools/call",
  "params": {
    "name": "execute_query",
    "arguments": {
      "filepath": "src/main.cpp",
      "query": "(invalid query syntax"
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
    "content": [
      {
        "type": "text",
        "text": "{\"success\":false,\"error\":\"Failed to compile query\"}"
      }
    ],
    "isError": true
  }
}
```

### Unknown Tool

**Request:**
```json
{
  "jsonrpc": "2.0",
  "id": 22,
  "method": "tools/call",
  "params": {
    "name": "unknown_tool",
    "arguments": {}
  }
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "id": 22,
  "error": {
    "code": -32601,
    "message": "Tool not found: unknown_tool"
  }
}
```

---

## Command-Line Testing

You can test the server directly from the command line:

```bash
# List available tools
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | mcp_stdio_server --log-level error

# Parse a single file
echo '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"src/main.cpp"}}}' | mcp_stdio_server --log-level error

# Find classes in directory
echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"src/","recursive":true,"file_patterns":["*.cpp","*.hpp"]}}}' | mcp_stdio_server --log-level error

# Execute custom query
echo '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"execute_query","arguments":{"filepath":"src/core/TreeSitterParser.hpp","query":"(class_specifier name: (type_identifier) @class_name)"}}}' | mcp_stdio_server --log-level error
```

---

## Notes

- All responses are JSON-RPC 2.0 formatted
- Content is returned as an array with `type: "text"` objects
- Language is auto-detected from file extension (`.cpp`/`.hpp` → `cpp`, `.py` → `python`)
- Batch operations support arrays of files or directory scanning
- Line/column numbers are 1-indexed
- Recursive directory scanning is enabled by default
- Default file patterns: `["*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx", "*.py"]`
