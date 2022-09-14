#ifndef UTIL_RESULT_
#define UTIL_RESULT_

#include <util/optional.h>

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#define RETURN_IF_ERROR(rexpr)                             \
    {                                                      \
        auto __result = (rexpr);                           \
        if (__result.is_err()) {                           \
            return {util::err, std::move(__result).err()}; \
        }                                                  \
    }

#define __RESULT_MACROS_CONCAT_NAME_INNER(x, y) x##y
#define __RESULT_MACROS_CONCAT_NAME(x, y) \
    __RESULT_MACROS_CONCAT_NAME_INNER(x, y)

#define __ASSIGN_OR_RETURN_IMPL(temp, lhs, rexpr)  \
    auto temp = (rexpr);                           \
    if (temp.is_err()) {                           \
        return {util::err, std::move(temp).err()}; \
    }                                              \
    lhs = temp.ok();

#define ASSIGN_OR_RETURN(lhs, rexpr) \
    __ASSIGN_OR_RETURN_IMPL(         \
        __RESULT_MACROS_CONCAT_NAME(__result, __COUNTER__), lhs, rexpr);

namespace util {

/**
 * @brief Type of "ok" value.
 *
 */
struct ok_t {
    struct init_tag {};
    explicit constexpr ok_t(init_tag) {}
};
/**
 * @brief Type of "err" value.
 *
 */
struct err_t {
    struct init_tag {};
    explicit constexpr err_t(init_tag) {}
};

/**
 * @brief A tag representing "OK" for a result object.
 *
 * @return constexpr ok_t
 */
constexpr ok_t ok((ok_t::init_tag()));
/**
 * @brief A tag representing "Error" for a result object.
 *
 * @return constexpr err_t
 */
constexpr err_t err((err_t::init_tag()));

/**
 * @brief Exception for bad access to a result object.
 *
 */
class bad_result_access : public std::logic_error {
   public:
    bad_result_access(ok_t)
        : std::logic_error(
              "Attempted to access the value of a result with an error") {}
    bad_result_access(err_t)
        : std::logic_error(
              "Attempted to access the error of a result with no error") {}
};

namespace result_detail {

/**
 * @brief Compile-time maximum of multiple integers.
 *
 * @tparam N
 * @tparam Ns
 */
template <std::size_t N, std::size_t... Ns>
struct max;

template <std::size_t N>
struct max<N> {
    static constexpr std::size_t value = N;
};

template <std::size_t N1, std::size_t N2, std::size_t... Ns>
struct max<N1, N2, Ns...> {
    static constexpr std::size_t value =
        N1 > N2 ? max<N1, Ns...>::value : max<N2, Ns...>::value;
};

/**
 * @brief Compile-time maximum of the size of multiple types.
 *
 * @tparam Ts
 */
template <typename... Ts>
struct max_sizeof : public max<sizeof(Ts)...> {};

/**
 * @brief Compile-time maximum of the alignment of multiple types.
 *
 * @tparam Ts
 */
template <typename... Ts>
struct max_alignof : public max<alignof(Ts)...> {};

/**
 * @brief Helper trait for checking if two types can form a valid result.
 *
 * The two types must be distinct from one another to avoid ambiguity in
 * constructors and other methods.
 *
 * @tparam T OK type
 * @tparam E Error type
 */
template <typename T, typename E>
struct valid_result_params {
    static constexpr bool value =
        !std::is_same<typename std::decay<T>::type,
                      typename std::decay<E>::type>::value;
};

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
 * @brief Base implementation of result.
 *
 * @tparam T OK type
 * @tparam E Error type
 * @tparam B Valid specialization
 */
template <typename T, typename E, bool B = valid_result_params<T, E>::value>
struct result_base;

/**
 * @brief Specialization of result_base, when T and E are valid, distinct types.
 *
 * `result` is implemented by keeping one block of memory that can hold either
 * value of type T (a result) or a value of type E (an error). Because a result
 * and an error cannot exist at the same time, a single boolean flag keeps track
 * of which type of data is stored in the wrapper at any given time.
 *
 * The `ok_t` and `err_t` type flags are used to specify which type of value is
 * set by helper methods. If a result is switching states (i.e., it contained a
 * result but now contains an error), then the previous value should be properly
 * destroyed (destructor called) and the new value should be properly allocated
 * (constructor called). If a result is changing values, a simple assignment
 * (copy or move constructor) will suffice.
 *
 * @tparam T OK time
 * @tparam E Error time
 */
template <typename T, typename E>
class result_base<T, E, true> {
   protected:
    using value_type = T;
    using unqualified_value_type = typename std::remove_cv<T>::type;
    using error_type = E;
    using unqualified_error_type = typename std::remove_cv<E>::type;

    using value_reference_type = T&;
    using value_reference_const_type = const T&;
    using value_rval_reference_type = T&&;
    using value_pointer_type = T*;
    using value_pointer_const_type = T const*;
    using value_argument_type = const T&;

    using error_reference_type = E&;
    using error_reference_const_type = const E&;
    using error_rval_reference_type = E&&;
    using error_pointer_type = E*;
    using error_pointer_const_type = E const*;
    using error_argument_type = const E&;

    result_base(ok_t, value_argument_type val) { construct(ok, val); }
    result_base(ok_t, value_rval_reference_type val) {
        construct(ok, std::move(val));
    }
    result_base(err_t, error_argument_type val) { construct(err, val); }
    result_base(err_t, error_rval_reference_type val) {
        construct(err, std::move(val));
    }

    template <typename... Args>
    result_base(ok_t, Args&&... args) {
        construct(ok, in_place_init, std::forward<Args>(args)...);
    }
    template <typename... Args>
    result_base(err_t, Args&&... args) {
        construct(err, in_place_init, std::forward<Args>(args)...);
    }

    result_base(const result_base& rhs) {
        if (rhs.is_ok()) {
            construct(ok, rhs.get_ok_impl());
        } else {
            construct(err, rhs.get_err_impl());
        }
    }
    result_base(result_base&& rhs) noexcept {
        if (rhs.is_ok()) {
            construct(ok, std::move(rhs.get_ok_impl()));
        } else {
            construct(err, std::move(rhs.get_err_impl()));
        }
    }

    result_base& operator=(const result_base& rhs) {
        assign(rhs);
        return *this;
    }
    result_base& operator=(result_base&& rhs) noexcept {
        assign(std::move(rhs));
        return *this;
    }

    ~result_base() { destroy(); }

    void assign(const result_base& rhs) {
        if (is_ok()) {
            if (rhs.is_ok()) {
                assign_value(ok, rhs.get_ok_impl());
            } else {
                destroy();
                construct(err, rhs.get_err_impl());
            }
        } else if (rhs.is_ok()) {
            destroy();
            construct(ok, rhs.get_ok_impl());
        } else {
            assign_value(err, rhs.get_err_impl());
        }
    }

    void assign(result_base&& rhs) {
        if (is_ok()) {
            if (rhs.is_ok()) {
                assign_value(ok, std::move(rhs.get_ok_impl()));
            } else {
                destroy();
                construct(err, std::move(rhs.get_err_impl()));
            }
        } else if (rhs.is_ok()) {
            destroy();
            construct(ok, std::move(rhs.get_ok_impl()));
        } else {
            assign_value(err, std::move(rhs.get_err_impl()));
        }
    }

    void assign(value_argument_type val) {
        if (is_ok()) {
            assign_value(ok, val);
        } else {
            destroy();
            construct(ok, val);
        }
    }

    void assign(value_rval_reference_type val) {
        if (is_ok()) {
            assign_value(ok, std::move(val));
        } else {
            destroy();
            construct(ok, std::move(val));
        }
    }

    void assign(error_argument_type val) {
        if (is_err()) {
            assign_value(err, val);
        } else {
            destroy();
            construct(err, val);
        }
    }

    void assign(error_rval_reference_type val) {
        if (is_err()) {
            assign_value(err, std::move(val));
        } else {
            destroy();
            construct(err, std::move(val));
        }
    }

    void construct(ok_t, value_argument_type val) {
        new (&storage_) unqualified_value_type(val);
        error_ = false;
    }

    void construct(ok_t, value_rval_reference_type val) {
        new (&storage_) unqualified_value_type(std::move(val));
        error_ = false;
    }

    template <typename... Args>
    void construct(ok_t, in_place_init_t, Args&&... args) {
        new (&storage_) unqualified_value_type(std::forward<Args>(args)...);
        error_ = false;
    }

    void construct(err_t, error_argument_type val) {
        new (&storage_) unqualified_error_type(val);
        error_ = true;
    }

    void construct(err_t, error_rval_reference_type val) {
        new (&storage_) unqualified_error_type(std::move(val));
        error_ = true;
    }

    template <typename... Args>
    void construct(err_t, in_place_init_t, Args&&... args) {
        new (&storage_) unqualified_error_type(std::forward<Args>(args)...);
        error_ = true;
    }

    /**
     * @brief Assigns a new value in-place.
     *
     * @tparam Args
     * @param args
     */
    template <typename... Args>
    void emplace_assign(ok_t, Args&&... args) {
        destroy();
        construct(ok, in_place_init, std::forward<Args>(args)...);
    }

    /**
     * @brief Assigns a new error in-place.
     *
     * @tparam Args
     * @param args
     */
    template <typename... Args>
    void emplace_assign(err_t, Args&&... args) {
        destroy();
        construct(err, in_place_init, std::forward<Args>(args)...);
    }

    void assign_value(ok_t, value_argument_type val) {
        get_ok_impl() = val;
        error_ = false;
    }

    void assign_value(ok_t, value_rval_reference_type val) {
        get_ok_impl() = std::move(val);
        error_ = false;
    }

    void assign_value(err_t, error_argument_type val) {
        get_err_impl() = val;
        error_ = true;
    }

    void assign_value(err_t, error_rval_reference_type val) {
        get_err_impl() = std::move(val);
        error_ = true;
    }

    value_pointer_const_type get_ok_ptr_impl() const {
        return reinterpret_cast<value_pointer_const_type>(&storage_);
    }

    value_pointer_type get_ok_ptr_impl() {
        return reinterpret_cast<value_pointer_type>(&storage_);
    }

    error_pointer_const_type get_err_ptr_impl() const {
        return reinterpret_cast<error_pointer_const_type>(&storage_);
    }

    error_pointer_type get_err_ptr_impl() {
        return reinterpret_cast<error_pointer_type>(&storage_);
    }

    value_reference_const_type get_ok_impl() const {
        return *get_ok_ptr_impl();
    }

    value_reference_type get_ok_impl() { return *get_ok_ptr_impl(); }

    error_reference_const_type get_err_impl() const {
        return *get_err_ptr_impl();
    }

    error_reference_type get_err_impl() { return *get_err_ptr_impl(); }

    /**
     * @brief Destroys the stored value, calling the appropriate destructor.
     *
     * Note, `destroy` should NEVER be called on its own without replacing the
     * storage with a new value, because a `result` object should always contain
     * some value.
     *
     */
    void destroy() {
        if (is_ok()) {
            get_ok_ptr_impl()->~T();
        } else {
            get_err_ptr_impl()->~E();
        }
    }

   public:
    /**
     * @brief Checks if the result is OK and does not contain an error.
     *
     * @return true
     * @return false
     */
    bool is_ok() const { return !error_; }

    /**
     * @brief Checks if the result contains an error.
     *
     * @return true
     * @return false
     */
    bool is_err() const { return error_; }

    /**
     * @brief Get the ok ptr objectReturns a pointer to the stored value.
     *
     * Returns `nullptr` if the result contains an error instead.
     *
     * @return value_pointer_const_type
     */
    value_pointer_const_type get_ok_ptr() const {
        return is_ok() ? get_ok_ptr_impl() : nullptr;
    }

    /**
     * @brief Get the ok ptr objectReturns a pointer to the stored value.
     *
     * Returns `nullptr` if the result contains an error instead.
     *
     * @return value_pointer_type
     */
    value_pointer_type get_ok_ptr() {
        return is_ok() ? get_ok_ptr_impl() : nullptr;
    }

    /**
     * @brief Get the err ptr objectReturns a pointer to the stored error.
     *
     * Returns `nullptr` if the result contains an OK value instead.
     *
     * @return error_pointer_const_type
     */
    error_pointer_const_type get_err_ptr() const {
        return !is_ok() ? get_err_ptr_impl() : nullptr;
    }

    /**
     * @brief Get the err ptr objectReturns a pointer to the stored error.
     *
     * Returns `nullptr` if the result contains an OK value instead.
     *
     * @return error_pointer_type
     */
    error_pointer_type get_err_ptr() {
        return !is_ok() ? get_err_ptr_impl() : nullptr;
    }

   private:
    static constexpr std::size_t storage_sizeof =
        result_detail::max_sizeof<T, E>::value;
    static constexpr std::size_t storage_alignof =
        result_detail::max_alignof<T, E>::value;

    using storage_type = alignas(storage_alignof) std::uint8_t[storage_sizeof];

    bool error_;
    storage_type storage_;
};

/**
 * @brief Specialization of result_base, when T is void.
 *
 * A result with a void OK type essentially represents an optional error. If
 * there is no error, no additional value must be stored, so the area of memory
 * need only represent an optional error value. This functionality is borrowed
 * from the `optional` class, and its API is adapted here.
 *
 * @tparam E Error type
 */
template <typename E>
class result_base<void, E, true> {
   protected:
    using value_type = void;
    using error_type = E;
    using unqualified_error_type = typename std::remove_cv<E>::type;

    using value_reference_type = void;
    using value_reference_const_type = void;
    using value_rval_reference_type = void;
    using value_pointer_type = void;
    using value_pointer_const_type = void;
    using value_argument_type = void;

    using error_reference_type = E&;
    using error_reference_const_type = const E&;
    using error_rval_reference_type = E&&;
    using error_pointer_type = E*;
    using error_pointer_const_type = E const*;
    using error_argument_type = const E&;

    result_base(ok_t) {}
    result_base(err_t, error_argument_type val) : error_(val) {}
    result_base(err_t, error_rval_reference_type val)
        : error_(std::move(val)) {}

    template <typename... Args>
    result_base(err_t, Args&&... args) : error_(std::forward<Args>(args)...) {}

    result_base(const result_base& rhs) = default;
    result_base(result_base&& rhs) = default;

    result_base& operator=(const result_base& rhs) = default;
    result_base& operator=(result_base&& rhs) noexcept = default;

    ~result_base() = default;

    /**
     * @brief Assigns a new error in-place.
     *
     * @tparam Args
     * @param args
     */
    template <typename... Args>
    void emplace_assign(err_t, Args&&... args) {
        error_.emplace(std::forward<Args>(args)...);
    }

    void assign(error_argument_type val) { error_ = val; }

    void assign(error_rval_reference_type val) { error_ = std::move(val); }

    error_pointer_const_type get_err_ptr_impl() const {
        return error_.get_ptr();
    }

    error_pointer_type get_err_ptr_impl() { return error_.get_ptr(); }

    error_reference_const_type get_err_impl() const { return error_.get(); }

    error_reference_type get_err_impl() { return error_.get(); }

    void destroy() { error_.reset(); }

   public:
    /**
     * @brief Checks if the result is OK and does not contain an error.
     *
     * @return true
     * @return false
     */
    bool is_ok() const { return !error_.has_value(); }

    /**
     * @brief Checks if the result contains an error.
     *
     * @return true
     * @return false
     */
    bool is_err() const { return error_.has_value(); }

    /**
     * @brief Get the err ptr objectReturns a pointer to the stored error.
     *
     * Returns `nullptr` if the result contains an OK value instead.
     *
     * @return error_pointer_const_type
     */
    error_pointer_const_type get_err_ptr() const {
        return !is_ok() ? get_err_ptr_impl() : nullptr;
    }

    /**
     * @brief Get the err ptr objectReturns a pointer to the stored error.
     *
     * Returns `nullptr` if the result contains an OK value instead.
     *
     * @return error_pointer_type
     */
    error_pointer_type get_err_ptr() {
        return !is_ok() ? get_err_ptr_impl() : nullptr;
    }

   private:
    optional<E> error_;
};

}  // namespace result_detail

/**
 * @brief Data structure for representing the result of some operation that
 * returns a result and may also fail with an error.
 *
 * `result` is similar to a type union between two types: a single block of
 * memory is interpreted to be one of two types at any given time. Several
 * methods are given for accessing, chaining, and modifying a result object.
 *
 * Result and error types may be anything, but the two types must be distinct.
 * The `ok` and `err` values can be used when initializing a result if
 * constructors are ambiguous due to implicit type conversions.
 *
 * @tparam T OK type
 * @tparam E Error type
 */
template <typename T, typename E>
class result : public result_detail::result_base<T, E> {
   private:
    using base = result_detail::result_base<T, E>;

   public:
    using value_type = T;
    using error_type = E;

    using value_reference_type = typename base::value_reference_type;
    using value_reference_const_type =
        typename base::value_reference_const_type;
    using value_rval_reference_type = typename base::value_rval_reference_type;
    using value_pointer_type = typename base::value_pointer_type;
    using value_pointer_const_type = typename base::value_pointer_const_type;
    using value_argument_type = typename base::value_argument_type;

    using error_reference_type = typename base::error_reference_type;
    using error_reference_const_type =
        typename base::error_reference_const_type;
    using error_rval_reference_type = typename base::error_rval_reference_type;
    using error_pointer_type = typename base::error_pointer_type;
    using error_pointer_const_type = typename base::error_pointer_const_type;
    using error_argument_type = typename base::error_argument_type;

    result(value_reference_type val) : base(util::ok, val) {}

    result(value_rval_reference_type val)
        : base(util::ok, std::forward<T>(val)) {}

    result(error_reference_type val) : base(util::err, val) {}

    result(error_rval_reference_type val)
        : base(util::err, std::forward<E>(val)) {}

    // Explicitly construct an OK value using a copy.
    result(ok_t, value_reference_type val) : base(util::ok, val) {}

    // Explicitly construct an OK value using a move.
    result(ok_t, value_rval_reference_type val)
        : base(util::ok, std::forward<T>(val)) {}

    // Explicitly construct an error value using a copy.
    result(err_t, error_reference_type val) : base(util::err, val) {}

    // Explicitly construct an error value using a move.
    result(err_t, error_rval_reference_type val)
        : base(util::err, std::forward<E>(val)) {}

    // In-place initialization for OK values.
    template <typename... Args,
              typename std::enable_if<std::is_constructible<T, Args...>::value,
                                      bool>::type = true>
    result(Args&&... args) : base(util::ok, std::forward<Args>(args)...) {}

    // In-place initialization for error values.
    template <typename... Args,
              typename std::enable_if<std::is_constructible<E, Args...>::value,
                                      bool>::type = true>
    result(Args&&... args) : base(util::err, std::forward<Args>(args)...) {}

    result(const result&) = default;
    result(result&&) = default;
    result& operator=(const result&) = default;
    result& operator=(result&&) noexcept = default;

    /**
     * @brief Emplaces an OK value using in-place initializaiton.
     *
     * @tparam Args
     * @param args
     */
    template <class... Args>
    void emplace(ok_t, Args&&... args) {
        this->emplace_assign(util::ok, std::forward<Args>(args)...);
    }

    /**
     * @brief Emplaces an error value using in-place initializaiton.
     *
     * @tparam Args
     * @param args
     */
    template <class... Args>
    void emplace(err_t, Args&&... args) {
        this->emplace_assign(util::err, std::forward<Args>(args)...);
    }

    /**
     * @brief Returns a reference to the stored OK value, assuming it is an OK
     * value.
     *
     * Throws `bad_optional_access` if result state is not OK.
     *
     * @return value_reference_const_type
     */
    value_reference_const_type ok() const& {
        if (this->is_ok()) {
            return this->get_ok_impl();
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored OK value, assuming it is an OK
     * value.
     *
     * Throws `bad_optional_access` if result state is not OK.
     *
     * @return value_reference_type
     */
    value_reference_type ok() & {
        if (this->is_ok()) {
            return this->get_ok_impl();
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored OK value, assuming it is an OK
     * value.
     *
     * Throws `bad_optional_access` if result state is not OK.
     *
     * @return value_rval_reference_type
     */
    value_rval_reference_type ok() && {
        if (this->is_ok()) {
            return std::move(this->get_ok_impl());
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored error value, assuming it is an
     * error value.
     *
     * Throws `bad_optional_access` if result state is not an error.
     *
     * @return error_reference_const_type
     */
    error_reference_const_type err() const& {
        if (this->is_err()) {
            return this->get_err_impl();
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored error value, assuming it is an
     * error value.
     *
     * Throws `bad_optional_access` if result state is not an error.
     *
     * @return error_reference_type
     */
    error_reference_type err() & {
        if (this->is_err()) {
            return this->get_err_impl();
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored error value, assuming it is an
     * error value.
     *
     * Throws `bad_optional_access` if result state is not an error.
     *
     * @return error_rval_reference_type
     */
    error_rval_reference_type err() && {
        if (this->is_err()) {
            return std::move(this->get_err_impl());
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a copy of the stored OK value if it exists. If not, a
     * default value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return T
     */
    template <typename U>
    T ok_or(U&& other) const& {
        if (this->is_ok()) {
            return this->get_ok_impl();
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Returns a copy of the stored OK value if it exists. If not, a
     * default value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return T
     */
    template <typename U>
    T ok_or(U&& other) && {
        if (this->is_ok()) {
            return std::move(this->get_ok_impl());
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Returns a copy of the stored error value if it exists. If not, a
     * default value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return E
     */
    template <typename U>
    E err_or(U&& other) const& {
        if (this->is_err()) {
            return this->get_err_impl();
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Returns a copy of the stored error value if it exists. If not, a
     * default value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return E
     */
    template <typename U>
    E err_or(U&& other) && {
        if (this->is_err()) {
            return std::move(this->get_err_impl());
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Chains another operation that produces a `result`.
     *
     * If the current result is an error, the operation is ignored and the same
     * error is returned back immediately. If the current result is OK, then the
     * result of the operation on the OK value is returned.
     *
     * @tparam U New OK type
     * @tparam F Function type
     * @param f Next operation
     * @return result<U, E> Next operation result, or previous error
     */
    template <typename U, typename F,
              typename std::enable_if<
                  std::is_same<result<U, E>, typename std::result_of<
                                                 F(const T&)>::type>::value,
                  bool>::type = true>
    result<U, E> and_then(F f) const& {
        if (this->is_ok()) {
            return f(this->get_ok_impl());
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Chains another operation that produces a `result`.
     *
     * If the current result is an error, the operation is ignored and the same
     * error is returned back immediately. If the current result is OK, then the
     * result of the operation on the OK value is returned.
     *
     * @tparam U New OK type
     * @tparam F Function type
     * @param f Next operation
     * @return result<U, E> Next operation result, or previous error
     */
    template <typename U, typename F,
              typename std::enable_if<
                  std::is_same<result<U, E>,
                               typename std::result_of<F(T&)>::type>::value,
                  bool>::type = true>
    result<U, E> and_then(F f) & {
        if (this->is_ok()) {
            return f(this->get_ok_impl());
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Chains another operation that produces a `result`.
     *
     * If the current result is an error, the operation is ignored and the same
     * error is returned back immediately. If the current result is OK, then the
     * result of the operation on the OK value is returned.
     *
     * @tparam U New OK type
     * @tparam F Function type
     * @param f Next operation
     * @return result<U, E> Next operation result, or previous error
     */
    template <typename U, typename F,
              typename std::enable_if<
                  std::is_same<result<U, E>,
                               typename std::result_of<F(T&&)>::type>::value,
                  bool>::type = true>
    result<U, E> and_then(F f) && {
        if (this->is_ok()) {
            return f(std::move(this->get_ok_impl()));
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the result
     * contains an OK value.
     *
     * If the result contains an error, the same error is copied and returned.
     *
     * @tparam F Function type
     * @param f Mapping function
     * @return result<U, E> Mapped result, or previous result
     */
    template <typename F,
              typename U = typename std::result_of<F(const T&)>::type>
    result<U, E> map(F f) const& {
        if (this->is_ok()) {
            return {util::ok, f(this->get_ok_impl())};
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the result
     * contains an OK value.
     *
     * If the result contains an error, the same error is copied and returned.
     *
     * @tparam F Function type
     * @param f Mapping function
     * @return result<U, E> Mapped result, or previous result
     */
    template <typename F, typename U = typename std::result_of<F(T&)>::type>
    result<U, E> map(F f) & {
        if (this->is_ok()) {
            return {util::ok, f(this->get_ok_impl())};
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the result
     * contains an OK value.
     *
     * If the result contains an error, the same error is copied and returned.
     *
     * @tparam F Function type
     * @param f Mapping function
     * @return result<U, E> Mapped result, or previous result
     */
    template <typename F, typename U = typename std::result_of<F(T&&)>::type>
    result<U, E> map(F f) && {
        if (this->is_ok()) {
            return {util::ok, f(std::move(this->get_ok_impl()))};
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored error value using the given function only if the
     * result contains an error value. If the result contains an OK value, the
     * same value is copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<T, R> Mapped error, or previous value
     */
    template <typename F,
              typename R = typename std::result_of<F(const E&)>::type>
    result<T, R> map_err(F f) const& {
        if (this->is_ok()) {
            return {util::ok, this->get_ok_impl()};
        } else {
            return {util::err, f(this->get_err_impl())};
        }
    }

    /**
     * @brief Maps the stored error value using the given function only if the
     * result contains an error value. If the result contains an OK value, the
     * same value is copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<T, R> Mapped error, or previous value
     */
    template <typename F, typename R = typename std::result_of<F(E&)>::type>
    result<T, R> map_err(F f) & {
        if (this->is_ok()) {
            return {util::ok, this->get_ok_impl()};
        } else {
            return {util::err, f(this->get_err_impl())};
        }
    }

    /**
     * @brief Maps the stored error value using the given function only if the
     * result contains an error value. If the result contains an OK value, the
     * same value is copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<T, R> Mapped error, or previous value
     */
    template <typename F, typename R = typename std::result_of<F(E&&)>::type>
    result<T, R> map_err(F f) && {
        if (this->is_ok()) {
            return {util::ok, this->get_ok_impl()};
        } else {
            return {util::err, f(std::move(this->get_err_impl()))};
        }
    }
};

/**
 * @brief Data structure for representing the result of some operation that
 * returns no result and may also fail with an error.
 *
 * `result<void, E>` is equivalent to `optional<E>` with an adapted API.
 *
 * @tparam E Error type
 */
template <typename E>
class result<void, E> : public result_detail::result_base<void, E> {
   private:
    using base = result_detail::result_base<void, E>;

   public:
    using value_type = void;
    using error_type = E;

    using value_reference_type = typename base::value_reference_type;
    using value_reference_const_type =
        typename base::value_reference_const_type;
    using value_rval_reference_type = typename base::value_rval_reference_type;
    using value_pointer_type = typename base::value_pointer_type;
    using value_pointer_const_type = typename base::value_pointer_const_type;
    using value_argument_type = typename base::value_argument_type;

    using error_reference_type = typename base::error_reference_type;
    using error_reference_const_type =
        typename base::error_reference_const_type;
    using error_rval_reference_type = typename base::error_rval_reference_type;
    using error_pointer_type = typename base::error_pointer_type;
    using error_pointer_const_type = typename base::error_pointer_const_type;
    using error_argument_type = typename base::error_argument_type;

    result() : base(util::ok) {}

    result(ok_t) : base(util::ok) {}

    result(error_reference_type val) : base(util::err, val) {}

    result(error_rval_reference_type val)
        : base(util::err, std::forward<E>(val)) {}

    result(err_t, error_reference_type val) : base(util::err, val) {}

    result(err_t, error_rval_reference_type val)
        : base(util::err, std::forward<E>(val)) {}

    // In-place initialization for error values.
    template <typename... Args,
              typename std::enable_if<std::is_constructible<E, Args...>::value,
                                      bool>::type = true>
    result(Args&&... args) : base(util::err, std::forward<Args>(args)...) {}

    result(const result&) = default;
    result(result&&) = default;
    result& operator=(const result&) = default;
    result& operator=(result&&) noexcept = default;

    /**
     * @brief Emplaces an OK value.
     *
     */
    void emplace(ok_t) { this->destroy(); }

    /**
     * @brief Emplaces an error value using in-place initializaiton.
     *
     * @tparam Args
     * @param args
     */
    template <class... Args>
    void emplace(err_t, Args&&... args) {
        this->emplace_assign(util::err, std::forward<Args>(args)...);
    }

    void set_ok() { this->destroy(); }

    /**
     * @brief Returns a reference to the stored error value, assuming it is an
     * error value.
     *
     * Throws `bad_optional_access` if result state is not an error.
     *
     * @return error_reference_const_type
     */
    error_reference_const_type err() const& {
        if (this->is_err()) {
            return this->get_err_impl();
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored error value, assuming it is an
     * error value.
     *
     * Throws `bad_optional_access` if result state is not an error.
     *
     * @return error_reference_type
     */
    error_reference_type err() & {
        if (this->is_err()) {
            return this->get_err_impl();
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a reference to the stored error value, assuming it is an
     * error value.
     *
     * Throws `bad_optional_access` if result state is not an error.
     *
     * @return error_rval_reference_type
     */
    error_rval_reference_type err() && {
        if (this->is_err()) {
            return std::move(this->get_err_impl());
        } else {
            throw bad_result_access(util::ok);
        }
    }

    /**
     * @brief Returns a copy of the stored error value if it exists. If not, a
     * default value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return E
     */
    template <typename U>
    E err_or(U&& other) const& {
        if (this->is_err()) {
            return this->get_err_impl();
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Returns a copy of the stored error value if it exists. If not, a
     * default value is constructed and returned instead.
     *
     * @tparam U
     * @param other
     * @return E
     */
    template <typename U>
    E err_or(U&& other) && {
        if (this->is_err()) {
            return std::move(this->get_err_impl());
        } else {
            return std::forward<U>(other);
        }
    }

    /**
     * @brief Chains another operation that produces a `result`.
     *
     * If the current result is an error, the operation is ignored and the same
     * error is returned back immediately. If the current result is OK, then the
     * result of the operation on the OK value is returned.
     *
     * @tparam U New OK type
     * @tparam F Function type
     * @param f Next operation
     * @return result<U, E> Next operation result, or previous error
     */
    template <typename U, typename F,
              typename std::enable_if<
                  std::is_same<result<U, E>,
                               typename std::result_of<F()>::type>::value,
                  bool>::type = true>
    result<U, E> and_then(F f) const& {
        if (this->is_ok()) {
            return f();
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Chains another operation that produces a `result`.
     *
     * If the current result is an error, the operation is ignored and the same
     * error is returned back immediately. If the current result is OK, then the
     * result of the operation on the OK value is returned.
     *
     * @tparam U New OK type
     * @tparam F Function type
     * @param f Next operation
     * @return result<U, E> Next operation result, or previous error
     */
    template <typename U, typename F,
              typename std::enable_if<
                  std::is_same<result<U, E>,
                               typename std::result_of<F()>::type>::value,
                  bool>::type = true>
    result<U, E> and_then(F f) & {
        if (this->is_ok()) {
            return f();
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Chains another operation that produces a `result`.
     *
     * If the current result is an error, the operation is ignored and the same
     * error is returned back immediately. If the current result is OK, then the
     * result of the operation on the OK value is returned.
     *
     * @tparam U New OK type
     * @tparam F Function type
     * @param f Next operation
     * @return result<U, E> Next operation result, or previous error
     */
    template <typename U, typename F,
              typename std::enable_if<
                  std::is_same<result<U, E>,
                               typename std::result_of<F()>::type>::value,
                  bool>::type = true>
    result<U, E> and_then(F f) && {
        if (this->is_ok()) {
            return f();
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the result
     * contains an OK value. If the result contains an error, the same error is
     * copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<U, E> Mapped result, or previous error
     */
    template <typename F, typename U = typename std::result_of<F()>::type>
    result<U, E> map(F f) const& {
        if (this->is_ok()) {
            return {util::ok, f()};
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the result
     * contains an OK value. If the result contains an error, the same error is
     * copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<U, E> Mapped result, or previous error
     */
    template <typename F, typename U = typename std::result_of<F()>::type>
    result<U, E> map(F f) & {
        if (this->is_ok()) {
            return {util::ok, f()};
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored value using the given function only if the result
     * contains an OK value. If the result contains an error, the same error is
     * copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<U, E> Mapped result, or previous error
     */
    template <typename F, typename U = typename std::result_of<F()>::type>
    result<U, E> map(F f) && {
        if (this->is_ok()) {
            return {util::ok, f()};
        } else {
            return {util::err, this->get_err_impl()};
        }
    }

    /**
     * @brief Maps the stored error value using the given function only if the
     * result contains an error value. If the result contains an OK value, the
     * same value is copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<void, R> Mapped error, or previous value
     */
    template <typename F,
              typename R = typename std::result_of<F(const E&)>::type>
    result<void, R> map_err(F f) const& {
        if (this->is_ok()) {
            return util::ok;
        } else {
            return {util::err, f(this->get_err_impl())};
        }
    }

    /**
     * @brief Maps the stored error value using the given function only if the
     * result contains an error value. If the result contains an OK value, the
     * same value is copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<void, R> Mapped error, or previous value
     */
    template <typename F, typename R = typename std::result_of<F(E&)>::type>
    result<void, R> map_err(F f) & {
        if (this->is_ok()) {
            return util::ok;
        } else {
            return {util::err, f(this->get_err_impl())};
        }
    }

    /**
     * @brief Maps the stored error value using the given function only if the
     * result contains an error value. If the result contains an OK value, the
     * same value is copied and returned.
     *
     * @tparam F Funcion type
     * @param f Mapping function
     * @return result<void, R> Mapped error, or previous value
     */
    template <typename F, typename R = typename std::result_of<F(E&&)>::type>
    result<void, R> map_err(F f) && {
        if (this->is_ok()) {
            return util::ok;
        } else {
            return {util::err, f(std::move(this->get_err_impl()))};
        }
    }
};

}  // namespace util

template <typename T, typename E>
std::ostream& operator<<(std::ostream& out, const util::result<T, E> res) {
    if (res.is_ok()) {
        out << res.ok();
    } else {
        out << res.err();
    }
    return out;
}

template <typename E>
std::ostream& operator<<(std::ostream& out, const util::result<void, E> res) {
    if (res.is_ok()) {
        out << "OK";
    } else {
        out << res.err();
    }
    return out;
}

#endif  // UTIL_RESULT_