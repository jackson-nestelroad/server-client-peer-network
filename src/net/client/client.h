#ifndef NET_CLIENT_CLIENT_
#define NET_CLIENT_CLIENT_

#include <net/client/client_components.h>
#include <net/network_service.h>
#include <net/socket.h>
#include <util/optional.h>

#include <functional>

namespace net {
namespace client {

/**
 * @brief A class representing a single client that connects to a single server.
 *
 */
class Client : public NetworkService {
   public:
    using stop_callback_t = std::function<void()>;

    Client(bool use_signals, Components& components,
           util::optional<stop_callback_t> on_stop = util::none);

   protected:
    /**
     * @brief Abstract function to be implemented by deriving classes for
     * running the client after it has started.
     *
     */
    virtual void Run() = 0;

   private:
    util::result<void, Error> SetUp() override;
    util::result<void, Error> OnStart() override;
    void OnStop() override;
    util::result<void, Error> CleanUp() override;

    void OnPeerNetworkReady(util::result<void, Error> result);
    void OnPeerNetworkError(Error error);

   protected:
    ClientComponents components_;

   private:
    util::optional<stop_callback_t> on_stop_;
};

}  // namespace client
}  // namespace net

#endif  // NET_CLIENT_CLIENT_