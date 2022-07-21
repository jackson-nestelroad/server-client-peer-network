#ifndef NET_SERVER_BASE_CONNECTION_HANDLER_FACTORY_
#define NET_SERVER_BASE_CONNECTION_HANDLER_FACTORY_

#include <net/server/base_connection_handler.h>

#include <memory>

namespace net {
namespace server {

struct ServerComponents;

/**
 * @brief Base class for a factory that creates connection handlers.
 *
 */
class BaseConnectionHandlerFactory {
   public:
    virtual std::unique_ptr<BaseConnectionHandler> Create(
        std::shared_ptr<Connection> client, ServerComponents& components) = 0;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_BASE_CONNECTION_HANDLER_FACTORY_