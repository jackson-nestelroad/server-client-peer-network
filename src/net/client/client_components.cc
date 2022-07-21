#include "client_components.h"

namespace net {
namespace client {

ClientComponents::ClientComponents(
    Components& common,
    const typename mutex::DistributedMutualExclusionService::ready_callback_t
        ready_callback,
    const typename mutex::DistributedMutualExclusionService::error_callback_t
        error_callback)
    : common(common),
      connection_service(common),
      distributed_mutex_service(*this, ready_callback, error_callback) {}

}  // namespace client
}  // namespace net