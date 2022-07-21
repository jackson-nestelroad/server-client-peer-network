#include "base_connection_handler.h"

#include <net/server/server_components.h>
#include <util/console.h>

namespace net {
namespace server {

BaseConnectionHandler::BaseConnectionHandler(std::shared_ptr<Connection> client,
                                             ServerComponents& components)
    : client_(client), components_(components), current_service_() {}

void BaseConnectionHandler::Start(const stop_func_t& on_finished) {
    on_finished_ = on_finished;
    current_service_ = FirstService(components_, *client_);
    if (current_service_) {
        components_.common.thread_pool.Schedule(
            [this]() { current_service_->Start(); });
    } else {
        on_finished_();
    }
}

void BaseConnectionHandler::Stop() {
    auto res = client_->socket.Close();
    if (res.is_err()) {
        util::safe_error_log::log(res.err());
    }
    on_finished_();
}

Connection& BaseConnectionHandler::Client() { return *client_; }

}  // namespace server
}  // namespace net