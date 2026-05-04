#pragma once

#include "Hominem/Core/Core.h"
#include "spdlog/spdlog.h"


namespace Hominem {

	class Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

}

//Core log macros
#define HMN_CORE_TRACE(...)      ::Hominem::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define HMN_CORE_INFO(...)       ::Hominem::Log::GetCoreLogger()->info(__VA_ARGS__)
#define HMN_CORE_WARN(...)       ::Hominem::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define HMN_CORE_ERROR(...)      ::Hominem::Log::GetCoreLogger()->error(__VA_ARGS__)
#define HMN_CORE_CRITICAL(...)   ::Hominem::Log::GetCoreLogger()->critical(__VA_ARGS__)


