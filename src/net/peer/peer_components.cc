#include "peer_components.h"

namespace net {
namespace peer {

PeerComponents::PeerComponents(Components& common)
    : common(common), node_id_service() {}

}  // namespace peer
}  // namespace net