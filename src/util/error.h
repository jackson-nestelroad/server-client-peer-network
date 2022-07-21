#ifndef PROGRAM_ERROR_
#define PROGRAM_ERROR_

#include <iostream>
#include <string>

namespace util {

/**
 * @brief Error data structure for reporting what error occurred.
 *
 */
class error {
   public:
    error(const std::string& message);

    inline const std::string& what() const& { return message_; }
    inline std::string& what() & { return message_; }
    inline std::string&& what() && { return std::move(message_); }

   private:
    std::string message_;
};

/**
 * @brief Reports the error as fatal and terminates the program.
 *
 * The program is terminated by throwing an exception to unwind the stack.
 *
 * @param error
 */
[[noreturn]] void fatal_error(const error& err);

}  // namespace util

std::ostream& operator<<(std::ostream& strm, const util::error& err);

#define EXIT_IF_ERROR(result_op)               \
    {                                          \
        auto result = result_op;               \
        if (result.is_err()) {                 \
            ::util::fatal_error(result.err()); \
        }                                      \
    }

#endif  // PROGRAM_ERROR_