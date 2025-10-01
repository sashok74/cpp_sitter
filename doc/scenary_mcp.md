# Сценарии и команды для MCP сервера на базе Tree-Sitter

## 🎯 Основные сценарии использования

### 1. **Навигация и понимание архитектуры**
Агенту нужно быстро понять структуру проекта перед внесением изменений.

### 2. **Сборка релевантного контекста**
Извлечение только нужных частей кода для промпта, избегая перегрузки контекста.

### 3. **Анализ зависимостей и влияния**
Понимание, какие части кода связаны и что будет затронуто изменениями.

### 4. **Поиск по коду**
Семантический поиск определений, использований, паттернов.

---

## 📋 Предлагаемые команды

### **1. get_symbol_context**
Получить полный контекст для символа (функции/класса) со всеми зависимостями.

**Назначение:** Собрать минимальный, но достаточный контекст для понимания и модификации кода.

**Параметры:**
```json
{
  "symbol_name": "ClassName::methodName",
  "filepath": "src/core/Module.cpp",
  "context_depth": 2,  // 0=только декларация, 1=+прямые зависимости, 2=+транзитивные
  "include_callers": true,
  "include_callees": true,
  "include_related_types": true,
  "max_context_lines": 500
}
```

**Формат вывода:**
```json
{
  "symbol": {
    "name": "TreeSitterParser::parse_file",
    "type": "method",
    "filepath": "src/core/TreeSitterParser.cpp",
    "line": 37,
    "signature": "std::optional<TSTree*> parse_file(const std::string& filepath)",
    "body": "...", // полный код функции
    "comment": "// Parse file and return AST tree",
    "is_virtual": false,
    "is_static": false,
    "access": "public"
  },
  "dependencies": {
    "direct_calls": [
      {
        "name": "ts_parser_parse_string",
        "type": "function",
        "filepath": "external/tree-sitter/api.h",
        "signature": "TSTree* ts_parser_parse_string(...)",
        "line": 245
      }
    ],
    "types_used": [
      {
        "name": "TSTree",
        "type": "struct",
        "filepath": "external/tree-sitter/api.h",
        "definition": "typedef struct TSTree TSTree;",
        "line": 50
      }
    ],
    "includes": [
      "\"core/TreeSitterParser.hpp\"",
      "<fstream>",
      "<spdlog/spdlog.h>"
    ]
  },
  "callers": [
    {
      "name": "ASTAnalyzer::analyze",
      "filepath": "src/core/ASTAnalyzer.cpp",
      "line": 123,
      "context_snippet": "auto tree = parser.parse_file(filepath);"
    }
  ],
  "related_symbols": [
    {
      "name": "TreeSitterParser::parse_string",
      "relationship": "sibling_method",
      "filepath": "src/core/TreeSitterParser.cpp"
    }
  ],
  "total_context_lines": 387,
  "success": true
}
```

---

### **2. get_class_hierarchy**
Построить граф наследования классов.

**Назначение:** Понять структуру наследования перед рефакторингом или добавлением функциональности.

**Параметры:**
```json
{
  "class_name": "BaseParser",
  "filepath": "src/core/BaseParser.hpp", // опционально, для уточнения
  "direction": "both", // "up" (родители), "down" (потомки), "both"
  "max_depth": 3,
  "include_virtual_methods": true,
  "include_member_variables": true
}
```

**Формат вывода:**
```json
{
  "root": {
    "name": "BaseParser",
    "filepath": "src/core/BaseParser.hpp",
    "line": 10,
    "is_abstract": true,
    "virtual_methods": [
      {"name": "parse", "signature": "virtual TSTree* parse(const string&) = 0", "is_pure": true}
    ],
    "member_variables": [
      {"name": "m_logger", "type": "spdlog::logger*", "access": "protected"}
    ]
  },
  "hierarchy": {
    "parents": [],
    "children": [
      {
        "name": "TreeSitterParser",
        "filepath": "src/core/TreeSitterParser.hpp",
        "line": 15,
        "overridden_methods": ["parse"],
        "added_methods": ["parse_file", "node_text"],
        "children": []
      },
      {
        "name": "CustomParser",
        "filepath": "src/parsers/CustomParser.hpp",
        "line": 8,
        "children": []
      }
    ]
  },
  "inheritance_depth": 2,
  "total_descendants": 2,
  "success": true
}
```

---

### **3. find_references**
Найти все использования символа в кодовой базе.

**Назначение:** Понять impact изменений, найти примеры использования.

**Параметры:**
```json
{
  "symbol_name": "parse_file",
  "symbol_type": "function", // "function", "class", "variable", "macro"
  "scope": "src/", // путь для поиска
  "include_comments": false,
  "include_tests": true,
  "group_by": "file" // "file" или "type"
}
```

**Формат вывода:**
```json
{
  "symbol": "parse_file",
  "total_references": 15,
  "references_by_file": [
    {
      "filepath": "src/mcp/MCPServer.cpp",
      "references": [
        {
          "line": 45,
          "column": 20,
          "context": "    auto result = parser.parse_file(filepath);",
          "type": "call",
          "scope": "MCPServer::handle_parse_request"
        },
        {
          "line": 78,
          "column": 35,
          "context": "    if (auto tree = parser.parse_file(f)) {",
          "type": "call",
          "scope": "MCPServer::batch_parse"
        }
      ]
    },
    {
      "filepath": "tests/test_parser.cpp",
      "references": [
        {
          "line": 23,
          "column": 15,
          "context": "TEST(Parser, parse_file) {",
          "type": "test",
          "scope": "global"
        }
      ]
    }
  ],
  "references_by_type": {
    "calls": 12,
    "declarations": 1,
    "tests": 2
  },
  "success": true
}
```

---

### **4. get_file_summary**
Получить структурированное оглавление файла.

**Назначение:** Быстрый обзор содержимого файла без загрузки всего кода.

**Параметры:**
```json
{
  "filepath": "src/core/TreeSitterParser.cpp",
  "include_signatures": true,
  "include_comments": true,
  "include_complexity_metrics": true,
  "extract_todos": true
}
```

**Формат вывода:**
```json
{
  "filepath": "src/core/TreeSitterParser.cpp",
  "language": "cpp",
  "total_lines": 487,
  "code_lines": 356,
  "comment_lines": 89,
  "blank_lines": 42,
  
  "includes": [
    {"line": 1, "path": "\"core/TreeSitterParser.hpp\"", "is_local": true},
    {"line": 2, "path": "<spdlog/spdlog.h>", "is_local": false}
  ],
  
  "namespaces": ["mcp"],
  
  "classes": [
    {
      "name": "TreeSitterParser",
      "line": 8,
      "methods": [
        {
          "name": "TreeSitterParser",
          "type": "constructor",
          "line": 10,
          "signature": "TreeSitterParser(const string& language)",
          "lines_of_code": 12,
          "cyclomatic_complexity": 3,
          "comment": "Initialize parser with language grammar"
        },
        {
          "name": "parse_file",
          "type": "method",
          "line": 37,
          "signature": "std::optional<TSTree*> parse_file(const std::string&)",
          "lines_of_code": 15,
          "cyclomatic_complexity": 5,
          "access": "public",
          "comment": null
        }
      ],
      "member_variables": [
        {"name": "m_parser", "type": "TSParser*", "line": 5},
        {"name": "m_language", "type": "TSLanguage*", "line": 6}
      ]
    }
  ],
  
  "free_functions": [
    {
      "name": "helper_function",
      "line": 150,
      "signature": "static void helper_function(int x)",
      "is_static": true
    }
  ],
  
  "todos": [
    {"line": 45, "text": "TODO: Add error handling for malformed files"},
    {"line": 89, "text": "FIXME: Memory leak in edge case"}
  ],
  
  "complexity_metrics": {
    "average_cyclomatic_complexity": 4.2,
    "max_cyclomatic_complexity": 12,
    "most_complex_function": "parse_complex_expression"
  },
  
  "success": true
}
```

---

### **5. get_dependency_graph**
Построить граф зависимостей между файлами.

**Назначение:** Визуализация архитектуры, выявление циклических зависимостей.

**Параметры:**
```json
{
  "root_path": "src/",
  "max_depth": 3,
  "include_external": false, // включать ли системные заголовки
  "detect_cycles": true,
  "filter_patterns": ["*.cpp", "*.hpp"]
}
```

**Формат вывода:**
```json
{
  "nodes": [
    {
      "id": "src/core/TreeSitterParser.cpp",
      "type": "source",
      "classes": ["TreeSitterParser"],
      "functions_count": 5,
      "lines_of_code": 487
    },
    {
      "id": "src/core/TreeSitterParser.hpp",
      "type": "header",
      "classes": ["TreeSitterParser"],
      "is_interface": false
    }
  ],
  
  "edges": [
    {
      "from": "src/core/TreeSitterParser.cpp",
      "to": "src/core/TreeSitterParser.hpp",
      "type": "includes",
      "line": 1
    },
    {
      "from": "src/mcp/MCPServer.cpp",
      "to": "src/core/TreeSitterParser.hpp",
      "type": "includes",
      "line": 5
    }
  ],
  
  "cycles": [
    {
      "cycle": [
        "src/core/ModuleA.hpp",
        "src/core/ModuleB.hpp",
        "src/core/ModuleA.hpp"
      ],
      "severity": "warning"
    }
  ],
  
  "layers": {
    "core": ["src/core/TreeSitterParser.cpp", "src/core/ASTAnalyzer.cpp"],
    "mcp": ["src/mcp/MCPServer.cpp"],
    "utils": ["src/utils/FileSystem.cpp"]
  },
  
  "metrics": {
    "total_files": 45,
    "average_dependencies_per_file": 5.2,
    "max_dependencies": 12,
    "most_depended_on_file": "src/core/TreeSitterParser.hpp"
  },
  
  "success": true
}
```

---

### **6. search_code_patterns**
Семантический поиск паттернов кода.

**Назначение:** Найти похожие конструкции, примеры использования, антипаттерны.

**Параметры:**
```json
{
  "pattern_type": "function_calls", // "loops", "error_handling", "resource_management"
  "query": {
    "function_name": "new", // искать вызовы new без соответствующего delete
    "scope": "src/"
  },
  "anti_pattern": "memory_leak",
  "context_lines": 3
}
```

**Формат вывода:**
```json
{
  "pattern": "memory_leak",
  "matches": [
    {
      "filepath": "src/core/Parser.cpp",
      "line": 45,
      "severity": "high",
      "code_snippet": "    auto* obj = new MyObject();\n    process(obj);\n    return; // Missing delete!",
      "description": "Allocated memory not freed before return",
      "suggestion": "Use unique_ptr or ensure delete is called"
    }
  ],
  "total_matches": 7,
  "success": true
}
```

---

### **7. extract_interface**
Извлечь только сигнатуры без реализации.

**Назначение:** Уменьшить размер контекста, показывая только API.

**Параметры:**
```json
{
  "filepath": "src/core/TreeSitterParser.hpp",
  "include_private": false,
  "include_comments": true,
  "include_default_arguments": true,
  "output_format": "header" // "header", "json", "markdown"
}
```

**Формат вывода (JSON):**
```json
{
  "filepath": "src/core/TreeSitterParser.hpp",
  "namespace": "mcp",
  
  "classes": [
    {
      "name": "TreeSitterParser",
      "comment": "/**\n * Parser for C++ and Python using tree-sitter\n */",
      "base_classes": [],
      
      "public_methods": [
        {
          "signature": "TreeSitterParser(const std::string& language)",
          "is_constructor": true,
          "comment": "// Initialize with language name"
        },
        {
          "signature": "std::optional<TSTree*> parse_file(const std::string& filepath)",
          "return_type": "std::optional<TSTree*>",
          "parameters": [
            {"name": "filepath", "type": "const std::string&"}
          ],
          "is_const": false,
          "is_virtual": false,
          "comment": null
        }
      ],
      
      "protected_methods": [],
      "private_methods": []
    }
  ],
  
  "free_functions": [],
  
  "compact_representation": "class TreeSitterParser {\npublic:\n  TreeSitterParser(const std::string& language);\n  std::optional<TSTree*> parse_file(const std::string& filepath);\n  // ... 3 more methods\n};",
  
  "success": true
}
```

---

### **8. analyze_function_flow**
Анализ потока выполнения функции.

**Назначение:** Понять логику функции, найти точки входа/выхода, условные ветви.

**Параметры:**
```json
{
  "function_name": "process_request",
  "filepath": "src/mcp/MCPServer.cpp",
  "include_callgraph": true,
  "max_callgraph_depth": 2
}
```

**Формат вывода:**
```json
{
  "function": {
    "name": "process_request",
    "signature": "json process_request(const json& request)",
    "line": 45,
    "lines_of_code": 67,
    "cyclomatic_complexity": 8
  },
  
  "control_flow": {
    "entry_points": [{"line": 45, "type": "function_start"}],
    
    "branches": [
      {
        "line": 48,
        "type": "if",
        "condition": "request.contains(\"method\")",
        "branches": ["true_branch", "false_branch"]
      },
      {
        "line": 52,
        "type": "switch",
        "expression": "method",
        "cases": ["parse_file", "find_classes", "default"]
      }
    ],
    
    "loops": [
      {
        "line": 78,
        "type": "for",
        "iterator": "file : files"
      }
    ],
    
    "exit_points": [
      {"line": 95, "type": "return", "value": "result"},
      {"line": 87, "type": "return", "value": "error_response"},
      {"line": 104, "type": "throw", "exception": "std::runtime_error"}
    ]
  },
  
  "call_graph": {
    "direct_calls": [
      {"name": "parse_file", "line": 55, "filepath": "src/core/TreeSitterParser.cpp"},
      {"name": "validate_request", "line": 47, "filepath": "src/mcp/MCPServer.cpp"}
    ],
    "indirect_calls": [
      {"name": "ts_parser_parse_string", "depth": 2}
    ]
  },
  
  "variables": {
    "parameters": [
      {"name": "request", "type": "const json&", "usage_count": 5}
    ],
    "local_vars": [
      {"name": "result", "type": "json", "line": 46, "scope": "function"},
      {"name": "method", "type": "string", "line": 49, "scope": "if_block"}
    ]
  },
  
  "success": true
}
```

---

### **9. get_smart_snippet**
Умное извлечение минимального работающего фрагмента кода.

**Назначение:** Получить ровно столько кода, сколько нужно для понимания/модификации.

**Параметры:**
```json
{
  "target": {
    "type": "function", // "function", "class", "line_range"
    "name": "parse_file",
    "filepath": "src/core/TreeSitterParser.cpp"
  },
  "auto_expand": true, // автоматически добавить зависимости
  "max_tokens": 2000, // ограничение по токенам
  "prioritize": ["direct_dependencies", "type_definitions", "constants"]
}
```

**Формат вывода:**
```json
{
  "primary_snippet": {
    "filepath": "src/core/TreeSitterParser.cpp",
    "line_start": 37,
    "line_end": 52,
    "code": "std::optional<TSTree*> TreeSitterParser::parse_file(const std::string& filepath) {\n  std::ifstream file(filepath);\n  ...\n}",
    "tokens": 156
  },
  
  "supporting_snippets": [
    {
      "type": "type_definition",
      "name": "TSTree",
      "filepath": "external/tree-sitter/api.h",
      "code": "typedef struct TSTree TSTree;",
      "reason": "return_type",
      "tokens": 12
    },
    {
      "type": "constant",
      "name": "MAX_FILE_SIZE",
      "filepath": "src/core/TreeSitterParser.hpp",
      "code": "constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;",
      "reason": "used_in_function",
      "tokens": 15
    }
  ],
  
  "omitted": {
    "reason": "token_limit",
    "count": 3,
    "suggestions": ["Include helper_function if needed", "Include error handling utilities"]
  },
  
  "total_tokens": 1847,
  "completeness": 0.85, // насколько полон контекст
  
  "success": true
}
```

---

### **10. compare_implementations**
Сравнить разные реализации похожих функций/классов.

**Назначение:** Найти лучшие практики, выявить inconsistency в кодовой базе.

**Параметры:**
```json
{
  "pattern": "constructor", // или конкретные имена функций
  "scope": "src/",
  "compare_aspects": ["error_handling", "resource_management", "parameter_validation"]
}
```

**Формат вывода:**
```json
{
  "implementations": [
    {
      "name": "TreeSitterParser::TreeSitterParser",
      "filepath": "src/core/TreeSitterParser.cpp",
      "line": 10,
      "features": {
        "error_handling": "exceptions",
        "resource_management": "RAII",
        "parameter_validation": true,
        "initialization_list": true
      },
      "snippet": "TreeSitterParser::TreeSitterParser(const string& lang) : m_parser(nullptr) {\n  if (lang.empty()) throw std::invalid_argument(...);\n  ...\n}"
    },
    {
      "name": "CustomParser::CustomParser",
      "filepath": "src/parsers/CustomParser.cpp",
      "line": 8,
      "features": {
        "error_handling": "return_codes",
        "resource_management": "manual",
        "parameter_validation": false,
        "initialization_list": false
      },
      "snippet": "CustomParser::CustomParser(const string& lang) {\n  m_parser = nullptr;\n  // No validation!\n  ...\n}"
    }
  ],
  
  "inconsistencies": [
    {
      "aspect": "error_handling",
      "description": "Mixed error handling strategies: exceptions vs return codes",
      "severity": "medium",
      "affected_files": 2
    }
  ],
  
  "recommendations": [
    "Standardize on exception-based error handling",
    "Always validate constructor parameters",
    "Use RAII consistently"
  ],
  
  "success": true
}
```

---

## 🔧 Вспомогательные команды

### **11. get_metrics**
Собрать метрики кодовой базы.

```json
{
  "scope": "src/",
  "metrics": ["complexity", "coupling", "cohesion", "test_coverage_hints"]
}
```

### **12. find_definition**
Быстрый поиск определения символа.

```json
{
  "symbol": "TreeSitterParser",
  "scope": "src/"
}
```

### **13. get_call_hierarchy**
Построить иерархию вызовов (кто кого вызывает).

```json
{
  "function_name": "parse_file",
  "direction": "callers", // "callers" или "callees"
  "max_depth": 3
}
```

---

## 💡 Форматы вывода для агента

### **Компактный JSON** (по умолчанию)
Структурированные данные для программной обработки.

### **Markdown** (для документации)
```json
{
  "output_format": "markdown"
}
```

### **Plain Text** (для прямого включения в промпт)
```json
{
  "output_format": "plain_text",
  "include_line_numbers": true
}
```

### **Graph Format** (для визуализации)
```json
{
  "output_format": "mermaid" // или "dot" для Graphviz
}
```

---

## 🎬 Примеры сценариев использования

### Сценарий 1: "Добавить новый метод в класс"
```
1. get_file_summary → понять структуру класса
2. get_class_hierarchy → проверить наследование
3. find_references (к похожим методам) → найти примеры
4. get_smart_snippet → получить контекст для промпта
```

### Сценарий 2: "Рефакторинг функции"
```
1. get_symbol_context → полный контекст функции
2. find_references → найти все использования
3. analyze_function_flow → понять логику
4. get_call_hierarchy → кто вызывает
```

### Сценарий 3: "Исследование незнакомого кода"
```
1. get_dependency_graph → общая архитектура
2. get_file_summary (ключевых файлов) → оглавление
3. extract_interface → API без реализации
4. get_smart_snippet (при необходимости) → детали
```

Эти команды дадут агенту мощный инструментарий для работы с большими C++ кодовыми базами!