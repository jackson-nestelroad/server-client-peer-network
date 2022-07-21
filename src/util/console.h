#ifndef UTIL_CONSOLE_
#define UTIL_CONSOLE_

#include <util/mutex.h>

#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#define LOG_INTERFACES(X)                         \
    X(console, "[CONSOLE]", ::std::cout, false)   \
    X(error_log, "  [ERROR]", ::std::cout, false) \
    X(debug, "  [DEBUG]", ::std::cout, true)

namespace util {

/**
 * @brief Type for stream manipulators.
 *
 */
using manip_t = std::ostream& (*)(std::ostream&);

/**
 * @brief Interface to function template stream manipulators to pass to stream
 * interfaces.
 *
 * These manipulators are typically templates, so they must instantiated with
 * the std::ostream type to be used in variadic template functions.
 *
 */
struct manip {
    static const manip_t endl;
    static const manip_t ends;
    static const manip_t flush;
};

/**
 * @brief Mutex for guarding access to a single stream pointer.
 *
 * Stream pointer is a compile-time constant to allow multiple logs that point
 * to the same sream to be synchronized.
 *
 * @tparam strm Output stream pointer
 */
template <std::ostream* strm>
class logging_mutex {
   protected:
    static std::mutex log_mutex;

   public:
    static std::lock_guard<std::mutex> acquire_lock_guard() {
        logging_mutex<strm>::log_mutex.lock();
        return {logging_mutex<strm>::log_mutex, std::adopt_lock};
    }
};

/**
 * @brief Mutex for safe access to the output stream pointer.
 *
 * @tparam strm Output stream pointer
 */
template <std::ostream* strm>
std::mutex logging_mutex<strm>::log_mutex;

/**
 * @brief Enum for log type.
 *
 */
enum class log_type {
#define CREATE_LOG_ENUM(name, prefix, stream, debug_only) name,
    LOG_INTERFACES(CREATE_LOG_ENUM)
#undef CREATE_LOG_ENUM
};

/**
 * @brief Returns the log prefix for the given log type.
 *
 * @param type Log type
 * @return const char* Log prefix
 */
const char* get_log_prefix(log_type type);

namespace console_detail {

template <std::ostream* strm>
class stream_template {
   public:
    /**
     * @brief Native stream pointer.
     *
     */
    static constexpr std::ostream* native = strm;
};

/**
 * @brief Base stream that provides a flexible API for streaming and logging
 * different types to an output stream.
 *
 * `base_stream::log` logs multiple values by placing spaces between them and
 * automatically placing a newline at the end.
 *
 * `base_stream::stream` streams multiple values with no additional whitespace.
 *
 * @tparam strm Output stream pointer
 */
template <std::ostream* strm>
class base_stream : public stream_template<strm> {
   private:
    base_stream() = delete;
    ~base_stream() = delete;

   public:
    inline static void log(void) { *strm << std::endl; }

    template <typename T>
    static void log(const T& t) {
        *strm << t << std::endl;
    }

    template <typename T, typename... Ts>
    static void log(const T& t, const Ts&... ts) {
        *strm << t << ' ';
        log(ts...);
    }

    inline static void stream(void) { strm->flush(); }

    template <typename T, typename... Ts>
    static void stream(const T& t, const Ts&... ts) {
        *strm << t;
        stream(ts...);
    }

    template <typename... Ts>
    static void stream(const manip_t& manip, const Ts&... ts) {
        (*manip)(*strm);
        stream(ts...);
    }
};

/**
 * @brief Log stream that prefixes every log statement with a timestamp and log
 * type.
 *
 * @tparam strm Output stream pointer
 * @tparam type Log ype
 */
template <std::ostream* strm, log_type type>
class log_stream : public stream_template<strm> {
   private:
    log_stream() = delete;
    ~log_stream() = delete;

    static void print_prefix() {
        time_t timer;
        tm* tmp;
        ::time(&timer);
        tmp = ::localtime(&timer);
        char buffer[30];
        int res =
            ::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.%z%Z", tmp);
        if (res >= 0) {
            *strm << '[' << buffer << "] ";
        }
        *strm << get_log_prefix(type) << " --- ";
    }

   public:
    template <typename... Ts>
    static void log(const Ts&... ts) {
        print_prefix();
        base_stream<strm>::log(ts...);
    }

    template <typename... Ts>
    static void stream(const Ts&... ts) {
        print_prefix();
        base_stream<strm>::stream(ts...);
    }

    template <typename... Ts>
    static void stream(const manip& manip, const Ts&... ts) {
        print_prefix();
        base_stream<strm>::stream(manip, ts...);
    }
};

/**
 * @brief Similar to `base_stream`, except every statement is guarded by a
 * shared mutex.
 *
 * @tparam strm Output stream pointer
 */
template <std::ostream* strm>
class mutex_guarded_base_stream : public stream_template<strm> {
   private:
    mutex_guarded_base_stream() = delete;
    ~mutex_guarded_base_stream() = delete;

   public:
    template <typename... Ts>
    static void log(const Ts&... ts) {
        auto&& lock = logging_mutex<strm>::acquire_lock_guard();
        base_stream<strm>::log(ts...);
    }

    template <typename... Ts>
    static void stream(const Ts&... ts) {
        auto&& lock = logging_mutex<strm>::acquire_lock_guard();
        base_stream<strm>::stream(ts...);
    }

    template <typename... Ts>
    static void stream(const manip_t& manip, const Ts&... ts) {
        auto&& lock = logging_mutex<strm>::acquire_lock_guard();
        base_stream<strm>::stream(manip, ts...);
    }
};

/**
 * @brief Similar to `log_stream`, except every statement is guarded by a
 * shared mutex.
 *
 * @tparam strm Output stream pointer
 */
template <std::ostream* strm, log_type type>
class mutex_guarded_log_stream : public stream_template<strm> {
   private:
    mutex_guarded_log_stream() = delete;
    ~mutex_guarded_log_stream() = delete;

   public:
    template <typename... Ts>
    static void log(const Ts&... ts) {
        auto&& lock = logging_mutex<strm>::acquire_lock_guard();
        log_stream<strm, type>::log(ts...);
    }

    template <typename... Ts>
    static void stream(const Ts&... ts) {
        auto&& lock = logging_mutex<strm>::acquire_lock_guard();
        log_stream<strm, type>::stream(ts...);
    }

    template <typename... Ts>
    static void stream(const manip_t& manip, const Ts&... ts) {
        auto&& lock = logging_mutex<strm>::acquire_lock_guard();
        log_stream<strm, type>::stream(manip, ts...);
    }
};

/**
 * @brief An object with the same API as `base_stream` that provides an empty
 * operation for every log or stream statement.
 *
 */
class nullopt_stream {
   private:
    nullopt_stream() = delete;
    ~nullopt_stream() = delete;

   public:
    template <typename... Ts>
    constexpr static void log(const Ts&... ts) {}

    template <typename... Ts>
    constexpr static void stream(const Ts&... ts) {}

    template <typename... Ts>
    constexpr static void stream(const manip_t& manip, const Ts&... ts) {}
};

}  // namespace console_detail

#ifndef NDEBUG

#define CREATE_LOG_CLASS(name, prefix, stream)                        \
    using name = console_detail::log_stream<&stream, log_type::name>; \
    using safe_##name =                                               \
        console_detail::mutex_guarded_log_stream<&stream, log_type::name>;

#define CREATE_LOG_CLASSES(name, prefix, stream, debug_only) \
    CREATE_LOG_CLASS(name, prefix, stream)

#define CREATE_STREAM_CLASS(name, prefix, stream)      \
    using name = console_detail::base_stream<&stream>; \
    using safe_##name = console_detail::mutex_guarded_base_stream<&stream>;

#define CREATE_STREAM_CLASSES(name, prefix, stream, debug_only) \
    CREATE_STREAM_CLASS(name, prefix, stream)

#else  // NDEBUG

#define CREATE_LOG_CLASS_true(name, prefix, stream) \
    using name = console_detail::nullopt_stream;    \
    using safe_##name = console_detail::nullopt_stream;
#define CREATE_LOG_CLASS_false(name, prefix, stream)                  \
    using name = console_detail::log_stream<&stream, log_type::name>; \
    using safe_##name =                                               \
        console_detail::mutex_guarded_log_stream<&stream, log_type::name>;

#define CREATE_LOG_CLASSES(name, prefix, stream, debug_only) \
    CREATE_LOG_CLASS_##debug_only(name, prefix, stream)

#define CREATE_STREAM_CLASS_true(name, prefix, stream) \
    using name = console_detail::nullopt_stream;       \
    using safe_##name = console_detail::nullopt_stream;
#define CREATE_STREAM_CLASS_false(name, prefix, stream) \
    using name = console_detail::base_stream<&stream>;  \
    using safe_##name = console_detail::mutex_guarded_base_stream<&stream>;

#define CREATE_STREAM_CLASSES(name, prefix, stream, debug_only) \
    CREATE_STREAM_CLASS_##debug_only(name, prefix, stream)

#endif  // NDEBUG

LOG_INTERFACES(CREATE_LOG_CLASSES)

// Provides access to the streaming API without log prefixes.
namespace nolog {
LOG_INTERFACES(CREATE_STREAM_CLASSES)
}

#ifndef NDEBUG
#undef CREATE_LOG_CLASSES
#undef CREATE_LOG_CLASS
#undef CREATE_STREAM_CLASSES
#undef CREATE_STREAM_CLASS
#else
#undef CREATE_LOG_CLASSES
#undef CREATE_LOG_CLASSES_true
#undef CREATE_LOG_CLASSES_false
#undef CREATE_STREAM_CLASSES
#undef CREATE_STREAM_CLASSES_true
#undef CREATE_STREAM_CLASSES_false
#endif

/**
 * @brief Thin wrapper for string concatenation using `std::stringstream` under
 * the same interface as `util::console`.
 *
 */
class string {
   private:
    string() = delete;
    ~string() = delete;

    inline static std::string stream_impl(std::stringstream& str) {
        return str.str();
    }

    template <typename T, typename... Ts>
    static std::string stream_impl(std::stringstream& str, const T& t,
                                   const Ts&... ts) {
        str << t;
        return stream_impl(str, ts...);
    }

   public:
    inline static std::string stream(void) { return ""; }

    template <typename T, typename... Ts>
    static std::string stream(const T& t, const Ts&... ts) {
        std::stringstream str;
        return stream_impl(str, t, ts...);
    }
};

}  // namespace util

#endif  // UTIL_CONSOLE_