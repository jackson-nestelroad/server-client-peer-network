#ifndef UTIL_STRING_
#define UTIL_STRING_

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

// Helper functions for operations on std::string

namespace util {
namespace strings {

/**
 * @brief Splits a string along a delimiter.
 *
 * @param src String
 * @param delim Delimiter
 * @return std::vector<std::string>
 */
std::vector<std::string> split(const std::string& src, char delim);

/**
 * @brief Splits a string along a delimiter.
 *
 * @param src String
 * @param delim Delimiter
 * @return std::vector<std::string>
 */
std::vector<std::string> split(const std::string& src,
                               const std::string& delim);

/**
 * @brief Splits a string along a delimiter, removing any linear white space
 from each entry.
 *
 * @param src String
 * @param delim Delimiter
 * @param whitespace String of whitespace characters
 * @return std::vector<std::string>
 */
std::vector<std::string> split_trim(const std::string& src, char delim,
                                    const std::string& whitespace = " \t");

/**
 * @brief Joins a range into a single string with a delimiter.
 *
 * @tparam Range
 * @tparam Range::value_type
 * @param range Range that can be iterated through
 * @param delim String delimiter to separate values by
 * @return std::string
 */
template <typename Range, typename Value = typename Range::value_type>
std::string join(const Range& range, const std::string& delim) {
    std::ostringstream str;
    auto begin = std::begin(range);
    auto end = std::end(range);

    if (begin != end) {
        std::copy(begin, std::prev(end),
                  std::ostream_iterator<Value>(str, delim.data()));
        begin = prev(end);
    }

    if (begin != end) {
        str << *begin;
    }

    return str.str();
}

bool ends_with(const std::string& str, const std::string& ending);

}  // namespace strings
}  // namespace util

#endif  // UTIL_STRING_