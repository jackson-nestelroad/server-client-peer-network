#ifndef NET_PEER_SERVICE_SEND_HANDSHAKE_SERVICE_
#define NET_PEER_SERVICE_SEND_HANDSHAKE_SERVICE_

#include <net/connectable_socket.h>
#include <net/connection.h>
#include <net/location.h>
#include <net/peer/peer_components.h>
#include <net/proto/async_message_service.h>
#include <util/state_machine.h>

#include <functional>

namespace net {
namespace peer {
namespace service {

class SendHandshakeService;

namespace send_handshake_service_states {

DEFINE_ASYNC_STATE(SendHandshakeService, Connect);
DEFINE_ASYNC_STATE(SendHandshakeService, SendHandshake);
DEFINE_ASYNC_STATE(SendHandshakeService, ReceiveHandshake);
DEFINE_ASYNC_STATE(SendHandshakeService, SendOk);
DEFINE_STOP_STATE(SendHandshakeService, Stop);

}  // namespace send_handshake_service_states

/**
 * @brief Service for sending a handshake to an upstream peer server.
 *
 */
class SendHandshakeService : public util::state_machine<SendHandshakeService> {
   public:
    struct Out {
        Location target;
        proto::node_id_t server_id;
        Socket socket;
    };

    SendHandshakeService(PeerComponents& components, const Location& target);

    const Location& Target() const;

    util::result<Out, Error> Export() &&;

    util::result<void, Error> Cancel();

   private:
    PeerComponents& components_;
    Location target_;
    proto::node_id_t server_id_;
    ConnectableSocket socket_;
    proto::AsyncMessageService message_service_;

    friend struct send_handshake_service_states::Connect;
    friend struct send_handshake_service_states::SendHandshake;
    friend struct send_handshake_service_states::ReceiveHandshake;
    friend struct send_handshake_service_states::SendOk;
    friend struct send_handshake_service_states::Stop;
};

}  // namespace service
}  // namespace peer
}  // namespace net

#endif  // NET_PEER_SERVICE_SEND_HANDSHAKE_SERVICE_