# Установка tree-sitter-mcp в Claude Code MCP

Пошаговая инструкция по установке и настройке tree-sitter-mcp сервера для работы с Claude Code.

---

## 📋 Предварительные требования

- C++20 компилятор (GCC 11+, Clang 14+, или MSVC 19.30+)
- CMake 3.20+
- Conan 2.x
- Claude Code Desktop (скачать с https://claude.ai/download)

---

## 🚀 Способ 1: Автоматическая установка (рекомендуется)

### Шаг 1: Сборка проекта

```bash
cd /home/raa/projects/cpp-sitter

# Настройка Conan (первый раз)
conan profile detect --force

# Установка зависимостей
conan install . --output-folder=build --build=missing

# Сборка
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Проверка тестов
ctest --output-on-failure
```

**Ожидаемый результат:** Все 42 теста должны пройти успешно.

### Шаг 2: Установка в систему

```bash
# Из директории build/
sudo cmake --install .
```

**Установленные файлы:**
- `/usr/local/bin/tree-sitter-mcp` - исполняемый файл сервера
- `/usr/local/share/tree-sitter-mcp/install_claude_agent.sh` - скрипт настройки

### Шаг 3: Настройка Claude Code

```bash
# Запуск скрипта автоматической настройки
sudo bash /usr/local/share/tree-sitter-mcp/install_claude_agent.sh
```

**Что делает скрипт:**
1. Регистрирует MCP сервер в `~/.config/claude/claude_desktop_config.json`
2. Устанавливает sub-agent конфигурацию `~/.claude/agents/ts-strategist.json`
3. Настраивает правильные пути и параметры

### Шаг 4: Перезапуск Claude Code

```bash
# Linux
pkill -9 claude
# Затем запустите Claude Code снова из меню приложений

# macOS
# Закройте Claude Code через Command+Q
# Затем запустите снова
```

### Шаг 5: Проверка установки

**Проверка 1: Сервер установлен**
```bash
which tree-sitter-mcp
# Должно вывести: /usr/local/bin/tree-sitter-mcp

tree-sitter-mcp --help
# Должно показать справку
```

**Проверка 2: Конфигурация Claude**
```bash
# Linux
cat ~/.config/claude/claude_desktop_config.json

# macOS
cat ~/Library/Application\ Support/Claude/claude_desktop_config.json
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

**Проверка 3: Sub-agent настроен**
```bash
ls -la ~/.claude/agents/ts-strategist.json
cat ~/.claude/agents/ts-strategist.json
```

**Проверка 4: Тест работы сервера**
```bash
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level error
```

Должен вернуть JSON со списком 4 инструментов.

---

## 🛠️ Способ 2: Ручная настройка (продвинутый)

### Шаг 1: Создание конфигурационного файла

**Linux:**
```bash
mkdir -p ~/.config/claude
nano ~/.config/claude/claude_desktop_config.json
```

**macOS:**
```bash
mkdir -p ~/Library/Application\ Support/Claude
nano ~/Library/Application\ Support/Claude/claude_desktop_config.json
```

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

**Примечание:** Если файл уже существует и содержит другие MCP серверы, добавьте `tree-sitter` внутри секции `mcpServers`:

```json
{
  "mcpServers": {
    "existing-server": {
      "command": "/path/to/other/server",
      "args": ["--some-arg"]
    },
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    }
  }
}
```

### Шаг 2: Установка sub-agent (опционально)

Sub-agent позволяет использовать специализированного агента `@ts-strategist` в Claude Code CLI.

```bash
mkdir -p ~/.claude/agents
cp /home/raa/projects/cpp-sitter/claude/agents/ts-strategist.json ~/.claude/agents/
```

### Шаг 3: Перезапуск Claude Code

Закройте и снова откройте Claude Code Desktop для применения изменений.

---

## ✅ Проверка работы

### Тест 1: Проверка в Claude Code Desktop

1. Откройте Claude Code Desktop
2. Создайте новый чат
3. Введите сообщение, которое требует анализа кода (например, "analyze this file: /path/to/file.cpp")
4. Claude должен автоматически использовать MCP инструменты

### Тест 2: Проверка через CLI (если установлен sub-agent)

```bash
# Список доступных инструментов
claude @ts-strategist "list available tools"

# Анализ C++ файла
claude @ts-strategist "analyze src/core/TreeSitterParser.cpp"

# Анализ Python файла
claude @ts-strategist "analyze tests/fixtures/simple_class.py"

# Поиск классов в директории
claude @ts-strategist "find all classes in src/"

# Поиск виртуальных функций
claude @ts-strategist "find all virtual functions in src/core/"

# Поиск async функций (Python)
claude @ts-strategist "find all async functions in tests/fixtures/"
```

### Тест 3: Прямая проверка MCP протокола

```bash
# Создайте тестовый C++ файл
cat > /tmp/test.cpp << 'EOF'
#include <iostream>

class Calculator {
public:
    int add(int a, int b) {
        return a + b;
    }
};

int main() {
    Calculator calc;
    return 0;
}
EOF

# Протестируйте parse_file
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"/tmp/test.cpp"}}}' | tree-sitter-mcp --log-level error

# Протестируйте find_classes
echo '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"/tmp/test.cpp"}}}' | tree-sitter-mcp --log-level error
```

---

## 🐛 Устранение проблем

### Проблема 1: "tree-sitter-mcp: command not found"

**Причина:** Сервер не установлен в систему.

**Решение:**
```bash
cd /home/raa/projects/cpp-sitter/build
sudo cmake --install .
which tree-sitter-mcp
```

### Проблема 2: Claude Code не видит MCP сервер

**Причина:** Конфигурация не загружена или неправильная.

**Решение:**
```bash
# Проверьте конфигурацию
cat ~/.config/claude/claude_desktop_config.json

# Проверьте путь к серверу
which tree-sitter-mcp

# Проверьте, что сервер запускается
tree-sitter-mcp --help

# Полностью перезапустите Claude Code
pkill -9 claude
# Запустите снова
```

### Проблема 3: Ошибка "permission denied"

**Причина:** Нет прав на исполнение.

**Решение:**
```bash
sudo chmod +x /usr/local/bin/tree-sitter-mcp
ls -la /usr/local/bin/tree-sitter-mcp
```

### Проблема 4: Сервер запускается, но не отвечает

**Причина:** Проблема с JSON-RPC протоколом.

**Решение:**
```bash
# Включите debug логирование
tree-sitter-mcp --log-level debug < /dev/null

# Проверьте логи Claude Code
# Linux: ~/.config/claude/logs/
# macOS: ~/Library/Logs/Claude/
```

### Проблема 5: "Failed to parse file" ошибки

**Причина:** Проблемы с правами доступа к файлам.

**Решение:**
```bash
# Проверьте права на файл
ls -la /path/to/file.cpp

# Проверьте, что файл существует
cat /path/to/file.cpp

# Попробуйте с абсолютным путём
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"'$(realpath /path/to/file.cpp)'"}}}' | tree-sitter-mcp --log-level debug
```

### Проблема 6: Sub-agent не работает

**Причина:** Конфигурация sub-agent отсутствует или неправильная.

**Решение:**
```bash
# Проверьте наличие файла
ls -la ~/.claude/agents/ts-strategist.json

# Если отсутствует, скопируйте
cp /home/raa/projects/cpp-sitter/claude/agents/ts-strategist.json ~/.claude/agents/

# Проверьте содержимое
cat ~/.claude/agents/ts-strategist.json
```

---

## 🔧 Настройка уровня логирования

Вы можете изменить уровень логирования в конфигурации:

```json
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "debug"]
    }
  }
}
```

**Доступные уровни:**
- `trace` - максимально подробный вывод
- `debug` - отладочная информация
- `info` - основная информация (по умолчанию)
- `warn` - только предупреждения
- `error` - только ошибки
- `critical` - только критичные ошибки

---

## 📚 Дополнительные ресурсы

- **Основная документация:** [README.md](../README.md)
- **MCP API Reference:** [MCP_API_REFERENCE.md](MCP_API_REFERENCE.md)
- **Quick Start (RU):** [quick-start-ru.md](quick-start-ru.md)
- **Build Instructions:** [BUILD.md](../BUILD.md)

---

## 🎯 Примеры использования

### Пример 1: Анализ одного файла

```bash
claude @ts-strategist "What classes are defined in src/core/TreeSitterParser.cpp?"
```

### Пример 2: Анализ директории

```bash
claude @ts-strategist "Find all virtual functions in src/core/"
```

### Пример 3: Мультиязычный анализ

```bash
# C++ анализ
claude @ts-strategist "Show me the class hierarchy in src/"

# Python анализ
claude @ts-strategist "Find all async functions in tests/fixtures/"
```

### Пример 4: Поиск паттернов

```bash
claude @ts-strategist "Are there any factory patterns in src/tools/?"
```

### Пример 5: Анализ зависимостей

```bash
claude @ts-strategist "What headers are included in src/core/TreeSitterParser.cpp?"
```

---

## ✨ Готово!

После выполнения всех шагов у вас должен быть полностью функциональный MCP сервер tree-sitter-mcp, интегрированный с Claude Code.

**Проверьте:**
- ✅ `which tree-sitter-mcp` возвращает путь
- ✅ `tree-sitter-mcp --help` работает
- ✅ Конфигурация Claude существует и корректна
- ✅ Claude Code может использовать MCP инструменты
- ✅ Sub-agent `@ts-strategist` доступен (если установлен)

**Наслаждайтесь мощным анализом кода с помощью tree-sitter и Claude! 🎉**
