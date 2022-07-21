#ifndef NET_CLIENT_SERVICE_CONNECTION_SERVICE_
#define NET_CLIENT_SERVICE_CONNECTION_SERVICE_

#include <net/shared/base_connection_service.h>

namespace net {
namespace client {

namespace service {

/**
 * @brief Service for establishing new connections to remote hosts.
 *
 */
class ConnectionService : public shared::BaseConnectionService {
   public:
    using BaseConnectionService::BaseConnectionService;

    /**
     * @brief Starts a new connection.
     *
     * @param hostname Hostname
     * @param port Port
     * @param callback Callback when connection is ready or if an error occurs
     */
    void NewConnection(const std::string& hostname, std::uint16_t port,
                       const connect_callback_t& callback,
                       std::size_t retries = 0);
};

}  // namespace service
}  // namespace client
}  // namespace net

#endif  // NET_CLIENT_SERVICE_CONNECTION_SERVICE_