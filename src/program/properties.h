#ifndef PROGRAM_PROPERTIES_
#define PROGRAM_PROPERTIES_

#include <util/error.h>
#include <util/optional.h>
#include <util/result.h>

#include <string>
#include <unordered_map>

namespace program {

/**
 * @brief Parser and reader for a file of key-value pairs.
 *
 */
class Properties {
   public:
    /**
     * @brief Get the value associated to a property key.
     *
     * @param key Key value to read
     * @return std::optional<std::string>
     */
    util::optional<std::string> Get(const std::string& key) const;

    /**
     * @brief Parse the values of a flat `.properties` file.
     *
     * @param file_path Location of file
     * @return util::result<void, util::Error>
     */
    util::result<void, util::error> ParseFile(const std::string& file_path);

   private:
    std::unordered_map<std::string, std::string> name_to_value_;
};

}  // namespace program

#endif  // PROGRAM_PROPERTIES_