#ifndef NET_SERVER_IMPL_CLIENT_SERVER_CONNECTION_HANDLER_FACTORY_
#define NET_SERVER_IMPL_CLIENT_SERVER_CONNECTION_HANDLER_FACTORY_

#include <net/server/base_connection_handler_factory.h>

namespace net {
namespace server {
namespace impl {

/**
 * @brief Factory for connection handlers for clients connected to the server.
 *
 */
class ClientServerConnectionHandlerFactory
    : public BaseConnectionHandlerFactory {
   public:
    std::unique_ptr<BaseConnectionHandler> Create(
        std::shared_ptr<Connection> client,
        ServerComponents& components) override;

    static std::unique_ptr<BaseConnectionHandlerFactory> CreateFactory();
};

}  // namespace impl
}  // namespace server
}  // namespace net

#endif  // NET_SERVER_IMPL_CLIENT_SERVER_CONNECTION_HANDLER_FACTORY_