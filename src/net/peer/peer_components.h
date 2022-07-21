#ifndef NET_PEER_PEER_COMPONENTS_
#define NET_PEER_PEER_COMPONENTS_

#include <net/components.h>
#include <net/peer/service/node_id_service.h>

namespace net {
namespace peer {

/**
 * @brief Components of a peer manager that can be passed down to other
 * classes for use.
 *
 * Make sure these objects are properly synchronized!
 *
 */
struct PeerComponents {
    PeerComponents(Components& common);

    Components& common;
    service::NodeIdService node_id_service;
};

}  // namespace peer
}  // namespace net

#endif  // NET_PEER_PEER_COMPONENTS_