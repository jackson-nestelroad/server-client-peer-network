#ifndef UTIL_FILESYSTEM_
#define UTIL_FILESYSTEM_

#include <util/error.h>
#include <util/path.h>
#include <util/result.h>

#include <vector>

namespace util {
namespace fs {

/**
 * @brief Creates a directory.
 *
 * @param location
 * @return result<void, error>
 */
result<void, error> create_directory(const std::string& location);

/**
 * @brief Create a directory if it does not exist.
 *
 * Supports recursive directories.
 *
 * @param loc
 * @return result<void, error>
 */
result<void, error> create_directory_if_not_exists(const std::string& loc,
                                                   bool recursive = false);

/**
 * @brief Creates a file.
 *
 * @param path
 * @return result<void, error>
 */
result<void, error> create_file(const std::string& path);

/**
 * @brief Creates a file if it does not exist.
 *
 * @param path
 * @return result<void, error>
 */
result<void, error> create_file_if_not_exists(const std::string& path);

/**
 * @brief Deletes a directory.
 *
 * @param path
 * @return util::result<void, error>
 */
util::result<void, error> delete_directory(const std::string& path);

/**
 * @brief Deletes a file.
 *
 * @param path
 * @return util::result<void, error>
 */
util::result<void, error> delete_file(const std::string& path);

/**
 * @brief Checks if a file or directory exists at the given path.
 *
 * @param path
 * @return true
 * @return false
 */
bool exists(const std::string& path);

/**
 * @brief Get the names of files in the given directory.
 *
 * @param path
 * @return std::vector<std::string>
 */
result<std::vector<std::string>, error> get_files_in_directory(
    const std::string& path);

}  // namespace fs
}  // namespace util

#endif  // UTIL_FILESYSTEM_