#include "peer_acceptor.h"

#include <util/console.h>
#include <util/number.h>

namespace net {
namespace peer {

PeerAcceptor::PeerAcceptor(PeerComponents& components,
                           const connect_callback_t& callback)
    : NetworkService(false),
      components_(components),
      callback_(callback),
      acceptor_(components_.common, [this](int sockfd) { OnAccept(sockfd); }) {}

util::result<void, Error> PeerAcceptor::SetUp() {
    port_t port = components_.common.options.port;
    util::safe_console::log("Starting peer server on port", port);
    acceptor_.SetPort(port);
    return util::ok;
}

util::result<void, Error> PeerAcceptor::OnStart() { return acceptor_.Start(); }

util::result<void, Error> PeerAcceptor::CleanUp() {
    acceptor_.Stop();
    for (auto& pair : pending_connections_) {
        auto service = pair.second;
        service->Cancel();
        components_.common.thread_pool.Schedule(
            [service]() { service->stop(); });
    }
    return util::ok;
}

void PeerAcceptor::AwaitConnectionFrom(const Location& location) {
    util::safe_console::log("Awaiting connection from", location);
    // Simply add this location to our allowed list.
    //
    // We have no way to initiate another client to send a request to our
    // server, so we just allow our server to await such a connection (whenever
    // the client decides to make it).
    //
    // The server connection callback will run when the connection is found.
    CRITICAL_SECTION(mutex_, allowed_.emplace(location.address));
}

void PeerAcceptor::OnAccept(int sockfd) {
    Socket socket(sockfd, SocketState::kConnected,
                  components_.common.options.timeout);
    auto peer_name_result = socket.PeerName();
    if (peer_name_result.is_err()) {
        util::safe_error_log::log(peer_name_result.err());
        return;
    }

    // Only accept this connection if it is in our allowed clients list.
    // We accept by moving it into our management map and giving it to a
    // service to receive the handshake.
    auto peer_name = std::move(peer_name_result).ok();
    util::safe_debug::log("Received connection from", peer_name);

    CRITICAL_SECTION_SAME_SCOPE(
        mutex_, auto it = allowed_.find(peer_name.address);
        if (it == allowed_.end()) {
            util::safe_debug::log("Rejecting connection from", peer_name);
            return;
        }

        socket.SetTimeout(Socket::kNoTimeout);

        auto emplace_result = pending_connections_.emplace(
            peer_name, std::make_shared<service::ReceiveHandshakeService>(
                           components_, peer_name, std::move(socket))););

    if (emplace_result.second) {
        util::safe_debug::log("Starting handshake with", peer_name);
        auto service = emplace_result.first->second;
        components_.common.thread_pool.Schedule([this, service, peer_name]() {
            service->start([this, service,
                            peer_name](util::result<void, util::error> result) {
                CRITICAL_SECTION(mutex_, {
                    pending_connections_.erase(peer_name);

                    if (result.is_err()) {
                        callback_(Error::Create(result.err().what()));
                    } else {
                        callback_(std::move(*service).Export());
                    }
                });
            });
        });
    }

    // Socket automatically closes on deconstruct.
}

}  // namespace peer
}  // namespace net