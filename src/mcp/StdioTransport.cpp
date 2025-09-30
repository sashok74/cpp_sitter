#include "StdioTransport.hpp"
#include <spdlog/spdlog.h>
#include <string>

namespace ts_mcp {

StdioTransport::StdioTransport(std::istream& in, std::ostream& out)
    : in_(in), out_(out) {
    spdlog::debug("StdioTransport initialized");
}

json StdioTransport::read_message() {
    std::string line;

    if (!std::getline(in_, line)) {
        if (in_.eof()) {
            spdlog::debug("Reached end of input stream");
        } else {
            spdlog::error("Error reading from input stream");
        }
        return json();  // Return empty JSON on EOF or error
    }

    if (line.empty()) {
        spdlog::debug("Read empty line, treating as no message");
        return json();
    }

    try {
        json message = json::parse(line);
        spdlog::debug("Read message: {}", line);
        return message;
    } catch (const json::parse_error& e) {
        spdlog::error("JSON parse error: {}", e.what());
        return json();
    }
}

void StdioTransport::write_message(const json& message) {
    std::string serialized = message.dump();
    out_ << serialized << std::endl;  // std::endl flushes automatically
    spdlog::debug("Wrote message: {}", serialized);
}

bool StdioTransport::is_open() const {
    return in_.good() && out_.good();
}

} // namespace ts_mcp
