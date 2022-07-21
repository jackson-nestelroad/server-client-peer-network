#ifndef UTIL_VALIDATE_
#define UTIL_VALIDATE_

#include <functional>

namespace util {
namespace validate {

template <typename T>
using validate_func = std::function<bool(const T&)>;

// Base case for resolve_default_value.
// Returns the only value left, regardless of whether it passes the validation
// function or not.
template <typename T>
T resolve_default_value(const std::function<bool(const T&)>& validate,
                        const T& val) {
    return val;
}

// Recursive case for resolve_default_value.
// Returns the first value to pass the validation function.Will return the very
// last value regardless of whether it passes the validation function or not.
template <typename T, typename... Ts>
T resolve_default_value(const std::function<bool(const T&)>& validate,
                        const T& val, const T& val2, const Ts&... vals) {
    return validate(val) ? val
                         : resolve_default_value<T>(validate, val2, vals...);
}

}  // namespace validate
}  // namespace util

#endif  // UTIL_VALIDATE_