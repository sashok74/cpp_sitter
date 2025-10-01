# Анализ MCP Roadmap и План Реализации

**Дата анализа:** 2025-10-01
**Анализируемый документ:** `doc/scenary_mcp.md`
**Текущая версия:** tree-sitter-mcp v1.0.0

---

## 📊 Executive Summary

**Документ `scenary_mcp.md`** описывает 13 продвинутых команд для MCP сервера, ориентированных на работу AI-агентов с большими C++ кодовыми базами.

### Ключевые Выводы:

1. ✅ **Отличная концепция** - команды решают реальные проблемы AI-агентов
2. ⚠️ **Несоответствие реализации** - документ описывает будущие возможности
3. 🎯 **Четкая приоритизация возможна** - не все команды одинаково важны
4. 🔧 **Реалистичная реализация** - 70% команд можно сделать на tree-sitter
5. 📅 **План на 6-12 месяцев** - поэтапная реализация в 4 фазы

---

## 🔍 Детальный Анализ Команд

### Текущее Состояние (v1.0.0)

**Реализованные инструменты:**

| Команда | Статус | Назначение |
|---------|--------|------------|
| `parse_file` | ✅ Implemented | Метаданные файла (классы, функции, ошибки) |
| `find_classes` | ✅ Implemented | Список всех классов с локациями |
| `find_functions` | ✅ Implemented | Список всех функций с локациями |
| `execute_query` | ✅ Implemented | Кастомные tree-sitter S-expression запросы |

**Поддерживаемые языки:** C++, Python
**Batch processing:** ✅ Массивы файлов, рекурсивное сканирование директорий
**Tests:** 42/42 passing

---

## 📋 Предложенные Команды (из scenary_mcp.md)

### Tier 1: Критически Важные (Реализовать Первыми)

#### 1. `extract_interface` ⭐ **HIGHEST PRIORITY**

**Назначение:** Извлечь только сигнатуры функций/классов без реализации
**Проблема, которую решает:** Context window overflow - показать только API

**Оценка сложности:** 🟢 **Низкая** (3-5 дней)

**Технология:**
- Tree-sitter queries для function/class declarations
- Фильтрация function bodies
- Генерация компактного вывода (JSON/header/markdown)

**Что нужно:**
```cpp
// Новые query patterns
"(function_definition declarator: (function_declarator) @func_decl)"
"(class_specifier name: (_) @class_name body: (_))"
// Filter out compound_statement (function body)
```

**Ценность для AI:** 🔥 **Критическая**
- Сокращает контекст в 5-10 раз
- Позволяет показать весь API модуля
- Быстрый обзор без деталей

**Приоритет:** ⭐⭐⭐⭐⭐ (5/5)

**Готовность к реализации:** 🟢 Можем начать немедленно

---

#### 2. `find_references` ⭐

**Назначение:** Найти все использования символа в кодовой базе
**Проблема:** Понять impact изменений, найти примеры использования

**Оценка сложности:** 🟡 **Средняя** (10-14 дней)

**Технология:**
- Phase 1: Text search (grep) по имени символа
- Phase 2: Tree-sitter validation (это реально reference или строка?)
- Phase 3: Классификация (call, declaration, definition)

**Алгоритм:**
```cpp
1. Fast text search across files
2. For each match:
   - Parse file with tree-sitter
   - Locate match in AST
   - Determine node type (call_expression, identifier, etc.)
   - Extract context (parent function, line numbers)
3. Group results by file/type
4. Return structured JSON
```

**Ценность для AI:** 🔥 **Критическая**
- Понимание зависимостей
- Impact analysis перед изменениями
- Примеры использования API

**Приоритет:** ⭐⭐⭐⭐⭐ (5/5)

**Готовность:** 🟡 Требует design для эффективного поиска

---

#### 3. `get_symbol_context` ⭐

**Назначение:** Получить полный контекст символа со всеми зависимостями
**Проблема:** Собрать минимальный self-contained код для понимания/модификации

**Оценка сложности:** 🟡 **Средняя** (10-14 дней)

**Технология:**
- Tree-sitter для AST
- Include analysis для зависимостей
- Transitive dependency resolution (глубина 0-2)

**Алгоритм:**
```cpp
1. Find target symbol (function/class) in file
2. Extract symbol definition + body
3. Analyze identifiers used in body
4. For each identifier:
   - Search in current file
   - Search in #includes
   - Add to context if found
5. If context_depth > 0:
   - Recursively resolve dependencies
6. Sort by relevance, return top N
```

**Ценность для AI:** 🔥 **Критическая**
- Автоматический сбор контекста
- Минимальный но достаточный код
- Избегает manual copy-paste

**Приоритет:** ⭐⭐⭐⭐ (4/5)

**Готовность:** 🟡 Нужен дизайн для dependency resolution

---

#### 4. `get_file_summary` (Enhanced) ⭐

**Назначение:** Расширенное оглавление файла с метриками
**Проблема:** Быстрый обзор без загрузки всего кода

**Оценка сложности:** 🟢 **Низкая** (5-7 дней)

**Что добавить к существующему `parse_file`:**
- ✅ Cyclomatic complexity (подсчет if/for/while/switch)
- ✅ TODO/FIXME/HACK extraction из комментариев
- ✅ Function signatures с параметрами
- ✅ Member variables с типами
- ✅ Docstring comments

**Технология:**
- Existing tree-sitter infrastructure
- Add complexity visitor (count branch nodes)
- Regex for TODO/FIXME in comments
- Query для member_declarations

**Ценность для AI:** 🔥 **Высокая**
- Быстрый overview файла
- Метрики для оценки сложности
- TODO list для maintenance

**Приоритет:** ⭐⭐⭐⭐ (4/5)

**Готовность:** 🟢 Можем расширить существующий tool

---

### Tier 2: Важные (Реализовать После Tier 1)

#### 5. `get_class_hierarchy`

**Оценка сложности:** 🟡 **Средняя** (7-10 дней)

**Технология:**
- Parse base_clause в class_specifier
- Build inheritance tree (graph)
- Traverse tree (parent/children)
- Identify virtual methods

**Ценность:** 🔥 **Высокая** - понимание OOP структуры

**Приоритет:** ⭐⭐⭐ (3/5)

---

#### 6. `get_dependency_graph`

**Оценка сложности:** 🟡 **Средняя** (10-14 дней)

**Технология:**
- Query для #include directives
- Build directed graph
- Cycle detection (Tarjan's algorithm)
- Topological sort для layers

**Ценность:** 🔥 **Высокая** - архитектурная визуализация

**Приоритет:** ⭐⭐⭐ (3/5)

---

#### 7. `get_smart_snippet`

**Оценка сложности:** 🔴 **Высокая** (14-21 дней)

**Технология:**
- Heuristics для relevance scoring
- Token counting
- Dependency prioritization
- ML-based relevance (опционально)

**Ценность:** 🔥 **Высокая** - умная минимизация контекста

**Приоритет:** ⭐⭐⭐ (3/5)

**Проблема:** Требует эвристики и настройки

---

### Tier 3: Полезные (Долгосрочная Перспектива)

#### 8. `analyze_function_flow`

**Оценка сложности:** 🔴 **Очень высокая** (21-30 дней)

**Технология:**
- ❌ Tree-sitter не достаточно
- ✅ Требует Clang LibTooling для CFG
- Control Flow Graph построение
- Data flow analysis

**Ценность:** 🟠 **Средняя** - полезно но не критично

**Приоритет:** ⭐⭐ (2/5)

**Блокер:** Требует интеграции Clang

---

#### 9. `search_code_patterns`

**Оценка сложности:** 🔴 **Очень высокая** (21-30 дней)

**Технология:**
- ❌ Tree-sitter может найти синтаксические паттерны
- ✅ Semantic patterns требуют Clang
- Static analysis для антипаттернов
- Pattern matching на AST

**Ценность:** 🟠 **Средняя** - nice to have

**Приоритет:** ⭐⭐ (2/5)

**Альтернатива:** Интеграция с cppcheck/clang-tidy

---

#### 10. `compare_implementations`

**Оценка сложности:** 🔴 **Очень высокая** (14-21 дней)

**Технология:**
- Feature extraction из функций
- Similarity metrics
- Inconsistency detection

**Ценность:** 🟠 **Средняя** - полезно для code review

**Приоритет:** ⭐⭐ (2/5)

---

### Tier 4: Опциональные

#### 11-13. `get_metrics`, `find_definition`, `get_call_hierarchy`

**Оценка:** 🟡 Средняя сложность, 🟠 Низкая-Средняя ценность

**Примечание:**
- `find_definition` частично покрыт `find_classes/functions`
- `get_call_hierarchy` требует cross-file semantic analysis
- `get_metrics` можно добавить в `get_file_summary`

**Приоритет:** ⭐ (1/5)

---

## 📅 Предлагаемый План Реализации

### **Phase 1: Enhance Current Tools** (2-3 недели)

**Цель:** Улучшить существующие 4 инструмента без добавления новых

**Задачи:**

1. **Расширить `parse_file`** (неделя 1)
   - Add cyclomatic complexity calculation
   - Extract TODO/FIXME/HACK comments
   - Include docstring comments
   - Add function parameters info
   - Add member variables with types

2. **Улучшить `find_classes`** (неделя 1-2)
   - Add base classes (inheritance)
   - Add virtual/override methods
   - Add access specifiers
   - Add template parameters

3. **Улучшить `find_functions`** (неделя 2)
   - Add function parameters
   - Add return types
   - Add const/static qualifiers
   - Add default arguments

4. **Optimize `execute_query`** (неделя 2-3)
   - Expand predefined query library
   - Add query validation
   - Better error messages
   - Query examples in docs

**Результат:** Текущие инструменты покроют ~40% потребностей из roadmap

**Метрики успеха:**
- ✅ Все 42 теста проходят
- ✅ Добавлено 15+ новых тестов
- ✅ Documentation обновлена
- ✅ Backward compatibility сохранена

---

### **Phase 2: Implement Tier 1 Commands** (4-6 недель)

#### Sprint 1: `extract_interface` (неделя 1-2)

**Deliverables:**
- `src/tools/ExtractInterfaceTool.hpp/cpp`
- `tests/tools/ExtractInterface_test.cpp` (10+ тестов)
- Documentation в MCP_API_REFERENCE.md
- Integration с MCPServer

**Queries needed:**
```scheme
;; Function declarations without bodies
(function_definition
  declarator: (function_declarator) @decl)

;; Class declarations with method signatures only
(class_specifier
  name: (_) @name
  body: (field_declaration_list) @body)
```

**Output formats:**
- JSON (структурированный)
- Header file format (.hpp)
- Markdown (документация)

**Success criteria:**
- ✅ Extract public API from любого .cpp/.hpp файла
- ✅ Support C++ and Python
- ✅ Output size < 20% от original
- ✅ 100% test coverage

---

#### Sprint 2: `find_references` (неделя 3-4)

**Deliverables:**
- `src/tools/FindReferencesTool.hpp/cpp`
- `tests/tools/FindReferences_test.cpp` (15+ тестов)
- Performance optimization (LRU cache)
- Documentation

**Algorithm:**
```
1. Text search (grep-like) across files in scope
   - Fast filter: only files containing symbol_name

2. For each match:
   - Parse file with tree-sitter (use cache!)
   - Find match position in AST
   - Classify node type:
     * call_expression → "call"
     * function_definition → "definition"
     * declaration → "declaration"
     * comment → "comment" (if requested)
   - Extract context (3 lines before/after)

3. Group results:
   - By file: {filepath: [references]}
   - By type: {calls: N, declarations: M}

4. Return structured JSON
```

**Performance targets:**
- Search 1000 files in < 5 seconds
- Cache hit rate > 80%
- Memory usage < 500MB

**Success criteria:**
- ✅ Find all references accurately
- ✅ No false positives from strings/comments
- ✅ Classify reference type correctly
- ✅ Fast enough for large codebases

---

#### Sprint 3: `get_symbol_context` (неделя 5-6)

**Deliverables:**
- `src/tools/GetSymbolContextTool.hpp/cpp`
- `src/core/DependencyResolver.hpp/cpp` (new component)
- `tests/tools/GetSymbolContext_test.cpp` (20+ тестов)
- Documentation

**Components:**

1. **Symbol Finder**
   - Locate target symbol in file
   - Extract full definition

2. **Dependency Resolver**
   - Parse #include directives
   - Find used types/functions
   - Resolve to definitions
   - Build dependency graph

3. **Context Builder**
   - Collect symbols by depth
   - Sort by relevance
   - Limit by max_context_lines

**Dependency Resolution Strategy:**
```cpp
struct SymbolDependency {
    std::string name;
    std::string type; // "function", "class", "typedef"
    std::string filepath;
    int line;
    int depth; // 0=direct, 1=transitive
};

class DependencyResolver {
public:
    std::vector<SymbolDependency> resolve(
        const std::string& symbol_name,
        const std::string& filepath,
        int max_depth
    );

private:
    // Cache: symbol_name → filepath
    std::map<std::string, std::string> symbol_index_;

    // Cache: filepath → parsed AST
    std::map<std::string, ParsedFile> file_cache_;
};
```

**Success criteria:**
- ✅ Find all direct dependencies
- ✅ Resolve transitive dependencies (depth 1-2)
- ✅ No circular dependencies issues
- ✅ Context is minimal but complete
- ✅ Fits in max_context_lines limit

---

**Phase 2 Results:**
- 3 new powerful commands
- Coverage ~70% потребностей AI-агентов
- All implemented using tree-sitter only
- No external dependencies needed

**Tests after Phase 2:** 42 + 45 = **87 tests**

---

### **Phase 3: Implement Tier 2 Commands** (6-8 недель)

#### Sprint 4: `get_class_hierarchy` (неделя 1-3)

**Algorithm:**
```cpp
1. Scan all headers in scope
2. Extract class declarations:
   - Class name
   - Base classes (from base_clause)
   - Virtual methods
   - Member variables
3. Build inheritance tree:
   - Map: class_name → {bases, children}
4. Traverse tree:
   - Find root (target class)
   - Traverse up (parents)
   - Traverse down (descendants)
5. Identify overrides:
   - Match virtual method signatures
6. Format as JSON graph
```

**Success criteria:**
- ✅ Correct inheritance tree
- ✅ Virtual methods identified
- ✅ Overrides detected
- ✅ Max depth limit respected

---

#### Sprint 5: `get_dependency_graph` (неделя 4-6)

**Algorithm:**
```cpp
1. Parse all files in scope
2. Extract #include directives
3. Build directed graph:
   - Nodes: files
   - Edges: includes
4. Cycle detection:
   - Tarjan's strongly connected components
5. Layer detection:
   - Topological sort
   - Group by depth from roots
6. Metrics:
   - Dependencies per file
   - Most depended-on files
7. Output formats:
   - JSON (structured)
   - Mermaid (visualization)
   - Graphviz DOT
```

**Success criteria:**
- ✅ Complete dependency graph
- ✅ All cycles detected
- ✅ Layers make sense
- ✅ Visualization works

---

#### Sprint 6: Enhanced `get_file_summary` (неделя 7-8)

Final polishing of file summary with all metrics.

**Phase 3 Results:**
- 3 more commands added
- Coverage ~85% потребностей
- Still tree-sitter based
- Architectural analysis capabilities

**Tests after Phase 3:** 87 + 30 = **117 tests**

---

### **Phase 4: Advanced Analysis** (Долгосрочно, 3+ месяца)

**⚠️ Requires Clang LibTooling Integration**

Commands:
- `analyze_function_flow` (CFG)
- `search_code_patterns` (static analysis)
- `compare_implementations` (semantic diff)
- `get_call_hierarchy` (cross-TU)

**Alternative Approach:** Integrate existing tools:
- **cppcheck** for static analysis
- **clang-tidy** for pattern detection
- **include-what-you-use** for includes

**Decision:** Postpone until Phase 1-3 are complete and stable

---

## 📝 Рекомендации по Документации

### 1. Переименовать `scenary_mcp.md` → `MCP_ROADMAP.md`

**Добавить disclaimer в начало:**
```markdown
# MCP Server Roadmap

> ⚠️ **Статус документа:** Roadmap - описание будущих возможностей
>
> **Текущий API:** См. [MCP_API_REFERENCE.md](MCP_API_REFERENCE.md)
>
> **План реализации:** См. [ROADMAP_ANALYSIS.md](ROADMAP_ANALYSIS.md)
```

### 2. Добавить статусы для каждой команды

```markdown
### 1. get_symbol_context
**Статус:** ⚪ Planned (Phase 2, Sprint 3)
**Приоритет:** ⭐⭐⭐⭐⭐ (Tier 1)
**Оценка:** 10-14 дней

[Существующая спецификация...]
```

**Легенда статусов:**
- ✅ Implemented - полностью реализовано
- 🟡 In Progress - в разработке
- ⚪ Planned - запланировано
- 🔵 Research Needed - требует исследования
- ❌ Blocked - заблокировано dependency

### 3. Создать `doc/MCP_API_CURRENT.md`

Краткий документ только с текущими 4 командами + примеры для AI-агентов.

### 4. Обновить `README.md`

```markdown
## Documentation

### For Developers
- [MCP API Reference](doc/MCP_API_REFERENCE.md) - Complete current API
- [MCP Roadmap](doc/MCP_ROADMAP.md) - Future features and vision
- [Roadmap Analysis](doc/ROADMAP_ANALYSIS.md) - Implementation plan

### For Users
- [Installation Guide](doc/INSTALL_CLAUDE_CODE.md)
- [Quick Start](doc/quick-start-ru.md)
```

---

## 🎯 Quick Wins (Начать Немедленно)

### Week 1-2: `extract_interface`

**Почему именно это:**
- ✅ Простая реализация (tree-sitter queries)
- ✅ Сразу полезно для AI-агентов
- ✅ Демонстрирует подход для остальных команд
- ✅ Не требует новых зависимостей
- ✅ Можно протестировать с Claude Code сразу

**Шаги:**
1. Создать `ExtractInterfaceTool` класс
2. Написать queries для function/class без bodies
3. Реализовать 3 output формата (JSON/header/markdown)
4. Написать 10+ тестов
5. Интегрировать с MCPServer
6. Обновить документацию
7. Протестировать в Claude Code

**Время:** 1-2 недели
**Результат:** Первая новая команда готова!

---

## 📊 Метрики Успеха

### Phase 1 (Enhance Tools)
- ✅ Все существующие тесты проходят
- ✅ Добавлено 15+ новых тестов
- ✅ Cyclomatic complexity работает
- ✅ TODO extraction работает
- ✅ Backward compatibility 100%

### Phase 2 (Tier 1 Commands)
- ✅ 3 новые команды реализованы
- ✅ 45+ новых тестов добавлено
- ✅ Общее покрытие тестами > 85%
- ✅ Performance: поиск 1000 файлов < 5 сек
- ✅ Протестировано в Claude Code
- ✅ Документация полная

### Phase 3 (Tier 2 Commands)
- ✅ 3 архитектурных команды работают
- ✅ Граф зависимостей строится корректно
- ✅ Cycle detection работает
- ✅ Visualizations генерируются

---

## ⚠️ Риски и Митигация

### Риск 1: Производительность на больших кодовых базах

**Проблема:** Поиск по 10000+ файлам может быть медленным

**Митигация:**
- Использовать LRU cache для parsed файлов
- Индексация символов (pre-build index)
- Параллельный парсинг (thread pool)
- Incremental updates (watch file changes)

### Риск 2: Семантический анализ ограничен

**Проблема:** Tree-sitter дает только syntax, не semantics

**Митигация:**
- Phase 1-3 не требуют semantics
- Phase 4 использует Clang LibTooling
- Альтернатива: интеграция cppcheck/clang-tidy

### Риск 3: Сложность dependency resolution

**Проблема:** Транзитивные зависимости могут быть глубокими

**Митигация:**
- Ограничение max_depth (0-2)
- Ограничение max_context_lines
- Heuristics для relevance

---

## 🎓 Lessons Learned

### Что работает хорошо:
- ✅ Tree-sitter excellent для syntax analysis
- ✅ Batch processing очень полезен
- ✅ Multi-language support (C++/Python) востребован
- ✅ JSON output удобен для AI

### Что можно улучшить:
- 🔧 Нужен symbol index для быстрого поиска
- 🔧 Cache strategy критична для performance
- 🔧 Documentation должна быть синхронизирована
- 🔧 Testing на больших кодовых базах нужен раньше

---

## 📚 Справочная Информация

### Технологии

**Tree-sitter:**
- Version: 0.22.6
- Grammars: tree-sitter-cpp 0.22.0, tree-sitter-python 0.21.0
- Query language: S-expressions

**Build System:**
- CMake 3.20+
- C++20 standard
- Conan 2.x для зависимостей

**Dependencies:**
- nlohmann/json для JSON
- spdlog для логирования
- CLI11 для аргументов
- GTest для тестирования

### Полезные Ссылки

- [Tree-sitter Documentation](https://tree-sitter.github.io/tree-sitter/)
- [Tree-sitter Playground](https://tree-sitter.github.io/tree-sitter/playground)
- [C++ Grammar](https://github.com/tree-sitter/tree-sitter-cpp)
- [Python Grammar](https://github.com/tree-sitter/tree-sitter-python)
- [MCP Specification](https://modelcontextprotocol.io/)

---

## ✅ Заключение

**Документ `scenary_mcp.md` является отличным roadmap для развития проекта.**

**Рекомендуемые действия:**

1. ✅ **Immediate:** Переименовать в `MCP_ROADMAP.md` с disclaimer
2. ✅ **Week 1-2:** Реализовать `extract_interface` (quick win)
3. ✅ **Month 1:** Complete Phase 1 (enhance tools)
4. ✅ **Month 2-3:** Complete Phase 2 (Tier 1 commands)
5. ✅ **Month 4-5:** Complete Phase 3 (Tier 2 commands)
6. 🔵 **Month 6+:** Research Phase 4 (advanced analysis with Clang)

**После Phase 2 (~3 месяца):**
- 7 powerful commands available
- 70% потребностей AI-агентов покрыты
- Production-ready для integration с Claude Code
- Strong foundation для Phase 3-4

**Проект имеет четкое видение и реалистичный план реализации! 🚀**
