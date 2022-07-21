#include "options.h"

#include <util/console.h>

namespace program {

void Options::PrintHelp() const {
    if (!command_name_.empty()) {
        util::nolog::console::log(
            "CS 6378.0U1 Project 2, by Jackson Nestelroad");
        PrintUsage();
        util::nolog::console::log();
        parser_.PrintOptions();
        util::nolog::console::log();
    }
}

void Options::PrintUsage() const {
    if (!command_name_.empty()) {
        util::nolog::console::log("Usage:", command_name_, usage_);
    }
}

util::result<void, util::error> Options::AddOptions() {
    RETURN_IF_ERROR(parser_.AddOption<bool>(
        "help", 'h', &help, false, "Display help and options.", {}, {}));

    RETURN_IF_ERROR(parser_.AddOption<bool>("server", 's', &server, false,
                                            "Run the program server.", {}, {}));

    RETURN_IF_ERROR(parser_.AddOption<bool>("client", 'c', &client, false,
                                            "Run a program client.", {}, {}));

    RETURN_IF_ERROR(parser_.AddOptionRequired<int>(
        "id", 'i', &id, util::none, "ID for client or server.",
        [](int id) { return id > 0; }, {}));

    RETURN_IF_ERROR(parser_.AddOptionRequired<std::string>(
        "props_file", 'r', &props_file, util::none, "Properties file.", {},
        {}));

    RETURN_IF_ERROR(parser_.AddOption<std::string>(
        "temp_dir", 'w', &temp_directory, ".proj2_temp",
        "Temporary directory for temporary files.",
        [](const std::string& dir) { return !dir.empty(); }, {}));

    RETURN_IF_ERROR(
        parser_.AddOption<int>("timeout", 't', &timeout, 1 * 60 * 1000,
                               "Timeout for socket operations in milliseconds.",
                               [](int timeout) { return timeout != 0; }, {}));

    RETURN_IF_ERROR(parser_.AddOption<int>(
        "retry_timeout", 'e', &retry_timeout, 15 * 1000,
        "Timeout for retrying a connection to a server in milliseconds.",
        [](int timeout) { return timeout != 0; }, {}));

    RETURN_IF_ERROR(parser_.AddOptionRequired<int>(
        "port", 'p', &port, 0, "Port of the server.",
        [](const int& port) { return port > 0 && port < (1 << 16); }, {}));

    return util::ok;
}

util::result<int, util::error> Options::ParseCmdLine(int argc, char* argv[]) {
    if (!options_added_) {
        auto res = AddOptions();
        options_added_ = true;
        if (res.is_err()) {
            return {util::err, std::move(res).err()};
        }
    }

    command_name_ = argv[0];
    usage_ = "[OPTIONS]";

    return parser_.Parse(argc, argv);
}

}  // namespace program