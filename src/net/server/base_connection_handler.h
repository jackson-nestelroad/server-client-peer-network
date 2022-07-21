#ifndef NET_SERVER_BASE_CONNECTION_HANDLER_
#define NET_SERVER_BASE_CONNECTION_HANDLER_

#include <net/connection.h>
#include <net/server/base_service.h>

#include <functional>
#include <memory>
#include <type_traits>

namespace net {
namespace server {

struct ServerComponents;

/**
 * @brief Base class for handling a single connection by submitting it to a
 * service.
 *
 */
class BaseConnectionHandler {
   public:
    using stop_func_t = std::function<void()>;

    BaseConnectionHandler(std::shared_ptr<Connection> client,
                          ServerComponents& components);

    /**
     * @brief Starts handling the connection.
     *
     * @param on_finished Callback to call when connection is finished or
     * stopped.
     */
    virtual void Start(const stop_func_t& on_finished) final;

    /**
     * @brief Stops the connection.
     *
     */
    virtual void Stop() final;

    virtual Connection& Client() final;

    template <typename T>
    typename std::enable_if<std::is_base_of<BaseService, T>::value, void>::type
    SwitchService() {
        current_service_.reset(new T(components_, client_));
        current_service_->Start();
    }

   protected:
    virtual std::unique_ptr<BaseService> FirstService(
        ServerComponents& components, Connection& client) = 0;

    ServerComponents& components_;

   private:
    std::shared_ptr<Connection> client_;
    stop_func_t on_finished_;
    std::unique_ptr<BaseService> current_service_;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_BASE_CONNECTION_HANDLER_