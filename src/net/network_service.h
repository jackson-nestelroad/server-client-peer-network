#ifndef NET_NETWORK_SERVICE_
#define NET_NETWORK_SERVICE_

#include <net/error.h>
#include <util/result.h>
#include <util/thread_blocker.h>

#include <atomic>

namespace net {

/**
 * @brief A class that provides services and/or performs operations over a
 * network, such as a server or client.
 *
 */
class NetworkService {
   public:
    NetworkService(bool use_signals);

    /**
     * @brief Starts the service by calling `SetUp`, setting internal state, and
     * then calling `OnStart`.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> Start();

    /**
     * @brief Sets up any resources prior to starting.
     *
     * @return util::result<void, Error>
     */
    virtual util::result<void, Error> SetUp() = 0;

    /**
     * @brief Runs after the service starts.
     *
     * @return util::result<void, Error>
     */
    virtual util::result<void, Error> OnStart() = 0;

    /**
     * @brief Signals for the service to stop and cleans up all resources.
     *
     * Should only be called on the same thread that started the service.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> Stop();

    /**
     * @brief Runs when the service has been signalled to stop.
     *
     */
    virtual void OnStop();

    /**
     * @brief Blocks the current thread until the service stops.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> AwaitStop();

    /**
     * @brief Switches whether the service uses signals or not.
     *
     * @param use_signals
     */
    void SwitchSignalHandling(bool use_signals);

    /**
     * @brief Checks if the service is currently running.
     *
     * @return true
     * @return false
     */
    bool Running() const;

   protected:
    /**
     * @brief Signals the service should stop from another thread.
     *
     */
    void SignalStop();

    /**
     * @brief Attempts to clean up.
     *
     * If the service has already been cleaned up, then nothing will happen.
     *
     */
    util::result<void, Error> AttemptCleanUp();

    /**
     * @brief Cleans up any resources the service has used.
     *
     * @return util::result<void, Error>
     */
    virtual util::result<void, Error> CleanUp() = 0;

   private:
    bool use_signals_;
    bool running_;
    std::atomic<bool> needs_cleanup_;
    util::thread_blocker blocker_;
};

}  // namespace net

#endif  // NET_NETWORK_SERVICE_