#ifndef NET_LOCATION_
#define NET_LOCATION_

#include <arpa/inet.h>
#include <net/error.h>
#include <util/result.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace net {

using address_t = ::in_addr_t;
using port_t = ::in_port_t;

/**
 * @brief The location of an upstream or downstream connection.
 *
 */
struct Location {
    static constexpr port_t kAnyPort = 0;

    Location(address_t address, port_t port = kAnyPort);

    bool operator==(const Location& rhs) const;
    bool operator!=(const Location& rhs) const;

    std::string HostName() const;

    static util::result<Location, Error> FromHostName(
        const std::string& hostname, port_t port = kAnyPort);

    /**
     * @brief Fetches the local machine's IP address.
     *
     * @return util::result<Location, Error>
     */
    static util::result<Location, Error> MyIpAddress();

    address_t address;
    port_t port;
};

}  // namespace net

namespace std {

template <>
struct hash<net::Location> {
    std::size_t operator()(const net::Location& target) const;
};

}  // namespace std

std::ostream& operator<<(std::ostream& out, const net::Location& location);

#endif  // NET_LOCATION_