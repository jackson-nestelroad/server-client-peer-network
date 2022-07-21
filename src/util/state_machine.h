#ifndef UTIL_STATE_MACHINE_
#define UTIL_STATE_MACHINE_

#include <util/error.h>
#include <util/mutex.h>
#include <util/result.h>
#include <util/singleton.h>
#include <util/thread_blocker.h>

#include <functional>
#include <mutex>
#include <type_traits>

#define __STATE_METHODS(T, name)                                              \
    void handle(T& instance, const ::util::sm_callback_t& callback) override; \
    typename ::util::state_machine<T>::state& next_state(T& instance) override;

#define __STATE_SHOULD_STOP bool should_stop() override;

#define __STATE_STRUCT_BEGIN(T, name, base)              \
    struct name : public ::util::state_machine<T>::base, \
                  public ::util::singleton<name> {
#define __STATE_STRUCT_END \
    }                      \
    ;

#define DEFINE_STATE(T, name)                 \
    __STATE_STRUCT_BEGIN(T, name, sync_state) \
    __STATE_METHODS(T, name)                  \
    __STATE_STRUCT_END

#define DEFINE_STOP_STATE(T, name)            \
    __STATE_STRUCT_BEGIN(T, name, sync_state) \
    __STATE_METHODS(T, name)                  \
    __STATE_SHOULD_STOP                       \
    __STATE_STRUCT_END

#define DEFINE_ASYNC_STATE(T, name)            \
    __STATE_STRUCT_BEGIN(T, name, async_state) \
    __STATE_METHODS(T, name)                   \
    __STATE_STRUCT_END

#define DEFINE_ASYNC_STOP_STATE(T, name)       \
    __STATE_STRUCT_BEGIN(T, name, async_state) \
    __STATE_METHODS(T, name)                   \
    __STATE_SHOULD_STOP                        \
    __STATE_STRUCT_END

#define IMPL_STATE_HANDLER(T, name) \
    void name::handle(T& instance, const ::util::sm_callback_t& callback)

#define IMPL_NEXT_STATE(T, name, next)                                        \
    typename ::util::state_machine<T>::state& name::next_state(T& instance) { \
        return next::instance();                                              \
    }

#define IMPL_STATE_HANDLER_SETS_NEXT_STATE(T, name) \
    IMPL_NEXT_STATE(T, name, name)

#define IMPL_STOP_STATE_SHOULD_STOP(name) \
    bool name::should_stop() { return true; }

namespace util {

/**
 * @brief Enumeration of the types of states for a state machine.
 *
 */
enum state_type_t {
    /**
     * @brief Synchronous, which means the state completes on the same
     * thread it started on.
     *
     */
    sync,
    /**
     * @brief Asynchronous, which means the state does not necessarily
     * complete on the same thread it started on.
     *
     */
    async,
};

using sm_callback_t = std::function<void(result<void, error>)>;

namespace state_machine_detail {

/**
 * @brief Base for a state machine with potentially asynchronous operations.
 *
 * @tparam T Instance type that is passed to state handlers.
 */
template <typename T>
class base_state_machine {
   public:
    /**
     * @brief A single state.
     *
     * Defines a handler (action for the current state) and a next state.
     *
     * May be synchronous or asynchronous. Deriving classes should use
     * `sync_state` or `async_state` respectively. Use `DEFINE_STATE` or
     * `DEFINE_ASYNC_STATE` to define the appropriate header for a new
     * state.
     *
     */
    struct state {
        /**
         * @brief Signals to the state machine if the machine should stop in
         * this state.
         *
         * Default is false.
         *
         * @return true
         * @return false
         */
        virtual bool should_stop() { return false; }

        /**
         * @brief Handles the current state.
         *
         * @param instance Machine instance
         * @param callback Callback for when handler finishes
         */
        virtual void handle(T& instance, const sm_callback_t& callback) = 0;

        /**
         * @brief Defines the next state.
         *
         * @param instance Machine instance
         * @return state&
         */
        virtual state& next_state(T& instance) = 0;

       protected:
        state(state_type_t type) : type_(type) {}

       private:
        // TODO: Rather than defining a variable here, it is better to use C++17
        // variants here to define separate synchronous and asynchronous classes
        // that can be stored in one memory location.
        //
        // Then state handlers may have different parameters, such as
        // synchronous states not having a callback.
        state_type_t type_;

        friend class base_state_machine<T>;
    };

    /**
     * @brief A synchronous state.
     *
     * Do not derive from directly. Use `DEFINE_STATE(state_name,
     * machine_type)`.
     *
     */
    struct sync_state : public state {
        sync_state() : state(sync) {}
    };

    /**
     * @brief An asynchronous state.
     *
     * Do not derive from directly. Use `DEFINE_ASYNC_STATE(state_name,
     * machine_type)`.
     */
    struct async_state : public state {
        async_state() : state(async) {}
    };

    /**
     * @brief Starts the state machine.
     *
     * When finished, the given callback is called.
     *
     * @param callback Callback when machine finishes
     */
    void start(const optional<sm_callback_t>& callback) {
        running_ = true;
        result_of_last_run_ = ok;
        run_callback_ = callback;
        run();
    }

    /**
     * @brief Forces the machine to stop with the given result.
     *
     * @param result
     */
    void stop_with_result(const result<void, error>& result) {
        result_of_last_run_ = result;
        stop();
    }

    /**
     * @brief Forces the machine to stop as soon as it can.
     *
     */
    void stop() {
        CRITICAL_SECTION(stop_mutex_, {
            if (!running_) {
                return;
            }
            running_ = false;
        });
        if (run_callback_.has_value()) {
            run_callback_.value()(result_of_last_run_);
        }
        blocker_.unblock();
    }

    /**
     * @brief Waits for the server to stop by blocking the current thread.
     *
     */
    void await_stop() {
        if (running_) {
            blocker_.block();
        }
    }

    /**
     * @brief Directly sets the next state for the machine, regardless of
     * what the current state handler says.
     *
     * If state handlers receive access to this machine method from the
     * `instance` parameter, then states can pick their next state according to
     * conditions in their handler. This can make logic more complicated to
     * follow, but it allows much more flexibility in moving across states.
     *
     * @param next_state
     */
    void set_next_state(state& next_state) { forced_next_state_ = &next_state; }

   protected:
    base_state_machine(T& instance, state& initial_state)
        : running_(false),
          instance_(instance),
          current_state_(&initial_state) {}

   private:
    /**
     * @brief Runs the state machine until it stops.
     *
     * If all state are guaranteed to be synchronous, then this method will
     * long as run as the state machine. If one asynchronous state is
     * reached, this method will return, and the asynchronous state will
     * resume tbe machine as soon as it is finished and calls its given
     * callback.
     *
     * @param callback Callback when machine hits an error or stops
     */
    void run() {
        // Loop in this thread for as long as possible.
        // If a state machine only uses synchronous states, this prevents
        // recursion.
        while (true) {
            if (!running_) {
                stop();
                break;
            }

            forced_next_state_ = nullptr;

            state_type_t type = current_state_->type_;

            // C++ has no way to check if something is asynchronous ahead of
            // time, so we just have to take the implementer's word for it.
            if (type == async) {
                // Asynchronous state, so the callback will be used to
                // continue the machine.
                bool called = false;
                run_current_state(
                    [this, called](result<void, error> result) mutable {
                        // Extra security to make sure this callback is not run
                        // more than once.
                        static std::mutex mutex;
                        CRITICAL_SECTION(mutex, {
                            if (called) {
                                return;
                            }
                            called = true;
                        });

                        if (result.is_err()) {
                            stop_with_result(result);
                        } else if (current_state_->should_stop()) {
                            stop();
                        } else {
                            goto_next_state();
                            run();
                        }
                    });

                // We should not loop back. The callback given above will
                // run when the asynchronous state calls it, which calls run
                // again.
                break;

                // Scenarios that can cause undefined behavior:
                //
                // A state calls the callback more than once. The machine
                // will then have two run threads going, and two states will
                // execute at once.
                //
                // The state machine is deallocated prior to an asynchronous
                // state properly finishing. The implementer should add a
                // way to signal for asynchronous operations to cancel
                // immediately if they do not want to wait when stopping the
                // state machine. The `await_stop` method is given to
                // protect against this very scenario.
            } else {
                // Synchronous state, so the callback will be used only to
                // record the result of the operation.
                //
                // A synchronous state should use the callback to report a
                // result to the state machine.
                result<void, error> state_handler_result;
                run_current_state(
                    [&state_handler_result](result<void, error> result) {
                        state_handler_result = result;
                    });

                // At this point, the state handler returned, which means
                // the synchronous state is finished, so we are ready to
                // transition to the next state.
                //
                // If the callback was never called, we just assume the
                // result is OK.
                //
                // If the programmer lied and the state is actually
                // asynchronous, which means the callback is dispatched and
                // called later, then the callback exhibits undefined
                // behavior, because state_handler_result will be
                // deallocated.
                if (state_handler_result.is_err() ||
                    current_state_->should_stop()) {
                    // Hit an error, or we have stopped, so exit out.
                    stop_with_result(state_handler_result);
                    break;
                }

                // We should continue, so we go to the next state and
                // continue with another iteration of this loop!
                goto_next_state();
            }
        }
    }

    /**
     * @brief Moves the machine to the next state.
     *
     */
    void goto_next_state() {
        if (forced_next_state_) {
            current_state_ = forced_next_state_;
        } else {
            current_state_ = &current_state_->next_state(instance_);
        }
    }

    /**
     * @brief Runs the current state.
     *
     */
    void run_current_state(const sm_callback_t& callback) {
        current_state_->handle(instance_, callback);
    }

   private:
    std::mutex stop_mutex_;
    bool running_;
    optional<sm_callback_t> run_callback_;
    result<void, error> result_of_last_run_;
    state* forced_next_state_;
    T& instance_;
    state* current_state_;
    thread_blocker blocker_;
};

}  // namespace state_machine_detail

/**
 * @brief A state machine of synchronous and/or asynchronous states.
 *
 * Use `DEFINE_STATE(state_name, machine_type)` and
 * `DEFINE_ASYNC_STATE(state_name, machine_type)` to define states for the
 * machine. The constructor requires a default state to be given, which will
 * be used to start the machine.
 *
 * @tparam T
 */
template <typename T>
using state_machine = state_machine_detail::base_state_machine<T>;

}  // namespace util

#endif  // UTIL_STATE_MACHINE_