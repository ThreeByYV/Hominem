#include "hmnpch.h"
#include "Texture.h"

#include "Hominem/Core/Core.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Hominem {

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared<OpenGLTexture2D>(width, height);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, TextureFormat format)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared<OpenGLTexture2D>(width, height, format);
		}
		
		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		static std::unordered_map<std::string, std::weak_ptr<Texture2D>> s_Cache;

		auto& weak = s_Cache[path];
		if (auto existing = weak.lock())
			return existing;

		Ref<Texture2D> tex;
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: tex = std::make_shared<OpenGLTexture2D>(path); break;
			default:                       HMN_CORE_ASSERT(false, "Unknown RendererAPI!") return nullptr;
		}

		weak = tex;
		return tex;
	}

	void Texture::UnbindAll()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			HMN_CORE_ASSERT(false, "RendererAPI::None not supported");
			return;

		case RendererAPI::API::OpenGL:
			OpenGLTexture2D::UnbindAll();
			return;
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
	}
}