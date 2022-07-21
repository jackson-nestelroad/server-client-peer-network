#include "receive_handshake_service.h"

#include <util/console.h>

namespace net {
namespace peer {
namespace service {

ReceiveHandshakeService::ReceiveHandshakeService(PeerComponents& components,
                                                 const Location& location,
                                                 Socket&& socket)
    : util::state_machine<ReceiveHandshakeService>(
          *this, receive_handshake_service_states::Initialize::instance()),
      components_(components),
      location_(location),
      client_id_(proto::kNoId),
      socket_(std::move(socket)),
      message_service_(socket_, components_.common) {}

util::result<ReceiveHandshakeService::Out, Error>
ReceiveHandshakeService::Export() && {
    return Out{location_, client_id_, std::move(socket_)};
}

util::result<void, Error> ReceiveHandshakeService::Cancel() {
    return socket_.Close();
}

namespace receive_handshake_service_states {

IMPL_STATE_HANDLER(ReceiveHandshakeService, Initialize) {
    auto password_value = instance.components_.common.props.Get("password");
    if (!password_value.has_value()) {
        callback(
            util::error("Property \"password\" is not defined for handshake"));
        return;
    }
    instance.expected_password_ = password_value.value();
    callback(util::ok);
}

IMPL_NEXT_STATE(ReceiveHandshakeService, Initialize, AwaitHandshake);

IMPL_STATE_HANDLER(ReceiveHandshakeService, AwaitHandshake) {
    util::safe_debug::log("Waiting for handshake from", instance.location_);
    instance.message_service_.ReadMessage(
        [&instance, callback](util::result<proto::Message, Error> result) {
            if (result.is_err()) {
                callback(result.err());
                return;
            }

            auto& msg = result.ok();
            switch (msg.opcode) {
                case proto::Opcode::kEstablishConnection: {
                    util::safe_debug::log("Received handshake from",
                                          instance.location_);
                    auto establish_msg_result =
                        std::move(msg).ToEstablishConnection();
                    if (establish_msg_result.is_err()) {
                        callback(establish_msg_result.err());
                        return;
                    }

                    auto establish_msg = std::move(establish_msg_result).ok();
                    if (instance.expected_password_ == establish_msg.message) {
                        instance.client_id_ = establish_msg.id;
                        callback(util::ok);
                    } else {
                        callback(
                            util::error("Invalid password received from client "
                                        "connected to peer server"));
                    }
                } break;
                case proto::Opcode::kError: {
                    auto error_msg_result = std::move(msg).ToError();
                    callback(
                        error_msg_result.is_err()
                            ? error_msg_result.err()
                            : util::error(
                                  std::move(error_msg_result).ok().message));
                } break;
                default: {
                    callback(util::error("Invalid opcode"));
                } break;
            }
        });
}

IMPL_NEXT_STATE(ReceiveHandshakeService, AwaitHandshake, SendResponse);

IMPL_STATE_HANDLER(ReceiveHandshakeService, SendResponse) {
    util::safe_debug::log("Sending handshake response to", instance.location_);
    instance.message_service_.WriteMessage(
        proto::EstablishConnectionMessage{
            static_cast<proto::node_id_t>(
                instance.components_.common.options.id)}
            .ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& err) -> util::error { return err; }));
        });
}

IMPL_NEXT_STATE(ReceiveHandshakeService, SendResponse, ReceiveOk);

IMPL_STATE_HANDLER(ReceiveHandshakeService, ReceiveOk) {
    util::safe_debug::log("Waiting for Ok from", instance.location_);
    instance.message_service_.ReadMessage(
        [this, &instance,
         callback](util::result<proto::Message, Error> result) {
            if (result.is_err()) {
                callback(result.err().what());
            } else {
                auto msg = std::move(result).ok();
                if (msg.opcode == proto::Opcode::kOk) {
                    util::safe_debug::log("Received Ok from",
                                          instance.location_);
                    callback(util::ok);
                } else {
                    callback(util::error("Peer server denied handshake"));
                }
            }
        });
}

IMPL_NEXT_STATE(ReceiveHandshakeService, ReceiveOk, Stop);

IMPL_STATE_HANDLER(ReceiveHandshakeService, Stop) {}

IMPL_NEXT_STATE(ReceiveHandshakeService, Stop, Stop);

IMPL_STOP_STATE_SHOULD_STOP(Stop);

}  // namespace receive_handshake_service_states

}  // namespace service
}  // namespace peer
}  // namespace net