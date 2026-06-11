#pragma once

#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>

    #define HMN_PROFILE_FRAME(name)     FrameMarkNamed(name)
    #define HMN_PROFILE_SCOPE(name)     ZoneScopedN(name)
    #define HMN_PROFILE_FUNCTION()      ZoneScoped
#else
    #define HMN_PROFILE_FRAME(name)
    #define HMN_PROFILE_SCOPE(name)
    #define HMN_PROFILE_FUNCTION()
#endif
