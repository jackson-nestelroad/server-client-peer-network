#ifndef NET_MUTEX_MUTUAL_EXCLUSION_SERVICE_
#define NET_MUTEX_MUTUAL_EXCLUSION_SERVICE_

#include <net/components.h>
#include <net/peer/peer_connection.h>
#include <net/proto/async_message_service.h>

#include <functional>

namespace net {
namespace mutex {

/**
 * @brief A service for a peer connection in a mutual exclusion algorithm.
 *
 */
class MutualExclusionService {
   public:
    MutualExclusionService(Components& components,
                           peer::PeerConnectionReference& connection_);

    /**
     * @brief Starts continually receiving messages, with each message being
     * delivered to the given callback.
     *
     * @param callback
     */
    void StartReceivingMessages(
        const proto::AsyncMessageService::recv_callback_t& callback);

    /**
     * @brief Sends a message to the peer contained in the service.
     *
     * Use this method for responding to received messages.
     *
     * @param callback
     */
    void SendMessage(
        proto::Message&& msg,
        const proto::AsyncMessageService::send_callback_t& callback);

    void Stop();

    peer::PeerConnectionReference& Connection();
    bool Running() const;

   private:
    void ScheduleNextRead();

    Components& components_;
    peer::PeerConnectionReference& connection_;
    proto::AsyncMessageService message_reader_;
    proto::AsyncMessageService message_writer_;
    bool running_;
    proto::AsyncMessageService::recv_callback_t recv_callback_;
};

}  // namespace mutex
}  // namespace net

#endif  // NET_MUTEX_MUTUAL_EXCLUSION_SERVICE_