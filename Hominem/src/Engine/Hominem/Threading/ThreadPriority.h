#pragma once

namespace Hominem {

inline void SetCurrentThreadPriorityLow()
{
#ifdef HMN_PLATFORM_WINDOWS
    constexpr int k_BelowNormal = -1; // THREAD_PRIORITY_BELOW_NORMAL
    SetThreadPriority(GetCurrentThread(), k_BelowNormal);
#endif
}

}
