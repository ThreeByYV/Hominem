#pragma once

#include <memory>

#ifdef HMN_ENABLE_ASSERTS	
	#define HMN_ASSERT(x, ...) { if(!(x)) { HMN_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define HMN_CORE_ASSERT(x, ...) { if(!(x)) { HMN_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define HMN_ASSERT(x, ...)
	#define HMN_CORE_ASSERT(x, ...)
#endif

#define HMN_BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

namespace Hominem {

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T>
	using Ref = std::shared_ptr<T>;
}
