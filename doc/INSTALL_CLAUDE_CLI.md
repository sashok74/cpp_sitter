# Установка tree-sitter-mcp в Claude Code CLI

Пошаговая инструкция по подключению tree-sitter-mcp сервера к Claude Code CLI (командная строка).

---

## 🎯 Что это дает

После установки вы сможете использовать:

```bash
# Прямой доступ к MCP инструментам через Claude CLI
claude "analyze this file: src/main.cpp"

# Специализированный sub-agent для анализа кода
claude @ts-strategist "find all classes in src/"
claude @ts-strategist "what virtual functions are in src/core/"
```

---

## 📋 Предварительные требования

### 1. Установите Claude Code CLI

```bash
# Проверьте, установлен ли Claude CLI
claude --version

# Если нет, установите:
# Следуйте инструкциям на https://github.com/anthropics/claude-cli
```

### 2. Соберите и установите tree-sitter-mcp

```bash
cd /home/raa/projects/cpp-sitter

# Сборка
conan install . --output-folder=build --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Тестирование
ctest --output-on-failure

# Установка в систему
sudo cmake --install .
```

**Проверка:**
```bash
which tree-sitter-mcp
# Должно вывести: /usr/local/bin/tree-sitter-mcp
```

---

## 🔧 Вариант 1: Автоматическая настройка (рекомендуется)

### Шаг 1: Запустите скрипт установки

```bash
sudo bash /usr/local/share/tree-sitter-mcp/install_claude_agent.sh
```

**Что делает скрипт:**

1. Определяет вашу ОС (Linux/macOS)
2. Создает конфигурационный файл Claude Code
3. Регистрирует MCP сервер `tree-sitter`
4. Устанавливает sub-agent `ts-strategist`

### Шаг 2: Проверка конфигурации

**Проверьте созданную конфигурацию:**
```bash
cat ~/.claude-code/mcp_config.json
```

**Ожидаемое содержимое:**
```json
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    }
  }
}
```

**Проверьте синтаксис JSON:**
```bash
python3 -m json.tool ~/.claude-code/mcp_config.json
```

### Шаг 3: Проверка sub-agent

```bash
cat ~/.claude/agents/ts-strategist.json
```

**Ожидаемое содержимое:**
```json
{
  "name": "ts-strategist",
  "description": "Tree-sitter code analysis specialist",
  "mcp_server": "tree-sitter",
  "system_prompt": "You are a code analysis expert..."
}
```

### Шаг 4: Тестирование

```bash
# Тест 1: Проверка доступности инструментов
claude "list all available MCP tools"

# Тест 2: Использование через sub-agent
claude @ts-strategist "list your capabilities"

# Тест 3: Анализ файла
claude @ts-strategist "analyze /tmp/test.cpp"
```

---

## 🛠️ Вариант 2: Ручная настройка

### Шаг 1: Создайте конфигурацию MCP сервера

**Для Claude Code CLI (командная строка):**

```bash
mkdir -p ~/.claude-code
nano ~/.claude-code/mcp_config.json
```

**ВАЖНО:** Для Claude Code CLI используется `~/.claude-code/mcp_config.json`,
а НЕ `~/.config/claude/claude_desktop_config.json` (который для Desktop app)

**Содержимое файла:**
```json
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    }
  }
}
```

**Сохраните файл:** Ctrl+O, Enter, Ctrl+X (в nano)

### Шаг 2: Создайте конфигурацию sub-agent

```bash
mkdir -p ~/.claude/agents
nano ~/.claude/agents/ts-strategist.json
```

**Содержимое файла:**
```json
{
  "name": "ts-strategist",
  "description": "Tree-sitter based code analysis specialist for C++ and Python. Analyzes code structure, finds classes, functions, virtual methods, async functions, decorators, and more.",
  "mcp_server": "tree-sitter",
  "instructions": [
    "You are an expert in static code analysis using tree-sitter.",
    "You can analyze C++ and Python code.",
    "Available tools: parse_file, find_classes, find_functions, execute_query.",
    "Always use absolute paths when analyzing files.",
    "Provide clear, structured analysis with line numbers.",
    "For C++: focus on classes, virtual functions, templates, includes.",
    "For Python: focus on classes, functions, async functions, decorators."
  ]
}
```

**Сохраните файл:** Ctrl+O, Enter, Ctrl+X

### Шаг 3: Проверьте права доступа

```bash
# Проверьте, что файлы созданы
ls -la ~/.config/claude/claude_desktop_config.json
ls -la ~/.claude/agents/ts-strategist.json

# Проверьте, что сервер исполняемый
ls -la /usr/local/bin/tree-sitter-mcp
```

---

## ✅ Проверка установки

### Тест 1: Прямая проверка сервера

```bash
# Проверка запуска
tree-sitter-mcp --help

# Проверка MCP протокола
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level error
```

**Ожидаемый результат:** JSON со списком 4 инструментов (parse_file, find_classes, find_functions, execute_query).

### Тест 2: Проверка через Claude CLI

```bash
# Создайте тестовый файл
cat > /tmp/test.cpp << 'EOF'
#include <iostream>

class Calculator {
public:
    int add(int a, int b) {
        return a + b;
    }

    virtual int multiply(int a, int b) {
        return a * b;
    }
};

int main() {
    Calculator calc;
    std::cout << calc.add(2, 3) << std::endl;
    return 0;
}
EOF

# Тест базового анализа
claude "analyze the file /tmp/test.cpp and tell me what classes it contains"

# Тест через sub-agent
claude @ts-strategist "analyze /tmp/test.cpp"
```

**Ожидаемый результат:** Claude должен найти класс `Calculator` с методами `add` и виртуальным методом `multiply`.

### Тест 3: Анализ Python файла

```bash
# Создайте тестовый Python файл
cat > /tmp/test.py << 'EOF'
import asyncio

class DataProcessor:
    def __init__(self):
        self.data = []

    @staticmethod
    def validate(value):
        return value > 0

    async def fetch_data(self):
        await asyncio.sleep(0.1)
        return "data"
EOF

# Тест анализа
claude @ts-strategist "analyze /tmp/test.py and find all methods"
```

**Ожидаемый результат:** Claude должен найти класс `DataProcessor` с методами `__init__`, `validate` (static), и `fetch_data` (async).

### Тест 4: Анализ директории

```bash
# Анализ проекта
claude @ts-strategist "find all classes in /home/raa/projects/cpp-sitter/src/core/"
```

**Ожидаемый результат:** Список всех классов в директории src/core/ с указанием файлов и номеров строк.

---

## 🎨 Примеры использования

### Пример 1: Общий анализ через Claude

```bash
claude "I need to understand the structure of src/main.cpp. What classes and functions does it have?"
```

Claude автоматически использует MCP инструменты для анализа.

### Пример 2: Специализированный анализ через sub-agent

```bash
# Поиск классов
claude @ts-strategist "find all classes in src/"

# Поиск виртуальных функций
claude @ts-strategist "find all virtual functions in src/core/"

# Поиск шаблонов
claude @ts-strategist "find all template classes in src/"

# Анализ зависимостей
claude @ts-strategist "what headers are included in src/core/TreeSitterParser.cpp"
```

### Пример 3: Python анализ

```bash
# Поиск async функций
claude @ts-strategist "find all async functions in tests/fixtures/"

# Поиск декораторов
claude @ts-strategist "what decorators are used in tests/fixtures/with_decorators.py"

# Поиск импортов
claude @ts-strategist "analyze imports in tests/fixtures/with_imports.py"
```

### Пример 4: Batch анализ

```bash
# Анализ нескольких файлов
claude @ts-strategist "compare classes in src/core/TreeSitterParser.cpp and src/core/QueryEngine.cpp"

# Анализ директории рекурсивно
claude @ts-strategist "give me a summary of all classes in src/"
```

### Пример 5: Кастомные запросы

```bash
# Поиск паттернов
claude @ts-strategist "are there any singleton patterns in src/tools/"

# Анализ архитектуры
claude @ts-strategist "explain the class hierarchy in src/core/"
```

---

## 🐛 Устранение проблем

### Проблема 1: "MCP server not found"

**Симптомы:**
```
Error: MCP server 'tree-sitter' not found
```

**Решение:**
```bash
# Проверьте конфигурацию
cat ~/.config/claude/claude_desktop_config.json

# Убедитесь, что ключ называется "tree-sitter"
# Проверьте путь к команде
which tree-sitter-mcp

# Если путь отличается, обновите конфигурацию
```

### Проблема 2: "@ts-strategist not found"

**Симптомы:**
```
Error: Agent 'ts-strategist' not found
```

**Решение:**
```bash
# Проверьте наличие файла sub-agent
ls -la ~/.claude/agents/ts-strategist.json

# Если отсутствует, создайте его
cp /home/raa/projects/cpp-sitter/claude/agents/ts-strategist.json ~/.claude/agents/

# Проверьте содержимое
cat ~/.claude/agents/ts-strategist.json
```

### Проблема 3: "Failed to execute tool"

**Симптомы:**
```
Error executing tool: parse_file
```

**Решение:**
```bash
# Проверьте, что сервер запускается
tree-sitter-mcp --help

# Проверьте права доступа
ls -la /usr/local/bin/tree-sitter-mcp

# Проверьте логи с debug уровнем
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level debug
```

### Проблема 4: "File not found" при анализе

**Симптомы:**
```
Error: Failed to open file: src/main.cpp
```

**Решение:**
```bash
# Используйте абсолютные пути
claude @ts-strategist "analyze $(pwd)/src/main.cpp"

# Или используйте realpath
claude @ts-strategist "analyze $(realpath src/main.cpp)"

# Проверьте, что файл существует
ls -la src/main.cpp
```

### Проблема 5: Медленная работа

**Симптомы:**
Анализ занимает много времени.

**Решение:**
```bash
# Уменьшите уровень логирования
nano ~/.config/claude/claude_desktop_config.json

# Измените на:
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "error"]
    }
  }
}

# Используйте file_patterns для фильтрации
claude @ts-strategist "find classes in src/ but only in .hpp files"
```

### Проблема 6: Конфликт с другими MCP серверами

**Симптомы:**
Другие MCP серверы перестали работать.

**Решение:**
```bash
# Проверьте синтаксис JSON
cat ~/.config/claude/claude_desktop_config.json | python3 -m json.tool

# Правильный формат с несколькими серверами:
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    },
    "other-server": {
      "command": "/path/to/other/server",
      "args": []
    }
  }
}
```

---

## 🔍 Отладка

### Включение подробного логирования

```bash
# Измените конфигурацию
nano ~/.config/claude/claude_desktop_config.json

# Установите debug уровень
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "debug"]
    }
  }
}
```

### Ручная проверка MCP протокола

```bash
# Список инструментов
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level error

# Вызов parse_file
echo '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"/tmp/test.cpp"}}}' | tree-sitter-mcp --log-level error

# Вызов find_classes
echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"/tmp/test.cpp"}}}' | tree-sitter-mcp --log-level error
```

### Проверка логов Claude CLI

```bash
# Логи обычно находятся в:
# Linux: ~/.cache/claude/logs/
# macOS: ~/Library/Logs/claude/

# Посмотрите последние логи
ls -lt ~/.cache/claude/logs/ | head
tail -f ~/.cache/claude/logs/latest.log
```

---

## 📚 Дополнительные ресурсы

- **MCP API Reference:** [MCP_API_REFERENCE.md](MCP_API_REFERENCE.md)
- **Быстрый старт (RU):** [quick-start-ru.md](quick-start-ru.md)
- **Установка в Desktop:** [INSTALL_CLAUDE_MCP.md](INSTALL_CLAUDE_MCP.md)
- **Основная документация:** [README.md](../README.md)

---

## 🎯 Быстрая справка команд

```bash
# Проверка установки
which tree-sitter-mcp
tree-sitter-mcp --help
claude --version

# Проверка конфигурации
cat ~/.config/claude/claude_desktop_config.json
cat ~/.claude/agents/ts-strategist.json

# Базовый анализ
claude @ts-strategist "analyze FILE"
claude @ts-strategist "find all classes in DIR"
claude @ts-strategist "find all virtual functions in DIR"

# Python анализ
claude @ts-strategist "find all async functions in FILE"
claude @ts-strategist "what decorators are used in FILE"

# Отладка
tree-sitter-mcp --log-level debug < /dev/null
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp
```

---

## ✨ Готово!

После выполнения всех шагов вы можете использовать tree-sitter-mcp через Claude Code CLI:

**Прямое использование:**
```bash
claude "analyze src/main.cpp"
```

**Через специализированного агента:**
```bash
claude @ts-strategist "find all classes in src/"
```

**Проверьте, что всё работает:**
- ✅ `tree-sitter-mcp --help` работает
- ✅ Конфигурация в `~/.config/claude/claude_desktop_config.json` корректна
- ✅ Sub-agent в `~/.claude/agents/ts-strategist.json` установлен
- ✅ `claude @ts-strategist "list your capabilities"` возвращает ответ

**Удачного анализа кода! 🚀**
