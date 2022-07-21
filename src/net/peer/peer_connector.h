#ifndef NET_PEER_PEER_CONNECTOR_
#define NET_PEER_PEER_CONNECTOR_

#include <net/location.h>
#include <net/network_service.h>
#include <net/peer/peer_components.h>
#include <net/peer/service/send_handshake_service.h>

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace net {
namespace peer {

/**
 * @brief Class for establishing a forward connection to other peer
 * servers.
 *
 */
class PeerConnector : public NetworkService {
   public:
    using connect_callback_t = std::function<void(
        util::result<service::SendHandshakeService::Out, Error>)>;

    PeerConnector(PeerComponents& components,
                  const connect_callback_t& callback);

    /**
     * @brief Initiates a new connection to the given server.
     *
     * @param target
     */
    void Connect(const Location& target);

   private:
    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    util::result<void, Error> CleanUp() override;

    PeerComponents& components_;
    connect_callback_t callback_;
    std::mutex mutex_;
    std::unordered_map<Location, std::shared_ptr<service::SendHandshakeService>>
        pending_connections_;
};

}  // namespace peer
}  // namespace net

#endif  // NET_PEER_PEER_CONNECTOR_