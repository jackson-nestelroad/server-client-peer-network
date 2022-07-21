#ifndef NET_CONNETABLE_SOCKET_
#define NET_CONNETABLE_SOCKET_

#include <net/socket.h>

#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
#include <string>

namespace net {

/**
 * @brief Interface for working with a socket that can be connected in various
 * ways.
 *
 */
class ConnectableSocket : public Socket {
   public:
    using connect_callback_t = std::function<void(util::result<void, Error>)>;
    static constexpr std::size_t kInfiniteRetries =
        std::numeric_limits<std::size_t>::max();

    ConnectableSocket(int timeout, int retry_timeout);

    /**
     * @brief Binds the socket to the given port.
     *
     * @param port
     * @return util::result<void, Error>
     */
    util::result<void, Error> Bind(std::uint16_t port);

    /**
     * @brief Begins listening for connections.
     *
     * @param connection_queue_limit Maximum number of connections to allow in
     * the acceptor queue at a time
     * @return util::result<void, Error>
     */
    util::result<void, Error> Listen(std::size_t connection_queue_limit = 10);

    /**
     * @brief Connects the socket to a remote server.
     *
     * @param hostname Target hostname
     * @param port Target port
     * @param callback Callback when connection is established or an error
     * occurs
     * @param retries Number of retries allowed
     *
     */
    void Connect(const std::string& hostname, std::uint16_t port,
                 const connect_callback_t& callback, std::size_t retries = 0);

    /**
     * @brief Shuts down and closes the socket.
     *
     * If the socket is still attempting to connect to a remote host, the
     * operation will be canceled.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> Close();

    /**
     * @brief Permanently downcasts to a normal socket.
     *
     * @return Socket&
     */
    Socket ToSocket() &&;

   private:
    int retry_timeout_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace net

#endif  // NET_CONNETABLE_SOCKET_