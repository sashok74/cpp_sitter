#!/bin/bash
# Quick install script for tree-sitter-mcp
# Builds, installs, and configures Claude Code integration

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== tree-sitter-mcp Quick Install ==="
echo "Project root: ${PROJECT_ROOT}"
echo ""

# Step 1: Build
echo "[1/4] Building project..."
cd "${PROJECT_ROOT}"

if [ ! -d "build" ]; then
    mkdir build
    cd build
    echo "Running conan install..."
    conan install .. --output-folder=. --build=missing
    echo "Configuring with CMake..."
    cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
else
    cd build
fi

echo "Building..."
cmake --build . -j$(nproc)

# Step 2: Test
echo ""
echo "[2/4] Running tests..."
ctest --output-on-failure

# Step 3: Install
echo ""
echo "[3/4] Installing (requires sudo)..."
sudo cmake --install .

# Step 4: Configure Claude
echo ""
echo "[4/4] Configuring Claude Code integration..."
if [ -f "${PROJECT_ROOT}/build/scripts/install_claude_agent.sh" ]; then
    bash "${PROJECT_ROOT}/build/scripts/install_claude_agent.sh"
else
    echo "WARNING: Claude integration script not found"
    echo "You may need to manually configure Claude Code"
fi

echo ""
echo "=== Installation Complete ==="
echo ""
echo "To verify:"
echo "  which tree-sitter-mcp"
echo "  tree-sitter-mcp --version"
echo ""
