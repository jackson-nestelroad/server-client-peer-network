#ifndef THREAD_THREAD_POOL_
#define THREAD_THREAD_POOL_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace thread {

/**
 * @brief A pool of threads for submitting jobs to.
 *
 */
class ThreadPool {
   public:
    using Job = std::function<void()>;

    ThreadPool(std::size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    /**
     * @brief Starts all threads.
     *
     */
    void Start();

    /**
     * @brief Schedules a job to be run on some thread at some time.
     *
     * @param job
     */
    void Schedule(const Job& job);

    /**
     * @brief Stops all threads.
     *
     * Should NOT be called from one of the threads in the thread pool.
     *
     */
    void Stop();

    bool IsRunning() const;

   private:
    void ThreadLoop();

    std::size_t num_threads_;
    bool running_;
    std::mutex stop_mutex_;
    std::mutex jobs_mutex_;
    std::condition_variable cv_;
    std::queue<Job> jobs_;
    std::vector<std::thread> threads_;
};
}  // namespace thread

#endif  // THREAD_THREAD_POOL_