#pragma once

#include "Hominem/Core/Core.h"

namespace Hominem {

	// GPU-side shader storage buffer (SSBO).
	// Mirrors the factory pattern of VertexBuffer/IndexBuffer.
	class StorageBuffer
	{
	public:
		virtual ~StorageBuffer() = default;

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		virtual void BindBase(uint32_t slot) const = 0;

		static Ref<StorageBuffer> Create(uint32_t capacity);
	};

	// GPU-side draw-indirect buffer (GL_DRAW_INDIRECT_BUFFER).
	class DrawIndirectBuffer
	{
	public:
		virtual ~DrawIndirectBuffer() = default;

		virtual void SetData(const void* data, uint32_t size) = 0;
		virtual void Bind()   const = 0;
		virtual void Unbind() const = 0;

		static Ref<DrawIndirectBuffer> Create(uint32_t capacity);
	};

}
