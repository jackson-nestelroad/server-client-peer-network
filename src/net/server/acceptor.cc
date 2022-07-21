#include "acceptor.h"

#include <arpa/inet.h>
#include <util/console.h>

namespace net {
namespace server {

Acceptor::Acceptor(Components& components, const accept_callback_t& on_accept)
    : NetworkService(false),
      components_(components),
      on_accept_(on_accept),
      port_(0),
      listener_(components_.options.timeout,
                components_.options.retry_timeout) {}

util::result<void, Error> Acceptor::SetUp() {
    // Start by setting up the listener socket, which will receive and accept
    // connections.
    RETURN_IF_ERROR(listener_.Bind(port_));
    RETURN_IF_ERROR(listener_.Listen());
    return util::ok;
}

util::result<void, Error> Acceptor::OnStart() {
    components_.thread_pool.Schedule([this]() { AcceptConnections(); });
    return util::ok;
}

util::result<void, Error> Acceptor::CleanUp() { return listener_.Close(); }

void* GetInAddr(sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &((reinterpret_cast<sockaddr_in*>(sa))->sin_addr);
    }
    return &((reinterpret_cast<sockaddr_in6*>(sa))->sin6_addr);
}

void Acceptor::AcceptConnections() {
    char ip_str[INET6_ADDRSTRLEN];
    // To make our life easier, just allow `accept` to block the listener
    // thread.
    listener_.SetNonBlocking(false);
    while (Running()) {
        sockaddr client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        int new_fd =
            ::accept(listener_.Native(), &client_addr, &client_addr_size);

        // Server may have stopped at this point, in which case we don't start a
        // connection here.
        if (!Running()) {
            break;
        }

        if (new_fd < 0) {
            util::safe_error_log::log(
                Error::CreateFromErrNo("Failed to accept new connection"));

            continue;
        }

        ::inet_ntop(client_addr.sa_family,
                    GetInAddr(static_cast<sockaddr*>(&client_addr)), ip_str,
                    sizeof(ip_str));
        util::safe_console::stream("Received connection from ", ip_str,
                                   " (sockfd = ", new_fd, ")",
                                   util::manip::endl);

        components_.thread_pool.Schedule(
            [this, new_fd]() { on_accept_(new_fd); });
    }
}

void Acceptor::SetPort(std::uint16_t port) { port_ = port; }

std::uint16_t Acceptor::Port() const { return listener_.Port().ok_or(0); }

}  // namespace server
}  // namespace net