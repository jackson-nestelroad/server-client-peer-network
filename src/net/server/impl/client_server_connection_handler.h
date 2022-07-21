#ifndef NET_SERVER_IMPL_CLIENT_SERVER_CONNECTION_HANDLER_
#define NET_SERVER_IMPL_CLIENT_SERVER_CONNECTION_HANDLER_

#include <net/server/base_connection_handler.h>

namespace net {
namespace server {
namespace impl {

/**
 * @brief Connection handler for clients connected to the server.
 *
 */
class ClientServerConnectionHandler : public BaseConnectionHandler {
   public:
    using BaseConnectionHandler::BaseConnectionHandler;

    std::unique_ptr<BaseService> FirstService(ServerComponents& components,
                                              Connection& client) override;
};

}  // namespace impl
}  // namespace server
}  // namespace net

#endif  // NET_SERVER_IMPL_CLIENT_SERVER_CONNECTION_HANDLER_