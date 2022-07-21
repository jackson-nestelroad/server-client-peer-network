#ifndef NET_PEER_PEER_ACCEPTOR_
#define NET_PEER_PEER_ACCEPTOR_

#include <net/components.h>
#include <net/location.h>
#include <net/network_service.h>
#include <net/peer/peer_components.h>
#include <net/peer/service/receive_handshake_service.h>
#include <net/server/acceptor.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace net {
namespace peer {

/**
 * @brief Class for the peer server to listen for and accept connections from
 * other peers.
 *
 */
class PeerAcceptor : public NetworkService {
   public:
    using connect_callback_t = std::function<void(
        util::result<service::ReceiveHandshakeService::Out, Error>)>;

    PeerAcceptor(PeerComponents& components,
                 const connect_callback_t& callback);

    /**
     * @brief Allows a single connection from a client at the given location.
     *
     * @param location
     */
    void AwaitConnectionFrom(const Location& location);

   private:
    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    util::result<void, Error> CleanUp() override;

    void OnAccept(int sockfd);

    PeerComponents& components_;
    connect_callback_t callback_;
    server::Acceptor acceptor_;
    std::mutex mutex_;
    // We use unordered multiset in the case of servers running on the same
    // machine.
    std::unordered_multiset<address_t> allowed_;
    std::unordered_map<Location,
                       std::shared_ptr<service::ReceiveHandshakeService>>
        pending_connections_;
};

}  // namespace peer
}  // namespace net

#endif  // NET_PEER_PEER_ACCEPTOR_