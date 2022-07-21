#include "thread_pool.h"

#include <util/console.h>
#include <util/mutex.h>

namespace thread {

ThreadPool::ThreadPool(std::size_t num_threads) : num_threads_(num_threads) {}

ThreadPool::~ThreadPool() {
    if (running_) {
        Stop();
    }
}

void ThreadPool::Start() {
    util::safe_debug::log("Starting thread pool of", num_threads_, "threads");
    threads_.resize(num_threads_);
    running_ = true;
    for (auto& thread : threads_) {
        thread = std::thread([this] { ThreadLoop(); });
    }
}

void ThreadPool::Stop() {
    CRITICAL_SECTION(stop_mutex_, {
        if (running_) {
            util::safe_debug::log("Stopping thread pool");
            running_ = false;
            cv_.notify_all();
            for (auto& thread : threads_) {
                thread.join();
            }
            threads_.clear();
            util::safe_debug::log("All threads have stopped");
        }
    });
}

void ThreadPool::Schedule(const Job& job) {
    CRITICAL_SECTION(jobs_mutex_, jobs_.push(job));
    cv_.notify_one();
}

void ThreadPool::ThreadLoop() {
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(jobs_mutex_);
            cv_.wait(lock, [this] { return !jobs_.empty() || !running_; });
            if (!running_) {
                return;
            }
            job = jobs_.front();
            jobs_.pop();
        }
        job();
    }
}

}  // namespace thread