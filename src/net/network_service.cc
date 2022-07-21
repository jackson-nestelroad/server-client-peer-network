#include "network_service.h"

#include <csignal>
#include <functional>

namespace net {

namespace {

std::function<void(int)> shutdown_handler;
void SignalHandler(int signal) { shutdown_handler(signal); }

}  // namespace

NetworkService::NetworkService(bool use_signals)
    : use_signals_(use_signals), running_(false), needs_cleanup_(false) {}

util::result<void, Error> NetworkService::Start() {
    RETURN_IF_ERROR(SetUp());

    if (use_signals_) {
        // Shut down the server when the process stops.
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGKILL, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        shutdown_handler = [this](int signum) { SignalStop(); };
    }

    running_ = true;
    needs_cleanup_ = true;
    return OnStart();
}

util::result<void, Error> NetworkService::Stop() {
    SignalStop();
    return AttemptCleanUp();
}

void NetworkService::SignalStop() {
    running_ = false;
    OnStop();
    blocker_.unblock();
}

void NetworkService::OnStop() {}

util::result<void, Error> NetworkService::AwaitStop() {
    if (running_) {
        blocker_.block();
    }
    return AttemptCleanUp();
}

util::result<void, Error> NetworkService::AttemptCleanUp() {
    if (needs_cleanup_.exchange(false)) {
        return CleanUp();
    }
    return util::ok;
}

void NetworkService::SwitchSignalHandling(bool use_signals) {
    if (use_signals_ && !use_signals) {
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGKILL, SIG_DFL);
    } else if (!use_signals && use_signals) {
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGKILL, SignalHandler);
        shutdown_handler = [this](int signum) { SignalStop(); };
    }
    use_signals_ = use_signals;
}

bool NetworkService::Running() const { return needs_cleanup_.load(); }

}  // namespace net