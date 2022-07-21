#ifndef NET_PEER_PEER_CONNECTION_
#define NET_PEER_PEER_CONNECTION_

#include <net/connection.h>
#include <net/location.h>
#include <net/proto/messages.h>

#include <string>

namespace net {
namespace peer {

/**
 * @brief A connection to another peer.
 *
 * The `in` connection was established by the other peer. The `out`
 * connection was established by this peer.
 *
 */
struct PeerConnection {
    Location location;
    proto::node_id_t id;
    std::shared_ptr<Connection> in;
    std::shared_ptr<Connection> out;
};

/**
 * @brief A reference to a peer connection and the underlying connections
 * inside of it.
 *
 */
struct PeerConnectionReference {
    proto::node_id_t id;
    Connection& in;
    Connection& out;
};

}  // namespace peer
}  // namespace net

#endif  // NET_PEER_PEER_CONNECTION_