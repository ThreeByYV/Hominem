#pragma once

#include "Hominem/Renderer/Shader.h"
#include <unordered_map>
#include <glm/glm.hpp>
#include <filesystem>

typedef unsigned int GLenum;
typedef int GLint;

namespace Hominem {

	class OpenGLShader : public Shader
	{
	public:
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		OpenGLShader(const std::string& name, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
		OpenGLShader(const std::string& filepath);

		~OpenGLShader();

		void Bind() const override;
		void Unbind() const override;
		const std::string& GetName() const override { return m_Name; }


		void UploadUniformInt(const std::string& name, int value);
		
		void UploadUniformFloat(const std::string& name, float value);
		void UploadUniformFloat2(const std::string& name, const glm::vec2& values);
		void UploadUniformFloat3(const std::string& name, const glm::vec3& values);
		void UploadUniformFloat4(const std::string& name, const glm::vec4& values);
		
		void UploadUniformMat3(const std::string& name, const glm::mat3& matrix);
		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);

	private:
		std::string m_Name;
		uint32_t m_RendererID;
		mutable std::unordered_map<std::string, GLint> m_UniformLocationCache;
	private:
		static std::string ReadTextFile(const std::filesystem::path& path);
		std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);
		void Compile(std::unordered_map<GLenum, std::string> shaderSources);
		
		GLint GetUniformLocation(const std::string& name) const;
	};
}