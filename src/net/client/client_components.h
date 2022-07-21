#ifndef NET_CLIENT_CLIENT_COMPONENTS_
#define NET_CLIENT_CLIENT_COMPONENTS_

#include <net/client/service/connection_service.h>
#include <net/components.h>
#include <net/mutex/distributed_mutual_exclusion_service.h>

namespace net {
namespace client {

/**
 * @brief Components of a client that can be passed down to other classes for
 * use.
 *
 * Make sure these objects are properly synchronized!
 *
 */
class ClientComponents {
   public:
    ClientComponents(Components& components,
                     const typename mutex::DistributedMutualExclusionService::
                         ready_callback_t ready_callback,
                     const typename mutex::DistributedMutualExclusionService::
                         error_callback_t error_callback);

    Components& common;
    service::ConnectionService connection_service;
    mutex::DistributedMutualExclusionService distributed_mutex_service;
};

}  // namespace client
}  // namespace net

#endif  // NET_CLIENT_CLIENT_COMPONENTS_