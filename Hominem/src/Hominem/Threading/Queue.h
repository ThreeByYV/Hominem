#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace Hominem {

	/// @brief Thread-safe queue for producer/consumer patterns.
	/// @tparam T Type of items stored in the queue
	template<typename T>
	class Queue
	{
	public:
		Queue() = default;
		~Queue() { Shutdown(); }

		Queue(const Queue&) = delete;
		Queue& operator=(const Queue&) = delete;

		void Push(T item)
		{
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				m_Queue.push(std::move(item));
			}
			m_Condition.notify_one();
		}

		/// @brief Non-blocking pop. Returns true if an item was retrieved.
		bool TryPop(T& item)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Queue.empty())
				return false;

			item = std::move(m_Queue.front());
			m_Queue.pop();
			return true;
		}

		/// @brief Blocking pop. Waits until an item is available or shutdown.
		std::optional<T> WaitAndPop()
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Condition.wait(lock, [this] { return !m_Queue.empty() || m_ShutdownFlag; });

			if (m_ShutdownFlag && m_Queue.empty())
				return std::nullopt;

			T item = std::move(m_Queue.front());
			m_Queue.pop();
			return item;
		}

		void Shutdown()
		{
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				m_ShutdownFlag = true;
			}
			m_Condition.notify_all();
		}

		bool IsEmpty() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_Queue.empty();
		}

		size_t Size() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_Queue.size();
		}

	private:
		std::queue<T> m_Queue;
		mutable std::mutex m_Mutex;
		std::condition_variable m_Condition;
		bool m_ShutdownFlag = false;
	};

}
