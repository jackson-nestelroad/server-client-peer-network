#ifndef NET_PEER_SERVICE_PEER_HANDSHAKE_SERVICE_
#define NET_PEER_SERVICE_PEER_HANDSHAKE_SERVICE_

#include <net/connection.h>
#include <net/location.h>
#include <net/peer/peer_components.h>
#include <net/proto/async_message_service.h>
#include <util/state_machine.h>

namespace net {
namespace peer {
namespace service {

class ReceiveHandshakeService;

namespace receive_handshake_service_states {

DEFINE_STATE(ReceiveHandshakeService, Initialize);
DEFINE_ASYNC_STATE(ReceiveHandshakeService, AwaitHandshake);
DEFINE_ASYNC_STATE(ReceiveHandshakeService, SendResponse);
DEFINE_ASYNC_STATE(ReceiveHandshakeService, ReceiveOk);
DEFINE_STOP_STATE(ReceiveHandshakeService, Stop);

}  // namespace receive_handshake_service_states

/**
 * @brief Service for receiving a handshake from a client.
 *
 */
class ReceiveHandshakeService
    : public util::state_machine<ReceiveHandshakeService> {
   public:
    struct Out {
        Location location;
        proto::node_id_t client_id;
        Socket socket;
    };

    ReceiveHandshakeService(PeerComponents& components,
                            const Location& location, Socket&& socket);

    util::result<Out, Error> Export() &&;

    util::result<void, Error> Cancel();

   private:
    PeerComponents& components_;
    Location location_;
    std::string expected_password_;
    proto::node_id_t client_id_;
    Socket socket_;
    proto::AsyncMessageService message_service_;

    friend struct receive_handshake_service_states::Initialize;
    friend struct receive_handshake_service_states::AwaitHandshake;
    friend struct receive_handshake_service_states::SendResponse;
    friend struct receive_handshake_service_states::ReceiveOk;
    friend struct receive_handshake_service_states::Stop;
};

}  // namespace service
}  // namespace peer
}  // namespace net

#endif  // NET_PEER_SERVICE_PEER_HANDSHAKE_SERVICE_