from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout

class CppTreesitterMcpConan(ConanFile):
    name = "cpp-treesitter-mcp"
    version = "1.0.0"

    # Metadata
    license = "MIT"
    author = "cpp-treesitter-mcp contributors"
    description = "MCP server for C++ code analysis with tree-sitter"
    topics = ("tree-sitter", "mcp", "c++", "static-analysis")

    # Settings
    settings = "os", "compiler", "build_type", "arch"

    # Options
    options = {
        "shared": [True, False],
        "build_tests": [True, False],
        "build_sse": [True, False]
    }
    default_options = {
        "shared": False,
        "build_tests": True,
        "build_sse": True
    }

    # Build requirements
    def requirements(self):
        self.requires("nlohmann_json/3.11.3")
        self.requires("spdlog/1.13.0")
        self.requires("cli11/2.4.1")

        if self.options.build_sse:
            self.requires("cpp-httplib/0.15.3")

    def build_requirements(self):
        if self.options.build_tests:
            self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = self.options.build_tests
        tc.variables["BUILD_SSE_SERVER"] = self.options.build_sse
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
