# Building tree-sitter-mcp

Detailed instructions for building the tree-sitter-mcp project from source.

## Prerequisites

### Required

- **C++20 Compiler**:
  - GCC 11+ (Linux)
  - Clang 14+ (macOS/Linux)
  - MSVC 19.30+ (Windows)
- **CMake**: 3.20 or later
- **Conan**: 2.x package manager

### Optional

- **Claude Code CLI**: For integration testing
- **jq or Python 3**: For automated config updates
- **Valgrind**: For memory leak detection
- **gcov/lcov**: For code coverage

## Installing Prerequisites

### Ubuntu/Debian

```bash
# Compiler and build tools
sudo apt-get update
sudo apt-get install -y build-essential cmake python3-pip

# Conan
pip3 install conan
```

### macOS

```bash
# Homebrew
brew install cmake conan

# Or via pip
pip3 install conan
```

### Windows

```bash
# Using chocolatey
choco install cmake python

# Conan
pip install conan
```

## Build Steps

### 1. Clone Repository

```bash
git clone https://github.com/yourusername/tree-sitter-mcp.git
cd tree-sitter-mcp
```

### 2. Detect Conan Profile (First Time Only)

```bash
conan profile detect --force
```

This creates `~/.conan2/profiles/default` with your system configuration.

### 3. Install Dependencies

```bash
conan install . --output-folder=build --build=missing
```

Dependencies installed:
- nlohmann_json (JSON handling)
- spdlog (logging)
- CLI11 (argument parsing)
- cpp-httplib (optional, for SSE transport)
- gtest (testing)

### 4. Configure with CMake

```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```

**Build Types**:
- `Release`: Optimized build (-O3)
- `Debug`: Debug symbols, no optimization
- `RelWithDebInfo`: Optimized with debug symbols
- `MinSizeRel`: Optimize for size

### 5. Build

```bash
# Parallel build (faster)
cmake --build . -j$(nproc)

# Single-threaded build
cmake --build .

# Build specific target
cmake --build . --target tree-sitter-mcp
```

**Targets**:
- `ts_mcp_core`: Core library
- `ts_mcp_protocol`: Protocol library
- `ts_mcp_tools`: Tools library
- `tree-sitter-mcp`: Main executable
- `core_tests`, `mcp_tests`, `tools_tests`, `integration_tests`: Test executables

### 6. Test

```bash
# Run all tests
ctest --output-on-failure

# Verbose output
ctest -V

# Run specific test
./tests/core/core_tests
./tests/mcp/mcp_tests

# Integration tests
bash ../tests/integration/test_claude_integration.sh
```

### 7. Install

```bash
# System install (requires sudo on Linux/macOS)
sudo cmake --install .

# Custom prefix
cmake --install . --prefix=/opt/tree-sitter-mcp
```

**Installed files**:
- `/usr/local/bin/tree-sitter-mcp` (or custom prefix)
- `/usr/local/share/tree-sitter-mcp/install_claude_agent.sh`

## CMake Options

Configure with custom options:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_STDIO_SERVER=ON \
  -DBUILD_SSE_SERVER=OFF \
  -DENABLE_COVERAGE=OFF
```

### Available Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `ON` | Build unit and integration tests |
| `BUILD_STDIO_SERVER` | `ON` | Build stdio MCP server |
| `BUILD_SSE_SERVER` | `ON` | Build HTTP/SSE MCP server |
| `ENABLE_COVERAGE` | `OFF` | Enable code coverage instrumentation |
| `CMAKE_BUILD_TYPE` | `Debug` | Build type (Release/Debug/RelWithDebInfo) |

## Development Build

For active development with fast iteration:

```bash
# Debug build with tests
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON

# Build and test in one command
cmake --build . -j$(nproc) && ctest --output-on-failure

# Watch for changes (requires entr or similar)
ls ../src/**/*.cpp | entr -c cmake --build . && ctest
```

## Code Coverage

Generate coverage reports:

```bash
# Configure with coverage enabled
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON

# Build and run tests
cmake --build .
ctest

# Generate coverage report (requires lcov)
cmake --build . --target coverage

# View HTML report
open coverage/index.html  # macOS
xdg-open coverage/index.html  # Linux
```

Target: 80% code coverage minimum

## Cross-Platform Notes

### Linux

- Use system compiler (GCC 11+ recommended)
- Ensure development packages installed: `libstdc++-11-dev`

### macOS

- Xcode Command Line Tools required: `xcode-select --install`
- Use Homebrew-installed CMake for best compatibility

### Windows (MSVC)

```powershell
# From Visual Studio Developer Command Prompt
conan install . --output-folder=build --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -G "Visual Studio 17 2022"
cmake --build . --config Release
ctest -C Release
```

### Windows (MinGW)

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -G "MinGW Makefiles"
cmake --build .
```

## Troubleshooting

### Conan Issues

**Problem**: `ConanInvalidConfiguration: Invalid configuration`

**Solution**: Update Conan profile or specify compiler manually:
```bash
conan profile show default
# Edit: ~/.conan2/profiles/default
```

**Problem**: Dependency conflicts

**Solution**: Clear Conan cache and rebuild:
```bash
conan cache clean "*"
conan install . --output-folder=build --build=missing
```

### CMake Issues

**Problem**: `Could not find tree-sitter`

**Solution**: Tree-sitter is built from source via FetchContent, ensure internet connection or check logs:
```bash
cat build/CMakeFiles/CMakeError.log
```

**Problem**: C++20 features not available

**Solution**: Ensure compiler supports C++20:
```bash
# Check GCC version
g++ --version  # Need 11+

# Check Clang version
clang++ --version  # Need 14+

# Force specific compiler
cmake .. -DCMAKE_CXX_COMPILER=g++-11
```

### Build Errors

**Problem**: `undefined reference to tree_sitter_cpp`

**Solution**: Full rebuild:
```bash
rm -rf build
mkdir build && cd build
conan install .. --output-folder=. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . -j$(nproc)
```

**Problem**: Linking errors with static libraries

**Solution**: Ensure consistent build type:
```bash
conan install . --output-folder=build --build=missing --settings=build_type=Release
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Test Failures

**Problem**: Fixture files not found

**Solution**: Run tests from build directory:
```bash
cd build
ctest --output-on-failure
```

**Problem**: Integration tests timeout

**Solution**: Increase timeout:
```bash
ctest --timeout 300
```

### Memory Issues

Check for memory leaks:

```bash
# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Run with valgrind
valgrind --leak-check=full ./build/src/tree-sitter-mcp < test_input.json
```

## Performance Optimization

### Release Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-march=native -O3"
```

### Link-Time Optimization (LTO)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

### Profiling

```bash
# Build with profiling
cmake .. -DCMAKE_CXX_FLAGS="-pg" -DCMAKE_EXE_LINKER_FLAGS="-pg"
cmake --build .

# Run and generate profile
./build/src/tree-sitter-mcp < input.json
gprof ./build/src/tree-sitter-mcp gmon.out > profile.txt
```

## Clean Build

Complete clean and rebuild:

```bash
# Remove build directory
rm -rf build

# Start fresh
mkdir build && cd build
conan install .. --output-folder=. --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## Continuous Integration

Example GitHub Actions workflow:

```yaml
name: Build and Test
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: |
          pip install conan
          conan profile detect --force
      - name: Build
        run: |
          conan install . --output-folder=build --build=missing
          cd build
          cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
          cmake --build . -j$(nproc)
      - name: Test
        run: cd build && ctest --output-on-failure
```

## Next Steps

After successful build:

1. **Verify installation**: `which tree-sitter-mcp`
2. **Test manually**: See README.md Quick Test section
3. **Configure Claude**: Run `install_claude_agent.sh`
4. **Integration test**: `bash tests/integration/test_claude_integration.sh`

For usage instructions, see [README.md](README.md).
