#include "project2_service.h"

#include <net/server/server_components.h>
#include <util/console.h>
#include <util/strings.h>

namespace net {
namespace server {
namespace impl {

Project2Service::Project2Service(ServerComponents& components,
                                 Connection& client,
                                 BaseConnectionHandler& owner)
    : BaseService(components, client, owner),
      util::state_machine<Project2Service>(*this,
                                           states::AwaitMessage::instance()),
      message_service_(client_.socket, components_.common) {}

void Project2Service::Run() {
    util::state_machine<Project2Service>::start(
        [this](util::result<void, util::error> result) {
            if (result.is_err()) {
                util::safe_error_log::log(result.err().what());
            }
            BaseService::Stop();
        });
}

void Project2Service::Stop() { util::state_machine<Project2Service>::stop(); }

namespace states {

IMPL_STATE_HANDLER(Project2Service, AwaitMessage) {
    instance.message_service_.ReadMessage(
        [&instance, callback](util::result<proto::Message, Error> result) {
            if (result.is_err()) {
                callback(std::move(result).err());
                return;
            }

            instance.last_received_ = std::move(result).ok();

            switch (instance.last_received_.opcode) {
                case proto::Opcode::kEnquiry: {
                    instance.set_next_state(HandleEnquiry::instance());
                } break;
                case proto::Opcode::kRead: {
                    instance.set_next_state(HandleRead::instance());
                } break;
                case proto::Opcode::kWrite: {
                    instance.set_next_state(HandleWrite::instance());
                } break;
                default: {
                    instance.set_next_state(HandleInvalidOpcode::instance());
                } break;
            }

            callback(util::ok);
        });
}

IMPL_STATE_HANDLER_SETS_NEXT_STATE(Project2Service, AwaitMessage);

IMPL_STATE_HANDLER(Project2Service, HandleEnquiry) {
    auto peer_name = instance.client_.socket.PeerName();
    if (peer_name.is_err()) {
        callback(std::move(peer_name).err());
        return;
    }
    util::safe_console::log("Received Enquiry from", peer_name.ok());

    auto files = instance.components_.file_service_.GetFiles();
    if (files.is_err()) {
        callback(std::move(files).err());
        return;
    }

    instance.message_service_.WriteMessage(
        proto::ResponseMessage{util::strings::join(files.ok(), ", ")}
            .ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& error) -> util::error { return error; }));
        });
}

IMPL_NEXT_STATE(Project2Service, HandleEnquiry, AwaitMessage);

IMPL_STATE_HANDLER(Project2Service, HandleRead) {
    auto peer_name = instance.client_.socket.PeerName();
    if (peer_name.is_err()) {
        callback(std::move(peer_name).err());
        return;
    }
    util::safe_console::log("Received Read from", peer_name.ok());

    auto read = std::move(instance.last_received_).ToRead().ok();
    auto last_line =
        instance.components_.file_service_.ReadLastLine(read.file_name);

    if (last_line.is_err()) {
        instance.message_service_.WriteMessage(
            proto::ErrorMessage{std::move(last_line).err().what()}.ToMessage(),
            [&instance, callback](util::result<void, Error> result) {
                instance.set_next_state(Stop::instance());
                callback(util::ok);
            });
        return;
    }

    instance.message_service_.WriteMessage(
        proto::ResponseMessage{std::move(last_line).ok()}.ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& error) -> util::error { return error; }));
        });
}

IMPL_NEXT_STATE(Project2Service, HandleRead, AwaitMessage);

IMPL_STATE_HANDLER(Project2Service, HandleWrite) {
    auto peer_name = instance.client_.socket.PeerName();
    if (peer_name.is_err()) {
        callback(std::move(peer_name).err());
        return;
    }
    util::safe_console::log("Received Write from", peer_name.ok());

    auto write = std::move(instance.last_received_).ToWrite().ok();
    auto result = instance.components_.file_service_.AppendLine(write.file_name,
                                                                write.line);

    if (result.is_err()) {
        instance.message_service_.WriteMessage(
            proto::ErrorMessage{std::move(result).err().what()}.ToMessage(),
            [&instance, callback](util::result<void, Error> result) {
                instance.set_next_state(Stop::instance());
                callback(util::ok);
            });
        return;
    }

    instance.message_service_.WriteMessage(
        proto::OkMessage{}.ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& error) -> util::error { return error; }));
        });
}

IMPL_NEXT_STATE(Project2Service, HandleWrite, AwaitMessage);

IMPL_STATE_HANDLER(Project2Service, HandleInvalidOpcode) {
    auto peer_name = instance.client_.socket.PeerName();
    if (peer_name.is_err()) {
        callback(std::move(peer_name).err());
        return;
    }
    util::safe_console::log("Received invalid opcode from", peer_name.ok());

    instance.message_service_.WriteMessage(
        proto::ErrorMessage{"Invalid opcode"}.ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& error) -> util::error { return error; }));
        });
}

IMPL_NEXT_STATE(Project2Service, HandleInvalidOpcode, Stop);

IMPL_STATE_HANDLER(Project2Service, Stop) {}
IMPL_STATE_HANDLER_SETS_NEXT_STATE(Project2Service, Stop);
IMPL_STOP_STATE_SHOULD_STOP(Stop);

}  // namespace states

}  // namespace impl
}  // namespace server
}  // namespace net