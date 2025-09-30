#pragma once

#include "mcp/ITransport.hpp"
#include <queue>
#include <nlohmann/json.hpp>

namespace ts_mcp {

/**
 * @brief Mock transport for testing MCP server
 *
 * Uses queues for simulating request/response flow without actual I/O
 */
class MockTransport : public ITransport {
public:
    MockTransport() = default;

    json read_message() override;
    void write_message(const json& message) override;
    bool is_open() const override;

    /**
     * @brief Add a request to the input queue
     * @param request JSON-RPC request
     */
    void push_request(const json& request);

    /**
     * @brief Get and remove response from output queue
     * @return JSON-RPC response
     */
    json pop_response();

    /**
     * @brief Check if there are pending responses
     * @return true if responses available
     */
    bool has_responses() const;

    /**
     * @brief Close the transport (causes read_message to return empty)
     */
    void close();

private:
    std::queue<json> requests_;
    std::queue<json> responses_;
    bool open_ = true;
};

} // namespace ts_mcp
