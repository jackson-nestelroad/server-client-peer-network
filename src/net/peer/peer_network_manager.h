#ifndef NET_PEER_PEER_NETWORK_MANAGER_
#define NET_PEER_PEER_NETWORK_MANAGER_

#include <net/connection.h>
#include <net/location.h>
#include <net/network_service.h>
#include <net/peer/peer_acceptor.h>
#include <net/peer/peer_components.h>
#include <net/peer/peer_connection.h>
#include <net/peer/peer_connector.h>

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace net {
namespace peer {

/**
 * @brief Manager for all connections in a connected peer network.
 *
 */
class PeerNetworkManager : public NetworkService {
   public:
    using PeerNetworkList = std::vector<PeerConnectionReference>;
    using connected_callback_t =
        std::function<void(util::result<PeerNetworkList, Error>)>;
    using recovered_callback_t = std::function<void(util::result<void, Error>)>;

    PeerNetworkManager(Components& components);

    /**
     * @brief Calls the given callback as soon as the network is fully connected
     * and ready for use.
     *
     * @param callback
     */
    void AwaitConnected(const connected_callback_t& callback);

    /**
     * @brief Reports an error with a connection in the peer network.
     *
     * The given callback is called as soon as the network has recovered.
     *
     * @param connection Faulty connection
     * @param callback
     */
    void ReportError(Connection& connection,
                     const recovered_callback_t& callback);

    /**
     * @brief Returns if the network manager is connected and usable.
     *
     * @return true
     * @return false
     */
    bool IsConnected() const;

   private:
    /**
     * @brief State of the peer network.
     *
     */
    enum class State {
        kInitializing,
        kConnected,
        kRecovering,
        kBroken,
        kClosed,
    };

    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    void OnStop() override;
    util::result<void, Error> CleanUp() override;

    PeerConnection& GetConnectionMapEntry(proto::node_id_t id);
    void UpdateState(State new_state);

    void SignalStopWithError(net::Error&& error);

    using callbacks_t = std::uint8_t;

    struct Callbacks {
        static constexpr std::uint8_t kConnected = 1 << 0;
        static constexpr std::uint8_t kRecovering = 1 << 1;
        static constexpr std::uint8_t kAll = 0b11;
    };

    void SendSuccessToCallbacks(callbacks_t callbacks);
    void SendErrorToCallbacks(Error error, callbacks_t callbacks);

    void OnClientConnection(
        util::result<service::SendHandshakeService::Out, Error> result);
    void OnServerConnection(
        util::result<service::ReceiveHandshakeService::Out, Error> result);

    bool IsConnectedImpl() const;
    void CheckIfConnected();
    PeerNetworkList ConstructNetwork();

    PeerComponents components_;
    std::uint16_t my_port_;
    std::vector<Location> peer_locations_;
    State state_;

    std::mutex connections_mutex_;
    std::unordered_map<proto::node_id_t, PeerConnection> managed_connections_;

    std::mutex callback_mutex_;
    std::vector<connected_callback_t> connected_callbacks_;
    std::vector<recovered_callback_t> recovered_callbacks_;

    util::optional<Error> stopping_error_;

    PeerConnector connector_;
    PeerAcceptor acceptor_;
};

}  // namespace peer
}  // namespace net

#endif  // NET_PEER_PEER_NETWORK_MANAGER_