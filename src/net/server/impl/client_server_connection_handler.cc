#include "client_server_connection_handler.h"

#include <net/server/impl/project2_service.h>

namespace net {
namespace server {
namespace impl {

std::unique_ptr<BaseService> ClientServerConnectionHandler::FirstService(
    server::ServerComponents& components, Connection& client) {
    return std::unique_ptr<BaseService>(
        new Project2Service(components, client, *this));
}

}  // namespace impl
}  // namespace server
}  // namespace net