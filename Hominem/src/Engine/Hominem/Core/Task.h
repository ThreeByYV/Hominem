#pragma once

#include "Hominem/Core/Core.h"
#include "Hominem/Core/Timestep.h"

#include <coroutine>

namespace Hominem {

// A Task is a "fire and forget" coroutine: calling the function starts it running
// immediately, and it cleans itself up when it finishes. Nobody holds on to the
// returned Task, so if the coroutine needs to hand back a result, write it into a
// member variable instead of returning it.
struct Task
{
    struct promise_type
    {
        Task get_return_object() noexcept { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend()   noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { HMN_CORE_ASSERT(false, "Task: unhandled exception in coroutine"); }
    };
};

// What co_await WaitSeconds(t) actually awaits. CoroutineScheduler holds the pending
// ones and wakes each up once its time is up.
struct WaitSecondsAwaiter
{
    float seconds;

    bool await_ready() const noexcept { return seconds <= 0.f; }
    void await_suspend(std::coroutine_handle<> handle) const;
    void await_resume() const noexcept {}
};

// co_await WaitSeconds(2.5f) pauses a Task for 2.5 seconds of game time.
inline WaitSecondsAwaiter WaitSeconds(float seconds) { return { seconds }; }

namespace CoroutineScheduler {

// Ticks every pending WaitSeconds() wait and resumes the ones whose time is up.
// Called once per frame from Application::Run, on the main thread.
void Update(Timestep ts);

}

}
