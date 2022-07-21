#ifndef NET_SERVER_CONNECTION_MANAGER_
#define NET_SERVER_CONNECTION_MANAGER_

#include <net/connection.h>
#include <net/server/base_connection_handler.h>
#include <util/optional.h>

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>

namespace net {
namespace server {

/**
 * @brief Class for managing all connections and their respective handlers.
 *
 */
class ConnectionManager {
   public:
    using stop_callback_t = std::function<void()>;

    ConnectionManager(ServerComponents& components);

    /**
     * @brief Creates a new connection over the given socket.
     *
     * @param sockfd Connected socket, received from `accept`.
     * @return ClientConnection&
     */
    Connection& NewConnection(int sockfd);

    /**
     * @brief Destroys the given client, assuming it has not yet been started.
     *
     * @param client
     */
    void Destroy(Connection& client);

    /**
     * @brief Starts handling and interacting with the given client.
     *
     * @param client
     */
    void Start(Connection& client);

    /**
     * @brief Stops the given connection handler and its associated client
     * connection.
     *
     * @param service
     */
    void Stop(const std::unique_ptr<BaseConnectionHandler>& service);

    /**
     * @brief Closes all connection handlers and sockets.
     *
     * Used to stop the server.
     *
     */
    void CloseAll();

   private:
    std::mutex data_mutex_;
    std::unordered_map<Connection::id_t, std::shared_ptr<Connection>>
        connections_;
    std::set<std::unique_ptr<BaseConnectionHandler>> handlers_;
    ServerComponents& components_;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_CONNECTION_MANAGER_