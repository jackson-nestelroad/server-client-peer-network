#include "node_id_service.h"

#include <util/console.h>
#include <util/mutex.h>

namespace net {
namespace peer {
namespace service {

util::optional<Location> NodeIdService::GetLocationById(
    proto::node_id_t id) const {
    auto it = id_to_entry_.find(id);
    if (it == id_to_entry_.end()) {
        return util::none;
    }
    return it->second->location;
}

util::optional<proto::node_id_t> NodeIdService::GetIdByLocation(
    const Location& location) const {
    auto it = location_to_entry_.find(location);
    if (it == location_to_entry_.end()) {
        return util::none;
    }
    return it->second->id;
}

util::result<void, Error> NodeIdService::Add(const Location& location,
                                             proto::node_id_t id) {
    auto it = id_to_entry_.find(id);
    if (it != id_to_entry_.end()) {
        // This ID has already been used.
        Location& previous_location = it->second->location;
        if (previous_location != location) {
            // This ID has already been used at a different location!
            // This means two nodes are attempting to use the same ID.
            return Error::Create(
                util::string::stream("Node with ID", id, "is already in use"));
        }

        // This ID has already been used at the same location.
        return util::ok;
    }

    // This ID has not already been used, so insert it.
    CRITICAL_SECTION(mutex_, {
        auto new_it = entries_list_.emplace(entries_list_.end(),
                                            NodeIdEntry{id, location});
        id_to_entry_.emplace(id, new_it);
        location_to_entry_.emplace(location, new_it);
        return util::ok;
    });
}

void NodeIdService::Remove(const Location& location) {
    CRITICAL_SECTION(mutex_, {
        auto it = location_to_entry_.find(location);
        if (it == location_to_entry_.end()) {
            return;
        }

        location_to_entry_.erase(it->second->location);
        id_to_entry_.erase(it->second->id);
        entries_list_.erase(it->second);
    });
}

void NodeIdService::Remove(proto::node_id_t id) {
    CRITICAL_SECTION(mutex_, {
        auto it = id_to_entry_.find(id);
        if (it == id_to_entry_.end()) {
            return;
        }

        location_to_entry_.erase(it->second->location);
        id_to_entry_.erase(it->second->id);
        entries_list_.erase(it->second);
    });
}

}  // namespace service
}  // namespace peer
}  // namespace net