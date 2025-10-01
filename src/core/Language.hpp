#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <vector>

// Forward declarations for tree-sitter C API
extern "C" {
    struct TSLanguage;
}

namespace ts_mcp {

/**
 * @brief Supported programming languages
 */
enum class Language {
    CPP,      // C++ language
    PYTHON,   // Python language
    UNKNOWN   // Unknown or unsupported language
};

/**
 * @brief Language utilities for tree-sitter multi-language support
 */
class LanguageUtils {
public:
    /**
     * @brief Detect language from file extension
     *
     * @param filepath Path to the file
     * @return Language enum value (UNKNOWN if not recognized)
     */
    static Language detect_from_extension(const std::filesystem::path& filepath);

    /**
     * @brief Detect language from file extension (string version)
     *
     * @param filepath Path to the file as string
     * @return Language enum value (UNKNOWN if not recognized)
     */
    static Language detect_from_extension(std::string_view filepath);

    /**
     * @brief Get tree-sitter TSLanguage for the given language
     *
     * @param lang Language enum value
     * @return Pointer to TSLanguage or nullptr if not supported
     */
    static const TSLanguage* get_ts_language(Language lang);

    /**
     * @brief Convert Language enum to string name
     *
     * @param lang Language enum value
     * @return String representation (e.g., "cpp", "python", "unknown")
     */
    static std::string_view to_string(Language lang);

    /**
     * @brief Convert string to Language enum
     *
     * @param name Language name (e.g., "cpp", "python", "c++", "py")
     * @return Language enum value (UNKNOWN if not recognized)
     */
    static Language from_string(std::string_view name);

    /**
     * @brief Get file extensions for a language
     *
     * @param lang Language enum value
     * @return Vector of file extensions (including dot, e.g., ".cpp")
     */
    static std::vector<std::string_view> get_extensions(Language lang);
};

}  // namespace ts_mcp
