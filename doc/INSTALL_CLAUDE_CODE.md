# Установка tree-sitter-mcp в Claude Code

Пошаговая инструкция по подключению tree-sitter-mcp сервера к Claude Code (современная версия с MCP поддержкой).

---

## 🎯 Что это дает

После установки вы сможете использовать tree-sitter-mcp как MCP инструмент в Claude Code для анализа C++ и Python кода:

```
# В чате Claude Code можете просто попросить
"analyze this C++ file and tell me what classes it contains"
"find all virtual functions in src/core/"
"what decorators are used in this Python file?"
```

Claude Code автоматически использует MCP инструменты когда это необходимо.

---

## 📋 Предварительные требования

### 1. Установите Claude Code

Скачайте и установите Claude Code с официального сайта:
- **Linux/macOS**: https://claude.ai/download
- **Windows**: https://claude.ai/download

Убедитесь что используете версию с поддержкой MCP (версия 0.8.0+).

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

tree-sitter-mcp --version
# Должно вывести: tree-sitter-mcp version 1.0.0
```

---

## 🔧 Настройка MCP сервера

Claude Code поддерживает **три уровня конфигурации** для MCP серверов:

1. **User config** (глобальная, для всех проектов): `~/.claude.json`
2. **Project config** (общая для всех, через git): `.mcp.json` в корне проекта
3. **Local config** (приватная, не коммитится): `~/.claude.json` с привязкой к проекту

**Рекомендация:** Используйте **project config** (.mcp.json) для удобства команды.

### Вариант 1: Project Config (рекомендуется)

Создайте файл `.mcp.json` в корне проекта:

```bash
cd /path/to/your/project
nano .mcp.json
```

**Содержимое `.mcp.json`:**
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

**Сохраните файл** (Ctrl+O, Enter, Ctrl+X в nano).

**Преимущества:**
- ✅ Работает для всех участников проекта (файл коммитится в git)
- ✅ Не нужно настраивать для каждого проекта отдельно
- ✅ Просто делиться с коллегами

**Добавьте в git:**
```bash
git add .mcp.json
git commit -m "Add tree-sitter-mcp configuration"
```

### Вариант 2: User Config (глобальная настройка)

Если вы хотите использовать tree-sitter-mcp **во всех своих проектах**, добавьте в глобальный конфиг.

**ВНИМАНИЕ:** Файл `~/.claude.json` — это большой JSON файл управляемый Claude Code. Ручное редактирование может сломать настройки!

**Безопасный способ:**
1. Откройте Claude Code
2. Нажмите `/mcp` в чате
3. Выберите "Manage MCP servers"
4. Нажмите "Add MCP server"
5. Заполните:
   - **Name**: tree-sitter
   - **Command**: /usr/local/bin/tree-sitter-mcp
   - **Args**: --log-level info
   - **Scope**: User (all projects)

### Вариант 3: Использование скрипта установки

Проект содержит скрипт для автоматической установки:

```bash
cd /home/raa/projects/cpp-sitter
# Этот скрипт создаст .mcp.json в текущем проекте
echo '{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    }
  }
}' > .mcp.json

# Проверка
cat .mcp.json | python3 -m json.tool
```

---

## ✅ Проверка установки

### Шаг 1: Перезапустите Claude Code

**Важно:** После изменения конфигурации MCP нужно перезапустить Claude Code:
- Закройте все окна Claude Code
- Откройте Claude Code снова

### Шаг 2: Проверьте список MCP серверов

В Claude Code введите команду:
```
/mcp
```

Вы должны увидеть:
```
✔ tree-sitter - connected
```

Если сервер не подключился, проверьте статус:
- ❌ **not connected**: Проблема с запуском сервера
- ⚠️  **error**: Ошибка в конфигурации

### Шаг 3: Проверьте доступные инструменты

В Claude Code введите:
```
/mcp
```
Выберите "tree-sitter" → "View details"

Вы должны увидеть 4 инструмента:
- ✅ **parse_file**: Парсинг и метаданные файла
- ✅ **find_classes**: Поиск всех классов
- ✅ **find_functions**: Поиск всех функций
- ✅ **execute_query**: Выполнение custom tree-sitter запросов

### Шаг 4: Тест с реальным файлом

Создайте тестовый C++ файл:
```bash
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
```

**Протестируйте в Claude Code:**
```
Please analyze /tmp/test.cpp and tell me:
1. What classes are defined?
2. What methods does the class have?
3. Are there any virtual methods?
```

Claude должен использовать MCP инструменты и ответить:
> The file contains one class `Calculator` with two methods:
> - `add(int a, int b)` at line 5
> - `multiply(int a, int b)` at line 9 (virtual)

### Шаг 5: Тест с Python файлом

Создайте тестовый Python файл:
```bash
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
```

**Протестируйте в Claude Code:**
```
Analyze /tmp/test.py and tell me:
1. What class is defined?
2. What methods are there?
3. Are there any async methods?
4. Are there any decorators?
```

Claude должен найти:
> Class `DataProcessor` with methods:
> - `__init__` (constructor)
> - `validate` (static method, decorator @staticmethod)
> - `fetch_data` (async method)

---

## 🎨 Примеры использования

### Пример 1: Анализ C++ проекта

```
Show me all classes in src/core/ directory
```

Claude автоматически использует `find_classes` и вернет список всех классов с номерами строк.

### Пример 2: Поиск виртуальных функций

```
Find all virtual functions in src/core/TreeSitterParser.cpp
```

Claude использует `execute_query` с tree-sitter запросом для виртуальных функций.

### Пример 3: Анализ Python декораторов

```
What decorators are used in tests/fixtures/with_decorators.py?
```

Claude найдет все декораторы (@staticmethod, @property, @custom, etc).

### Пример 4: Проверка синтаксиса

```
Check if src/main.cpp has any syntax errors
```

Claude использует `parse_file` и сообщит о наличии ошибок парсинга.

### Пример 5: Комплексный анализ

```
Analyze the entire src/ directory and give me:
1. Total number of C++ classes
2. Classes with virtual functions
3. Classes with templates
```

Claude выполнит batch-анализ всех файлов в директории.

---

## 🐛 Устранение проблем

### Проблема 1: "tree-sitter not connected"

**Симптомы:**
В `/mcp` списке видите `❌ tree-sitter - not connected`

**Решение:**
```bash
# Проверьте, что сервер запускается
/usr/local/bin/tree-sitter-mcp --help

# Проверьте MCP протокол
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level error

# Если не работает, переустановите
cd /home/raa/projects/cpp-sitter/build
sudo cmake --install .
```

### Проблема 2: "Command not found"

**Симптомы:**
```
Error: command "/usr/local/bin/tree-sitter-mcp" not found
```

**Решение:**
```bash
# Проверьте установку
which tree-sitter-mcp

# Если не найдено, переустановите
cd /home/raa/projects/cpp-sitter/build
sudo cmake --install .

# Проверьте путь в конфигурации
cat .mcp.json
# Убедитесь что "command" соответствует реальному пути
```

### Проблема 3: "Invalid JSON in .mcp.json"

**Симптомы:**
Claude Code не видит MCP сервер после добавления .mcp.json

**Решение:**
```bash
# Проверьте синтаксис JSON
python3 -m json.tool .mcp.json

# Если ошибка, исправьте файл
nano .mcp.json

# Базовая структура:
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    }
  }
}
```

### Проблема 4: "Permission denied"

**Симптомы:**
```
Error: EACCES: permission denied, access '/usr/local/bin/tree-sitter-mcp'
```

**Решение:**
```bash
# Проверьте права
ls -la /usr/local/bin/tree-sitter-mcp

# Должно быть:
# -rwxr-xr-x  1 root root ... /usr/local/bin/tree-sitter-mcp

# Если нет, исправьте
sudo chmod +x /usr/local/bin/tree-sitter-mcp
```

### Проблема 5: "Tools not appearing"

**Симптомы:**
MCP сервер подключен, но Claude не использует инструменты

**Решение:**
1. **Перезапустите Claude Code** (важно!)
2. Проверьте список инструментов: `/mcp` → tree-sitter → View details
3. Попробуйте явно попросить: "Use tree-sitter-mcp to analyze this file"

### Проблема 6: ".mcp.json не подхватывается"

**Симптомы:**
Создали .mcp.json в проекте, но Claude Code его не видит

**Решение:**
```bash
# Убедитесь что файл в правильной директории
ls -la /path/to/your/project/.mcp.json

# Проверьте что Claude Code открыт в этой директории
pwd

# Перезапустите Claude Code
# Файл .mcp.json читается только при запуске
```

---

## 🔍 Отладка

### Включение подробного логирования

Измените уровень логирования в конфигурации:

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

Уровни логирования:
- `error`: Только критические ошибки (минимум вывода)
- `warn`: Предупреждения и ошибки
- `info`: Общая информация (рекомендуется)
- `debug`: Подробная отладочная информация
- `trace`: Максимальная детализация (для разработки)

### Проверка MCP протокола вручную

```bash
# Список инструментов
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp --log-level error

# Вызов parse_file
echo '{
  "jsonrpc":"2.0",
  "id":2,
  "method":"tools/call",
  "params":{
    "name":"parse_file",
    "arguments":{"filepath":"/tmp/test.cpp"}
  }
}' | tree-sitter-mcp --log-level error

# Вызов find_classes
echo '{
  "jsonrpc":"2.0",
  "id":3,
  "method":"tools/call",
  "params":{
    "name":"find_classes",
    "arguments":{"filepath":"/tmp/test.cpp"}
  }
}' | tree-sitter-mcp --log-level error
```

---

## 📚 Дополнительные ресурсы

- **[MCP API Reference](MCP_API_REFERENCE.md)** - Полное описание всех инструментов
- **[Quick Start (RU)](quick-start-ru.md)** - Быстрый старт на русском языке
- **[README](../README.md)** - Основная документация проекта
- **[Claude Code MCP Docs](https://docs.claude.com/en/docs/claude-code/mcp)** - Официальная документация MCP

---

## 🎯 Быстрая справка команд

```bash
# Проверка установки
which tree-sitter-mcp
tree-sitter-mcp --version

# Проверка конфигурации
cat .mcp.json                    # Project config
cat ~/.claude.json | grep -A 10 mcpServers  # User config (осторожно!)

# Тестирование сервера
echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | tree-sitter-mcp

# В Claude Code
/mcp                             # Управление MCP серверами
```

---

## ✨ Готово!

После выполнения всех шагов вы можете использовать tree-sitter-mcp в Claude Code:

**Проверьте, что всё работает:**
- ✅ `tree-sitter-mcp --version` работает
- ✅ Файл `.mcp.json` создан в проекте
- ✅ `/mcp` в Claude Code показывает `✔ tree-sitter - connected`
- ✅ Claude может анализировать C++ и Python файлы

**Удачного анализа кода! 🚀**
