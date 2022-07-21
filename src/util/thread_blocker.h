#ifndef UTIL_THREAD_BLOCKER_
#define UTIL_THREAD_BLOCKER_

#include <condition_variable>
#include <mutex>
#include <thread>

namespace util {

/**
 * @brief Utility class for blocking the current thread.
 *
 */
class thread_blocker {
   public:
    /**
     * @brief Blocks the current thread.
     *
     */
    void block();

    /**
     * @brief Unblocks the current thread.
     *
     */
    void unblock();

   private:
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace util

#endif  // UTIL_THREAD_BLOCKER_