#include "properties.h"

#include <fstream>

namespace program {

util::optional<std::string> Properties::Get(const std::string& key) const {
    auto it = name_to_value_.find(key.data());
    if (it == name_to_value_.end()) {
        return util::none;
    }
    return it->second;
}

util::result<void, util::error> Properties::ParseFile(
    const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        return util::error("Could not open properties file \"" + file_path +
                           "\" for reading.");
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.at(0) != '#') {
            auto split_loc = line.find('=');
            if (split_loc == 0) {
                return util::error("Malformed property \"" + line + "\".");
            }
            if (split_loc == std::string::npos) {
                return util::error("Property \"" + line +
                                   "\" does not have a value.");
            }
            name_to_value_.emplace(line.substr(0, split_loc),
                                   line.substr(split_loc + 1));
        }
    }

    return util::ok;
}

}  // namespace program