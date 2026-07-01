#include "hmnpch.h"
#include "Shader.h"

#include "Platform/OpenGL/OpenGLShader.h"

namespace Hominem {

	Ref<Shader> Shader::Create(const std::string& filepath)
	{
		return CreateRef<OpenGLShader>(filepath);
	}

	Ref<Shader> Shader::Create(const std::string& filepath, const std::vector<std::string>& defines)
	{
		return CreateRef<OpenGLShader>(filepath, defines);
	}

	Ref<Shader> Shader::Create(const std::string& name, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
	{
		return CreateRef<OpenGLShader>(name, vertexPath, fragmentPath);
	}

	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		return CreateRef<OpenGLShader>(name, vertexSrc, fragmentSrc);
	}

	void Shader::UnbindAll()
	{
		OpenGLShader::UnbindAll();
	}

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		HMN_CORE_ASSERT(!Exists(name), "Shader already exists in Shader Library!");
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Reload(const std::string& name)
	{
		HMN_CORE_ASSERT(Exists(name), "Shader not found in Shader Library: {0}", name);
		m_Shaders[name]->Reload();
	}

	void ShaderLibrary::ReloadAll()
	{
		for (auto& [name, shader] : m_Shaders)
		{
			HMN_CORE_INFO("Hot reloading shader '{0}'", name);
			shader->Reload();
		}
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

	Ref<ComputeShader> ComputeShader::Create(const std::string& filepath)
	{
		return CreateRef<OpenGLComputeShader>(filepath);
	}

}
