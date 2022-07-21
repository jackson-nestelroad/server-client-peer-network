#include "filesystem.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>

namespace util {
namespace fs {

result<void, error> create_directory(const std::string& path) {
    if (::mkdir(path.c_str(), 0700) < 0) {
        return error("Failed to create directory at " + path);
    }
    return util::ok;
}

result<void, error> create_directory_if_not_exists(const std::string& path,
                                                   bool recursive) {
    if (!recursive) {
        struct stat statbuf;
        if (exists(path)) {
            ::mkdir(path.c_str(), 0700);
        }
    } else {
        // Path object for the path we are creating.
        fs::path path_obj(path);
        path_obj = path_obj.lexically_normal();
        // Path object that will represent the first path that exists.
        fs::path path_exists(path_obj);
        // Iterator pointing to first name on the path that must be created.
        auto it = path_obj.end();
        for (it; it != path_obj.begin(); ++it) {
            if (exists(path_exists.string())) {
                break;
            }
            // Does not exist, so move backwards.
            path_exists = path_exists.parent_path();
            --it;
        }

        // Move back to the end, creating each directory as we go.
        for (it; it != path_obj.end(); ++it) {
            path_exists /= *it;
            RETURN_IF_ERROR(create_directory(path_exists.string()));
        }
    }
    return util::ok;
}

result<void, error> create_file(const std::string& path) {
    std::ofstream file(path);
    if (!file) {
        return error("Failed to create file at " + path);
    }
    return util::ok;
}

result<void, error> create_file_if_not_exists(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return create_file(path);
    }
    return util::ok;
}

util::result<void, error> delete_directory(const std::string& path) {
    DIR* dir = ::opendir(path.c_str());
    std::size_t path_length = path.length();
    util::result<void, error> res = util::ok;

    if (!dir) {
        return error("Failed to open directory for deletion");
    }

    dirent* entry;
    while (res.is_ok() && (entry = ::readdir(dir))) {
        // Ignore these entries.
        if (::strcmp(entry->d_name, ".") == 0 ||
            ::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string full_path = path + "/" + entry->d_name;
        struct stat statbuf;

        if (::stat(full_path.data(), &statbuf) == 0) {
            if (statbuf.st_mode & S_IFDIR) {
                res = delete_directory(full_path);
            } else {
                res = delete_file(full_path);
            }
        }
    }
    // Don't return early so that this cleanup step happens.
    ::closedir(dir);

    RETURN_IF_ERROR(res);

    if (::rmdir(path.c_str()) != 0) {
        return error("Failed to delete directory " + path);
    }

    return util::ok;
}

util::result<void, error> delete_file(const std::string& path) {
    int res = ::unlink(path.c_str());
    if (res != 0) {
        return error("Failed to unlink file " + path);
    }
    return util::ok;
}

bool exists(const std::string& path) {
    struct stat statbuf;
    return ::stat(path.c_str(), &statbuf) == 0;
}

result<std::vector<std::string>, error> get_files_in_directory(
    const std::string& path) {
    DIR* dir = ::opendir(path.c_str());
    std::size_t path_length = path.length();
    util::result<void, error> res = util::ok;

    if (!dir) {
        return error("Failed to open directory for deletion");
    }

    std::vector<std::string> files;
    dirent* entry;
    while (res.is_ok() && (entry = ::readdir(dir))) {
        // Ignore these entries.
        if (::strcmp(entry->d_name, ".") == 0 ||
            ::strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        std::string full_path = path + "/" + entry->d_name;
        struct stat statbuf;

        if (::stat(full_path.data(), &statbuf) == 0) {
            if (statbuf.st_mode & S_IFREG) {
                files.emplace_back(entry->d_name);
            }
        }
    }
    // Don't return early so that this cleanup step happens.
    ::closedir(dir);

    RETURN_IF_ERROR(res);
    return files;
}

}  // namespace fs
}  // namespace util