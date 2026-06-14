#pragma once

#include "Hominem/Core/Core.h"
#include <array>

namespace Hominem {

	enum class TextureFormat
	{
		None = 0,
		RGB8,
		RGBA8,
		RED8
	};

	enum class TextureWrap { Repeat, ClampToEdge, MirroredRepeat };
	
	class Texture : public RefCounted
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual void SetData(const void* data, uint32_t size) = 0;
		virtual void Bind(uint32_t slot = 0) const = 0;

		static void UnbindAll();
	};

	class TextureCube : public Texture
	{
	public:
		static Ref<TextureCube> Create(const std::array<std::string, 6>& faces);
		static Ref<TextureCube> CreateEmpty(uint32_t resolution);

		virtual uint32_t GetRendererID() const = 0;

		// Number of mip levels allocated for a CreateEmpty() cube (full chain down
		// to 1x1). Valid only after EnsureCreated().
		virtual uint32_t GetMipLevels() const = 0;

		// Allocates the GL cubemap storage for a CreateEmpty() texture, including
		// the full mip chain down to 1x1. Must be called on the render thread
		// before the texture is used as a render target or sampled. Safe to call
		// multiple times (no-op if already created).
		virtual void EnsureCreated() = 0;

		// Generates a full mip chain from the current face contents (e.g. after
		// rendering into mip 0 of each face).
		virtual void GenerateMipmaps() = 0;
	};

	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, TextureFormat format);
		static Ref<Texture2D> Create(const std::string& path);

		/// Decodes a compressed image (PNG/JPG) from a memory blob.
		static Ref<Texture2D> CreateFromMemory(const uint8_t* data, uint32_t byteSize);

		virtual void QueueUpload() {}

		virtual void SetWrapS(TextureWrap wrap) = 0;
		virtual void SetWrapT(TextureWrap wrap) = 0;
	};

}

