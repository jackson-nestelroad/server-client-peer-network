#include "temp_file_service.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <util/filesystem.h>
#include <util/result.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace net {
namespace shared {

TempFileService::TempFileService(const std::string& dir) : directory_(dir) {
    EXIT_IF_ERROR(CreateDirectoryIfNotExists(dir));
}

TempFileService::~TempFileService() { Clean(); }

util::result<void, Error> TempFileService::Delete(const std::string& path) {
    auto it = owned_files_.find(path);
    if (it != owned_files_.end()) {
        RETURN_IF_ERROR(util::fs::delete_file(path).map_err(
            [](util::error&& error) { return Error::Create(error.what()); }));
        owned_files_.erase(it);
    }

    it = owned_dirs_.find(path);
    if (it != owned_dirs_.end()) {
        RETURN_IF_ERROR(util::fs::delete_directory(path).map_err(
            [](util::error&& error) { return Error::Create(error.what()); }));
        owned_dirs_.erase(it);
    }

    return Error::Create("File service cannot delete a path it does not own");
}

util::result<void, Error> TempFileService::Clean() {
    for (const auto& file : owned_files_) {
        util::fs::delete_file(file);
    }
    owned_files_.clear();

    for (const auto& dir : owned_dirs_) {
        util::fs::delete_file(dir);
    }
    owned_dirs_.clear();
    return util::ok;
}

util::result<std::string, Error> TempFileService::CreateFile(
    const std::string& path) {
    auto created_path = util::fs::path(directory_) / path;
    RETURN_IF_ERROR(util::fs::create_file(created_path.string())
                        .map_err([](util::error&& error) {
                            return Error::Create(error.what());
                        }));
    owned_files_.emplace(path);
    return created_path.string();
}

util::result<void, Error> TempFileService::CreateDirectoryIfNotExists(
    const std::string& path) {
    if (!util::fs::exists(path)) {
        RETURN_IF_ERROR(util::fs::create_directory(path).map_err(
            [](util::error&& error) { return Error::Create(error.what()); }));
    }
    return util::ok;
}

}  // namespace shared
}  // namespace net