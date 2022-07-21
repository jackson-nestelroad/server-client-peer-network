#ifndef NET_SERVER_ACCEPTOR_
#define NET_SERVER_ACCEPTOR_

#include <net/components.h>
#include <net/connectable_socket.h>
#include <net/network_service.h>
#include <util/optional.h>

#include <functional>

namespace net {
namespace server {

/**
 * @brief Class for accepting incoming connections over a listening port.
 *
 */
class Acceptor : public NetworkService {
   public:
    using accept_callback_t = std::function<void(int)>;

    Acceptor(Components& components, const accept_callback_t& on_accept);

    void SetPort(std::uint16_t port);
    std::uint16_t Port() const;

   private:
    /**
     * @brief Loop for accepting connections from the listener port.
     *
     */
    void AcceptConnections();

    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    util::result<void, Error> CleanUp() override;

    Components& components_;
    accept_callback_t on_accept_;
    std::uint16_t port_;
    ConnectableSocket listener_;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_ACCEPTOR_