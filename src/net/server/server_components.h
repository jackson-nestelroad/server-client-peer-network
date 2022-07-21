#ifndef NET_SERVER_SERVER_COMPONENTS_
#define NET_SERVER_SERVER_COMPONENTS_

#include <net/components.h>
#include <net/server/base_connection_handler_factory.h>
#include <net/server/connection_manager.h>
#include <net/server/service/file_service.h>

namespace net {
namespace server {

/**
 * @brief Components of a server that can be passed down to other classes for
 * use.
 *
 * Make sure these objects are properly synchronized!
 *
 */
struct ServerComponents {
    ServerComponents(Components& common,
                     std::unique_ptr<BaseConnectionHandlerFactory>&&
                         connection_handler_factory);

    Components& common;
    std::unique_ptr<BaseConnectionHandlerFactory> connection_handler_factory;
    ConnectionManager connection_manager;
    service::FileService file_service_;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_SERVER_COMPONENTS_