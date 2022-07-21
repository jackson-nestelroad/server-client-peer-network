#ifndef NET_ERROR_
#define NET_ERROR_

#include <util/error.h>

namespace net {
/**
 * @brief Error data structure for errors in the network library.
 *
 */
class Error : public util::error {
   public:
    Error(int code, const std::string& message);

    inline int code() { return code_; }

    /**
     * @brief Creates an error from the current value in `errno`.
     *
     * @param message
     * @return Error
     */
    static Error CreateFromErrNo(const std::string& message);

    /**
     * @brief Creates an error with the given message.
     *
     * @param message
     * @return Error
     */
    static Error Create(const std::string& message);

   private:
    int code_;
};

}  // namespace net

#endif  // NET_ERROR_