#include "peer_connector.h"

#include <util/console.h>
#include <util/mutex.h>

namespace net {
namespace peer {

PeerConnector::PeerConnector(PeerComponents& components,
                             const connect_callback_t& callback)
    : NetworkService(false), components_(components), callback_(callback) {}

void PeerConnector::Connect(const Location& target) {
    util::safe_console::log("Attempting to connect to", target);

    auto emplace = [this, &target]() {
        CRITICAL_SECTION(mutex_, {
            return pending_connections_.emplace(
                target, std::make_shared<service::SendHandshakeService>(
                            components_, target));
        });
    };
    auto emplace_result = emplace();

    if (!emplace_result.second) {
        callback_(Error::Create("Duplicate peer server target found"));
    } else {
        auto service = emplace_result.first->second;
        components_.common.thread_pool.Schedule([this, &target, service]() {
            service->start([this, &target,
                            service](util::result<void, util::error> result) {
                CRITICAL_SECTION(mutex_, {
                    pending_connections_.erase(target);
                    if (result.is_err()) {
                        callback_(Error::Create(result.err().what()));
                    } else {
                        callback_(std::move(*service).Export());
                    }
                });
            });
        });
    }
}

util::result<void, Error> PeerConnector::SetUp() { return util::ok; }

util::result<void, Error> PeerConnector::OnStart() { return util::ok; }

util::result<void, Error> PeerConnector::CleanUp() {
    CRITICAL_SECTION(mutex_, {
        for (auto& pair : pending_connections_) {
            auto service = pair.second;
            service->Cancel();
            // Schedule stopping in another thread, because the stop callback
            // mutates this map we are iterating through.
            components_.common.thread_pool.Schedule(
                [service]() { service->stop(); });
        }
        return util::ok;
    });
}

}  // namespace peer
}  // namespace net