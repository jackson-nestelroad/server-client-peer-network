#include "server.h"

#include <util/console.h>

namespace net {
namespace server {

Server::Server(
    bool use_signals, net::Components& components,
    std::unique_ptr<BaseConnectionHandlerFactory>&& connection_handler_factory)
    : NetworkService(use_signals),
      components_(components, std::move(connection_handler_factory)),
      acceptor_(components_.common, [this](int sockfd) { OnAccept(sockfd); }) {}

util::result<void, Error> Server::SetUp() {
    std::uint16_t port = components_.common.options.port;
    acceptor_.SetPort(port);
    auto root_dir_prop = components_.common.props.Get("root_dir");
    if (!root_dir_prop.has_value()) {
        return Error::Create(
            "Missing \"root_dir\" property in properties file");
    }
    auto root_dir = std::move(root_dir_prop).value();
    RETURN_IF_ERROR(components_.file_service_.Initialize(root_dir));
    return util::ok;
}

util::result<void, Error> Server::OnStart() {
    util::safe_console::log("Starting server on port",
                            components_.common.options.port);
    return acceptor_.Start();
}

void Server::OnStop() { util::safe_console::log("Stopping server"); }

util::result<void, Error> Server::CleanUp() {
    util::safe_debug::log("Cleaning up server");
    acceptor_.Stop();
    components_.connection_manager.CloseAll();
    components_.common.thread_pool.Stop();
    return util::ok;
}

void Server::OnAccept(int sockfd) {
    Connection& new_connection =
        components_.connection_manager.NewConnection(sockfd);
    components_.connection_manager.Start(new_connection);
}

std::uint16_t Server::Port() const { return acceptor_.Port(); }

ServerComponents& Server::Components() { return components_; }

}  // namespace server
}  // namespace net