#include "mutual_exclusion_service.h"

namespace net {
namespace mutex {

MutualExclusionService::MutualExclusionService(
    Components& components, peer::PeerConnectionReference& connection)
    : components_(components),
      connection_(connection),
      message_reader_(connection_.in.socket, components_),
      message_writer_(connection_.out.socket, components_) {}

void MutualExclusionService::StartReceivingMessages(
    const proto::AsyncMessageService::recv_callback_t& callback) {
    running_ = true;
    recv_callback_ = callback;
    ScheduleNextRead();
}

void MutualExclusionService::ScheduleNextRead() {
    components_.thread_pool.Schedule([this]() {
        message_reader_.ReadMessage(
            [this](util::result<proto::Message, Error> result) {
                if (result.is_err()) {
                    running_ = false;
                    recv_callback_(std::move(result));
                } else {
                    ScheduleNextRead();
                    recv_callback_(std::move(result));
                }
            });
    });
}

void MutualExclusionService::SendMessage(
    proto::Message&& msg,
    const proto::AsyncMessageService::send_callback_t& callback) {
    message_writer_.WriteMessage(std::move(msg), callback);
}

void MutualExclusionService::Stop() { running_ = false; }

peer::PeerConnectionReference& MutualExclusionService::Connection() {
    return connection_;
}

bool MutualExclusionService::Running() const { return running_; }

}  // namespace mutex
}  // namespace net