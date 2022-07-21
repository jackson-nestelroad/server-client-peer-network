#include "send_handshake_service.h"

#include <util/console.h>

namespace net {
namespace peer {
namespace service {

SendHandshakeService::SendHandshakeService(PeerComponents& components,
                                           const Location& target)
    : util::state_machine<SendHandshakeService>(
          *this, send_handshake_service_states::Connect::instance()),
      components_(components),
      target_(target),
      server_id_(proto::kNoId),
      socket_(components_.common.options.timeout,
              components_.common.options.retry_timeout),
      message_service_(socket_, components_.common) {}

const Location& SendHandshakeService::Target() const { return target_; }

util::result<SendHandshakeService::Out, Error>
SendHandshakeService::Export() && {
    if (socket_.State() != SocketState::kConnected) {
        return Error::Create("Socket is not connected and ready for export");
    }

    return Out{target_, server_id_, std::move(socket_).ToSocket()};
}

util::result<void, Error> SendHandshakeService::Cancel() {
    util::safe_debug::log("Canceling handshake service for connecting to",
                          target_);
    return socket_.Close();
}

namespace send_handshake_service_states {

IMPL_STATE_HANDLER(SendHandshakeService, Connect) {
    instance.socket_.Connect(
        instance.target_.HostName(), instance.target_.port,
        [this, callback](util::result<void, Error> result) {
            if (result.is_err()) {
                callback(result.err().what());
            } else {
                callback(util::ok);
            }
        },
        10);
}

IMPL_NEXT_STATE(SendHandshakeService, Connect, SendHandshake);

IMPL_STATE_HANDLER(SendHandshakeService, SendHandshake) {
    auto password = instance.components_.common.props.Get("password");
    if (!password.has_value()) {
        callback(
            util::error("Property \"password\" is not defined for handshake"));
        return;
    }
    util::safe_debug::log("Sending handshake to", instance.target_);
    instance.message_service_.WriteMessage(
        proto::EstablishConnectionMessage{
            static_cast<proto::node_id_t>(
                instance.components_.common.options.id),
            password.value()}
            .ToMessage(),
        [this, callback](util::result<void, Error> result) {
            if (result.is_err()) {
                callback(result.err().what());
            } else {
                callback(util::ok);
            }
        });
}

IMPL_NEXT_STATE(SendHandshakeService, SendHandshake, ReceiveHandshake);

IMPL_STATE_HANDLER(SendHandshakeService, ReceiveHandshake) {
    util::safe_debug::log("Waiting for response from", instance.target_);
    instance.message_service_.ReadMessage(
        [this, &instance,
         callback](util::result<proto::Message, Error> result) {
            if (result.is_err()) {
                callback(result.err().what());
            } else {
                auto msg = std::move(result).ok();
                if (msg.opcode == proto::Opcode::kEstablishConnection) {
                    util::safe_debug::log("Received handshake response from",
                                          instance.target_);
                    auto establish =
                        std::move(msg).ToEstablishConnection().ok();
                    instance.server_id_ = establish.id;
                    callback(util::ok);
                } else {
                    callback(util::error("Peer server denied handshake"));
                }
            }
        });
}

IMPL_NEXT_STATE(SendHandshakeService, ReceiveHandshake, SendOk);

IMPL_STATE_HANDLER(SendHandshakeService, SendOk) {
    util::safe_debug::log("Sending Ok to", instance.target_);
    instance.message_service_.WriteMessage(
        proto::OkMessage().ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& err) -> util::error { return err; }));
        });
}

IMPL_NEXT_STATE(SendHandshakeService, SendOk, Stop);

IMPL_STATE_HANDLER(SendHandshakeService, Stop) {}

IMPL_NEXT_STATE(SendHandshakeService, Stop, Stop);

IMPL_STOP_STATE_SHOULD_STOP(Stop);

}  // namespace send_handshake_service_states

}  // namespace service
}  // namespace peer
}  // namespace net