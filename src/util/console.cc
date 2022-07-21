#include "console.h"

namespace util {

const char* get_log_prefix(log_type type) {
    switch (type) {
#define CREATE_LOG_SWITCH_CASE(name, prefix, stream, debug_only) \
    case log_type::name:                                         \
        return prefix;
        LOG_INTERFACES(CREATE_LOG_SWITCH_CASE)
#undef CREATE_LOG_SWITCH_CASE
    }
    return "NO NAME";
}

const manip_t manip::endl = static_cast<manip_t>(std::endl);
const manip_t manip::ends = static_cast<manip_t>(std::ends);
const manip_t manip::flush = static_cast<manip_t>(std::flush);

}  // namespace util