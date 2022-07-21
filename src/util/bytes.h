#ifndef UTIL_BYTES_
#define UTIL_BYTES_

#include <util/buffer.h>

#include <cstdint>
#include <type_traits>
#include <vector>

namespace util {
namespace bytes {

using byte_vector = std::vector<std::uint8_t>;

namespace bytes_detail {

template <int N>
struct is_valid_byte_string_size {
    static constexpr bool value = (N <= 8) && (N > 0);
};

template <int N, typename B>
struct is_valid_byte_string {
    static constexpr bool value =
        is_valid_byte_string_size<N>::value && std::is_integral<B>::value;
};

template <int N, typename = void>
struct minimum_int_type;

template <int N>
struct minimum_int_type<N, typename std::enable_if<(N > 0) && (N <= 1)>::type> {
    using type = std::uint8_t;
};

template <int N>
struct minimum_int_type<N, typename std::enable_if<(N > 1) && (N <= 2)>::type> {
    using type = std::uint16_t;
};

template <int N>
struct minimum_int_type<N, typename std::enable_if<(N > 2) && (N <= 4)>::type> {
    using type = std::uint32_t;
};

template <int N>
struct minimum_int_type<N, typename std::enable_if<(N > 4) && (N <= 8)>::type> {
    using type = std::uint64_t;
};

template <int N, typename B>
typename std::enable_if<N <= 8 && std::is_integral<B>::value,
                        std::uint64_t>::type
concat(B b1) {
    return b1;
}

template <int N, typename B1, typename B2, typename... Bs>
typename std::enable_if<N <= 8 && std::is_integral<B1>::value &&
                            std::is_integral<B2>::value,
                        std::uint64_t>::type
concat(B1 b1, B2 b2, Bs... bs) {
    std::uint64_t left = (b1 << 8) | b2;
    return concat<N + 1>(left, bs...);
}

}  // namespace bytes_detail

template <int N>
using minimum_byte_string_t = typename bytes_detail::minimum_int_type<N>::type;

/**
 * @brief Concatenates 1 to 8 bytes into a single byte string.
 *
 * @tparam B
 * @tparam Bs
 * @param b1
 * @param bs
 * @return std::enable_if_t<std::is_integral_v<B>, std::uint64_t>::type
 */
template <typename B, typename... Bs>
typename std::enable_if<std::is_integral<B>::value, std::uint64_t>::type concat(
    B b1, Bs... bs) {
    return bytes_detail::concat<1>(b1, bs...);
}

/**
 * @brief Inserts N bytes into the given byte array.
 *
 * The bytes are derived from the byte string given, with the least-significant
 * byte being inserted first.
 *
 * @tparam N
 * @tparam B
 * @param dest
 * @param byte_string
 * @return std::enable_if<bytes_detail::is_valid_byte_string<N, B>::value,
 * void>::type
 */
template <int N, typename B>
typename std::enable_if<bytes_detail::is_valid_byte_string<N, B>::value,
                        void>::type
insert(byte_vector& dest, B byte_string) {
    for (int i = 0; i < N; ++i, byte_string >>= 8) {
        dest.push_back(byte_string & 0xFF);
    }
}

/**
 * @brief Inserts N bytes into the given buffer.
 *
 * The bytes are derived from the byte string given, with the least-significant
 * byte being inserted first.
 *
 * @tparam N
 * @tparam B
 * @param dest
 * @param byte_string
 * @return std::enable_if<bytes_detail::is_valid_byte_string<N, B>::value,
 * void>::type
 */
template <int N, typename B>
typename std::enable_if<bytes_detail::is_valid_byte_string<N, B>::value,
                        void>::type
insert(buffer& dest, B byte_string) {
    for (int i = 0; i < N; ++i, byte_string >>= 8) {
        dest.put(byte_string & 0xFF, true);
    }
}

/**
 * @brief Extracts N bytes from the given buffer.
 *
 * Returns the smallest possible integer type to fit the extracted bytes.
 *
 * Does not check for buffer underflow.
 *
 * @tparam N
 * @param src
 * @return std::enable_if<bytes_detail::is_valid_byte_string_size<N>::value,
                        minimum_byte_string_t<N>>::type
 */
template <int N>
typename std::enable_if<bytes_detail::is_valid_byte_string_size<N>::value,
                        minimum_byte_string_t<N>>::type
extract(buffer& src) {
    minimum_byte_string_t<N> result = 0;
    for (std::size_t i = 0; i < N; ++i) {
        result |= src.get() << (i << 3);
    }
    return result;
}

}  // namespace bytes
}  // namespace util

#endif  // UTIL_BYTES_