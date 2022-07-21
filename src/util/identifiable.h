#ifndef UTIL_IDENTIFIABLE_
#define UTIL_IDENTIFIABLE_

#include <atomic>
#include <type_traits>

namespace util {
namespace id_detail {

template <bool B, typename T>
struct identifiable;

template <typename T>
struct identifiable<true, T> {
   public:
    using id_t = T;

   private:
    static T next_id() {
        static std::atomic<T> next_id{};
        return next_id++;
    }

   protected:
    const id_t id_;

    identifiable() : id_(next_id()) {}
    identifiable(identifiable&& other) : id_(other.id_) {}

   public:
    /**
     * @brief The unique local ID associated with this instance of this type.
     *
     * @return id_t
     */
    id_t id() const { return id_; }
};

}  // namespace id_detail

/**
 * @brief Data structure that automatically attaches a unique identifier to each
 * instance of deriving types.
 *
 * @tparam T
 */
template <typename T>
using identifiable = id_detail::identifiable<
    std::is_integral<T>::value && std::is_default_constructible<T>::value, T>;
}  // namespace util

#endif  // UTIL_IDENTIFIABLE_