#!/bin/bash
# Integration test for Claude Code CLI integration

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
MCP_SERVER="${BUILD_DIR}/src/mcp_stdio_server"

echo "=== cpp-treesitter-mcp Claude Integration Test ==="
echo ""

# Test 1: Check MCP server executable exists
echo "[1/6] Checking MCP server executable..."
if [ -f "${MCP_SERVER}" ]; then
    echo "  ✓ MCP server found at ${MCP_SERVER}"
else
    echo "  ✗ MCP server not found at ${MCP_SERVER}"
    echo "  Please build the project first: cmake --build build"
    exit 1
fi

# Test 2: Check server responds to help
echo ""
echo "[2/6] Testing server help output..."
if "${MCP_SERVER}" --help | grep -q "MCP Stdio Server"; then
    echo "  ✓ Server help works"
else
    echo "  ✗ Server help failed"
    exit 1
fi

# Test 3: Test tools/list method
echo ""
echo "[3/6] Testing tools/list method..."
RESPONSE=$(echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | "${MCP_SERVER}" --log-level error)
if echo "${RESPONSE}" | grep -q '"parse_file"'; then
    echo "  ✓ tools/list returned expected tools"
else
    echo "  ✗ tools/list failed"
    echo "  Response: ${RESPONSE}"
    exit 1
fi

# Test 4: Test parse_file tool
echo ""
echo "[4/6] Testing parse_file tool..."
TEST_FILE="${PROJECT_ROOT}/tests/fixtures/simple_class.cpp"
if [ ! -f "${TEST_FILE}" ]; then
    echo "  ✗ Test fixture not found: ${TEST_FILE}"
    exit 1
fi

PARSE_REQUEST=$(cat <<EOF
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"parse_file","arguments":{"filepath":"${TEST_FILE}"}}}
EOF
)

RESPONSE=$(echo "${PARSE_REQUEST}" | "${MCP_SERVER}" --log-level error)
if echo "${RESPONSE}" | grep -q 'success.*true'; then
    echo "  ✓ parse_file tool works"
else
    echo "  ✗ parse_file tool failed"
    echo "  Response: ${RESPONSE}"
    exit 1
fi

# Test 5: Check Claude CLI installed (optional)
echo ""
echo "[5/6] Checking Claude CLI installation (optional)..."
if command -v claude &> /dev/null; then
    echo "  ✓ Claude CLI found: $(which claude)"
    CLAUDE_VERSION=$(claude --version 2>&1 || echo "unknown")
    echo "    Version: ${CLAUDE_VERSION}"
else
    echo "  ⚠ Claude CLI not found (optional)"
    echo "    Install from: https://github.com/anthropics/claude-cli"
fi

# Test 6: Check config files (if installed)
echo ""
echo "[6/6] Checking configuration files..."

if [[ "$OSTYPE" == "darwin"* ]]; then
    CLAUDE_CONFIG_DIR="$HOME/Library/Application Support/Claude"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    CLAUDE_CONFIG_DIR="$HOME/.config/claude"
else
    echo "  ⚠ Unknown OS, skipping config check"
    CLAUDE_CONFIG_DIR=""
fi

if [ -n "${CLAUDE_CONFIG_DIR}" ]; then
    CONFIG_FILE="${CLAUDE_CONFIG_DIR}/claude_desktop_config.json"
    if [ -f "${CONFIG_FILE}" ]; then
        echo "  ✓ Claude config found: ${CONFIG_FILE}"
        if grep -q "cpp-treesitter" "${CONFIG_FILE}" 2>/dev/null; then
            echo "    ✓ MCP server registered in config"
        else
            echo "    ⚠ MCP server not registered yet"
            echo "      Run: bash ${BUILD_DIR}/scripts/install_claude_agent.sh"
        fi
    else
        echo "  ⚠ Claude config not found"
        echo "    Will be created on first run"
    fi

    AGENT_FILE="$HOME/.claude/agents/ts-strategist.json"
    if [ -f "${AGENT_FILE}" ]; then
        echo "  ✓ Sub-agent config found: ${AGENT_FILE}"
    else
        echo "  ⚠ Sub-agent config not installed"
        echo "    Run: bash ${BUILD_DIR}/scripts/install_claude_agent.sh"
    fi
fi

echo ""
echo "=== Integration Test Complete ==="
echo ""
echo "Summary:"
echo "  - MCP server is functional"
echo "  - All tools respond correctly"
echo "  - Server is ready for Claude Code integration"
echo ""
echo "Next steps:"
echo "  1. Install: sudo cmake --install build"
echo "  2. Configure: bash build/scripts/install_claude_agent.sh"
echo "  3. Use: claude @ts-strategist \"analyze your-file.cpp\""
echo ""
