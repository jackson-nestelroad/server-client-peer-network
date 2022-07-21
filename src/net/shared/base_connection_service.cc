#include "base_connection_service.h"

#include <util/mutex.h>

namespace net {
namespace shared {

BaseConnectionService::BaseConnectionService(Components& components)
    : components_(components) {}

typename BaseConnectionService::PendingConnectionIterator
BaseConnectionService::NewSocket() {
    CRITICAL_SECTION(mutex_, {
        return pending_connections_.emplace(pending_connections_.end(),
                                            components_.options.timeout,
                                            components_.options.retry_timeout);
    });
}

Connection BaseConnectionService::DispatchConnection(
    PendingConnectionIterator it) {
    Connection connection = std::move(*it);
    CRITICAL_SECTION(mutex_, pending_connections_.erase(it));
    return std::move(connection);
}

typename ConnectableSocket::connect_callback_t
BaseConnectionService::ConnectCallback(const connect_callback_t& callback,
                                       PendingConnectionIterator it) {
    return [this, it, callback](util::result<void, Error> res) {
        if (res.is_err()) {
            callback(res.err());
        } else {
            callback(DispatchConnection(it));
        }
    };
}

void BaseConnectionService::CancelPendingConnections() {
    for (auto& socket : pending_connections_) {
        socket.Close();
    }
}

}  // namespace shared
}  // namespace net