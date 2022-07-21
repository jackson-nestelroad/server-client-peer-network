#ifndef NET_SHARED_TEMP_FILE_SERVICE_
#define NET_SHARED_TEMP_FILE_SERVICE_

#include <net/error.h>
#include <util/result.h>

#include <string>
#include <unordered_set>

namespace net {
namespace shared {

/**
 * @brief Service for managing temporary files and directories.
 *
 */
class TempFileService {
   public:
    TempFileService(const std::string& dir);
    ~TempFileService();

    /**
     * @brief Creates and manages a file.
     *
     * @param path
     * @return util::result<std::string, Error>
     */
    util::result<std::string, Error> CreateFile(const std::string& path);

    /**
     * @brief Creates a directory and manages it if it does not exist.
     *
     * If the given directory does exist, it can still be used, but it will not
     * be deleted by this service.
     *
     * @param path
     * @return util::result<void, Error>
     */
    util::result<void, Error> CreateDirectoryIfNotExists(
        const std::string& path);

    /**
     * @brief Deletes everything owned by the service.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> Clean();

    /**
     * @brief Deletes the given file.
     *
     * @param path
     * @return util::result<void, Error>
     */
    util::result<void, Error> Delete(const std::string& path);

   private:
    std::string directory_;
    std::unordered_set<std::string> owned_files_;
    std::unordered_set<std::string> owned_dirs_;
};

}  // namespace shared
}  // namespace net

#endif  // NET_SHARED_TEMP_FILE_SERVICE_