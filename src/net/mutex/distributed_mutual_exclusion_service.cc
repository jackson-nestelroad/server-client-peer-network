#include "distributed_mutual_exclusion_service.h"

#include <net/client/client_components.h>
#include <util/console.h>
#include <util/mutex.h>

#include <algorithm>

namespace net {
namespace mutex {

DistributedMutualExclusionService::DistributedMutualExclusionService(
    client::ClientComponents& components,
    const ready_callback_t& ready_callback,
    const error_callback_t& error_callback)
    : NetworkService(false),
      components_(components),
      ready_callback_(ready_callback),
      error_callback_(error_callback),
      network_manager_(components_.common),
      timestamp_(0),
      state_(State::kWaiting) {}

std::size_t DistributedMutualExclusionService::Timestamp() const {
    return timestamp_;
}

util::result<void, Error> DistributedMutualExclusionService::SetUp() {
    return util::ok;
}

util::result<void, Error> DistributedMutualExclusionService::OnStart() {
    network_manager_.AwaitConnected(
        [this](util::result<typename peer::PeerNetworkManager::PeerNetworkList,
                            Error>
                   result) {
            if (result.is_err()) {
                ready_callback_(std::move(result).err());
            } else {
                OnNetworkConnected(std::move(result).ok());
            }
        });
    RETURN_IF_ERROR(network_manager_.Start());
    return util::ok;
}

util::result<void, Error> DistributedMutualExclusionService::CleanUp() {
    for (auto& entry : network_) {
        entry.service->Stop();
    }
    return network_manager_.Stop();
}

void DistributedMutualExclusionService::OnNetworkConnected(
    typename peer::PeerNetworkManager::PeerNetworkList network) {
    network_.reserve(network.size());
    for (auto& connection : network) {
        network_.emplace_back(PeerNetworkEntry{connection, nullptr});
        auto& back = network_.back();
        back.service.reset(
            new MutualExclusionService(components_.common, back.connection));
        back.service->StartReceivingMessages(
            [this, &back](util::result<proto::Message, Error> result) {
                OnReceiveMessage(back, std::move(result));
            });
    }

    ready_callback_(util::ok);
}

void DistributedMutualExclusionService::OnReceiveMessage(
    PeerNetworkEntry& entry, util::result<proto::Message, Error> result) {
    if (result.is_err()) {
        network_manager_.ReportError(entry.connection.in,
                                     [this](util::result<void, Error> result) {
                                         OnNetworkRecovery(std::move(result));
                                     });
        return;
    }

    auto msg = std::move(result).ok();
    switch (msg.opcode) {
        case proto::Opcode::kError: {
            // An Error message is sent between peers when a distributed
            // operation fails.
            //
            // For now, we report an error here.
            auto error = std::move(msg).ToError().ok();
            util::safe_error_log::log("Received Error from a peer:",
                                      error.message);
            network_manager_.ReportError(
                entry.connection.in, [this](util::result<void, Error> result) {
                    OnNetworkRecovery(std::move(result));
                });
        } break;
        case proto::Opcode::kReply: {
            auto reply = std::move(msg).ToReply().ok();
            std::string& file_name = reply.file_name;

            util::safe_debug::log("Received Reply from peer",
                                  static_cast<int>(entry.connection.id), "for",
                                  file_name);

            // Replies force the timestamp higher.
            CRITICAL_SECTION(state_mutex_, {
                timestamp_ = std::max(reply.timestamp + 1, timestamp_ + 1);
                entry.have_permission_for.emplace(file_name);
            });
            CheckForMutualExclusion();
        } break;
        case proto::Opcode::kRequest: {
            OnReceiveRequest(entry, std::move(msg).ToRequest().ok());
        } break;
        default: {
            // Ignore invalid opcodes.
        } break;
    }
}

void DistributedMutualExclusionService::OnReceiveRequest(
    PeerNetworkEntry& entry, proto::mutex::RequestMessage request) {
    std::string& file_name = request.file_name;

    util::safe_debug::log("Received Request from peer",
                          static_cast<int>(entry.connection.id), "for",
                          file_name);

    // Requests force the timestamp higher.
    CRITICAL_SECTION(state_mutex_, {
        timestamp_ = std::max(request.timestamp + 1, timestamp_ + 1);
    });

    switch (state_) {
        case State::kWaiting: {
            // I am not requesting for the critical section nor in it, so I will
            // send my reply now.

            // I lost permission for this filefrom the sender.
            CRITICAL_SECTION(state_mutex_,
                             entry.have_permission_for.erase(file_name));
            entry.service->SendMessage(
                proto::mutex::ReplyMessage(timestamp_, file_name).ToMessage(),
                [this, &entry](util::result<void, Error> result) {
                    OnSendMessage(entry, std::move(result));
                });
        } break;
        case State::kInCriticalSection: {
            // I am in the critical section, so wait to reply.
            CRITICAL_SECTION(state_mutex_,
                             delayed_requests_.emplace(
                                 DelayedRequest{entry, std::move(request)}));
        } break;
        case State::kRequesting: {
            // I am not in my critical section, but I have a request out.
            auto& mine = my_request_.value();
            if (mine.file_name != file_name) {
                // Different file, so we can reply.
                CRITICAL_SECTION(state_mutex_,
                                 entry.have_permission_for.erase(file_name));
                entry.service->SendMessage(
                    proto::mutex::ReplyMessage(timestamp_, file_name)
                        .ToMessage(),
                    [this, &entry](util::result<void, Error> result) {
                        OnSendMessage(entry, std::move(result));
                    });
            } else if (mine.timestamp > request.timestamp ||
                       (mine.timestamp == request.timestamp &&
                        components_.common.options.id > entry.connection.id)) {
                // Their request for the same file has higher priority over
                // mine.
                //
                // I should not even have their permission anyway.
                entry.service->SendMessage(
                    proto::mutex::ReplyMessage(timestamp_, file_name)
                        .ToMessage(),
                    [this, &entry](util::result<void, Error> result) {
                        OnSendMessage(entry, std::move(result));
                    });
            } else {
                // My request has higher priority, so I will not reply now.
                CRITICAL_SECTION(state_mutex_,
                                 delayed_requests_.emplace(DelayedRequest{
                                     entry, std::move(request)}));
            }
        } break;
    }
}

void DistributedMutualExclusionService::OnSendMessage(
    PeerNetworkEntry& entry, util::result<void, Error> result) {
    if (result.is_err()) {
        network_manager_.ReportError(entry.connection.out,
                                     [this](util::result<void, Error> result) {
                                         OnNetworkRecovery(std::move(result));
                                     });
        return;
    }

    // Nothing else to do after a message is sent. We await a reply.
}

void DistributedMutualExclusionService::OnNetworkRecovery(
    util::result<void, Error> result) {
    if (result.is_err()) {
        error_callback_(std::move(result).err());
    } else {
        // This never actually happens, because recovery is not implemented.
        // When recovery is implemented, iterate over all of the network
        // entries and see what messages need to be sent (such as a reply or
        // a request).
    }
}

void DistributedMutualExclusionService::RunWithMutualExclusion(
    const std::string& file_name, const mutex_operation_t& operation) {
    util::safe_debug::log("Requesting mutual exclusion for", file_name);

    CRITICAL_SECTION(state_mutex_, {
        if (my_request_.has_value() || state_ != State::kWaiting) {
            operation(Error::Create("Operation already in progress"));
            return;
        }

        my_request_.emplace(
            MutualExclusionRequest{file_name, operation, timestamp_});

        // Request mutual exclusion from all nodes.
        // This will send a request to every node we need permission from.
        RequestMutualExclusion();

        // We may already have permission from everyone, so immediately check
        // for mutual exclusion.
        CheckForMutualExclusion();
    });
}

void DistributedMutualExclusionService::RequestMutualExclusion() {
    state_ = State::kRequesting;
    auto& req = my_request_.value();

    // Request permission from every node I do not have permission from.
    for (auto& entry : network_) {
        if (entry.have_permission_for.find(req.file_name) ==
            entry.have_permission_for.end()) {
            util::safe_debug::log("Sending Request to peer",
                                  static_cast<int>(entry.connection.id));
            entry.service->SendMessage(
                proto::mutex::RequestMessage(req.timestamp, req.file_name)
                    .ToMessage(),
                [this, &entry](util::result<void, Error> result) {
                    OnSendMessage(entry, std::move(result));
                });
        } else {
            util::safe_debug::log("Already have permission from peer",
                                  static_cast<int>(entry.connection.id));
        }
    }
}

void DistributedMutualExclusionService::PerformCriticalSection() {
    util::safe_debug::log("Entering the critical section");
    auto& req = my_request_.value();
    req.operation([this](const release_callback_t& callback) {
        ReleaseMutualExclusion();
        callback(util::ok);
    });
}

void DistributedMutualExclusionService::ReleaseMutualExclusion() {
    util::safe_debug::log("Releasing mutual exclusion");

    // Schedule the callback for another thread.
    my_request_.reset();
    state_ = State::kWaiting;

    DeliverDelayedRequests();
}

void DistributedMutualExclusionService::DeliverDelayedRequests() {
    util::safe_debug::log("Delivering delayed requests");
    while (!delayed_requests_.empty()) {
        auto& next = delayed_requests_.front();
        OnReceiveRequest(next.entry, std::move(next.request));
        delayed_requests_.pop();
    }
}

void DistributedMutualExclusionService::CheckForMutualExclusion() {
    auto& req = my_request_.value();
    std::string& file_name = req.file_name;

    bool has_mutual_exclusion =
        std::all_of(network_.begin(), network_.end(),
                    [&file_name](PeerNetworkEntry& entry) {
                        return entry.have_permission_for.find(file_name) !=
                               entry.have_permission_for.end();
                    });
    if (has_mutual_exclusion) {
        state_ = State::kInCriticalSection;
        PerformCriticalSection();
    }
}

}  // namespace mutex
}  // namespace net