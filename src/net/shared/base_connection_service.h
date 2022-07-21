#ifndef NET_SHARED_BASE_CONNECTION_SERVICE_
#define NET_SHARED_BASE_CONNECTION_SERVICE_

#include <net/components.h>
#include <net/connectable_socket.h>
#include <net/connection.h>

#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace net {
namespace shared {

/**
 * @brief Base service for establishing new connections to remote hosts.
 *
 * Deriving classes implement how exactly the connection is established.
 *
 */
class BaseConnectionService {
   public:
    using connect_callback_t =
        std::function<void(util::result<Connection, Error>)>;

    using PendingConnectionsList = std::list<ConnectableSocket>;
    using PendingConnectionIterator = typename PendingConnectionsList::iterator;

    BaseConnectionService(Components& components);

    /**
     * @brief Cancels all pending connections.
     *
     */
    void CancelPendingConnections();

   protected:
    /**
     * @brief Creates a new socket.
     *
     * @return PendingConnectionIterator
     */
    PendingConnectionIterator NewSocket();

    /**
     * @brief Creates a callback for the `ConnectableSocket::Connect` method.
     *
     * @param callback Callback to caller, for giving the connection out
     * @param it Iterator where socket is found
     * @return ConnectableSocket::connect_callback_t
     */
    typename ConnectableSocket::connect_callback_t ConnectCallback(
        const connect_callback_t& callback, PendingConnectionIterator it);

   private:
    /**
     * @brief Dispatches a connection by removing it from the pending list and
     * returning it out.
     *
     * @param it
     * @return Connection
     */
    Connection DispatchConnection(PendingConnectionIterator it);

   protected:
    Components& components_;

   private:
    std::mutex mutex_;
    PendingConnectionsList pending_connections_;
};

}  // namespace shared
}  // namespace net

#endif  // NET_SHARED_BASE_CONNECTION_SERVICE_