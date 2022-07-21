#include "client.h"

#include <util/console.h>

#include <csignal>
#include <cstring>
#include <functional>

namespace net {
namespace client {

Client::Client(bool use_signals, Components& components,
               util::optional<stop_callback_t> on_stop)
    : NetworkService(use_signals),
      components_(
          components,
          [this](util::result<void, Error> result) {
              OnPeerNetworkReady(std::move(result));
          },
          [this](Error error) { OnPeerNetworkError(std::move(error)); }),
      on_stop_(on_stop) {}

util::result<void, Error> Client::SetUp() { return util::ok; }

util::result<void, Error> Client::OnStart() {
    util::safe_console::log("Starting distributed mutual exclusion service");
    return components_.distributed_mutex_service.Start();
}

void Client::OnStop() { util::safe_console::log("Stopping client"); }

util::result<void, Error> Client::CleanUp() {
    util::safe_debug::log("Cleaning up client");
    if (on_stop_.has_value()) {
        on_stop_.value()();
    }
    components_.connection_service.CancelPendingConnections();
    components_.distributed_mutex_service.Stop();
    components_.common.thread_pool.Stop();
    return util::ok;
}

void Client::OnPeerNetworkReady(util::result<void, Error> result) {
    if (result.is_err()) {
        util::safe_error_log::log("Failed to set up client peer network:",
                                  result.err());
        SignalStop();
    } else {
        util::safe_console::log(
            "Client peer network is up, starting client program");

        Run();
    }
}
void Client::OnPeerNetworkError(Error error) {
    util::safe_error_log::log("Error in peer network:", error);
    SignalStop();
}

}  // namespace client
}  // namespace net