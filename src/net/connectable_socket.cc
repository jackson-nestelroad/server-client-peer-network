#include "connectable_socket.h"

#include <util/console.h>

#include <cstring>

namespace net {

ConnectableSocket::ConnectableSocket(int timeout, int retry_timeout)
    : Socket(timeout), retry_timeout_(retry_timeout) {}

util::result<void, Error> ConnectableSocket::Bind(std::uint16_t port) {
    struct addrinfo hints, *addr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    std::string port_string = std::to_string(port);
    int res = ::getaddrinfo(NULL, port_string.data(), &hints, &addr);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to get address info");
    }

    int yes = 1;
    res = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (res < 0) {
        return Error::CreateFromErrNo(
            "Failed to set reuse address option on socket");
    }

    res = ::bind(sockfd_, addr->ai_addr, addr->ai_addrlen);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to bind to port");
    }

    return util::ok;
}

util::result<void, Error> ConnectableSocket::Listen(
    std::size_t connection_queue_limit) {
    int res = ::listen(sockfd_, connection_queue_limit);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to listen on socket");
    }
    SetState(SocketState::kConnected);
    return util::ok;
}

void ConnectableSocket::Connect(const std::string& hostname, std::uint16_t port,
                                const connect_callback_t& callback,
                                std::size_t retries) {
    // To make our life easier, we let this call block the thread. We can't
    // really do anything until we're connected anyway.
    SetNonBlocking(false);

    sockaddr_in server_addr;
    hostent* server = ::gethostbyname(hostname.data());
    if (!server) {
        callback(Error::Create("No such host"));
        return;
    }

    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    std::memmove(&server_addr.sin_addr.s_addr, server->h_addr,
                 server->h_length);
    server_addr.sin_port = ::htons(port);

    std::size_t attempts = std::max(retries, retries + 1);
    for (std::size_t attempt = 0; attempt < attempts; ++attempt) {
        if (attempt != 0) {
            // Wait for retry timeout.
            // Using a conditional variable allows this timeout to be
            // canceled.
            std::unique_lock<std::mutex> lock(mutex_);
            auto result =
                cv_.wait_for(lock, std::chrono::milliseconds(retry_timeout_));
            if (result == std::cv_status::no_timeout) {
                util::safe_debug::log("Stopping connection attempts");
                return;
            }
        }

        int res = ::connect(sockfd_, reinterpret_cast<sockaddr*>(&server_addr),
                            sizeof(server_addr));
        if (res < 0) {
            if (errno == ECONNREFUSED) {
                util::safe_debug::stream("Attempt ", attempt + 1,
                                         ": failed to connect to ", hostname,
                                         ':', port, ", waiting to retry",
                                         util::manip::endl);

                continue;
            }

            callback(Error::CreateFromErrNo("Failed to connect"));
            return;
        } else {
            SetState(SocketState::kConnected);
            SetNonBlocking(true);
            callback(util::ok);
            return;
        }
    }

    callback(Error::Create(util::string::stream(
        "Failed to connect to ", hostname, ':', port, " in ", attempts,
        " attempt", attempts == 1 ? "" : "s")));
}

util::result<void, Error> ConnectableSocket::Close() {
    cv_.notify_all();
    return Socket::Close();
}

Socket ConnectableSocket::ToSocket() && {
    Socket out = Socket(sockfd_, state_, timeout_);
    sockfd_ = 0;
    state_ = SocketState::kClosed;
    return out;
}

}  // namespace net