#include "smpp_client_handler.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace smpp_client {

SmppClientHandler::SmppClientHandler(int socket_fd)
    : socket_fd_(socket_fd), running_(false) {
    // Initialize logger
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    std::vector<spdlog::sink_ptr> sinks{console_sink};
    logger_ = std::make_shared<spdlog::logger>("SmppClientHandler", sinks.begin(), sinks.end());
    logger_->set_level(spdlog::level::info);
    spdlog::register_logger(logger_);

    // Initialize state machine with logger
    state_machine_ = SmppStateMachine(logger_);

    logger_->info("SmppClientHandler created with fd: {}", socket_fd_);
}

SmppClientHandler::~SmppClientHandler() {
    if (running_) {
        stop();
    }
    logger_->info("SmppClientHandler destroyed");
}

void SmppClientHandler::init() {
    logger_->debug("Initializing SmppClientHandler");
    // Validate socket file descriptor
    if (socket_fd_ < 0) {
        logger_->error("Invalid socket file descriptor: {}", socket_fd_);
        if (error_callback_) {
            error_callback_("Invalid socket file descriptor");
        }
    }
}

void SmppClientHandler::start() {
    if (running_) {
        logger_->warn("SmppClientHandler already running");
        return;
    }

    running_ = true;
    logger_->info("SmppClientHandler started");

    // Start processing PDUs
    process_pdu();
}

void SmppClientHandler::stop() {
    running_ = false;
    logger_->info("SmppClientHandler stopped");
}

void SmppClientHandler::set_error_callback(ErrorCallback callback) {
    error_callback_ = callback;
}

void SmppClientHandler::process_pdu() {
    // TODO: Implement PDU extraction using SMPPFramer
    // When a PDU is received, call on_pdu_received with the SmppEvent
    logger_->debug("Processing PDUs");
}

// PDU reception and routing handled via state machine callbacks in v2 API
// void SmppClientHandler::on_pdu_received(const SmppEvent& event) {
//     // TODO: Implement with v2 API
// }

void SmppClientHandler::handle_bind_request(uint32_t bind_command_id) {
    // TODO: Validate bind credentials, handle BIND request
    logger_->info("Handling BIND request: 0x{:08x}", bind_command_id);

    // Call state machine's handle_bind with v2 API
    state_machine_.handle_bind(bind_command_id, "", "");
    // TODO: Send bind response to client based on authentication result
}

void SmppClientHandler::handle_unbind_request() {
    // TODO: Handle UNBIND PDU - graceful disconnect
    logger_->info("Handling UNBIND request");

    // Call state machine's handle_unbind with v2 API
    state_machine_.handle_unbind();
    // TODO: Send unbind response to client
}

void SmppClientHandler::handle_enquire_link_request() {
    // TODO: Handle ENQUIRE_LINK PDU - keep-alive
    logger_->debug("Handling ENQUIRE_LINK request");

    // Send ENQUIRE_LINK_RESP if bound
    if (is_bound()) {
        logger_->debug("Sending ENQUIRE_LINK_RESP");
        state_machine_.handle_enquire_link();
        // TODO: Send enquire_link_resp to client
    }
}

} // namespace smpp_client
