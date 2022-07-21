#ifndef NET_PEER_SERVICE_NODE_ID_SERVICE_
#define NET_PEER_SERVICE_NODE_ID_SERVICE_

#include <net/error.h>
#include <net/location.h>
#include <net/proto/messages.h>
#include <util/optional.h>
#include <util/result.h>

#include <list>
#include <mutex>
#include <unordered_map>

namespace net {
namespace peer {
namespace service {

/**
 * @brief Service for keeping track of nodes and IDs.
 *
 */
class NodeIdService {
   public:
    /**
     * @brief Gets a node's location by its ID.
     *
     * @param id
     * @return util::optional<Location>
     */
    util::optional<Location> GetLocationById(proto::node_id_t id) const;

    /**
     * @brief Gets a node's ID by its location.
     *
     * @param location
     * @return util::optional<proto::node_id_t>
     */
    util::optional<proto::node_id_t> GetIdByLocation(
        const Location& location) const;

    /**
     * @brief Adds a location and node ID pair to the service set.
     *
     * Once registered, an ID can only belong to one server location.
     *
     * @param location
     * @param id
     * @return util::result<void, Error>
     */
    util::result<void, Error> Add(const Location& location,
                                  proto::node_id_t id);

    /**
     * @brief Removes a node ID mapping by location.
     *
     * @param location
     */
    void Remove(const Location& location);

    /**
     * @brief Removes a node ID mapping by ID.
     *
     * @param id
     */
    void Remove(proto::node_id_t id);

   private:
    struct NodeIdEntry {
        proto::node_id_t id;
        Location location;
    };

    std::mutex mutex_;
    std::list<NodeIdEntry> entries_list_;

    using value_type = decltype(entries_list_)::iterator;

    std::unordered_map<proto::node_id_t, value_type> id_to_entry_;
    std::unordered_map<Location, value_type> location_to_entry_;
};

}  // namespace service
}  // namespace peer
}  // namespace net

#endif  // NET_PEER_SERVICE_NODE_ID_SERVICE_