#pragma once

#include "ITransport.hpp"
#include <iostream>

namespace ts_mcp {

/**
 * @brief Transport using standard input/output streams
 *
 * Reads JSON messages line-by-line from stdin
 * Writes JSON messages line-by-line to stdout with flush
 * Suitable for use with Claude Code CLI and similar tools
 */
class StdioTransport : public ITransport {
public:
    /**
     * @brief Construct stdio transport
     * @param in Input stream (default: std::cin)
     * @param out Output stream (default: std::cout)
     */
    explicit StdioTransport(std::istream& in = std::cin, std::ostream& out = std::cout);

    json read_message() override;
    void write_message(const json& message) override;
    bool is_open() const override;

private:
    std::istream& in_;
    std::ostream& out_;
};

} // namespace ts_mcp
