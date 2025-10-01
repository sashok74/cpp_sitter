#!/bin/bash
# Быстрая установка tree-sitter-mcp в Claude Code CLI
# Использование: bash quick_claude_cli_setup.sh

set -e

echo "=== Быстрая установка tree-sitter-mcp в Claude Code CLI ==="
echo ""

# Проверка предварительных требований
echo "[1/5] Проверка требований..."

if ! command -v tree-sitter-mcp &> /dev/null; then
    echo "  ✗ tree-sitter-mcp не установлен"
    echo "  Сначала соберите и установите проект:"
    echo "    cd /home/raa/projects/cpp-sitter"
    echo "    conan install . --output-folder=build --build=missing"
    echo "    cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake"
    echo "    cmake --build . -j\$(nproc)"
    echo "    sudo cmake --install ."
    exit 1
fi

echo "  ✓ tree-sitter-mcp найден: $(which tree-sitter-mcp)"

if ! command -v claude &> /dev/null; then
    echo "  ⚠ Claude CLI не установлен (опционально)"
    echo "  Скачать: https://github.com/anthropics/claude-cli"
else
    echo "  ✓ Claude CLI найден: $(which claude)"
fi

# Определение ОС
echo ""
echo "[2/5] Определение ОС..."

if [[ "$OSTYPE" == "darwin"* ]]; then
    CLAUDE_CONFIG_DIR="$HOME/Library/Application Support/Claude"
    echo "  ✓ macOS detected"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    CLAUDE_CONFIG_DIR="$HOME/.config/claude"
    echo "  ✓ Linux detected"
else
    echo "  ✗ Unsupported OS: $OSTYPE"
    exit 1
fi

# Создание конфигурации MCP
echo ""
echo "[3/5] Настройка MCP сервера..."

mkdir -p "${CLAUDE_CONFIG_DIR}"
CONFIG_FILE="${CLAUDE_CONFIG_DIR}/claude_desktop_config.json"

if [ -f "${CONFIG_FILE}" ]; then
    echo "  ⚠ Конфигурация уже существует: ${CONFIG_FILE}"
    echo "  Создаю резервную копию..."
    cp "${CONFIG_FILE}" "${CONFIG_FILE}.backup.$(date +%s)"
fi

# Используем Python или jq для обновления JSON
if command -v python3 &> /dev/null; then
    python3 << 'PYTHON_EOF'
import json
import os
import sys

config_file = os.environ['CONFIG_FILE']

# Читаем существующую конфигурацию или создаём новую
if os.path.exists(config_file):
    with open(config_file, 'r') as f:
        config = json.load(f)
else:
    config = {}

# Добавляем/обновляем MCP сервер
if 'mcpServers' not in config:
    config['mcpServers'] = {}

config['mcpServers']['tree-sitter'] = {
    'command': '/usr/local/bin/tree-sitter-mcp',
    'args': ['--log-level', 'info']
}

# Сохраняем
with open(config_file, 'w') as f:
    json.dump(config, f, indent=2)

print("  ✓ MCP сервер зарегистрирован")
PYTHON_EOF
else
    echo "  ⚠ Python3 не найден, создаю базовую конфигурацию"
    cat > "${CONFIG_FILE}" << 'JSON_EOF'
{
  "mcpServers": {
    "tree-sitter": {
      "command": "/usr/local/bin/tree-sitter-mcp",
      "args": ["--log-level", "info"]
    }
  }
}
JSON_EOF
    echo "  ✓ Базовая конфигурация создана"
fi

# Создание sub-agent
echo ""
echo "[4/5] Настройка sub-agent @ts-strategist..."

mkdir -p "$HOME/.claude/agents"
AGENT_FILE="$HOME/.claude/agents/ts-strategist.json"

cat > "${AGENT_FILE}" << 'AGENT_EOF'
{
  "name": "ts-strategist",
  "description": "Tree-sitter based code analysis specialist for C++ and Python",
  "mcp_server": "tree-sitter",
  "instructions": [
    "You are an expert in static code analysis using tree-sitter.",
    "You can analyze C++ and Python code.",
    "Available tools: parse_file, find_classes, find_functions, execute_query.",
    "Always provide clear analysis with file paths and line numbers.",
    "For C++: focus on classes, virtual functions, templates, includes.",
    "For Python: focus on classes, functions, async functions, decorators."
  ]
}
AGENT_EOF

echo "  ✓ Sub-agent настроен: ${AGENT_FILE}"

# Проверка
echo ""
echo "[5/5] Проверка установки..."

echo "  Тест сервера..."
if tree-sitter-mcp --help > /dev/null 2>&1; then
    echo "  ✓ Сервер отвечает"
else
    echo "  ✗ Сервер не отвечает"
    exit 1
fi

echo ""
echo "=== Установка завершена! ==="
echo ""
echo "Конфигурация:"
echo "  MCP сервер: ${CONFIG_FILE}"
echo "  Sub-agent:  ${AGENT_FILE}"
echo ""
echo "Использование:"
echo "  # Общий анализ"
echo "  claude \"analyze this file: /path/to/file.cpp\""
echo ""
echo "  # Через sub-agent"
echo "  claude @ts-strategist \"find all classes in src/\""
echo "  claude @ts-strategist \"analyze /path/to/file.py\""
echo ""
echo "Примеры:"
echo "  claude @ts-strategist \"find all virtual functions in src/core/\""
echo "  claude @ts-strategist \"find all async functions in tests/\""
echo "  claude @ts-strategist \"what decorators are used in file.py\""
echo ""
echo "Документация: /home/raa/projects/cpp-sitter/doc/INSTALL_CLAUDE_CLI.md"
echo ""
