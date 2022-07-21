#include "connection_service.h"

#include <net/client/client_components.h>

namespace net {
namespace client {
namespace service {

void ConnectionService::NewConnection(const std::string& hostname,
                                      std::uint16_t port,
                                      const connect_callback_t& callback,
                                      std::size_t retries) {
    auto it = NewSocket();
    it->Connect(hostname, port, ConnectCallback(callback, it), retries);
}

}  // namespace service
}  // namespace client
}  // namespace net