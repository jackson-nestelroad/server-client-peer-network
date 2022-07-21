#include <mutex>

#define CRITICAL_SECTION(name, code)              \
    {                                             \
        std::lock_guard<std::mutex> __lock(name); \
        code;                                     \
    }

#define CRITICAL_SECTION_SAME_SCOPE(name, code)        \
    std::unique_lock<std::mutex> __##name__lock(name); \
    code;                                              \
    __##name__lock.unlock();