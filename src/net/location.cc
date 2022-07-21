#include "location.h"

#include <netdb.h>
#include <unistd.h>

#include <cstring>

namespace net {

decltype(Location::kAnyPort) constexpr Location::kAnyPort;

Location::Location(address_t address, port_t port)
    : address(address), port(port) {}

bool Location::operator==(const Location& rhs) const {
    return address == rhs.address &&
           ((port == kAnyPort) || (rhs.port == kAnyPort) || (port == rhs.port));
}

bool Location::operator!=(const Location& rhs) const { return !(*this == rhs); }

std::string Location::HostName() const {
    return ::inet_ntoa(::in_addr{address});
}

util::result<Location, Error> Location::FromHostName(
    const std::string& hostname, port_t port) {
    hostent* host_entry = ::gethostbyname(hostname.data());
    if (!host_entry) {
        return Error::Create("Host does not exist");
    }

    address_t address;
    std::memmove(&address, host_entry->h_addr, host_entry->h_length);

    return Location{address, port};
}

util::result<Location, Error> Location::MyIpAddress() {
    char hostname[256];
    if (::gethostname(hostname, sizeof(hostname)) < 0) {
        return Error::Create("Failed to get machine host name");
    }

    // Google's DNS server IP address.
    static constexpr const char kTargetName[] = "8.8.8.8";
    static constexpr const char kTargetPort[] = "53";

    ::addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ::addrinfo* info;
    int res = ::getaddrinfo(kTargetName, kTargetPort, &hints, &info);
    if (res != 0) {
        return Error::Create("Failed to get address info for DNS server");
    }

    if (info->ai_family == AF_INET6) {
        return Error::Create("Program does not support IPv6 yet");
    }

    int sockfd =
        ::socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (sockfd <= 0) {
        return Error::Create("Failed to create socket to DNS server");
    }

    res = ::connect(sockfd, info->ai_addr, info->ai_addrlen);
    if (res < 0) {
        return Error::Create("Failed to connect to DNS sever");
    }

    ::sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    res = ::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local_addr),
                        &addr_len);
    if (res < 0) {
        return Error::Create("Failed to get socket name");
    }

    close(sockfd);

    return Location(local_addr.sin_addr.s_addr, kAnyPort);
}

}  // namespace net

namespace std {
std::size_t hash<net::Location>::operator()(const net::Location& target) const {
    return (hash<net::address_t>()(target.address) ^
            (hash<net::port_t>()(target.port) << 1)) >>
           1;
}
}  // namespace std

std::ostream& operator<<(std::ostream& out, const net::Location& location) {
    out << location.HostName() << ':' << location.port;
    return out;
}