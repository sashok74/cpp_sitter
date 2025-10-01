#include <gtest/gtest.h>
#include "core/PathResolver.hpp"
#include <filesystem>
#include <fstream>

using namespace ts_mcp;
namespace fs = std::filesystem;

class PathResolverTest : public ::testing::Test {
protected:
    fs::path test_dir_;

    void SetUp() override {
        // Create temporary test directory structure
        test_dir_ = fs::temp_directory_path() / "path_resolver_test";
        fs::remove_all(test_dir_);
        fs::create_directories(test_dir_);

        // Create test files
        create_file(test_dir_ / "file1.cpp", "class A {};");
        create_file(test_dir_ / "file2.hpp", "class B {};");
        create_file(test_dir_ / "file3.h", "class C {};");
        create_file(test_dir_ / "readme.txt", "Not a C++ file");

        // Create subdirectory
        auto subdir = test_dir_ / "subdir";
        fs::create_directories(subdir);
        create_file(subdir / "nested1.cpp", "class D {};");
        create_file(subdir / "nested2.cc", "class E {};");

        // Create deeper nested directory
        auto deep_dir = subdir / "deep";
        fs::create_directories(deep_dir);
        create_file(deep_dir / "deep_file.cxx", "class F {};");
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    void create_file(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }
};

TEST_F(PathResolverTest, SingleFile) {
    auto file = test_dir_ / "file1.cpp";
    auto results = PathResolver::resolve_paths({file.string()});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], fs::canonical(file));
}

TEST_F(PathResolverTest, SingleDirectory) {
    auto results = PathResolver::resolve_paths({test_dir_.string()}, false);

    // Should find file1.cpp, file2.hpp, file3.h (but not in subdir)
    ASSERT_EQ(results.size(), 3);

    // Results are sorted
    EXPECT_TRUE(results[0].filename() == "file1.cpp");
    EXPECT_TRUE(results[1].filename() == "file2.hpp");
    EXPECT_TRUE(results[2].filename() == "file3.h");
}

TEST_F(PathResolverTest, RecursiveDirectory) {
    auto results = PathResolver::resolve_paths({test_dir_.string()}, true);

    // Should find all .cpp, .hpp, .h, .cc, .cxx files recursively
    // file1.cpp, file2.hpp, file3.h, nested1.cpp, nested2.cc, deep_file.cxx
    ASSERT_EQ(results.size(), 6);

    // Verify deep file is found
    bool found_deep = false;
    for (const auto& path : results) {
        if (path.filename() == "deep_file.cxx") {
            found_deep = true;
            break;
        }
    }
    EXPECT_TRUE(found_deep);
}

TEST_F(PathResolverTest, NonRecursiveDirectory) {
    auto results = PathResolver::resolve_paths({test_dir_.string()}, false);

    // Should NOT find files in subdirectories
    ASSERT_EQ(results.size(), 3);

    for (const auto& path : results) {
        EXPECT_EQ(path.parent_path(), fs::canonical(test_dir_));
    }
}

TEST_F(PathResolverTest, MultiplePaths) {
    auto file1 = test_dir_ / "file1.cpp";
    auto file2 = test_dir_ / "file2.hpp";
    auto subdir = test_dir_ / "subdir";

    auto results = PathResolver::resolve_paths(
        {file1.string(), file2.string(), subdir.string()},
        false
    );

    // file1.cpp, file2.hpp, nested1.cpp, nested2.cc
    ASSERT_EQ(results.size(), 4);
}

TEST_F(PathResolverTest, FilePatternFilter) {
    // Only .cpp files
    auto results = PathResolver::resolve_paths(
        {test_dir_.string()},
        true,
        {"*.cpp"}
    );

    // file1.cpp, nested1.cpp
    ASSERT_EQ(results.size(), 2);

    for (const auto& path : results) {
        EXPECT_EQ(path.extension().string(), ".cpp");
    }
}

TEST_F(PathResolverTest, NonexistentPath) {
    auto fake_path = test_dir_ / "nonexistent.cpp";
    auto results = PathResolver::resolve_paths({fake_path.string()});

    // Should return empty (path doesn't exist)
    EXPECT_EQ(results.size(), 0);
}

TEST_F(PathResolverTest, EmptyResult) {
    auto empty_dir = test_dir_ / "empty";
    fs::create_directories(empty_dir);

    auto results = PathResolver::resolve_paths({empty_dir.string()});

    EXPECT_EQ(results.size(), 0);
}
