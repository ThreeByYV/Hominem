#pragma once

#include <string>

namespace Hominem {

	class Shader
	{
	public:
		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		static Shader* Create(const std::string& vertexSrc, const std::string& fragmentSrc);
		static Shader* Create(const std::string& filepath);
		static Shader* Create(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);

		/*static Shader* Create(const std::filesystem::path& path);*/
		//static Shader* ReloadShader(uint32_t shaderHandle, const std::string& shaderPath);
	};
}