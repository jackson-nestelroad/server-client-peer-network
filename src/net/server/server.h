#ifndef NET_SERVER_SERVER_
#define NET_SERVER_SERVER_

#include <net/network_service.h>
#include <net/server/acceptor.h>
#include <net/server/base_connection_handler_factory.h>
#include <net/server/server_components.h>
#include <thread/thread_pool.h>

namespace net {
namespace server {

/**
 * @brief A class representing a single server that receives connections over
 * the Internet.
 *
 */
class Server : public NetworkService {
   public:
    Server(bool use_signals, net::Components& components,
           std::unique_ptr<BaseConnectionHandlerFactory>&&
               connection_handler_factory);

    std::uint16_t Port() const;
    ServerComponents& Components();

   private:
    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    void OnStop() override;
    util::result<void, Error> CleanUp() override;

    void OnAccept(int sockfd);

    ServerComponents components_;
    Acceptor acceptor_;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_SERVER_