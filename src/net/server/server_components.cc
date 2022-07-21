#include "server_components.h"

namespace net {
namespace server {

ServerComponents::ServerComponents(
    Components& common,
    std::unique_ptr<BaseConnectionHandlerFactory>&& connection_handler_factory)
    : common(common),
      connection_handler_factory(std::move(connection_handler_factory)),
      connection_manager(*this),
      file_service_() {}

}  // namespace server
}  // namespace net