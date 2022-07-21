// Optional data structure, adapted from standard library and Boost
// implementations.

#ifndef UTIL_OPTIONAL_
#define UTIL_OPTIONAL_

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace util {

/**
 * @brief Exception for bad access to an optional object.
 *
 */
class bad_optional_access : public std::logic_error {
   public:
    bad_optional_access()
        : std::logic_error(
              "Attempted to access the value of an uninitialized optional "
              "object.") {}
};

/**
 * @brief Type of the "none" value.
 *
 */
struct none_t {
    struct init_tag {};
    explicit constexpr none_t(init_tag) {}
};

/**
 * @brief A value representing "no value" in an optional object.
 *
 * @return constexpr none_t
 */
constexpr none_t none((none_t::init_tag()));

namespace optional_detail {

/**
 * @brief Tag for in-place initialization.
 *
 */
struct in_place_init_t {
    struct init_tag {};
    explicit constexpr in_place_init_t(init_tag) {}
};
constexpr in_place_init_t in_place_init((in_place_init_t::init_tag()));

/**
 * @brief Tag for initializing the value in the constructor.
 *
 */
struct init_value_tag {};

/**
 * @brief Base implementation of optional.
 *
 * `optional` is implemented by keeping track of the initialization state of a
 * single block of memory that can hold a value of type T.
 *
 * @tparam T Value type
 */
template <typename T>
class optional_base {
   private:
    using storage_type = alignas(T) std::uint8_t[sizeof(T)];

   protected:
    using value_type = T;
    using unqualified_value_type = typename std::remove_cv<T>::type;

    using reference_type = T&;
    using reference_const_type = const T&;
    using rval_reference_type = T&&;
    using pointer_type = T*;
    using pointer_const_type = T const*;
    using argument_type = const T&;

    optional_base() : initialized_(false) {}

    optional_base(none_t) : initialized_(false) {}

    optional_base(init_value_tag, argument_type val) : initialized_(false) {
        construct(val);
    }

    optional_base(init_value_tag, rval_reference_type val)
        : initialized_(false) {
        construct(std::move(val));
    }

    optional_base(const optional_base& rhs) : initialized_(false) {
        if (rhs.is_initialized()) {
            construct(rhs.get_impl());
        }
    }

    optional_base(optional_base&& rhs) noexcept : initialized_(false) {
        if (rhs.is_initialized()) {
            construct(std::move(rhs.get_impl()));
        }
    }

    optional_base& operator=(const optional_base& rhs) {
        assign(rhs);
        return *this;
    }

    optional_base& operator=(optional_base&& rhs) noexcept {
        assign(std::move(rhs));
        return *this;
    }

    template <typename... Args>
    optional_base(Args&&... args) : initialized_(false) {
        construct(in_place_init, std::forward<Args>(args)...);
    }

    ~optional_base() { destroy(); }

    void assign(const optional_base& rhs) {
        if (is_initialized()) {
            if (rhs.is_initialized()) {
                assign_value(rhs.get_impl());
            } else {
                destroy();
            }
        } else if (rhs.is_initialized()) {
            construct(rhs.get_impl());
        }
    }

    void assign(optional_base&& rhs) {
        if (is_initialized()) {
            if (rhs.is_initialized()) {
                assign_value(std::move(rhs.get_impl()));
            } else {
                destroy();
            }
        } else if (rhs.is_initialized()) {
            construct(std::move(rhs.get_impl()));
        }
    }

    template <typename U>
    void assign(const optional_base<U>& rhs) {
        if (is_initialized()) {
            if (rhs.is_initialized()) {
                assign_value(rhs.get());
            } else {
                destroy();
            }
        } else if (rhs.is_initialized()) {
            construct(rhs.get());
        }
    }

    template <typename U>
    void assign(optional_base<U>&& rhs) {
        if (is_initialized()) {
            if (rhs.is_initialized()) {
                assign_value(std::move(rhs.get()));
            } else {
                destroy();
            }
        } else if (rhs.is_initialized()) {
            construct(std::move(rhs.get()));
        }
    }

    void assign(argument_type val) {
        if (is_initialized()) {
            assign_value(val);
        } else {
            construct(val);
        }
    }

    void assign(rval_reference_type val) {
        if (is_initialized()) {
            assign_value(std::move(val));
        } else {
            construct(std::move(val));
        }
    }

    void assign(none_t) noexcept { destroy(); }

    void construct(argument_type val) {
        new (&storage_) unqualified_value_type(val);
        initialized_ = true;
    }

    void construct(rval_reference_type val) {
        new (&storage_) unqualified_value_type(std::move(val));
        initialized_ = true;
    }

    template <typename... Args>
    void construct(in_place_init_t, Args&&... args) {
        new (&storage_) unqualified_value_type(std::forward<Args>(args)...);
        initialized_ = true;
    }

    /**
     * @brief Assigns a new value in-place.
     *
     * @tparam Args
     * @param args
     */
    template <typename... Args>
    void emplace_assign(Args&&... args) {
        destroy();
        construct(in_place_init, std::forward<Args>(args)...);
    }

    void assign_value(argument_type val) { get_impl() = val; }

    void assign_value(rval_reference_type val) { get_impl() = std::move(val); }

    /**
     * @brief Destroys any stored value.
     *
     */
    void destroy() {
        if (initialized_) {
            destroy_impl();
        }
    }

    pointer_const_type get_ptr_impl() const {
        return reinterpret_cast<pointer_const_type>(&storage_);
    }

    pointer_type get_ptr_impl() {
        return reinterpret_cast<pointer_type>(&storage_);
    }

    reference_const_type get_impl() const { return *get_ptr_impl(); }

    reference_type get_impl() { return *get_ptr_impl(); }

    void destroy_impl() {
        get_ptr_impl()->~T();
        initialized_ = false;
    }

    bool is_initialized() const noexcept { return initialized_; }

   public:
    /**
     * @brief Resets the object to have no value.
     *
     */
    void reset() noexcept { destroy(); }

    /**
     * @brief Get the ptr objectReturns a pointer to the stored value.
     *
     * Returns `nullptr` if no value is present.
     *
     * @return pointer_const_type
     */
    pointer_const_type get_ptr() const {
        return initialized_ ? get_ptr_impl() : nullptr;
    }

    /**
     * @brief Get the ptr objectReturns a pointer to the stored value.
     *
     * Returns `nullptr` if no value is present.
     *
     * @return pointer_type
     */
    pointer_type get_ptr() { return initialized_ ? get_ptr_impl() : nullptr; }

   private:
    bool initialized_;
    storage_type storage_;
};
}  // namespace optional_detail

template <typename T>
/**
 * @brief Data structure for representing the presence or absence of a value of
 * a data type.
 *
 * The optional class simply wraps a block of memory for a data type, tracking
 * if it is currently occupied by a value or not.
 *
 * `optional` is similar to a pointer, except no dynamic memory allocation is
 * involved. Instead, memory is kept completely on the stack, so the size of the
 * object is known at compile-time.
 *
 * Use `none` or the default constructor to represent the absence of a value.
 *
 */
class optional : public optional_detail::optional_base<T> {
   private:
    using base = optional_detail::optional_base<T>;

   public:
    using value_type = typename base::value_type;
    using reference_type = typename base::reference_type;
    using reference_const_type = typename base::reference_const_type;
    using rval_reference_type = typename base::rval_reference_type;
    using pointer_type = typename base::pointer_type;
    using pointer_const_type = typename base::pointer_const_type;
    using argument_type = typename base::argument_type;

    optional() noexcept : base() {}

    optional(none_t none) noexcept : base(none) {}

    optional(argument_type val)
        : base(optional_detail::init_value_tag(), val) {}

    optional(rval_reference_type val)
        : base(optional_detail::init_value_tag(), std::forward<T>(val)) {}

    // Construct from compatible optional type.
    template <typename U>
    explicit optional(
        const optional<U>& rhs,
        typename std::enable_if<std::is_constructible<T, U>::value,
                                bool>::type = true)
        : base() {
        if (rhs.is_initialized()) {
            construct(rhs.get());
        }
    }

    // Construct from moving out of a compatible optional type.
    template <typename U>
    explicit optional(
        optional<U>&& rhs,
        typename std::enable_if<std::is_constructible<T, U>::value,
                                bool>::type = true)
        : base() {
        if (rhs.is_initialized()) {
            construct(std::move(rhs.get()));
        }
    }

    // In-place initialization.
    template <typename... Args,
              typename std::enable_if<std::is_constructible<T, Args...>::value,
                                      bool>::type = true>
    optional(Args&&... args) : base(std::forward<Args>(args)...) {}

    optional(const optional&) = default;
    optional(optional&&) = default;
    optional& operator=(const optional& rhs) = default;
    optional& operator=(optional&& rs) = default;

    template <typename U>
    optional& operator=(const optional<U>&& rhs) {
        this->assign(rhs);
        return *this;
    }

    template <typename U>
    optional& operator=(optional<U>&& rhs) {
        this->assign(std::move(rhs));
        return *this;
    }

    template <typename U>
    typename std::enable_if<
        std::is_same<T, typename std::decay<U>::type>::value, optional&>::type
    operator=(U&& val) {
        this->assign(std::forward<U>(val));
        return *this;
    }

    optional& operator=(none_t none) noexcept {
        this->assign(none);
        return *this;
    }

    /**
     * @brief Emplaces a value using in-place initialization.
     *
     * @tparam Args
     * @param args
     */
    template <class... Args>
    void emplace(Args&&... args) {
        this->emplace_assign(std::forward<Args>(args)...);
    }

    void swap(optional& other) { std::swap(*this, other); }

    /**
     * @brief Returns a reference to the stored value, assuming it is not
     * `none`.
     *
     * Behavior is undefined if the data is uninitialized.
     *
     * @return reference_const_type
     */
    reference_const_type get() const { return this->get_impl(); }

    /**
     * @brief Returns a reference to the stored value, assuming it is not
     * `none`.
     *
     * Behavior is undefined if the data is uninitialized.
     *
     * @return reference_type
     */
    reference_type get() { return this->get_impl(); }

    pointer_const_type operator->() const { return this->get_ptr_impl(); }
    pointer_type operator->() { return this->get_ptr_impl(); }

    reference_const_type operator*() const& { return get(); }
    reference_type operator*() & { return get(); }
    rval_reference_type operator*() && { return std::move(get()); }

    /**
     * @brief Checks if the wrapper has a stored object.
     *
     * @return true
     * @return false
     */
    bool has_value() const { return this->is_initialized(); }

    /**
     * @brief Returns a reference to the stored value, assuming it is not
     * `none`.
     *
     * Throws `bad_optional_access` if the data is uninitialized.
     *
     * @return reference_const_type
     */
    reference_const_type value() const& {
        if (this->is_initialized()) {
            return get();
        } else {
            throw bad_optional_access();
        }
    }

    /**
     * @brief Returns a reference to the stored value, assuming it is not
     * `none`.
     *
     * Throws `bad_optional_access` if the data is uninitialized.
     *
     * @return reference_type
     */
    reference_type value() & {
        if (this->is_initialized()) {
            return get();
        } else {
            throw bad_optional_access();
        }
    }

    /**
     * @brief Returns a reference to the stored value, assuming it is not
     * `none`.
     *
     * Throws `bad_optional_access` if the data is uninitialized.
     *
     * @return rval_reference_type
     */
    rval_reference_type value() && {
        if (this->is_initialized()) {
            return std::move(get());
        } else {
            throw bad_optional_access();
        }
    }

    /**
     * @brief Returns a copy of the stored value if it exists. If not, a default
     * value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return value_type
     */
    template <class U>
    value_type value_or(U&& other) const& {
        if (this->is_initialized()) {
            return get();
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Returns a copy of the stored value if it exists. If not, a default
     * value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return value_type
     */
    template <class U>
    value_type value_or(U&& other) && {
        if (this->is_initialized()) {
            return std::move(get());
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the wrapper
     * does not contain `none`.
     *
     * @tparam F Function type
     * @param f Mapping function
     * @return Result of mapping function, or none
     */
    template <typename F>
    optional<typename std::result_of<F(reference_type)>::type> map(F f) & {
        if (this->is_initialized()) {
            return f(get());
        } else {
            return none;
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the wrapper
     * does not contain `none`.
     *
     * @tparam F Function type
     * @param f Mapping function
     * @return Result of mapping function, or none
     */
    template <typename F>
    optional<typename std::result_of<F(reference_const_type)>::type> map(
        F f) const& {
        if (this->is_initialized()) {
            return f(get());
        } else {
            return none;
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the wrapper
     * does not contain `none`.
     *
     * @tparam F Function type
     * @param f Mapping function
     * @return Result of mapping function, or none
     */
    template <typename F>
    optional<typename std::result_of<F(rval_reference_type)>::type> map(
        F f) && {
        if (this->is_initialized()) {
            return f(std::move(get()));
        } else {
            return none;
        }
    }
};

}  // namespace util

std::ostream& operator<<(std::ostream& out, util::none_t);

template <typename T>
std::ostream& operator<<(std::ostream& out, const util::optional<T> opt);

#endif  // UTIL_OPTIONAL_