#include "MockTransport.hpp"

namespace ts_mcp {

json MockTransport::read_message() {
    if (!open_ || requests_.empty()) {
        return json();  // Return empty on EOF
    }

    json request = requests_.front();
    requests_.pop();
    return request;
}

void MockTransport::write_message(const json& message) {
    if (open_) {
        responses_.push(message);
    }
}

bool MockTransport::is_open() const {
    return open_;
}

void MockTransport::push_request(const json& request) {
    requests_.push(request);
}

json MockTransport::pop_response() {
    if (responses_.empty()) {
        return json();
    }

    json response = responses_.front();
    responses_.pop();
    return response;
}

bool MockTransport::has_responses() const {
    return !responses_.empty();
}

void MockTransport::close() {
    open_ = false;
}

} // namespace ts_mcp
