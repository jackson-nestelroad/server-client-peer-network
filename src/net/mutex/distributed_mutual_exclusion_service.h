#ifndef NET_MUTEX_DISTRIBUTED_MUTUAL_EXCLUSION_SERVICE_
#define NET_MUTEX_DISTRIBUTED_MUTUAL_EXCLUSION_SERVICE_

#include <net/mutex/mutual_exclusion_service.h>
#include <net/network_service.h>
#include <net/peer/peer_network_manager.h>

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <vector>

namespace net {

namespace client {

struct ClientComponents;

}  // namespace client

namespace mutex {

/**
 * @brief Class for gaining mutual exclusion among a distributed network of
 * peer servers to perform an operation at multiple locations.
 *
 */
class DistributedMutualExclusionService : public NetworkService {
   public:
    using ready_callback_t = std::function<void(util::result<void, Error>)>;
    using release_callback_t = std::function<void(util::result<void, Error>)>;
    using mutex_operation_done_t =
        std::function<void(const release_callback_t&)>;
    using mutex_operation_t =
        std::function<void(util::result<mutex_operation_done_t, Error>)>;
    using error_callback_t = std::function<void(Error)>;

    DistributedMutualExclusionService(client::ClientComponents& components,
                                      const ready_callback_t& ready_callback,
                                      const error_callback_t& error_callback);

    /**
     * @brief Runs the given callback with distributed mutual exclusion gained
     * over the peer network.
     *
     * @param msg Message to perform
     * @param callback
     */
    void RunWithMutualExclusion(const std::string& file_name,
                                const mutex_operation_t& operation);

    std::size_t Timestamp() const;

   private:
    /**
     * @brief An entry in the peer network list.
     *
     */
    struct PeerNetworkEntry {
        peer::PeerConnectionReference connection;
        std::unique_ptr<MutualExclusionService> service;
        std::unordered_set<std::string> have_permission_for;
    };

    /**
     * @brief A request for mutual exclusion.
     *
     */
    struct MutualExclusionRequest {
        std::string file_name;
        mutex_operation_t operation;
        std::size_t timestamp;
    };

    /**
     * @brief A delayed request received from another peer.
     *
     */
    struct DelayedRequest {
        PeerNetworkEntry& entry;
        proto::mutex::RequestMessage request;
    };

    /**
     * @brief State of mutual exclusion for a write on the current file.
     *
     */
    enum class State {
        kWaiting,
        kRequesting,
        kInCriticalSection,
    };

    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    util::result<void, Error> CleanUp() override;

    void OnNetworkConnected(
        typename peer::PeerNetworkManager::PeerNetworkList network);

    void OnReceiveMessage(PeerNetworkEntry& entry,
                          util::result<proto::Message, Error> result);
    void OnReceiveRequest(PeerNetworkEntry& entry,
                          proto::mutex::RequestMessage request);
    void OnSendMessage(PeerNetworkEntry& entry,
                       util::result<void, Error> result);

    void OnNetworkRecovery(util::result<void, Error> result);

    /**
     * @brief Requests mutual exclusion on the network.
     *
     */
    void RequestMutualExclusion();

    /**
     * @brief Performs the operation in the critical section.
     *
     */
    void PerformCriticalSection();

    /**
     * @brief Releases mutual exclusion on the network.
     *
     * This method does not send any messages over the network. This process has
     * access to the critical section until another request comes in.
     *
     */
    void ReleaseMutualExclusion();

    /**
     * @brief Delivers and handles new incoming and outgoing requests since we
     * have been in the critical section.
     *
     */
    void DeliverDelayedRequests();

    void CheckForMutualExclusion();

    client::ClientComponents& components_;
    ready_callback_t ready_callback_;
    error_callback_t error_callback_;
    peer::PeerNetworkManager network_manager_;
    std::vector<PeerNetworkEntry> network_;

    std::mutex state_mutex_;
    std::size_t timestamp_;
    State state_;
    util::optional<MutualExclusionRequest> my_request_;
    std::queue<DelayedRequest> delayed_requests_;
};

}  // namespace mutex
}  // namespace net

#endif  // NET_MUTEX_DISTRIBUTED_MUTUAL_EXCLUSION_SERVICE_