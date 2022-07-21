#ifndef PROGRAM_OPTIONS_
#define PROGRAM_OPTIONS_

#include <program/options_parser.h>

#include <string>
#include <vector>

namespace program {

struct ServerLocation {
    std::string url;
    std::uint16_t port;
};

/**
 * @brief Options for the program with built-in parsing.
 *
 */
class Options {
   public:
    bool help;

    int id;
    std::string props_file;
    std::string temp_directory;
    int timeout;
    int retry_timeout;

    bool server;
    int port;

    bool client;

    std::vector<ServerLocation> servers;

    /**
     * @brief Parses options into this object from the command-line.
     *
     * @param argc Argument count for `argv`
     * @param argv Argument vector
     *
     * @return util::result<int, Error> Index of last option read, or
     * error
     */
    util::result<int, util::error> ParseCmdLine(int argc, char* argv[]);

    /**
     * @brief Prints help for this command, including details on all options.
     *
     * Does nothing if `ParseCmdLine` has not been called yet.
     *
     */
    void PrintHelp() const;

    /**
     * @brief Prints the usage for this command.
     *
     * Does nothing if `ParseCmdLine` has not been called yet.
     */
    void PrintUsage() const;

   private:
    /**
     * @brief Adds all options to the parser at once.
     *
     * @return util::result<void, Error>
     */
    util::result<void, util::error> AddOptions();

    OptionsParser parser_;
    bool options_added_ = false;
    std::string command_name_;
    std::string usage_;
};

}  // namespace program

#endif  // PROGRAM_OPTIONS_