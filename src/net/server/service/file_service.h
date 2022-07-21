#ifndef NET_SERVER_SERVICE_FILE_SERVICE_
#define NET_SERVER_SERVICE_FILE_SERVICE_

#include <net/error.h>
#include <util/filesystem.h>
#include <util/result.h>

#include <string>

namespace net {
namespace server {
namespace service {

/**
 * @brief Service for working with files accoring to the specifications of
 * Project 2.
 *
 */
class FileService {
   public:
    /**
     * @brief Initializes the file system to manage the directory at the given
     * root path.
     *
     * @param root
     * @return util::result<void, Error>
     */
    util::result<void, Error> Initialize(const std::string& root);

    /**
     * @brief Returns the list of files in the root directory.
     *
     * @return util::result<std::vector<std::string>, Error>
     */
    util::result<std::vector<std::string>, Error> GetFiles();

    /**
     * @brief Reads the last line of the given file.
     *
     * @param name
     * @return util::result<std::string, Error>
     */
    util::result<std::string, Error> ReadLastLine(const std::string& name);

    /**
     * @brief Appends a new line to the given file.
     *
     * No synchronization is provided. Writes are assumed to be synchronized by
     * clients.
     *
     * @param name
     * @param line
     * @return util::result<void, Error>
     */
    util::result<void, Error> AppendLine(const std::string& name,
                                         const std::string& line);

   private:
    util::fs::path root_;
};

}  // namespace service
}  // namespace server
}  // namespace net

#endif  // NET_SERVER_SERVICE_FILE_SERVICE_