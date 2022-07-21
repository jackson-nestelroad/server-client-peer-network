#include "file_service.h"

#include <algorithm>
#include <fstream>

namespace net {
namespace server {
namespace service {

util::result<void, Error> FileService::Initialize(const std::string& root) {
    if (!util::fs::exists(root)) {
        return Error::Create("Managed directory root does not exist");
    }

    auto files = util::fs::get_files_in_directory(root);
    if (files.is_err() || files.ok().size() == 0) {
        return Error::Create("Managed directory root contains no files");
    }

    root_ = util::fs::path(root).lexically_normal();

    return util::ok;
}

util::result<std::vector<std::string>, Error> FileService::GetFiles() {
    return util::fs::get_files_in_directory(root_.string())
        .map_err(
            [](util::error&& error) { return Error::Create(error.what()); })
        .map([](std::vector<std::string>&& files) {
            // Remove hidden files.
            files.erase(std::remove_if(files.begin(), files.end(),
                                       [](const std::string& name) {
                                           return name[0] == '.';
                                       }),
                        files.end());
            return files;
        });
}

util::result<std::string, Error> FileService::ReadLastLine(
    const std::string& name) {
    auto full_path = root_ / name;
    full_path = full_path.lexically_normal();
    if (root_.lexically_relative(full_path) != ".." || name[0] == '.') {
        return Error::Create("Invalid file access");
    }
    std::ifstream file(full_path.string());
    if (!file) {
        return Error::Create("Failed to open file " + name);
    }

    // Move to the last character.
    if (!file.seekg(-1, std::ios_base::end)) {
        // No character before EOF, so the file is empty.
        return "";
    }

    if (file.peek() == '\n' && !file.seekg(-1, std::ios_base::cur)) {
        // Found newline as the last character, and there is no characters
        // before it, so last line is empty.
        return "";
    }

    // At this point, we are pointing to a character before the trailing newline
    // that is not a newline.
    //
    // We now look for the next newline going backwards.
    int ch = 0;
    while (ch != '\n') {
        if (!file.seekg(-1, std::ios::cur)) {
            break;
        }
        ch = file.peek();
    }

    // We are at a newline, eat it.
    if (ch == '\n') {
        file.get();
    }

    std::string last_line;
    std::getline(file, last_line);
    return last_line;
}

util::result<void, Error> FileService::AppendLine(const std::string& name,
                                                  const std::string& line) {
    auto full_path = root_ / name;
    full_path = full_path.lexically_normal();
    if (root_.lexically_relative(full_path) != "..") {
        return Error::Create("Invalid file access");
    }
    std::ofstream file(full_path.string(), std::ios::app);
    if (!file) {
        return Error::Create("Failed to open file " + name);
    }

    file << line << '\n';
    return util::ok;
}

}  // namespace service
}  // namespace server
}  // namespace net