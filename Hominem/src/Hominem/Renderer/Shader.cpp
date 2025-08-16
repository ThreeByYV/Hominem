#include "hmnpch.h"
#include "Shader.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Renderer.h"

namespace Hominem {

	Shader* Shader::Create(const std::string& filepath)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return new OpenGLShader(filepath);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	//Shader* Shader::Create(const std::filesystem::path& path)
	//{
	//	switch (Renderer::GetAPI())
	//	{
	//	case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
	//	case RendererAPI::API::OpenGL: return new OpenGLShader(path);
	//	}

	//	HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
	//	return nullptr;
	//}


	Shader* Shader::Create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return new OpenGLShader(vertexPath, fragmentPath);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	//Shader* Shader::ReloadShader(uint32_t shaderHandle, const std::string& shaderPath)
	//{
	//	switch (Renderer::GetAPI())
	//	{
	//		case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
	//		case RendererAPI::API::OpenGL: return new OpenGLShader->ReloadShader(shaderPath);
	//	}

	//	HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
	//	return nullptr;
	//}

	Shader* Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return new OpenGLShader(vertexSrc, fragmentSrc);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
