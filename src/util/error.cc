#include "error.h"

#include <util/console.h>

namespace util {

error::error(const std::string& message) : message_(message) {}

void fatal_error(const error& err) {
    util::error_log::log(err.what());
    throw 1;
}

}  // namespace util

std::ostream& operator<<(std::ostream& strm, const util::error& err) {
    strm << err.what();
    return strm;
}