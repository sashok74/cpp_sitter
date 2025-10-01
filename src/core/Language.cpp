#include "Language.hpp"
#include <algorithm>
#include <cctype>

// Tree-sitter C language parsers
extern "C" {
    const TSLanguage* tree_sitter_cpp();
    const TSLanguage* tree_sitter_python();
}

namespace ts_mcp {

Language LanguageUtils::detect_from_extension(const std::filesystem::path& filepath) {
    if (filepath.empty()) {
        return Language::UNKNOWN;
    }

    std::string ext = filepath.extension().string();
    if (ext.empty()) {
        return Language::UNKNOWN;
    }

    // Convert to lowercase for case-insensitive comparison
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // C++ extensions
    if (ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".c++" ||
        ext == ".hpp" || ext == ".hxx" || ext == ".hh" || ext == ".h++" ||
        ext == ".h" || ext == ".c") {
        return Language::CPP;
    }

    // Python extensions
    if (ext == ".py" || ext == ".pyi" || ext == ".pyw") {
        return Language::PYTHON;
    }

    return Language::UNKNOWN;
}

Language LanguageUtils::detect_from_extension(std::string_view filepath) {
    return detect_from_extension(std::filesystem::path(filepath));
}

const TSLanguage* LanguageUtils::get_ts_language(Language lang) {
    switch (lang) {
        case Language::CPP:
            return tree_sitter_cpp();
        case Language::PYTHON:
            return tree_sitter_python();
        case Language::UNKNOWN:
        default:
            return nullptr;
    }
}

std::string_view LanguageUtils::to_string(Language lang) {
    switch (lang) {
        case Language::CPP:
            return "cpp";
        case Language::PYTHON:
            return "python";
        case Language::UNKNOWN:
        default:
            return "unknown";
    }
}

Language LanguageUtils::from_string(std::string_view name) {
    // Convert to lowercase for comparison
    std::string lower_name;
    lower_name.resize(name.size());
    std::transform(name.begin(), name.end(), lower_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // C++ variations
    if (lower_name == "cpp" || lower_name == "c++" || lower_name == "cxx" ||
        lower_name == "cplusplus" || lower_name == "c") {
        return Language::CPP;
    }

    // Python variations
    if (lower_name == "python" || lower_name == "py") {
        return Language::PYTHON;
    }

    return Language::UNKNOWN;
}

std::vector<std::string_view> LanguageUtils::get_extensions(Language lang) {
    switch (lang) {
        case Language::CPP:
            return {".cpp", ".cxx", ".cc", ".c++", ".hpp", ".hxx", ".hh", ".h++", ".h", ".c"};
        case Language::PYTHON:
            return {".py", ".pyi", ".pyw"};
        case Language::UNKNOWN:
        default:
            return {};
    }
}

}  // namespace ts_mcp
