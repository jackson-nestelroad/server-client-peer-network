#include <net/client/impl/project2_client.h>
#include <net/server/impl/client_server_connection_handler_factory.h>
#include <net/server/server.h>
#include <program/options.h>
#include <program/properties.h>
#include <util/buffer.h>
#include <util/console.h>

#include <iostream>
#include <string>

int RunProgram(net::Components& components) {
    if (components.options.server) {
        net::server::Server server(
            true, components,
            net::server::impl::ClientServerConnectionHandlerFactory::
                CreateFactory());
        auto res = server.Start();
        if (res.is_err()) {
            util::safe_error_log::log(res);
            server.Stop();
            return 1;
        }

        server.AwaitStop();
    } else if (components.options.client) {
        net::client::impl::Project2Client client(components);
        auto res = client.Start();
        if (res.is_err()) {
            util::safe_error_log::log(res);
            client.Stop();
            return 1;
        }

        client.AwaitStop();
    }

    return 0;
}

int main(int argc, char* argv[]) {
    program::Options options;
    auto res = options.ParseCmdLine(argc, argv);
    if (res.is_err()) {
        util::nolog::error_log::log(res.err().what());
        options.PrintUsage();
        util::nolog::error_log::stream(
            "Use `", argv[0], " --help` to view options", util::manip::endl);
        return 1;
    }

    if (options.help) {
        options.PrintHelp();
        return 0;
    }

    if (!options.server && !options.client) {
        util::nolog::error_log::log(
            "ERROR: One of --server or --client must be turned "
            "on");
        return 1;
    }

    net::Components components(options);

    auto result = components.props.ParseFile(components.options.props_file);
    if (result.is_err()) {
        util::nolog::error_log::log(result.err().what());
        return 1;
    }

    components.thread_pool.Start();

    int exit_code = RunProgram(components);

    components.thread_pool.Stop();

    return exit_code;
}
