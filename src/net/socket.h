#ifndef NET_TCP_SOCKET_
#define NET_TCP_SOCKET_

#include <net/error.h>
#include <net/location.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <util/buffer.h>
#include <util/result.h>

#include <mutex>
#include <stdexcept>

namespace net {

/**
 * @brief Current state of the socket.
 *
 */
enum class SocketState {
    kUninitialized,
    kInitialized,
    kConnected,
    kHalfClosed,
    kClosed,
};

/**
 * @brief Option for `Socket::Poll`.
 *
 */
enum class PollOption {
    kAny,
    kRead,
    kWrite,
};

/**
 * @brief Result of `Socket::Poll`.
 *
 */
enum class PollStatus {
    Expire,
    Failure,
    Success,
};

/**
 * @brief Interface for reading from and writing to non-blocking UNIX sockets.
 *
 */
class Socket {
   public:
    static constexpr int kInvalidSocket =
        std::numeric_limits<port_t>::max() + 1;

    static constexpr int kNoTimeout = -1;

    Socket(int timeout);
    Socket(int sockfd, SocketState state, int timeout);
    ~Socket();
    Socket(const Socket& other) = delete;
    Socket(Socket&& other) noexcept;

    /**
     * @brief Polls the socket for some event to occur.
     *
     * @param option
     * @return util::result<PollResult, Error>
     */
    util::result<PollStatus, Error> Poll(PollOption option);

    /**
     * @brief Sets the non-blocking property on the socket accordingly.
     *
     * @param val
     * @return util::result<void, Error>
     */
    util::result<void, Error> SetNonBlocking(bool val);

    /**
     * @brief Set the keep alive proprety on the socket accordingly.
     *
     * @param value
     * @return util::result<void, Error>
     */
    util::result<void, Error> SetKeepAlive(bool value);

    /**
     * @brief Sends as much of the output buffer as possible without blocking.
     *
     * @return util::result<std::size_t, Error> Number of bytes sent
     */
    util::result<std::size_t, Error> Send();

    /**
     * @brief Receives as much data as readily available from the socket into
     * the input buffer.
     *
     * @param bytes Maximum number of bytes to receive.
     * @return util::result<std::size_t, Error>
     */
    util::result<std::size_t, Error> Receive(std::size_t bytes = 1024);

    /**
     * @brief Shuts down and closes the socket.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> Close();

    util::buffer& Input() &;
    util::buffer& Output() &;

    util::result<port_t, Error> Port() const;
    util::result<Location, Error> PeerName() const;
    util::result<Location, Error> HostName() const;

    bool Open() const;
    bool Closed() const;

    /**
     * @brief Returns the socket state.
     *
     * @return SocketState
     */
    SocketState State() const;

    /**
     * @brief Sets the socket state, which is helpful for cleanup.
     *
     * @param state
     */
    void SetState(SocketState state);

    /**
     * @brief Sets the timeout for socket operations.
     *
     * @param timeout
     */
    void SetTimeout(int timeout);

    /**
     * @brief The underlying socket file descriptor.
     *
     * @return int
     */
    int Native();

   protected:
    util::result<void, Error> Initialize();

    std::mutex close_mutex_;
    SocketState state_;
    int sockfd_;
    int timeout_;
    util::buffer input_buffer_;
    util::buffer output_buffer_;
};

}  // namespace net

#endif  // NET_TCP_SOCKET_