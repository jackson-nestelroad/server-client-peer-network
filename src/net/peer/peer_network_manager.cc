#include "peer_network_manager.h"

#include <util/console.h>
#include <util/mutex.h>
#include <util/number.h>
#include <util/strings.h>

#include <sstream>

namespace net {
namespace peer {

PeerNetworkManager::PeerNetworkManager(Components& components)
    : NetworkService(false),
      components_(components),
      state_(State::kInitializing),
      connector_(
          components_,
          [this](
              util::result<service::SendHandshakeService::Out, Error> result) {
              OnClientConnection(std::move(result));
          }),
      acceptor_(
          components_,
          [this](util::result<service::ReceiveHandshakeService::Out, Error>
                     result) { OnServerConnection(std::move(result)); }) {}

void PeerNetworkManager::AwaitConnected(const connected_callback_t& callback) {
    if (state_ == State::kConnected) {
        // Network is already connected, so run immediately.
        callback(ConstructNetwork());
        return;
    }

    // Network is not ready, wait for state change.
    CRITICAL_SECTION(callback_mutex_,
                     connected_callbacks_.emplace_back(callback));
}

void PeerNetworkManager::ReportError(Connection& connection,
                                     const recovered_callback_t& callback) {
    CRITICAL_SECTION(callback_mutex_,
                     recovered_callbacks_.emplace_back(callback));

    // For now, we do not implement dynamic network recovery. The network
    // immediately breaks.
    //
    // In the future:
    //  Save a set of connections that have been reported broken, so the same
    //  connection that is reported in multiple locations is handled only once.
    //
    //  Find the connection in the `managed_connections_` map.
    //
    //  Issue the appropriate request to the connector or acceptor.
    //
    //  When network is once again complete, the recovered callbacks will be
    //  called.
    UpdateState(State::kBroken);
}

util::result<void, Error> PeerNetworkManager::SetUp() {
    my_port_ = static_cast<port_t>(components_.common.options.port);

    // Get list of servers.
    auto servers_list = components_.common.props.Get("clients");
    if (!servers_list.has_value()) {
        return Error::Create(
            "Property \"servers\" must be set in properties file");
    }

    auto localhost_result = Location::FromHostName("localhost", my_port_);
    RETURN_IF_ERROR(localhost_result);
    Location localhost = std::move(localhost_result).ok();

    auto my_ip_result = Location::MyIpAddress();
    RETURN_IF_ERROR(my_ip_result);
    Location my_ip = std::move(my_ip_result).ok();
    my_ip.port = my_port_;

    // For each server, save the actual host name in the host entry table.
    auto servers = util::strings::split(servers_list.value(), ',');
    for (const auto& server : servers) {
        auto name_port = util::strings::split(server, ':');
        if (name_port.size() == 0 || name_port.size() > 2) {
            return Error::Create(
                "Malformed peer server location in properties file");
        }
        const auto& name = name_port[0];
        std::uint16_t port = my_port_;
        if (name_port.size() == 2) {
            const auto& port_string = name_port[1];

            auto port_result = util::num::string_to_num<port_t>(port_string);
            if (port_result.is_err()) {
                return Error::Create("Invalid port for peer server");
            }
            port = port_result.ok();
        }

        auto target_location_result = Location::FromHostName(name, port);
        RETURN_IF_ERROR(target_location_result);
        auto target_location = std::move(target_location_result).ok();

        if (localhost == target_location || my_ip == target_location) {
            // Don't connect to yourself.
            continue;
        }

        peer_locations_.emplace_back(std::move(target_location));
    }

    return util::ok;
}

util::result<void, Error> PeerNetworkManager::OnStart() {
    util::safe_console::log("Starting peer network");
    RETURN_IF_ERROR(acceptor_.Start());
    RETURN_IF_ERROR(connector_.Start());
    for (auto& location : peer_locations_) {
        connector_.Connect(location);
        acceptor_.AwaitConnectionFrom(location);
    }
    return util::ok;
}

void PeerNetworkManager::OnStop() {
    util::safe_console::log("Stopping peer network");
    // If the state has not already been set to closed internally, then the
    // callbacks should be run, which this state transition guarantees.
    UpdateState(State::kClosed);
}

util::result<void, Error> PeerNetworkManager::CleanUp() {
    connector_.Stop();
    acceptor_.Stop();
    for (auto& conn : managed_connections_) {
        if (conn.second.in) {
            conn.second.in->socket.Close();
        }
        if (conn.second.out) {
            conn.second.out->socket.Close();
        }
    }
    return util::ok;
}

PeerConnection& PeerNetworkManager::GetConnectionMapEntry(proto::node_id_t id) {
    auto it = managed_connections_.find(id);
    if (it == managed_connections_.end()) {
        auto res = managed_connections_.emplace(
            id, PeerConnection{Location(0, 0), id, nullptr, nullptr});
        return res.first->second;
    }
    return it->second;
}

void PeerNetworkManager::UpdateState(State new_state) {
    if (new_state == state_ || state_ == State::kClosed) {
        return;
    }

    State old_state = state_;
    state_ = new_state;

    switch (state_) {
        case State::kConnected: {
            switch (old_state) {
                case State::kRecovering: {
                    // We were in the recovering state, so run all recovered
                    // callbacks.
                    SendSuccessToCallbacks(Callbacks::kRecovering);
                }  // Intentional follow through.
                case State::kInitializing: {
                    // We were in the initializing OR recovering state, so run
                    // all connected callbacks.
                    //
                    // Some outside entity may not have known the network
                    // manager was recovering at all and they only waited for
                    // the network to be connected. In this case, they will have
                    // added a connected callback while the manager was in
                    // recovery mode. The follow through above handles this
                    // case.
                    SendSuccessToCallbacks(Callbacks::kConnected);
                } break;
            }
            break;
        }
        case State::kBroken: {
            // Network is broken, so we need to tell anyone who is waiting
            // for us to call their callback.
            SendErrorToCallbacks(
                stopping_error_.value_or(Error::Create(
                    "Peer network disconnected and cannot be recovered")),
                Callbacks::kAll);
        } break;
        case State::kClosed: {
            SendErrorToCallbacks(
                stopping_error_.value_or(Error::Create("Peer network stopped")),
                Callbacks::kAll);
            SignalStop();
        } break;
    }
}

void PeerNetworkManager::SignalStopWithError(net::Error&& error) {
    stopping_error_ = std::move(error);
    UpdateState(State::kClosed);
}

void PeerNetworkManager::SendSuccessToCallbacks(callbacks_t callbacks) {
    if (callbacks & Callbacks::kConnected) {
        CRITICAL_SECTION(callback_mutex_, {
            for (const auto& callback : connected_callbacks_) {
                components_.common.thread_pool.Schedule(
                    [this, callback]() { callback(ConstructNetwork()); });
            }
            connected_callbacks_.clear();
        });
    }

    if (callbacks & Callbacks::kRecovering) {
        CRITICAL_SECTION(callback_mutex_, {
            for (const auto& callback : recovered_callbacks_) {
                components_.common.thread_pool.Schedule(
                    [callback]() { callback(util::ok); });
            }
            recovered_callbacks_.clear();
        });
    }
}

void PeerNetworkManager::SendErrorToCallbacks(Error error,
                                              callbacks_t callbacks) {
    if (callbacks & Callbacks::kConnected) {
        CRITICAL_SECTION(callback_mutex_, {
            for (const auto& callback : connected_callbacks_) {
                components_.common.thread_pool.Schedule(
                    [callback, error]() { callback(std::move(error)); });
            }
            connected_callbacks_.clear();
        });
    }

    if (callbacks & Callbacks::kRecovering) {
        CRITICAL_SECTION(callback_mutex_, {
            for (const auto& callback : recovered_callbacks_) {
                components_.common.thread_pool.Schedule(
                    [callback, error]() { callback(std::move(error)); });
            }
            recovered_callbacks_.clear();
        });
    }
}

void PeerNetworkManager::OnClientConnection(
    util::result<service::SendHandshakeService::Out, Error> result) {
    if (result.is_err()) {
        // If we failed to initiate a connection to a peer server, it must be
        // down.
        //
        // Currently, we just end the program. However, it may be possible to
        // come up with error recovery schemes.
        SignalStopWithError(Error::Create(util::string::stream(
            "Failed to connect to peer server: ", result.err())));
        return;
    }

    auto out = std::move(result).ok();
    util::safe_debug::log("Verified client connection to", out.target);
    CRITICAL_SECTION(connections_mutex_, {
        auto& entry = GetConnectionMapEntry(out.server_id);
        entry.location = out.target;
        entry.out = std::make_shared<Connection>(std::move(out).socket);

        CheckIfConnected();
    });
}

void PeerNetworkManager::OnServerConnection(
    util::result<service::ReceiveHandshakeService::Out, Error> result) {
    if (result.is_err()) {
        // Currently, our callback for server connections only yields an error
        // if the socket failed to properly handshake with this peer server.
        //
        // It is unlikely that client will attempt to reconnect and get it right
        // the second time, so just end the program for now.
        SignalStopWithError(Error::Create(util::string::stream(
            "Failed to get a connection from peer server:", result.err())));
        return;
    }

    auto out = std::move(result).ok();
    util::safe_debug::log("Verified server connection from client",
                          static_cast<int>(out.client_id));
    CRITICAL_SECTION(connections_mutex_, {
        auto& entry = GetConnectionMapEntry(out.client_id);
        entry.in = std::make_shared<Connection>(std::move(out).socket);

        CheckIfConnected();
    });
}

bool PeerNetworkManager::IsConnected() const {
    return state_ == State::kConnected;
}

bool PeerNetworkManager::IsConnectedImpl() const {
    if (IsConnected()) {
        // Cached connected state.
        return true;
    }

    // Every location should have an associated `PeerConnection` object with
    // two active connections.
    std::unordered_set<Location> unconnected_locations_(peer_locations_.begin(),
                                                        peer_locations_.end());

    for (const auto& pair : managed_connections_) {
        auto& connection = pair.second;
        unconnected_locations_.erase(connection.location);
        if (!connection.in || !connection.out) {
            return false;
        }
    }

    // Saw all peer locations in the managed connections map.
    return unconnected_locations_.empty();
}

void PeerNetworkManager::CheckIfConnected() {
    if (IsConnectedImpl()) {
        UpdateState(State::kConnected);
    }
}

PeerNetworkManager::PeerNetworkList PeerNetworkManager::ConstructNetwork() {
    // This method assumes the network is ready to go, which means
    // `IsConnected()` returns true.
    PeerNetworkList network;
    for (const auto& pair : managed_connections_) {
        auto& connection = pair.second;
        network.push_back(PeerConnectionReference{connection.id, *connection.in,
                                                  *connection.out});
    }
    return network;
}

}  // namespace peer
}  // namespace net