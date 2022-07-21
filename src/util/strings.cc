#include "strings.h"

namespace util {
namespace strings {

std::vector<std::string> split(const std::string& src, char delim) {
    std::vector<std::string> tokens;
    std::size_t prev = 0;
    std::size_t pos = 0;
    while ((pos = src.find(delim, prev)) != std::string::npos) {
        tokens.emplace_back(src.substr(prev, pos - prev));
        prev = pos + 1;
    }
    // Push everything else to the end of the string.
    tokens.emplace_back(src.substr(prev));
    return tokens;
}

std::vector<std::string> split(const std::string& src,
                               const std::string& delim) {
    std::vector<std::string> tokens;
    std::size_t delim_size = delim.length();
    std::size_t prev = 0;
    std::size_t pos = 0;
    while ((pos = src.find(delim, prev)) != std::string::npos) {
        tokens.emplace_back(src.substr(prev, pos - prev));
        prev = pos + delim_size;
    }
    // Push everything else to the end of the string.
    tokens.emplace_back(src.substr(prev));
    return tokens;
}

std::vector<std::string> split_trim(const std::string& src, char delim,
                                    const std::string& whitespace) {
    std::vector<std::string> tokens;
    std::size_t prev = 0;
    std::size_t pos = 0;
    while ((pos = src.find(delim, prev)) != std::string::npos) {
        std::size_t begin = src.find_first_not_of(whitespace, prev);
        // The whole entry is whitespace.
        // Unless delim contains whitespace, begin is guaranteed to be 0 <=
        // begin <= pos.
        if (begin >= pos) {
            prev = begin == std::string::npos ? begin : begin + 1;
        } else {
            // This would fail if pos == 0, because the whole string would be
            // searched. However, the previous if statement assures that begin <
            // pos so if pos == 0, begin == 0 because it is the first
            // non-whitespace character, so this statement is never reached.
            std::size_t end = src.find_last_not_of(whitespace, pos - 1);
            // prev <= begin <= end < pos.
            tokens.emplace_back(src.substr(begin, end - begin + 1));
            prev = pos + 1;
        }
    }
    // Push everything else to the end of the string, if it's not whitespace.
    std::size_t begin = src.find_first_not_of(whitespace, prev);
    if (begin != std::string::npos) {
        std::size_t end = src.find_last_not_of(whitespace);
        tokens.emplace_back(src.substr(begin, end - begin + 1));
    }
    return tokens;
}

bool ends_with(const std::string& str, const std::string& ending) {
    if (ending.size() > str.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), ending.rbegin());
}

}  // namespace strings
}  // namespace util