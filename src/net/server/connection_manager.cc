#include "connection_manager.h"

#include <net/server/server_components.h>
#include <util/console.h>
#include <util/mutex.h>

namespace net {
namespace server {

ConnectionManager::ConnectionManager(ServerComponents& components)
    : data_mutex_(), connections_(), handlers_(), components_(components) {}

Connection& ConnectionManager::NewConnection(int sockfd) {
    Connection connection(Socket(sockfd, SocketState::kConnected,
                                 components_.common.options.timeout));
    // Be careful allocating here, because `ClientConnection` must know its
    // shared_ptr!
    CRITICAL_SECTION(data_mutex_, {
        auto res = connections_.emplace(
            connection.id(),
            std::make_shared<Connection>(std::move(connection)));
        return *res.first->second;
    });
}

void ConnectionManager::Destroy(Connection& client) {
    CRITICAL_SECTION(data_mutex_, connections_.erase(client.id()));
}

void ConnectionManager::Start(Connection& client) {
    util::safe_debug::log("Starting handler for connection", client.id());
    CRITICAL_SECTION(data_mutex_, {
        auto res = handlers_.emplace(
            std::move(components_.connection_handler_factory->Create(
                client.shared_from_this(), components_)));
        const std::unique_ptr<BaseConnectionHandler>& handler = *res.first;
        handler->Start([this, &handler]() { Stop(handler); });
    });
}

void ConnectionManager::Stop(
    const std::unique_ptr<BaseConnectionHandler>& service) {
    Connection& client = service->Client();
    util::safe_debug::log("Stopping handler for connection", client.id());
    CRITICAL_SECTION(data_mutex_, {
        handlers_.erase(service);
        connections_.erase(client.id());
    });
}

void ConnectionManager::CloseAll() {
    for (auto& client : connections_) {
        client.second->socket.Close();
    }
}

}  // namespace server
}  // namespace net