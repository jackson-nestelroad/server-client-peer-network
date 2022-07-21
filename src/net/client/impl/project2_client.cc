#include "project2_client.h"

#include <util/console.h>
#include <util/iterator.h>
#include <util/mutex.h>
#include <util/number.h>
#include <util/strings.h>

#include <random>

namespace net {
namespace client {
namespace impl {

Project2Client::Server::Server(Connection&& moved_connection,
                               Components& components)
    : connection(std::make_shared<Connection>(std::move(moved_connection))),
      message_service(connection->socket, components),
      operation_sent(false),
      performed_write(false) {}

Project2Client::Project2Client(Components& components)
    : Client(true, components, [this]() { StopStateMachine(); }),
      util::state_machine<Project2Client>(*this,
                                          states::ConnectToServers::instance()),
      current_server_(nullptr),
      current_file_name_(nullptr) {}

void Project2Client::Run() {
    // Start state machine.
    util::state_machine<Project2Client>::start(
        [this](util::result<void, util::error> result) {
            if (result.is_err()) {
                util::safe_error_log::log(result.err().what());
            }
            SignalStop();
        });
}

void Project2Client::StopStateMachine() {
    util::state_machine<Project2Client>::stop();
}

util::result<void, Error> Project2Client::ChangeServer() {
    auto it = util::iterator::random(servers_.begin(), servers_.end());
    if (it == servers_.end()) {
        return Error::Create("Failed to select random server from list");
    }

    current_server_ = &*it;
    return util::ok;
}

util::result<void, Error> Project2Client::ChangeFile() {
    auto it = util::iterator::random(file_names_.begin(), file_names_.end());
    if (it == file_names_.end()) {
        return Error::Create("Failed to select random file from list");
    }

    current_file_name_ = &*it;
    return util::ok;
}

namespace states {

IMPL_STATE_HANDLER(Project2Client, ConnectToServers) {
    util::safe_debug::log("Connecting to servers");
    auto servers_string = instance.components_.common.props.Get("servers");
    if (!servers_string.has_value()) {
        callback(
            util::error("Properties file does not contain a list of servers"));
        return;
    }

    if (servers_string.value().empty()) {
        callback(util::error("No servers found in properties file"));
        return;
    }

    auto servers = util::strings::split(servers_string.value(), ',');
    instance.num_servers_ = servers.size();

    for (const auto& server : servers) {
        util::safe_console::log("Connecting to", server);
        auto host_port = util::strings::split(server, ':');
        if (host_port.size() != 2) {
            callback(
                util::error("Malformed server location in properties file"));
            return;
        }
        auto full_hostname = host_port[0];
        auto port_result = util::num::string_to_num<port_t>(host_port[1]);
        if (port_result.is_err()) {
            callback(util::error("Malformed server port in properties file"));
            return;
        }
        port_t port = std::move(port_result).ok();
        instance.components_.connection_service.NewConnection(
            full_hostname, port,
            [&instance, server,
             callback](util::result<Connection, Error> result) {
                if (result.is_err()) {
                    callback(std::move(result).err());
                } else {
                    auto connection = std::move(result).ok();
                    util::safe_console::log("Connected to server", server);
                    bool done = false;
                    CRITICAL_SECTION(instance.mutex_, {
                        instance.servers_.emplace_back(
                            std::move(connection), instance.components_.common);
                        if (instance.servers_.size() == instance.num_servers_) {
                            done = true;
                        }
                    });

                    if (done) {
                        callback(util::ok);
                    }
                }
            });
    }
}

IMPL_NEXT_STATE(Project2Client, ConnectToServers, SendEnquiry);

IMPL_STATE_HANDLER(Project2Client, SendEnquiry) {
    auto res = instance.ChangeServer();
    if (res.is_err()) {
        callback(std::move(res).err());
        return;
    }

    util::safe_console::log("Fetching file names");

    instance.current_server_->message_service.WriteMessage(
        proto::EnquiryMessage{}.ToMessage(),
        [callback](util::result<void, Error> result) {
            callback(std::move(result).map_err(
                [](Error&& error) -> util::error { return error; }));
        });
}

IMPL_NEXT_STATE(Project2Client, SendEnquiry, ReceiveEnquiryResponse);

IMPL_STATE_HANDLER(Project2Client, ReceiveEnquiryResponse) {
    instance.current_server_->message_service.ReadMessage(
        [&instance, callback](util::result<proto::Message, Error> result) {
            if (result.is_err()) {
                callback(std::move(result).err());
                return;
            }

            auto msg = std::move(result).ok();
            switch (msg.opcode) {
                case proto::Opcode::kResponse: {
                    auto resp = std::move(msg).ToResponse().ok();
                    instance.file_names_ =
                        util::strings::split_trim(resp.message, ',');
                    if (instance.file_names_.empty()) {
                        callback(util::error(
                            "Server responded to enquiry with 0 file names"));
                    } else {
                        util::safe_debug::log("Received",
                                              instance.file_names_.size(),
                                              "file names");
                        callback(util::ok);
                    }
                } break;
                case proto::Opcode::kError: {
                    auto err = std::move(msg).ToError().ok();
                    util::safe_error_log::log("Error from server:",
                                              err.message);
                    instance.set_next_state(Stop::instance());
                    callback(util::ok);
                } break;
                default: {
                    callback(util::error(util::string::stream(
                        "Received message type ", static_cast<int>(msg.opcode),
                        " from server in enquiry response state, expected ",
                        static_cast<int>(proto::Opcode::kResponse))));
                } break;
            }
        });
}

IMPL_NEXT_STATE(Project2Client, ReceiveEnquiryResponse, Wait);

IMPL_STATE_HANDLER(Project2Client, Wait) {
    // Get a random number of milliseconds to wait.
    static std::random_device device;
    static std::mt19937 rng(device());
    std::uniform_int_distribution<std::size_t> ms_dis(500, 5000);
    std::size_t ms_to_sleep = ms_dis(rng);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms_to_sleep));

    // Randomly branch to the read or write state.
    std::uniform_int_distribution<> bool_dis(0, 1);
    bool should_write = bool_dis(rng);
    if (should_write) {
        instance.set_next_state(SendWrite::instance());
    } else {
        instance.set_next_state(SendRead::instance());
    }

    // Select a random server to send the next request to.
    auto res = instance.ChangeServer();
    if (res.is_err()) {
        callback(std::move(res).err());
        return;
    }

    // Select a random file to work with.
    res = instance.ChangeFile();
    if (res.is_err()) {
        callback(std::move(res).err());
        return;
    }

    callback(util::ok);
}

IMPL_STATE_HANDLER_SETS_NEXT_STATE(Project2Client, Wait);

IMPL_STATE_HANDLER(Project2Client, SendRead) {
    util::safe_debug::log("Beginning mutually exclusive read on",
                          *instance.current_file_name_);

    for (auto& server : instance.servers_) {
        server.operation_sent = false;
    }

    instance.components_.distributed_mutex_service.RunWithMutualExclusion(
        *instance.current_file_name_,
        [&instance, callback](
            util::result<typename mutex::DistributedMutualExclusionService::
                             mutex_operation_done_t,
                         Error>
                result) {
            if (result.is_err()) {
                callback(std::move(result).err());
                return;
            }

            instance.finished_critical_section_callback_ =
                std::move(result).ok();

            instance.current_server_->message_service.WriteMessage(
                proto::ReadMessage{*instance.current_file_name_}.ToMessage(),
                [&instance, callback](util::result<void, Error> result) {
                    callback(std::move(result).map_err(
                        [](Error&& error) -> util::error { return error; }));
                });
        });
}

IMPL_NEXT_STATE(Project2Client, SendRead, ReceiveReadResponse);

IMPL_STATE_HANDLER(Project2Client, ReceiveReadResponse) {
    instance.current_server_->message_service.ReadMessage(
        [&instance, callback](util::result<proto::Message, Error> result) {
            if (result.is_err()) {
                callback(std::move(result).err());
                return;
            }

            auto msg = std::move(result).ok();
            switch (msg.opcode) {
                case proto::Opcode::kResponse: {
                    auto resp = std::move(msg).ToResponse().ok();
                    util::safe_console::stream(
                        "Last line of ", *instance.current_file_name_, " is \"",
                        resp.message, "\"", util::manip::endl);
                    // Release mutual exclusion.
                    instance.finished_critical_section_callback_.value()(
                        [callback](util::result<void, Error> result) {
                            // Then, go to the next state.
                            callback(std::move(result).map_err(
                                [](Error&& error) -> util::error {
                                    return error;
                                }));
                        });
                } break;
                case proto::Opcode::kError: {
                    auto err = std::move(msg).ToError().ok();
                    util::safe_error_log::log("Error from server on read:",
                                              err.message);
                    instance.set_next_state(Stop::instance());
                    callback(util::ok);
                } break;
                default: {
                    callback(util::error(util::string::stream(
                        "Received message type ", static_cast<int>(msg.opcode),
                        " from server in read response state, expected ",
                        static_cast<int>(proto::Opcode::kOk))));
                } break;
            }
        });
}

IMPL_NEXT_STATE(Project2Client, ReceiveReadResponse, Wait);

IMPL_STATE_HANDLER(Project2Client, SendWrite) {
    util::safe_debug::log("Beginning mutually exclusive write on",
                          *instance.current_file_name_);

    for (auto& server : instance.servers_) {
        server.operation_sent = false;
    }

    instance.components_.distributed_mutex_service.RunWithMutualExclusion(
        *instance.current_file_name_,
        [&instance, callback](
            util::result<typename mutex::DistributedMutualExclusionService::
                             mutex_operation_done_t,
                         Error>
                result) {
            if (result.is_err()) {
                callback(std::move(result).err());
                return;
            }

            instance.finished_critical_section_callback_ =
                std::move(result).ok();

            std::string append = util::string::stream(
                '(', instance.components_.common.options.id, ", ",
                instance.components_.distributed_mutex_service.Timestamp(),
                ')');

            util::safe_console::stream("Appending \"", append, "\" to ",
                                       *instance.current_file_name_,
                                       util::manip::endl);

            // Send appended line to every server.
            for (auto& server : instance.servers_) {
                server.message_service.WriteMessage(
                    proto::WriteMessage{*instance.current_file_name_, append}
                        .ToMessage(),
                    [&instance, &server,
                     callback](util::result<void, Error> result) {
                        if (result.is_err()) {
                            // Failed to send one message to a server, so things
                            // are going to get desynchronized.
                            //
                            // We don't need to call `done` to release mutual
                            // exclusion, because working with this file will
                            // now be inconsistent.
                            //
                            // We close this state machine and client, so the
                            // whole peer network will go down.
                            callback(std::move(result).err());
                            return;
                        }

                        server.operation_sent = true;

                        // Only move on when every server has received my
                        // operation.
                        bool done = false;
                        CRITICAL_SECTION(instance.mutex_, {
                            done =
                                std::all_of(instance.servers_.begin(),
                                            instance.servers_.end(),
                                            [](Project2Client::Server& server) {
                                                return server.operation_sent;
                                            });
                        });
                        if (done) {
                            callback(util::ok);
                        }
                    });
            }
        });
}

IMPL_NEXT_STATE(Project2Client, SendWrite, ReceiveWriteResponse);

IMPL_STATE_HANDLER(Project2Client, ReceiveWriteResponse) {
    for (auto& server : instance.servers_) {
        server.performed_write = false;
    }

    for (auto& server : instance.servers_) {
        server.message_service.ReadMessage(
            [&instance, &server,
             callback](util::result<proto::Message, Error> result) {
                if (result.is_err()) {
                    // No way to verify if the server performed our operation.
                    callback(std::move(result).err());
                    return;
                }

                auto msg = std::move(result).ok();
                switch (msg.opcode) {
                    case proto::Opcode::kOk: {
                        server.performed_write = true;

                        // Only move on when every server has performed my
                        // operation.
                        bool done = false;
                        CRITICAL_SECTION(instance.mutex_, {
                            done =
                                std::all_of(instance.servers_.begin(),
                                            instance.servers_.end(),
                                            [](Project2Client::Server& server) {
                                                return server.performed_write;
                                            });
                        });
                        if (done) {
                            // First, release mutual exclusion.
                            instance.finished_critical_section_callback_
                                .value()([callback](
                                             util::result<void, Error> result) {
                                    // Then, go to the next state.
                                    callback(std::move(result).map_err(
                                        [](Error&& error) -> util::error {
                                            return error;
                                        }));
                                });
                        }
                    } break;
                    case proto::Opcode::kError: {
                        auto err = std::move(msg).ToError().ok();
                        util::safe_error_log::log("Error from server on write:",
                                                  err.message);
                        instance.set_next_state(Stop::instance());
                        callback(util::ok);
                    } break;
                    default: {
                        callback(util::error(util::string::stream(
                            "Received message type ",
                            static_cast<int>(msg.opcode),
                            " from server in write response state, expected ",
                            static_cast<int>(proto::Opcode::kOk))));
                    } break;
                }
            });
    }
}

IMPL_NEXT_STATE(Project2Client, ReceiveWriteResponse, Wait);

IMPL_STATE_HANDLER(Project2Client, Stop) {}
IMPL_STATE_HANDLER_SETS_NEXT_STATE(Project2Client, Stop);
IMPL_STOP_STATE_SHOULD_STOP(Stop);

}  // namespace states

}  // namespace impl
}  // namespace client
}  // namespace net