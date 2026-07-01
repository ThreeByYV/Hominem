#include "hmnpch.h"
#include "Texture.h"

#include "Hominem/Core/Core.h"
#include "RendererAPI.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Hominem {

	Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& faces)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(faces);
			default: HMN_CORE_ASSERT(false, "Unknown RendererAPI!"); return nullptr;
		}
	}

	Ref<TextureCube> TextureCube::CreateEmpty(uint32_t resolution)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTextureCube>(resolution);
			default: HMN_CORE_ASSERT(false, "Unknown RendererAPI!"); return nullptr;
		}
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is not supported") return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(width, height);
		}
		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, TextureFormat format)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is not supported") return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(width, height, format);
		}
		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is not supported") return nullptr;
			case RendererAPI::API::OpenGL: {
				auto ref = CreateRef<OpenGLTexture2D>(path);
				ref->QueueUpload();
				return ref;
			}
			default: HMN_CORE_ASSERT(false, "Unknown RendererAPI!") return nullptr;
		}
	}

	Ref<Texture2D> Texture2D::CreateFromMemory(const uint8_t* data, uint32_t byteSize)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is not supported") return nullptr;
			case RendererAPI::API::OpenGL: {
				auto ref = CreateRef<OpenGLTexture2D>(data, byteSize);
				ref->QueueUpload();
				return ref;
			}
			default: HMN_CORE_ASSERT(false, "Unknown RendererAPI!") return nullptr;
		}
	}

	void Texture::UnbindAll()
	{
		switch (RendererAPI::GetAPI())
		{
		case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None not supported"); return;
		case RendererAPI::API::OpenGL: OpenGLTexture2D::UnbindAll(); return;
		}
		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
	}
}
