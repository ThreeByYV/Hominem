#pragma once

#include "Hominem/Core/Ref.h"

#ifdef HMN_ENABLE_ASSERTS
	#define HMN_ASSERT(x, ...) { if(!(x)) { HMN_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define HMN_CORE_ASSERT(x, ...) { if(!(x)) { HMN_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define HMN_ASSERT(x, ...)
	#define HMN_CORE_ASSERT(x, ...)
#endif

#define HMN_BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)
#define ARRAY_SIZE_IN_ELEMENTS(a) (sizeof(a) / sizeof((a)[0]))
