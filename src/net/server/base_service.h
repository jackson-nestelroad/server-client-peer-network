#ifndef NET_SERVER_BASE_SERVICE_
#define NET_SERVER_BASE_SERVICE_

#include <net/connection.h>

#include <functional>

namespace net {
namespace server {

struct ServerComponents;
struct BaseConnectionHandler;

/**
 * @brief The base class for a service which handles a client connection to
 * the server.
 *
 */
class BaseService {
   public:
    ~BaseService();

    /**
     * @brief Starts the service.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> Start();

    /**
     * @brief Stops the service by stopping the client connection and deleting
     * it.
     *
     * If overriding, be sure to call the base method to assure the service is
     * properly deleted on the owner object.
     *
     */
    virtual void Stop();

    /**
     * @brief Schedules for the next `Run()` on the thread pool.
     *
     * Promotes concurrency and prevents infinite recursion.
     *
     */
    void ScheduleRun();

   protected:
    BaseService(ServerComponents& components, Connection& client,
                BaseConnectionHandler& owner);

    /**
     * @brief Method for initializing the service.
     *
     * Called before the first call to `Run()`.
     *
     * @return util::result<void, Error>
     */
    virtual util::result<void, Error> Initialize();

   private:
    /**
     * @brief Method for running the service in the current state.
     *
     */
    virtual void Run();

   protected:
    ServerComponents& components_;
    Connection& client_;

   private:
    BaseConnectionHandler& owner_;
};

}  // namespace server
}  // namespace net

#endif  // NET_SERVER_BASE_SERVICE_