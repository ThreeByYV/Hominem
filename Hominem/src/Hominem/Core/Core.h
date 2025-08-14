#pragma once

//#if defined(_WIN32) || defined(_WIN64)
//	#ifdef HOMINEM_BUILD_DLL
//		#define HOMINEM_API __declspec(dllexport)
//	#else 
//		#define HOMINEM_API __declspec(dllimport)
//	#endif
//#else
//	#error Hominem is only supported on Windows!
//#endif

#ifdef HMN_ENABLE_ASSERTS	
	#define HMN_ASSERT(x, ...) { if(!(x)) { HMN_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define HMN_CORE_ASSERT(x, ...) { if(!(x)) { HMN_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define HMN_ASSERT(x, ...)
	#define HMN_CORE_ASSERT(x, ...)
#endif

#define HMN_BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)
