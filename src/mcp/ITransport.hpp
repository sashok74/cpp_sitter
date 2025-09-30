#pragma once

#include <nlohmann/json.hpp>

namespace ts_mcp {

using json = nlohmann::json;

/**
 * @brief Abstract interface for MCP transport mechanisms
 *
 * Implementations handle reading/writing JSON-RPC messages via different
 * transport protocols (stdio, HTTP/SSE, websockets, etc.)
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /**
     * @brief Read next JSON-RPC message from transport
     * @return JSON message or empty object on EOF/error
     */
    virtual json read_message() = 0;

    /**
     * @brief Write JSON-RPC message to transport
     * @param message JSON message to write
     */
    virtual void write_message(const json& message) = 0;

    /**
     * @brief Check if transport is still open
     * @return true if transport can read/write, false otherwise
     */
    virtual bool is_open() const = 0;
};

} // namespace ts_mcp
