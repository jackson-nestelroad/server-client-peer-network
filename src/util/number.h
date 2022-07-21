#ifndef UTIL_NUMBER_
#define UTIL_NUMBER_

#include <util/error.h>
#include <util/result.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>

namespace util {
namespace num {

/**
 * @brief Converts a string to the given integer type.
 *
 * @tparam T
 * @param str
 * @return std::enable_if<std::is_arithmetic<T>::value, result<T, error>>::type
 */
template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value, result<T, error>>::type
string_to_num(const std::string& str) {
    std::stringstream strm;
    strm << str;
    T out;
    if (!(strm >> out)) {
        return error("Failed to convert string to number");
    }
    return out;
}

}  // namespace num
}  // namespace util

#endif  // UTIL_NUMBER_