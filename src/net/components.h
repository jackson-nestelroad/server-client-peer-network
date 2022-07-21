#ifndef NET_COMPONENTS_
#define NET_COMPONENTS_

#include <net/shared/temp_file_service.h>
#include <program/options.h>
#include <program/properties.h>
#include <thread/thread_pool.h>

namespace net {

/**
 * @brief Components of a client and/or server that can be passed down to other
 * classes for use.
 *
 * Make sure these objects are properly synchronized!
 *
 */
class Components {
   public:
    Components(const program::Options& options);

    program::Options options;
    program::Properties props;
    thread::ThreadPool thread_pool;
    shared::TempFileService temp_file_service;
};

}  // namespace net

#endif  // NET_COMPONENTS_