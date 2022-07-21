#ifndef NET_CONNECTION_
#define NET_CONNECTION_

#include <net/components.h>
#include <net/socket.h>
#include <util/identifiable.h>

#include <memory>

namespace net {

struct Connection : public util::identifiable<std::size_t>,
                    public std::enable_shared_from_this<Connection> {
    Connection(Socket&& socket);
    Connection(Connection&& other) = default;

    Socket socket;
};

}  // namespace net

#endif  // NET_CONNECTION_