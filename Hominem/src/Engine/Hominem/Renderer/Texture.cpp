#include "hmnpch.h"
#include "Texture.h"

#include "Hominem/Core/Core.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Hominem {

	Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& faces)
	{
		return CreateRef<OpenGLTextureCube>(faces);
	}

	Ref<TextureCube> TextureCube::CreateEmpty(uint32_t resolution)
	{
		return CreateRef<OpenGLTextureCube>(resolution);
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		return CreateRef<OpenGLTexture2D>(width, height);
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, TextureFormat format)
	{
		return CreateRef<OpenGLTexture2D>(width, height, format);
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		auto ref = CreateRef<OpenGLTexture2D>(path);
		ref->QueueUpload();
		return ref;
	}

	Ref<Texture2D> Texture2D::CreateFromMemory(const uint8_t* data, uint32_t byteSize)
	{
		auto ref = CreateRef<OpenGLTexture2D>(data, byteSize);
		ref->QueueUpload();
		return ref;
	}

	void Texture::UnbindAll()
	{
		OpenGLTexture2D::UnbindAll();
	}
}
