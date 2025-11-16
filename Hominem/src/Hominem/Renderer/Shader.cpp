#include "hmnpch.h"
#include "Shader.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Renderer.h"

namespace Hominem {

	Ref<Shader> Shader::Create(const std::string& filepath)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared<OpenGLShader>(filepath);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	void Shader::UnbindAll()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
			HMN_CORE_ASSERT(false, "RendererAPI::None is not supported");
			return;

		case RendererAPI::API::OpenGL:
			OpenGLShader::UnbindAll();
			return;
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
	}

	Ref<Shader> Shader::Create(const std::string& name, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared <OpenGLShader>(name, vertexPath, fragmentPath);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: HMN_CORE_ASSERT(false, "RendererAPI::None is currently not supported") return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared <OpenGLShader>(name, vertexSrc, fragmentSrc);
		}

		HMN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		//suppports adding a custom shader name instead of default filename
		HMN_CORE_ASSERT(!Exists(name), "Shader already exists in Shader Library!");
		m_Shaders[name] = shader;
	}


	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		auto& name = shader->GetName();
		Add(name, shader);
	}

	Ref<Shader> ShaderLibrary::Get(const std::string& name)
	{
		HMN_CORE_ASSERT(Exists(name), "Shader not found in the Shader Library!");
		return m_Shaders[name];
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}

	bool ShaderLibrary::Exists(const std::string& name) const
	{
		return m_Shaders.find(name) != m_Shaders.end();
	}

}
