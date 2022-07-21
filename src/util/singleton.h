#ifndef UTIL_SINGLETON_
#define UTIL_SINGLETON_

#include <memory>
#include <mutex>

namespace util {

namespace singleton_detail {

template <typename Self, typename Return>
class base_singleton {
   protected:
    base_singleton() = default;
    ~base_singleton() = default;
    base_singleton(const base_singleton &other) = delete;
    base_singleton &operator=(const base_singleton &other) = delete;
    base_singleton(base_singleton &&other) noexcept = delete;
    base_singleton &operator=(base_singleton &&other) noexcept = delete;

   public:
    /**
     * @brief Access the singleton instance.
     *
     * @return Return&
     */
    static Return &instance() {
        static Self single_instance;
        return single_instance;
    }
};

}  // namespace singleton_detail

template <typename Self>
class singleton : public singleton_detail::base_singleton<Self, Self> {};

template <typename Self>
class const_singleton
    : public singleton_detail::base_singleton<Self, const Self> {};

}  // namespace util

#endif  // UTIL_SINGLETON_