#include "hmnpch.h"
#include "Task.h"

#include <vector>

namespace Hominem {

namespace {

struct PendingWait
{
    float                   remaining;
    std::coroutine_handle<> handle;
};

std::vector<PendingWait> s_PendingWaits;

}

void WaitSecondsAwaiter::await_suspend(std::coroutine_handle<> handle) const
{
    s_PendingWaits.push_back({ seconds, handle });
}

namespace CoroutineScheduler {

void Update(Timestep ts)
{
    // Figure out which waits are done first, then resume them afterwards. Resuming
    // one can immediately hit its next WaitSeconds() and push a new entry onto
    // s_PendingWaits, which would corrupt an iteration still in progress.
    std::vector<std::coroutine_handle<>> ready;
    for (auto it = s_PendingWaits.begin(); it != s_PendingWaits.end(); )
    {
        it->remaining -= ts.GetSeconds();
        if (it->remaining <= 0.f)
        {
            ready.push_back(it->handle);
            it = s_PendingWaits.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto handle : ready)
        handle.resume();
}

}

}
