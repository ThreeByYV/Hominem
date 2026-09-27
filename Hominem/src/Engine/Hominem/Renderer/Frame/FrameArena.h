#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <memory>

namespace Hominem {

	/**
	 * Bump allocator for scratch data while a frame is built and recorded, main thread only.
	 * Recording copies what the render thread needs into the command lists, so Application
	 * resets it right after Record().
	 *
	 * Default 2MB covers ~30k mat4 bones. Increase k_Size if the overflow assert fires.
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
