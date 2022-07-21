#include "error.h"

#include <errno.h>
#include <string.h>

namespace net {

Error::Error(int code, const std::string& message)
    : code_(code), util::error(message) {}

Error Error::CreateFromErrNo(const std::string& message) {
    return Error(errno, message + ": " + ::strerror(errno));
}

Error Error::Create(const std::string& message) { return Error(0, message); }

}  // namespace net