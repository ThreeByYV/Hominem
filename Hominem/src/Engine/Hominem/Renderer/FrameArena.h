#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <memory>

namespace Hominem {

	/**
	 * Bump allocator for per-frame render data.
	 *
	 * All allocations live until Reset() — call it at the start of each frame.
	 * Two instances are double-buffered in RenderThread so main thread writes
	 * into one while the render thread reads the other.
	 *
	 * Default 2MB covers: ~30k mat4 bones, thousands of draw command payloads.
	 * Increase k_Size if HMN_CORE_ASSERT fires on overflow.
	 */
	class FrameArena
	{
	public:
		static constexpr size_t k_Size = 2 * 1024 * 1024; // 2 MB

		FrameArena() : m_Buffer(new uint8_t[k_Size]), m_Offset(0) {}

		template<typename T>
		T* Alloc(size_t count = 1)
		{
			const size_t align   = alignof(T);
			const size_t aligned = (m_Offset + align - 1) & ~(align - 1);
			const size_t end     = aligned + sizeof(T) * count;
			assert(end <= k_Size && "FrameArena overflow — increase k_Size");
			m_Offset = end;
			return reinterpret_cast<T*>(m_Buffer.get() + aligned);
		}

		void   Reset()      { m_Offset = 0; }
		size_t Used() const { return m_Offset; }

	private:
		std::unique_ptr<uint8_t[]> m_Buffer;
		size_t                     m_Offset = 0;
	};

}
