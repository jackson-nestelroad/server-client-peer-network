#include "client_server_connection_handler_factory.h"

#include <net/server/impl/client_server_connection_handler.h>
#include <net/server/server_components.h>

namespace net {
namespace server {
namespace impl {

std::unique_ptr<BaseConnectionHandler>
ClientServerConnectionHandlerFactory::Create(std::shared_ptr<Connection> client,
                                             ServerComponents& components) {
    return std::unique_ptr<BaseConnectionHandler>(
        new ClientServerConnectionHandler(client, components));
}

std::unique_ptr<BaseConnectionHandlerFactory>
ClientServerConnectionHandlerFactory::CreateFactory() {
    return std::unique_ptr<BaseConnectionHandlerFactory>(
        new ClientServerConnectionHandlerFactory());
}

}  // namespace impl
}  // namespace server
}  // namespace net