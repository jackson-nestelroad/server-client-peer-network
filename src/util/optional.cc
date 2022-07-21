#include "optional.h"

namespace util {

std::ostream& operator<<(std::ostream& out, util::none_t) {
    out << "none";
    return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const util::optional<T> opt) {
    if (opt.has_value()) {
        out << opt.value();
    } else {
        out << util::none;
    }
    return out;
}

}  // namespace util