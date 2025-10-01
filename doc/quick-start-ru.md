# 🚀 Быстрый старт cpp-treesitter-mcp

## Описание

**cpp-treesitter-mcp** - это высокопроизводительный MCP (Model Context Protocol) сервер на C++20 для глубокого анализа C++ кода с использованием tree-sitter. Интегрируется с Claude Code CLI для интеллектуального анализа исходного кода.

---

## 📋 Предварительные требования

### Обязательные компоненты

```bash
# Проверьте наличие необходимых инструментов
gcc --version    # Требуется >= 11.0 (или clang >= 14.0)
cmake --version  # Требуется >= 3.20
conan --version  # Требуется >= 2.0
git --version
```

### Установка отсутствующих компонентов

**Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install build-essential cmake git python3-pip
pip install conan
```

**Arch Linux:**
```bash
sudo pacman -S base-devel cmake git python-pip
pip install conan
```

---

## 🔧 Установка MCP сервера

### Вариант 1: Автоматическая установка (рекомендуется)

```bash
# Перейдите в директорию проекта
cd /home/raa/projects/cpp-sitter

# Запустите скрипт быстрой установки
chmod +x scripts/quick_install.sh
./scripts/quick_install.sh
```

Скрипт выполнит:
- Определение профиля Conan
- Установку зависимостей
- Сборку проекта в режиме Release
- Запуск всех тестов
- Установку в систему (требует sudo)
- Настройку интеграции с Claude Code

### Вариант 2: Ручная установка

```bash
# 1. Настройка Conan
conan profile detect --force

# 2. Установка зависимостей
conan install . --output-folder=build --build=missing -s build_type=Release

# 3. Конфигурация CMake
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
         -DCMAKE_BUILD_TYPE=Release

# 4. Сборка (используем все ядра CPU)
cmake --build . -j$(nproc)

# 5. Запуск тестов
ctest --output-on-failure

# 6. Установка в систему
sudo cmake --install .

# 7. Настройка Claude Code
sudo bash /usr/local/share/cpp-treesitter-mcp/install_claude_agent.sh
```

### Проверка установки

```bash
# Проверьте, что бинарник установлен
which mcp_stdio_server
# Должно вывести: /usr/local/bin/mcp_stdio_server

# Проверьте версию и справку
mcp_stdio_server --help
```

---

## ✅ Ручная проверка работы сервера

### Тест 1: Список доступных инструментов

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | \
  mcp_stdio_server --log-level error
```

**Ожидаемый результат:** JSON с 4 инструментами:
- `parse_file` - анализ метаданных файла
- `find_classes` - поиск всех классов
- `find_functions` - поиск всех функций
- `execute_query` - выполнение произвольных tree-sitter запросов

### Тест 2: Парсинг тестового файла

```bash
# Создайте тестовый C++ файл
cat > /tmp/test.cpp << 'EOF'
#include <iostream>

class Calculator {
public:
    int add(int a, int b) {
        return a + b;
    }

    int subtract(int a, int b) {
        return a - b;
    }
};

int main() {
    Calculator calc;
    std::cout << calc.add(5, 3) << std::endl;
    return 0;
}
EOF

# Выполните парсинг
echo '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"/tmp/test.cpp"}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

**Ожидаемый результат:**
```json
{
    "id": 2,
    "jsonrpc": "2.0",
    "result": {
        "content": [
            {
                "text": "{\"class_count\":1,\"filepath\":\"/tmp/test.cpp\",\"function_count\":3,\"has_errors\":false,\"include_count\":1,\"success\":true}",
                "type": "text"
            }
        ]
    }
}
```

### Тест 3: Поиск классов

```bash
echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"/tmp/test.cpp"}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

**Ожидаемый результат:** Список найденных классов с номерами строк.

### Тест 4: Поиск функций

```bash
echo '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"find_functions","arguments":{"filepath":"/tmp/test.cpp"}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

### Тест 5: Произвольный tree-sitter запрос

```bash
# Найти все include директивы
echo '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"execute_query","arguments":{"filepath":"/tmp/test.cpp","query":"(preproc_include) @include"}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

### Тест 6: Массив файлов (Batch обработка)

```bash
# Создайте дополнительные тестовые файлы
cat > /tmp/test_a.cpp << 'EOF'
class Alpha { void methodA(); };
EOF

cat > /tmp/test_b.cpp << 'EOF'
class Beta { void methodB(); };
EOF

# Проанализируйте несколько файлов одновременно
echo '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":["/tmp/test_a.cpp","/tmp/test_b.cpp"]}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

**Ожидаемый результат:**
```json
{
  "success": true,
  "total_files": 2,
  "processed_files": 2,
  "failed_files": 0,
  "results": [
    {"filepath": "/tmp/test_a.cpp", "class_count": 1, ...},
    {"filepath": "/tmp/test_b.cpp", "class_count": 1, ...}
  ]
}
```

### Тест 7: Рекурсивное сканирование директории

```bash
# Создайте структуру директорий
mkdir -p /tmp/test_project/subdir
cat > /tmp/test_project/main.cpp << 'EOF'
class Main { void run(); };
EOF

cat > /tmp/test_project/subdir/utils.cpp << 'EOF'
class Utils { void helper(); };
EOF

# Просканируйте всю директорию рекурсивно
echo '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"/tmp/test_project","recursive":true}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

**Ожидаемый результат:** Найдены все классы из всех файлов в директории и поддиректориях.

### Тест 8: Фильтрация по паттернам файлов

```bash
# Добавьте заголовочный файл
cat > /tmp/test_project/header.hpp << 'EOF'
class Header { void declare(); };
EOF

# Найдите классы только в .hpp файлах
echo '{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"/tmp/test_project","recursive":true,"file_patterns":["*.hpp"]}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

**Ожидаемый результат:** Найден только класс `Header` из файла header.hpp, файлы .cpp проигнорированы.

---

## 🤖 Проверка в Claude Code

### Шаг 1: Проверка установки Claude Code CLI

```bash
# Проверьте, что Claude Code установлен
claude --version
```

Если Claude Code не установлен, следуйте инструкциям на https://claude.ai/download

### Шаг 2: Проверка конфигурации MCP сервера

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
    "cpp-treesitter": {
      "command": "/usr/local/bin/mcp_stdio_server",
      "args": ["--log-level", "info"]
    }
  }
}
```

Если конфигурация отсутствует, запустите:
```bash
sudo bash /usr/local/share/cpp-treesitter-mcp/install_claude_agent.sh
```

### Шаг 3: Проверка конфигурации sub-agent

```bash
# Проверьте файл ts-strategist.json
ls -la ~/.claude/agents/ts-strategist.json
cat ~/.claude/agents/ts-strategist.json
```

### Шаг 4: Перезапуск Claude Code

После настройки конфигурации **перезапустите Claude Code Desktop** для применения изменений.

### Шаг 5: Тестирование в Claude Code CLI

```bash
# Тест 1: Список доступных инструментов
claude @ts-strategist "list available tools"
```

**Ожидаемый ответ:** Claude должен перечислить 4 доступных инструмента.

```bash
# Тест 2: Анализ тестового файла
claude @ts-strategist "analyze /tmp/test.cpp"
```

**Ожидаемый ответ:** Подробный анализ файла с указанием количества классов, функций и возможных проблем.

```bash
# Тест 3: Поиск классов в директории (рекурсивно)
claude @ts-strategist "find all classes in /home/raa/projects/cpp-sitter/src/"
```

```bash
# Тест 4: Выполнение custom query
claude @ts-strategist "execute query to find all virtual functions in /tmp/test.cpp"
```

### Шаг 6: Тестирование в Claude Code Desktop

1. Откройте **Claude Code Desktop**
2. Создайте новый чат
3. Введите: `@ts-strategist help`
4. Claude должен ответить описанием доступных возможностей

**Примеры запросов:**
- `@ts-strategist analyze src/main.cpp`
- `@ts-strategist find all classes in src/`
- `@ts-strategist show me all virtual methods`
- `@ts-strategist what includes are used in this file: src/core/TreeSitterParser.hpp`

---

## 🐛 Устранение неполадок

### Проблема 1: "mcp_stdio_server: command not found"

**Решение:**
```bash
# Проверьте установку
which mcp_stdio_server

# Если не найден, переустановите
cd /home/raa/projects/cpp-sitter/build
sudo cmake --install .
```

### Проблема 2: Claude Code не видит MCP сервер

**Решение:**
```bash
# 1. Проверьте путь к серверу в конфигурации
cat ~/.config/claude/claude_desktop_config.json

# 2. Проверьте, что сервер запускается
mcp_stdio_server --help

# 3. Проверьте права доступа
ls -la /usr/local/bin/mcp_stdio_server

# 4. Перезапустите Claude Code Desktop
pkill -9 claude  # или через GUI

# 5. Запустите тестовый скрипт
bash /home/raa/projects/cpp-sitter/tests/integration/test_claude_integration.sh
```

### Проблема 3: Ошибка "Failed to parse source code"

**Причины и решения:**
- **Файл не существует**: проверьте путь к файлу
- **Нет прав доступа**: проверьте права на чтение файла
- **Невалидный C++**: проверьте синтаксис в файле

```bash
# Проверьте файл вручную
cat /path/to/file.cpp

# Проверьте права
ls -la /path/to/file.cpp

# Попробуйте распарсить напрямую
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"/path/to/file.cpp"}}}' | \
  mcp_stdio_server --log-level debug
```

### Проблема 4: Сборка не удается

**Решение:**
```bash
# Полная пересборка
cd /home/raa/projects/cpp-sitter
rm -rf build
mkdir build

# Переустановите зависимости
conan install . --output-folder=build --build=missing -s build_type=Release

# Соберите заново
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake
cmake --build . -j$(nproc)
```

### Проблема 5: Тесты не проходят

```bash
# Запустите тесты с подробным выводом
cd /home/raa/projects/cpp-sitter/build
ctest --output-on-failure --verbose

# Запустите конкретный тест
./tests/core/core_tests --gtest_filter="TreeSitterParserTest.*"
```

### Проблема 6: "No C++ files found" при сканировании директории

**Причины и решения:**
- **Директория пуста**: проверьте наличие файлов
- **Неверные паттерны**: убедитесь, что file_patterns соответствуют вашим файлам
- **Рекурсия отключена**: установите `"recursive": true`

```bash
# Проверьте файлы в директории
ls -R /path/to/directory/*.cpp

# Попробуйте с явным указанием паттернов
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"/path/to/directory","recursive":true,"file_patterns":["*.cpp","*.hpp","*.h"]}}}' | \
  mcp_stdio_server --log-level debug
```

### Проблема 7: Batch обработка слишком медленная

**Оптимизация:**
- Используйте file_patterns для фильтрации только нужных файлов
- Отключите рекурсию если она не нужна: `"recursive": false`
- Кэширование работает автоматически при повторных запросах

```bash
# Оптимизированный запрос - только .hpp в корневой директории
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":"src/","recursive":false,"file_patterns":["*.hpp"]}}}' | \
  mcp_stdio_server --log-level error
```

---

## 📚 Дополнительные примеры использования

### Пример 1: Анализ всего проекта (Batch mode)

```bash
# Анализ всей директории src/ одним запросом (быстрее чем по файлу)
claude @ts-strategist "analyze all files in src/ and show summary"

# Или напрямую через MCP
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"src/","recursive":true}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

### Пример 2: Анализ нескольких конкретных файлов

```bash
# Массив путей для точечного анализа
echo '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"find_classes","arguments":{"filepath":["src/core/ASTAnalyzer.cpp","src/mcp/MCPServer.cpp"]}}}' | \
  mcp_stdio_server --log-level error | python3 -m json.tool
```

### Пример 3: Поиск виртуальных методов

```bash
claude @ts-strategist "find all virtual methods in src/core/"
```

### Пример 4: Анализ зависимостей через includes

```bash
claude @ts-strategist "show all includes in src/core/TreeSitterParser.cpp"
```

### Пример 5: Поиск шаблонных классов

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"execute_query","arguments":{"filepath":"/tmp/test.cpp","query":"(template_declaration (class_specifier name: (type_identifier) @name))"}}}' | \
  mcp_stdio_server --log-level error
```

---

## 📖 Tree-sitter Query Примеры

### Базовый синтаксис

Tree-sitter использует S-expression запросы:

```scheme
; Найти все классы
(class_specifier
  name: (type_identifier) @class_name)

; Найти все функции
(function_definition
  declarator: (function_declarator
    declarator: (identifier) @func_name))

; Найти виртуальные методы
(function_definition
  (virtual_specifier)
  declarator: (function_declarator) @virtual_func)

; Найти include директивы
(preproc_include) @include

; Найти namespace
(namespace_definition
  name: (identifier) @namespace_name)

; Найти template классы
(template_declaration
  (class_specifier
    name: (type_identifier) @template_class))
```

### Использование в Claude Code

```bash
claude @ts-strategist 'execute this query: "(class_specifier name: (type_identifier) @name)" on file src/core/ASTAnalyzer.hpp'
```

---

## 🎯 Рекомендуемый рабочий процесс

### Для анализа нового проекта

1. **Первичный анализ структуры**
   ```bash
   claude @ts-strategist "analyze the structure of src/"
   ```

2. **Поиск всех классов**
   ```bash
   claude @ts-strategist "find all classes in src/"
   ```

3. **Анализ иерархии**
   ```bash
   claude @ts-strategist "find all classes with virtual methods"
   ```

4. **Поиск проблемных участков**
   ```bash
   claude @ts-strategist "check for syntax errors in src/"
   ```

### Для рефакторинга

1. Найдите целевые классы/функции
2. Проанализируйте их структуру
3. Используйте custom queries для специфичного поиска
4. Проверьте изменения после рефакторинга

---

## 📞 Поддержка

### Документация

- **Основная документация**: `/home/raa/projects/cpp-sitter/README.md`
- **Инструкции по сборке**: `/home/raa/projects/cpp-sitter/BUILD.md`
- **Техническая спецификация**: `/home/raa/projects/cpp-sitter/doc/tz.md`
- **Контекст для Claude**: `/home/raa/projects/cpp-sitter/claude/CLAUDE.md`

### Полезные ссылки

- Tree-sitter документация: https://tree-sitter.github.io/tree-sitter/
- MCP Protocol: https://modelcontextprotocol.io/
- Claude Code: https://claude.ai/code

### Репортинг проблем

При возникновении проблем:

1. Проверьте логи сервера: `mcp_stdio_server --log-level debug`
2. Запустите интеграционные тесты: `ctest --output-on-failure`
3. Проверьте конфигурацию Claude Code
4. Создайте issue с подробным описанием проблемы

---

## ✅ Контрольный список готовности

Убедитесь, что все пункты выполнены:

- [ ] Все зависимости установлены (gcc, cmake, conan)
- [ ] Проект успешно собран (`cmake --build .`)
- [ ] Все тесты проходят (`ctest` - 33/33)
- [ ] Сервер установлен (`which mcp_stdio_server`)
- [ ] Ручные тесты работают (8 сценариев: single/array/directory/patterns)
- [ ] Claude Code CLI установлен (`claude --version`)
- [ ] MCP сервер зарегистрирован в конфигурации Claude
- [ ] Sub-agent ts-strategist настроен
- [ ] Тесты в Claude Code работают (`@ts-strategist list tools`)

**Если все пункты выполнены - поздравляем! Система готова к работе! 🎉**

---

**Версия документа**: 1.0
**Дата**: 2025-10-01
**Проект**: cpp-treesitter-mcp
