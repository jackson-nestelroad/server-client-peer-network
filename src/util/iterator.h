#ifndef UTIL_ITERATOR_
#define UTIL_ITERATOR_

#include <iterator>
#include <random>

namespace util {
namespace iterator {

/**
 * @brief Selects a random element in the given iterator range based on the
 * generator given.
 *
 * @tparam Iter
 * @tparam Rng
 * @param begin
 * @param end
 * @param rng
 * @return Iter
 */
template <typename Iter, typename Rng>
Iter random(Iter begin, Iter end, Rng& rng) {
    std::uniform_int_distribution<> dis(0, std::distance(begin, end) - 1);
    std::advance(begin, dis(rng));
    return begin;
}

/**
 * @brief Selects a random element in the given iterator range.
 *
 * @tparam Iter
 * @param begin
 * @param end
 * @return Iter
 */
template <typename Iter>
Iter random(Iter begin, Iter end) {
    static std::random_device device;
    static std::mt19937 rng(device());
    return random(begin, end, rng);
}

}  // namespace iterator
}  // namespace util

#endif  // UTIL_ITERATOR_