#pragma once

#include "Hominem/Core/Ref.h"

#include <future>
#include <functional>
#include <chrono>

namespace Hominem {

	template<typename T>
	class AsyncLoad
	{
	public:
		using Loader = std::function<Ref<T>()>;

		/// Idempotent - only the first call starts the load.
		void Begin(Loader loader)
		{
			if (m_Started)
				return;
			m_Started = true;
			m_Future  = std::async(std::launch::async, std::move(loader));
		}

		/// Non-blocking. Returns the loaded value once ready (cached), else nullptr.
		/// Safe to call repeatedly - every caller after the load completes gets the same
		/// instance, not nullptr.
		Ref<T> TryGet()
		{
			if (m_Done)
				return m_Cached;
			if (!m_Future.valid())
				return nullptr;
			if (m_Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				return nullptr;
			m_Cached = m_Future.get();
			m_Done   = true;
			return m_Cached;
		}

		/// True once the async task has finished, regardless of whether it succeeded.
		bool IsDone() const { return m_Done; }

	private:
		std::future<Ref<T>> m_Future;
		Ref<T>              m_Cached;
		bool                m_Started = false;
		bool                m_Done    = false;
	};

}
