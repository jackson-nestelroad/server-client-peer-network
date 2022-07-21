#include "socket.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/poll.h>
#include <unistd.h>
#include <util/console.h>
#include <util/mutex.h>

#include <cstring>

namespace net {

Socket::Socket(int timeout)
    : state_(SocketState::kUninitialized),
      sockfd_(kInvalidSocket),
      timeout_(timeout) {
    EXIT_IF_ERROR(Initialize());
}

Socket::Socket(int sockfd, SocketState state, int timeout)
    : state_(state), sockfd_(sockfd), timeout_(timeout) {
    SetNonBlocking(true);
    SetKeepAlive(true);
}

Socket::~Socket() { Close(); }

Socket::Socket(Socket&& other) noexcept
    : state_(std::move(other.state_)),
      sockfd_(other.sockfd_),
      timeout_(other.timeout_),
      input_buffer_(std::move(other.input_buffer_)),
      output_buffer_(std::move(other.output_buffer_)) {
    // This is important so that the socket is not destroyed.
    other.sockfd_ = kInvalidSocket;
    other.state_ = SocketState::kClosed;
}

util::result<void, Error> Socket::Initialize() {
    sockfd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd_ < 0) {
        return Error::CreateFromErrNo("Failed to open socket");
    }
    util::safe_debug::log("Opened fd", sockfd_);
    state_ = SocketState::kInitialized;

    RETURN_IF_ERROR(SetNonBlocking(true));
    RETURN_IF_ERROR(SetKeepAlive(true));

    return util::ok;
}

util::result<void, Error> Socket::Close() {
    if (Closed()) {
        return util::ok;
    }

    int res = 0;
    CRITICAL_SECTION(close_mutex_, {
        switch (state_) {
            case SocketState::kConnected:
                // This helps stop poll, recv, and send operations.
                res = ::shutdown(sockfd_, SHUT_RDWR);
                if (res < 0) {
                    return Error::CreateFromErrNo("Failed to shutdown socket");
                }
            case SocketState::kHalfClosed:
            case SocketState::kInitialized:
                // This closes the file descriptor for good.
                util::safe_debug::log("Closing fd", sockfd_);
                res = ::close(sockfd_);
                if (res < 0) {
                    return Error::CreateFromErrNo("Failed to close socket");
                }
        }

        sockfd_ = kInvalidSocket;
        state_ = SocketState::kClosed;
        return util::ok;
    });
}

util::buffer& Socket::Input() & { return input_buffer_; }

util::buffer& Socket::Output() & { return output_buffer_; }

util::result<PollStatus, Error> Socket::Poll(PollOption option) {
    if (Closed()) {
        return PollStatus::Failure;
    }

    pollfd pfd;
    std::memset(&pfd, 0, sizeof(pfd));
    pfd.fd = sockfd_;

    switch (option) {
        case PollOption::kAny:
            pfd.events = POLLIN | POLLOUT | POLLHUP;
            break;
        case PollOption::kWrite:
            pfd.events = POLLOUT;
            break;
        case PollOption::kRead:
        default:
            pfd.events = POLLIN;
            break;
    }

    int res = ::poll(&pfd, 1, timeout_);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to poll socket");
    }

    if (res == 0) {
        return PollStatus::Expire;
    }

    // TODO: Error messages for each case?
    if (pfd.revents & (POLLERR | POLLNVAL)) {
        return PollStatus::Failure;
    }

    if (pfd.revents & POLLHUP) {
        state_ = SocketState::kHalfClosed;
    }

    switch (option) {
        case PollOption::kWrite:
            return (pfd.revents & POLLOUT) ? PollStatus::Success
                                           : PollStatus::Failure;
        case PollOption::kRead:
            return (pfd.revents & POLLIN) ? PollStatus::Success
                                          : PollStatus::Failure;
        default:
            return PollStatus::Success;
    }
}

util::result<void, Error> Socket::SetNonBlocking(bool val) {
    if (Closed()) {
        return Error::Create("Cannot set option on closed socket");
    }

    int flags = ::fcntl(sockfd_, F_GETFL);
    if (val) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    int res = ::fcntl(sockfd_, F_SETFL, flags);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to change non-blocking setting");
    }
    return util::ok;
}

util::result<void, Error> Socket::SetKeepAlive(bool val) {
    if (Closed()) {
        return Error::Create("Cannot set option on closed socket");
    }

    int flags = val ? 1 : 0;
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &flags,
                     sizeof(flags))) {
        return Error::CreateFromErrNo("Failed to change keep alive setting");
    };

    flags = 1;
    if (::setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPIDLE, &flags,
                     sizeof(flags))) {
        return Error::CreateFromErrNo("Failed to change keep alive setting");
    }

    flags = 1;
    if (::setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPINTVL, &flags,
                     sizeof(flags))) {
        return Error::CreateFromErrNo("Failed to change keep alive setting");
    }

    flags = 10;
    if (::setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPCNT, &flags,
                     sizeof(flags))) {
        return Error::CreateFromErrNo("Failed to change keep alive setting");
    }

    return util::ok;
}

util::result<std::size_t, Error> Socket::Send() {
    if (!Open()) {
        return Error::Create("Cannot send over a closed socket");
    }

    std::size_t total_bytes_sent = 0;

    // Get a view of the buffer, and send it all (if possible).
    auto views = output_buffer_.view();
    for (auto view : views) {
        std::size_t local_bytes_sent = 0;
        while (local_bytes_sent < view.size) {
            auto bytes_sent = ::write(sockfd_, view.data, view.size);
            if (bytes_sent < 0) {
                if (errno == EWOULDBLOCK) {
                    break;
                }
                return Error::CreateFromErrNo("Failed to send");
            }
            local_bytes_sent += bytes_sent;
        }
        total_bytes_sent += local_bytes_sent;
    }
    output_buffer_.consume(total_bytes_sent);
    return total_bytes_sent;
}

util::result<std::size_t, Error> Socket::Receive(std::size_t bytes) {
    if (!Open()) {
        return Error::Create("Cannot receive from a closed socket");
    }

    // Read into a space in the input buffer.
    auto read_buffer = input_buffer_.reserve(bytes);
    auto bytes_received = ::read(sockfd_, read_buffer, bytes);
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK) {
            return 0;
        }
        return Error::CreateFromErrNo("Failed to receive");
    }
    input_buffer_.commit(bytes_received);
    return bytes_received;
}

util::result<port_t, Error> Socket::Port() const {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int res = ::getsockname(sockfd_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to get socket name");
    }
    return ::ntohs(addr.sin_port);
}

util::result<Location, Error> Socket::PeerName() const {
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int res =
        ::getpeername(sockfd_, reinterpret_cast<sockaddr*>(&addr), &addrlen);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to get peer name");
    }
    return Location(addr.sin_addr.s_addr, ::ntohs(addr.sin_port));
}

util::result<Location, Error> Socket::HostName() const {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int res = ::getsockname(sockfd_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (res < 0) {
        return Error::CreateFromErrNo("Failed to get socket name");
    }
    return Location(addr.sin_addr.s_addr, ::ntohs(addr.sin_port));
}

bool Socket::Open() const {
    return state_ == SocketState::kConnected && sockfd_ != kInvalidSocket;
}

bool Socket::Closed() const {
    return state_ == SocketState::kUninitialized ||
           state_ == SocketState::kClosed || sockfd_ == kInvalidSocket;
}

SocketState Socket::State() const { return state_; }

void Socket::SetState(SocketState state) { state_ = state; }

void Socket::SetTimeout(int timeout) { timeout_ = timeout; }

int Socket::Native() { return sockfd_; }

}  // namespace net