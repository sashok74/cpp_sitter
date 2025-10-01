# Архитектура MCP Сервера: Руководство для Разработчиков

Полное руководство по созданию MCP (Model Context Protocol) сервера на C++20. Этот документ основан на реальной реализации tree-sitter-mcp и описывает все необходимые компоненты для создания собственного MCP сервера.

---

## 📋 Содержание

1. [Обзор MCP Протокола](#обзор-mcp-протокола)
2. [Требования к Реализации](#требования-к-реализации)
3. [Архитектура Трёх Слоёв](#архитектура-трёх-слоёв)
4. [Протокол Инициализации](#протокол-инициализации)
5. [Структура Проекта](#структура-проекта)
6. [Реализация Транспорта](#реализация-транспорта)
7. [Реализация Сервера](#реализация-сервера)
8. [Реализация Инструментов](#реализация-инструментов)
9. [Интеграция с Claude Code](#интеграция-с-claude-code)
10. [Примеры Кода](#примеры-кода)
11. [Тестирование](#тестирование)

---

## Обзор MCP Протокола

### Что такое MCP?

**Model Context Protocol (MCP)** — это открытый протокол для интеграции AI-ассистентов с внешними инструментами и источниками данных. Разработан Anthropic для Claude Code и других AI-приложений.

### Ключевые Концепции

- **Транспорт**: Механизм передачи сообщений (stdio, HTTP/SSE, WebSocket)
- **JSON-RPC 2.0**: Протокол обмена сообщениями
- **Инструменты (Tools)**: Функции, которые AI может вызывать
- **Ресурсы (Resources)**: Данные, которые AI может читать (опционально)
- **Промпты (Prompts)**: Шаблоны для AI (опционально)

### Версия Протокола

Текущая версия: **2024-11-05**

Документация: https://spec.modelcontextprotocol.io/

---

## Требования к Реализации

### Обязательные Компоненты

1. **Транспортный слой**: Stdio (stdin/stdout) — минимально необходимый
2. **JSON-RPC 2.0**: Сервер должен корректно обрабатывать JSON-RPC запросы
3. **Метод initialize**: ОБЯЗАТЕЛЬНЫЙ для handshake с клиентом
4. **Уведомление notifications/initialized**: Принимать без ответа
5. **Хотя бы одна возможность**: Tools, Resources или Prompts

### Рекомендуемые Технологии

- **C++20** или новее (для concepts, ranges, designated initializers)
- **nlohmann/json**: Библиотека для работы с JSON
- **spdlog**: Логирование
- **CLI11**: Парсинг аргументов командной строки
- **CMake 3.20+**: Система сборки
- **GTest**: Юнит-тестирование

---

## Архитектура Трёх Слоёв

### Диаграмма Архитектуры

```
┌─────────────────────────────────────────────────────────┐
│  Слой 3: MCP Tools (src/tools/)                         │
│  ├─ ParseFileTool                                       │
│  ├─ FindClassesTool                                     │
│  ├─ FindFunctionsTool                                   │
│  └─ ExecuteQueryTool                                    │
│                                                          │
│  Каждый инструмент:                                     │
│  - Предоставляет статический get_info() → ToolInfo     │
│  - Реализует execute(args) → json result               │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  Слой 2: MCP Protocol (src/mcp/)                        │
│  ├─ MCPServer                                           │
│  │  ├─ handle_initialize()         [ОБЯЗАТЕЛЬНО]       │
│  │  ├─ handle_initialized_notification()               │
│  │  ├─ handle_tools_list()                             │
│  │  ├─ handle_tools_call()                             │
│  │  └─ create_error_response()                         │
│  │                                                      │
│  ├─ ITransport (abstract interface)                    │
│  └─ StdioTransport (stdin/stdout)                      │
│     ├─ read_message() → json                           │
│     └─ write_message(json)                             │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────┐
│  Слой 1: Core Logic (src/core/)                         │
│  ├─ Ваша бизнес-логика                                  │
│  ├─ Парсеры, анализаторы, процессоры                   │
│  └─ Независимые от MCP компоненты                      │
│                                                          │
│  Примеры:                                               │
│  - TreeSitterParser (tree-sitter wrapper)               │
│  - QueryEngine (S-expression queries)                   │
│  - ASTAnalyzer (high-level API)                         │
└─────────────────────────────────────────────────────────┘
```

### Принципы Разделения

1. **Слой 1 (Core)**: Не знает о MCP, можно использовать отдельно
2. **Слой 2 (Protocol)**: Реализует MCP, не знает о бизнес-логике
3. **Слой 3 (Tools)**: Связывает Core и Protocol

---

## Протокол Инициализации

### КРИТИЧЕСКИ ВАЖНО: Handshake Sequence

```
Client (Claude Code)              Server (ваше приложение)
      │                                    │
      │  1. initialize request             │
      ├────────────────────────────────────>│
      │    {                                │
      │      "method": "initialize",        │
      │      "params": {                    │
      │        "protocolVersion": "2024-11-05",
      │        "clientInfo": {...}          │
      │      }                              │
      │    }                                │
      │                                     │
      │  2. initialize response             │
      │<────────────────────────────────────┤
      │    {                                │
      │      "result": {                    │
      │        "protocolVersion": "2024-11-05",
      │        "capabilities": {            │
      │          "tools": {}                │
      │        },                           │
      │        "serverInfo": {              │
      │          "name": "my-server",       │
      │          "version": "1.0.0"         │
      │        }                            │
      │      }                              │
      │    }                                │
      │                                     │
      │  3. notifications/initialized       │
      ├────────────────────────────────────>│
      │    {                                │
      │      "method": "notifications/initialized"
      │    }                                │
      │    (NO RESPONSE!)                   │
      │                                     │
      │  4. Normal operation                │
      │  tools/list, tools/call, etc.       │
      │<───────────────────────────────────>│
```

### Ошибка "Method not found: initialize"

Если вы видите эту ошибку:
```json
{"error": {"code": -32601, "message": "Method not found: initialize"}}
```

**Решение**: Добавьте обработчик `initialize` в ваш `handle_request()` метод!

---

## Структура Проекта

### Минимальная Файловая Структура

```
my-mcp-server/
├── CMakeLists.txt              # Корневой CMake
├── conanfile.txt               # Зависимости (опционально)
├── .mcp.json                   # Конфигурация для Claude Code
├── README.md
│
├── src/
│   ├── CMakeLists.txt
│   │
│   ├── core/                   # Слой 1: Бизнес-логика
│   │   ├── CMakeLists.txt
│   │   ├── MyCore.hpp
│   │   └── MyCore.cpp
│   │
│   ├── mcp/                    # Слой 2: Протокол
│   │   ├── CMakeLists.txt
│   │   ├── ITransport.hpp      # Интерфейс
│   │   ├── StdioTransport.hpp
│   │   ├── StdioTransport.cpp
│   │   ├── MCPServer.hpp       # Основной сервер
│   │   └── MCPServer.cpp
│   │
│   ├── tools/                  # Слой 3: Инструменты
│   │   ├── CMakeLists.txt
│   │   ├── MyTool.hpp
│   │   └── MyTool.cpp
│   │
│   └── main_stdio.cpp          # Точка входа
│
├── tests/                      # Тесты
│   ├── CMakeLists.txt
│   ├── core/
│   ├── mcp/
│   └── tools/
│
└── doc/                        # Документация
    └── MCP_API_REFERENCE.md
```

### Файл .mcp.json (для Claude Code)

```json
{
  "mcpServers": {
    "my-server": {
      "command": "/usr/local/bin/my-mcp-server",
      "args": ["--log-level", "info"]
    }
  }
}
```

---

## Реализация Транспорта

### ITransport.hpp - Абстрактный Интерфейс

```cpp
#pragma once
#include <nlohmann/json.hpp>

namespace my_mcp {

using json = nlohmann::json;

/**
 * @brief Интерфейс для транспортного слоя MCP
 *
 * Реализации: StdioTransport, SSETransport, WebSocketTransport
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /**
     * @brief Прочитать одно JSON-RPC сообщение
     * @return JSON объект или null при EOF
     */
    virtual json read_message() = 0;

    /**
     * @brief Записать JSON-RPC сообщение
     * @param message JSON объект для отправки
     */
    virtual void write_message(const json& message) = 0;

    /**
     * @brief Проверить открыт ли транспорт
     */
    virtual bool is_open() const = 0;
};

} // namespace my_mcp
```

### StdioTransport.hpp - Реализация для Stdio

```cpp
#pragma once
#include "ITransport.hpp"
#include <iostream>

namespace my_mcp {

/**
 * @brief Транспорт через stdin/stdout
 *
 * Формат: Одно JSON-сообщение на строку (newline-delimited JSON)
 *
 * Важно:
 * - Каждое сообщение завершается '\n'
 * - Stderr используется для логов (НЕ stdout!)
 * - EOF на stdin означает закрытие соединения
 */
class StdioTransport : public ITransport {
public:
    StdioTransport() = default;

    json read_message() override {
        std::string line;
        if (!std::getline(std::cin, line)) {
            // EOF или ошибка чтения
            return json();
        }

        if (line.empty()) {
            return json();
        }

        try {
            return json::parse(line);
        } catch (const json::parse_error& e) {
            throw std::runtime_error(
                std::string("JSON parse error: ") + e.what()
            );
        }
    }

    void write_message(const json& message) override {
        // ВАЖНО: Всё в одну строку + '\n'
        std::cout << message.dump() << std::endl;
        std::cout.flush();
    }

    bool is_open() const override {
        return std::cin.good();
    }
};

} // namespace my_mcp
```

**КРИТИЧЕСКИ ВАЖНО:**
- ✅ Логи в **stderr** (`spdlog` настроен правильно)
- ✅ Сообщения в **stdout** (только JSON)
- ✅ Каждое сообщение на **отдельной строке**
- ❌ НЕ пишите ничего кроме JSON в stdout!

---

## Реализация Сервера

### MCPServer.hpp - Основной Класс Сервера

```cpp
#pragma once
#include "ITransport.hpp"
#include <functional>
#include <map>
#include <memory>
#include <atomic>

namespace my_mcp {

/**
 * @brief Метаданные MCP инструмента
 */
struct ToolInfo {
    std::string name;
    std::string description;
    json input_schema;  // JSON Schema для валидации аргументов
};

/**
 * @brief Функция-обработчик инструмента
 */
using ToolHandler = std::function<json(const json& args)>;

/**
 * @brief MCP Сервер с поддержкой JSON-RPC 2.0
 *
 * Обязательные методы:
 * - initialize
 * - notifications/initialized
 * - tools/list
 * - tools/call
 */
class MCPServer {
public:
    explicit MCPServer(std::unique_ptr<ITransport> transport);

    // Регистрация инструментов
    void register_tool(const ToolInfo& info, ToolHandler handler);

    // Главный цикл сервера
    void run();
    void stop();

private:
    // Обработка запросов
    json handle_request(const json& request);

    // ОБЯЗАТЕЛЬНЫЕ методы протокола
    json handle_initialize(const json& params);
    void handle_initialized_notification(const json& params);

    // Методы для инструментов
    json handle_tools_list();
    json handle_tools_call(const json& params);

    // Утилиты
    json create_error_response(const json& id, int code,
                              const std::string& message);

    std::unique_ptr<ITransport> transport_;
    std::map<std::string, ToolInfo> tools_;
    std::map<std::string, ToolHandler> handlers_;
    std::atomic<bool> running_{false};
    bool initialized_{false};
};

} // namespace my_mcp
```

### MCPServer.cpp - Реализация

```cpp
#include "MCPServer.hpp"
#include <spdlog/spdlog.h>

namespace my_mcp {

MCPServer::MCPServer(std::unique_ptr<ITransport> transport)
    : transport_(std::move(transport)) {
    if (!transport_) {
        throw std::invalid_argument("Transport cannot be null");
    }
    spdlog::info("MCPServer initialized");
}

void MCPServer::register_tool(const ToolInfo& info, ToolHandler handler) {
    if (info.name.empty()) {
        throw std::invalid_argument("Tool name cannot be empty");
    }
    if (!handler) {
        throw std::invalid_argument("Tool handler cannot be null");
    }

    tools_[info.name] = info;
    handlers_[info.name] = std::move(handler);
    spdlog::info("Registered tool: {}", info.name);
}

void MCPServer::run() {
    running_ = true;
    spdlog::info("MCPServer starting main loop");

    while (running_ && transport_->is_open()) {
        try {
            json request = transport_->read_message();

            // EOF или пустое сообщение
            if (request.empty() || request.is_null()) {
                spdlog::info("Received empty message, stopping server");
                break;
            }

            json response = handle_request(request);

            // Уведомления не требуют ответа
            if (!response.empty() && !response.is_null()) {
                transport_->write_message(response);
            }

        } catch (const std::exception& e) {
            spdlog::error("Error in main loop: {}", e.what());
        }
    }

    running_ = false;
    spdlog::info("MCPServer stopped");
}

void MCPServer::stop() {
    spdlog::info("MCPServer stop requested");
    running_ = false;
}

json MCPServer::handle_request(const json& request) {
    // Валидация JSON-RPC 2.0
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return create_error_response(
            json(), -32600,
            "Invalid Request: missing or invalid jsonrpc field"
        );
    }

    json id = request.value("id", json());

    if (!request.contains("method")) {
        return create_error_response(
            id, -32600,
            "Invalid Request: missing method field"
        );
    }

    std::string method = request["method"];
    json params = request.value("params", json::object());

    spdlog::debug("Handling request: method={}, id={}",
                  method, id.dump());

    try {
        // КРИТИЧЕСКИ ВАЖНО: initialize должен быть ПЕРВЫМ!
        if (method == "initialize") {
            json result = handle_initialize(params);
            initialized_ = true;
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", result}
            };
        }
        else if (method == "notifications/initialized") {
            // Уведомление - НЕ отправляем ответ
            handle_initialized_notification(params);
            return json();  // Пустой ответ
        }
        else if (method == "tools/list") {
            json result = handle_tools_list();
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", result}
            };
        }
        else if (method == "tools/call") {
            json result = handle_tools_call(params);
            return {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"result", result}
            };
        }
        else {
            return create_error_response(
                id, -32601,
                "Method not found: " + method
            );
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling method {}: {}", method, e.what());
        return create_error_response(
            id, -32603,
            std::string("Internal error: ") + e.what()
        );
    }
}

json MCPServer::handle_initialize(const json& params) {
    spdlog::info("Handling initialize request");

    // Логируем информацию о клиенте
    if (params.contains("clientInfo")) {
        std::string client_name = params["clientInfo"].value("name", "unknown");
        std::string client_version = params["clientInfo"].value("version", "unknown");
        spdlog::info("Client: {} version {}", client_name, client_version);
    }

    // Возвращаем capabilities сервера
    return {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"tools", json::object()}  // Мы поддерживаем инструменты
            // Опционально:
            // {"resources", json::object()},
            // {"prompts", json::object()}
        }},
        {"serverInfo", {
            {"name", "my-mcp-server"},
            {"version", "1.0.0"}
        }}
    };
}

void MCPServer::handle_initialized_notification(const json& params) {
    spdlog::info("Client sent initialized notification, server is ready");
    // Здесь можно выполнить post-init настройки
    // НЕ возвращаем ответ - это уведомление!
}

json MCPServer::handle_tools_list() {
    json tools_array = json::array();

    for (const auto& [name, info] : tools_) {
        tools_array.push_back({
            {"name", info.name},
            {"description", info.description},
            {"inputSchema", info.input_schema}
        });
    }

    spdlog::debug("Returning {} tools", tools_array.size());

    return {
        {"tools", tools_array}
    };
}

json MCPServer::handle_tools_call(const json& params) {
    // Валидация параметров
    if (!params.contains("name")) {
        throw std::runtime_error("Missing 'name' in tools/call params");
    }

    std::string tool_name = params["name"];
    json arguments = params.value("arguments", json::object());

    spdlog::debug("Calling tool: {}", tool_name);

    // Поиск обработчика
    auto handler_it = handlers_.find(tool_name);
    if (handler_it == handlers_.end()) {
        throw std::runtime_error("Unknown tool: " + tool_name);
    }

    // Выполнение инструмента
    json result = handler_it->second(arguments);

    // MCP требует формат с "content"
    return {
        {"content", json::array({
            {
                {"type", "text"},
                {"text", result.dump()}
            }
        })}
    };
}

json MCPServer::create_error_response(const json& id, int code,
                                     const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}

} // namespace my_mcp
```

### Коды Ошибок JSON-RPC 2.0

| Код    | Значение           | Когда использовать                    |
|--------|--------------------|---------------------------------------|
| -32700 | Parse error        | Невалидный JSON                       |
| -32600 | Invalid Request    | Отсутствует "method" или "jsonrpc"    |
| -32601 | Method not found   | Неизвестный метод                     |
| -32602 | Invalid params     | Неправильные параметры метода         |
| -32603 | Internal error     | Внутренняя ошибка сервера             |

---

## Реализация Инструментов

### Базовая Структура Инструмента

```cpp
#pragma once
#include "mcp/MCPServer.hpp"
#include "core/MyCore.hpp"
#include <memory>

namespace my_mcp {

/**
 * @brief Пример MCP инструмента
 *
 * Каждый инструмент должен:
 * 1. Предоставлять статический get_info() с JSON Schema
 * 2. Реализовать execute(args) → json result
 */
class MyTool {
public:
    explicit MyTool(std::shared_ptr<MyCore> core);

    /**
     * @brief Метаданные инструмента
     * @return ToolInfo с name, description, input_schema
     */
    static ToolInfo get_info();

    /**
     * @brief Выполнить инструмент
     * @param args JSON объект с аргументами
     * @return JSON результат
     * @throws std::runtime_error при ошибке выполнения
     */
    json execute(const json& args);

private:
    std::shared_ptr<MyCore> core_;
};

} // namespace my_mcp
```

### Реализация Инструмента

```cpp
#include "MyTool.hpp"
#include <spdlog/spdlog.h>

namespace my_mcp {

MyTool::MyTool(std::shared_ptr<MyCore> core)
    : core_(std::move(core)) {
    if (!core_) {
        throw std::invalid_argument("Core cannot be null");
    }
}

ToolInfo MyTool::get_info() {
    return {
        .name = "my_tool",
        .description = "Does something useful with files",
        .input_schema = {
            {"type", "object"},
            {"properties", {
                {"filepath", {
                    {"type", "string"},
                    {"description", "Path to file to process"}
                }},
                {"option", {
                    {"type", "boolean"},
                    {"description", "Optional flag"},
                    {"default", false}
                }}
            }},
            {"required", json::array({"filepath"})}
        }
    };
}

json MyTool::execute(const json& args) {
    // Валидация аргументов
    if (!args.contains("filepath")) {
        throw std::runtime_error("Missing required argument: filepath");
    }

    std::string filepath = args["filepath"];
    bool option = args.value("option", false);

    spdlog::debug("MyTool executing: filepath={}, option={}",
                  filepath, option);

    try {
        // Вызов бизнес-логики
        auto result = core_->process(filepath, option);

        // Возврат результата
        return {
            {"success", true},
            {"result", result},
            {"filepath", filepath}
        };

    } catch (const std::exception& e) {
        spdlog::error("MyTool failed: {}", e.what());
        return {
            {"success", false},
            {"error", e.what()},
            {"filepath", filepath}
        };
    }
}

} // namespace my_mcp
```

### JSON Schema для Валидации

MCP использует [JSON Schema](https://json-schema.org/) для описания аргументов:

```json
{
  "type": "object",
  "properties": {
    "filepath": {
      "type": "string",
      "description": "Path to the file"
    },
    "recursive": {
      "type": "boolean",
      "description": "Scan recursively",
      "default": true
    },
    "patterns": {
      "type": "array",
      "items": {"type": "string"},
      "description": "File patterns (glob)",
      "default": ["*.cpp", "*.hpp"]
    }
  },
  "required": ["filepath"]
}
```

**Типы данных:**
- `string`: Строка
- `number`: Число (int или float)
- `boolean`: true/false
- `array`: Массив
- `object`: Объект
- `null`: Null значение

---

## Интеграция с Claude Code

### main_stdio.cpp - Точка Входа

```cpp
#include "core/MyCore.hpp"
#include "mcp/MCPServer.hpp"
#include "mcp/StdioTransport.hpp"
#include "tools/MyTool.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <csignal>
#include <memory>
#include <atomic>

namespace {
    std::atomic<bool> shutdown_requested{false};
    my_mcp::MCPServer* global_server = nullptr;

    void signal_handler(int signal) {
        spdlog::info("Received signal {}, shutting down", signal);
        shutdown_requested = true;
        if (global_server) {
            global_server->stop();
        }
    }

    void setup_signal_handlers() {
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
    }
}

int main(int argc, char** argv) {
    // Парсинг аргументов
    CLI::App app{"My MCP Server"};

    std::string log_level = "info";
    app.add_option("-l,--log-level", log_level,
                   "Log level (trace, debug, info, warn, error, critical)")
        ->default_val("info");

    bool version = false;
    app.add_flag("-v,--version", version, "Print version");

    CLI11_PARSE(app, argc, argv);

    if (version) {
        std::cout << "my-mcp-server version 1.0.0" << std::endl;
        return 0;
    }

    // Настройка логирования
    if (log_level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (log_level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (log_level == "info") {
        spdlog::set_level(spdlog::level::info);
    } else if (log_level == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (log_level == "error") {
        spdlog::set_level(spdlog::level::err);
    } else if (log_level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else {
        std::cerr << "Invalid log level: " << log_level << std::endl;
        return 1;
    }

    spdlog::info("Starting My MCP Server");
    spdlog::info("Log level: {}", log_level);

    try {
        // Настройка graceful shutdown
        setup_signal_handlers();

        // Создание компонентов
        auto core = std::make_shared<my_mcp::MyCore>();
        auto transport = std::make_unique<my_mcp::StdioTransport>();
        auto server = std::make_unique<my_mcp::MCPServer>(std::move(transport));

        // Сохранить для signal handler
        global_server = server.get();

        // Создать и зарегистрировать инструменты
        auto my_tool = std::make_shared<my_mcp::MyTool>(core);
        server->register_tool(
            my_mcp::MyTool::get_info(),
            [my_tool](const json& args) {
                return my_tool->execute(args);
            }
        );

        spdlog::info("All tools registered, starting server");

        // Запуск сервера (блокируется до stop())
        server->run();

        global_server = nullptr;
        spdlog::info("Server stopped cleanly");
        return 0;

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    }
}
```

### CMakeLists.txt - Сборка

```cmake
cmake_minimum_required(VERSION 3.20)
project(my-mcp-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Зависимости
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)

# Слой 1: Core
add_library(my_core
    src/core/MyCore.cpp
)
target_include_directories(my_core PUBLIC src)
target_link_libraries(my_core PUBLIC nlohmann_json::nlohmann_json)

# Слой 2: Protocol
add_library(my_protocol
    src/mcp/MCPServer.cpp
    src/mcp/StdioTransport.cpp
)
target_include_directories(my_protocol PUBLIC src)
target_link_libraries(my_protocol
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# Слой 3: Tools
add_library(my_tools
    src/tools/MyTool.cpp
)
target_include_directories(my_tools PUBLIC src)
target_link_libraries(my_tools
    PUBLIC
        my_core
        my_protocol
)

# Исполняемый файл
add_executable(my-mcp-server
    src/main_stdio.cpp
)
target_link_libraries(my-mcp-server
    PRIVATE
        my_tools
        CLI11::CLI11
        spdlog::spdlog
)

# Установка
install(TARGETS my-mcp-server
    RUNTIME DESTINATION bin
)
```

---

## Примеры Кода

### Пример 1: Простой Инструмент "echo"

```cpp
class EchoTool {
public:
    static ToolInfo get_info() {
        return {
            .name = "echo",
            .description = "Returns the input message",
            .input_schema = {
                {"type", "object"},
                {"properties", {
                    {"message", {
                        {"type", "string"},
                        {"description", "Message to echo"}
                    }}
                }},
                {"required", json::array({"message"})}
            }
        };
    }

    json execute(const json& args) {
        std::string message = args["message"];
        return {
            {"echo", message}
        };
    }
};
```

### Пример 2: Инструмент с Массивами

```cpp
class SumTool {
public:
    static ToolInfo get_info() {
        return {
            .name = "sum",
            .description = "Sums an array of numbers",
            .input_schema = {
                {"type", "object"},
                {"properties", {
                    {"numbers", {
                        {"type", "array"},
                        {"items", {"type", "number"}},
                        {"description", "Array of numbers to sum"}
                    }}
                }},
                {"required", json::array({"numbers"})}
            }
        };
    }

    json execute(const json& args) {
        const auto& numbers = args["numbers"];
        double sum = 0.0;
        for (const auto& num : numbers) {
            sum += num.get<double>();
        }
        return {
            {"sum", sum},
            {"count", numbers.size()}
        };
    }
};
```

### Пример 3: Инструмент с Файловой Системой

```cpp
class ListFilesTool {
public:
    static ToolInfo get_info() {
        return {
            .name = "list_files",
            .description = "Lists files in a directory",
            .input_schema = {
                {"type", "object"},
                {"properties", {
                    {"directory", {
                        {"type", "string"},
                        {"description", "Directory path"}
                    }},
                    {"pattern", {
                        {"type", "string"},
                        {"description", "Glob pattern (e.g., *.txt)"},
                        {"default", "*"}
                    }}
                }},
                {"required", json::array({"directory"})}
            }
        };
    }

    json execute(const json& args) {
        std::filesystem::path dir = args["directory"];
        std::string pattern = args.value("pattern", "*");

        json files = json::array();

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                // Простая glob проверка
                if (pattern == "*" || entry.path().filename().string().find(pattern) != std::string::npos) {
                    files.push_back({
                        {"path", entry.path().string()},
                        {"size", entry.file_size()}
                    });
                }
            }
        }

        return {
            {"files", files},
            {"count", files.size()},
            {"directory", dir.string()}
        };
    }
};
```

---

## Тестирование

### Ручное Тестирование через Echo

```bash
# Тест 1: Initialize
echo '{
  "jsonrpc":"2.0",
  "id":1,
  "method":"initialize",
  "params":{
    "protocolVersion":"2024-11-05",
    "clientInfo":{"name":"test","version":"1.0"}
  }
}' | ./my-mcp-server --log-level error

# Ожидаемый результат:
# {"id":1,"jsonrpc":"2.0","result":{...}}

# Тест 2: Tools List
echo '{
  "jsonrpc":"2.0",
  "id":2,
  "method":"tools/list",
  "params":{}
}' | ./my-mcp-server --log-level error

# Тест 3: Tools Call
echo '{
  "jsonrpc":"2.0",
  "id":3,
  "method":"tools/call",
  "params":{
    "name":"my_tool",
    "arguments":{"filepath":"/tmp/test.txt"}
  }
}' | ./my-mcp-server --log-level error
```

### Unit-тесты с GTest

```cpp
#include <gtest/gtest.h>
#include "mcp/MCPServer.hpp"
#include "mcp/MockTransport.hpp"

TEST(MCPServerTest, Initialize) {
    auto transport = std::make_unique<MockTransport>();
    MCPServer server(std::move(transport));

    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2024-11-05"}
        }}
    };

    json response = server.handle_request(request);

    EXPECT_EQ(response["jsonrpc"], "2.0");
    EXPECT_EQ(response["id"], 1);
    EXPECT_TRUE(response.contains("result"));
    EXPECT_EQ(response["result"]["protocolVersion"], "2024-11-05");
}

TEST(MCPServerTest, ToolsList) {
    auto transport = std::make_unique<MockTransport>();
    MCPServer server(std::move(transport));

    // Регистрация тестового инструмента
    server.register_tool(
        {"test_tool", "Test", json::object()},
        [](const json& args) { return json::object(); }
    );

    json request = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"method", "tools/list"},
        {"params", {}}
    };

    json response = server.handle_request(request);

    EXPECT_TRUE(response["result"].contains("tools"));
    EXPECT_EQ(response["result"]["tools"].size(), 1);
    EXPECT_EQ(response["result"]["tools"][0]["name"], "test_tool");
}
```

### Интеграционный Тест

```bash
#!/bin/bash
# test_integration.sh

set -e

SERVER="./my-mcp-server"

echo "Test 1: Initialize handshake"
RESULT=$(echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | $SERVER --log-level error)
echo "$RESULT" | grep -q '"protocolVersion":"2024-11-05"' || {
    echo "FAILED: Initialize"
    exit 1
}
echo "PASSED: Initialize"

echo "Test 2: Tools list"
RESULT=$(echo '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' | $SERVER --log-level error)
echo "$RESULT" | grep -q '"tools"' || {
    echo "FAILED: Tools list"
    exit 1
}
echo "PASSED: Tools list"

echo "All tests passed!"
```

---

## Чек-лист для Создания MCP Сервера

### Обязательные Компоненты

- [ ] **ITransport интерфейс** с read_message/write_message
- [ ] **StdioTransport реализация** (stdin/stdout)
- [ ] **MCPServer класс** с методами:
  - [ ] `handle_initialize()` - КРИТИЧЕСКИ ВАЖНО!
  - [ ] `handle_initialized_notification()` - уведомление
  - [ ] `handle_tools_list()` - список инструментов
  - [ ] `handle_tools_call()` - вызов инструмента
  - [ ] `create_error_response()` - обработка ошибок
- [ ] **ToolInfo структура** с name, description, input_schema
- [ ] **Хотя бы один инструмент** с get_info() и execute()
- [ ] **main_stdio.cpp** с правильной регистрацией инструментов
- [ ] **CMakeLists.txt** с правильными зависимостями
- [ ] **.mcp.json** для интеграции с Claude Code

### Логирование

- [ ] Используется **spdlog** или аналог
- [ ] Логи идут в **stderr**, НЕ в stdout
- [ ] Поддержка уровней: trace, debug, info, warn, error, critical
- [ ] CLI флаг `--log-level` для настройки

### JSON-RPC 2.0

- [ ] Валидация поля `"jsonrpc": "2.0"`
- [ ] Поддержка `"id"` (может быть null для уведомлений)
- [ ] Правильные коды ошибок (-32601, -32600, etc.)
- [ ] Формат ответа: `{"jsonrpc":"2.0","id":X,"result":{...}}`
- [ ] Формат ошибки: `{"jsonrpc":"2.0","id":X,"error":{...}}`

### Тестирование

- [ ] Unit-тесты для MCPServer
- [ ] Unit-тесты для каждого инструмента
- [ ] Интеграционный тест через bash скрипт
- [ ] Тест handshake (initialize → initialized)
- [ ] Тест с реальным Claude Code

---

## Частые Ошибки и Решения

### Ошибка 1: "Method not found: initialize"

**Симптом:**
```json
{"error":{"code":-32601,"message":"Method not found: initialize"}}
```

**Причина:** Не реализован метод `initialize` в `handle_request()`

**Решение:** Добавить обработчик:
```cpp
if (method == "initialize") {
    json result = handle_initialize(params);
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}
```

### Ошибка 2: Сервер зависает после initialize

**Причина:** Ждёте ответ на `notifications/initialized`, но это уведомление

**Решение:** Возвращать пустой JSON для уведомлений:
```cpp
if (method == "notifications/initialized") {
    handle_initialized_notification(params);
    return json();  // НЕ отправляем ответ!
}
```

### Ошибка 3: Логи мешают JSON в stdout

**Причина:** Логи пишутся в stdout вместо stderr

**Решение:** Убедитесь что spdlog настроен на stderr:
```cpp
auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
```

### Ошибка 4: Claude Code не видит инструменты

**Причина:** Неправильный формат ответа `tools/list`

**Решение:** Формат должен быть:
```json
{
  "tools": [
    {
      "name": "...",
      "description": "...",
      "inputSchema": {...}
    }
  ]
}
```

### Ошибка 5: Аргументы не передаются в инструмент

**Причина:** tools/call требует "arguments" в params

**Решение:**
```cpp
json arguments = params.value("arguments", json::object());
```

---

## Дополнительные Ресурсы

### Официальная Документация

- **MCP Specification**: https://spec.modelcontextprotocol.io/
- **MCP GitHub**: https://github.com/modelcontextprotocol
- **Claude Code Docs**: https://docs.claude.com/en/docs/claude-code/mcp

### Примеры Реализаций

- **TypeScript SDK**: https://github.com/modelcontextprotocol/typescript-sdk
- **Python SDK**: https://github.com/modelcontextprotocol/python-sdk
- **Reference Servers**: https://github.com/modelcontextprotocol/servers

### Инструменты

- **JSON Schema Validator**: https://www.jsonschemavalidator.net/
- **JSON-RPC Tester**: https://www.jsonrpc.org/

---

## Заключение

### Минимальная Рабочая Реализация

Для создания простейшего MCP сервера вам нужно:

1. **~200 строк** для ITransport и StdioTransport
2. **~300 строк** для MCPServer с initialize/tools
3. **~100 строк** на каждый инструмент
4. **~50 строк** для main_stdio.cpp

**Итого: ~650 строк кода** для базового функционального MCP сервера!

### Следующие Шаги

1. Скопируйте структуру из этого документа
2. Реализуйте свою бизнес-логику в `src/core/`
3. Создайте инструменты в `src/tools/`
4. Соберите и протестируйте
5. Добавьте `.mcp.json` в ваш проект
6. Запустите Claude Code и проверьте `/mcp`

### Расширенные Возможности

После базовой реализации можно добавить:

- **Resources**: Для предоставления данных AI
- **Prompts**: Для шаблонов запросов
- **HTTP/SSE Transport**: Для веб-интеграций
- **WebSocket Transport**: Для real-time коммуникации
- **Streaming**: Для больших результатов
- **Progress notifications**: Для длительных операций

---

**Удачи в создании вашего MCP сервера! 🚀**

Если у вас возникнут вопросы, обратитесь к:
- Этому документу
- [MCP API Reference](MCP_API_REFERENCE.md)
- [Исходному коду tree-sitter-mcp](../src/)
