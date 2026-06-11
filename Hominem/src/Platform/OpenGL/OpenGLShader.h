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
		OpenGLShader(const std::string& filepath, const std::vector<std::string>& defines);

		~OpenGLShader();

		void Bind() const override;
		void Unbind() const override;
		void Reload() override;

		static void UnbindAll();

		const std::string& GetName() const override { return m_Name; }

		void SetInt(const std::string& name, int value) override;
		void SetFloat(const std::string& name, float value) override;
		void SetFloat3(const std::string& name, const glm::vec3& value) override;
		void SetFloat4(const std::string& name, const glm::vec4& value) override;
		void SetMat4(const std::string& name, const glm::mat4& value) override;

		void UploadUniformInt(const std::string& name, int value);

		void UploadUniformFloat(const std::string& name, float value);
		void UploadUniformFloat2(const std::string& name, const glm::vec2& values);
		void UploadUniformFloat3(const std::string& name, const glm::vec3& values);
		void UploadUniformFloat4(const std::string& name, const glm::vec4& values);

		void UploadUniformMat3(const std::string& name, const glm::mat3& matrix);
		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);

		GLint GetUniformLocation(const std::string& name) const override;

	private:
		std::string m_Name;
		uint32_t m_RendererID;
		mutable std::unordered_map<std::string, GLint> m_UniformLocationCache;
		std::string              m_Filepath;
		std::vector<std::string> m_Defines;
		bool                     m_IsSingleFile = false;
		// Maps GLSL source IDs (from #line directives) to file paths for readable error messages.
		std::unordered_map<int, std::string> m_SourceMap;

	private:
		static std::string ReadTextFile(const std::filesystem::path& path);
		// Expands #include directives and injects #line directives so GLSL error messages
		// report file:line rather than a meaningless absolute line in the expanded blob.
		// sourceMap is filled with fileId → path for printing on compile failure.
		static std::string ResolveIncludes(const std::string& source,
		                                    const std::string& shaderDir,
		                                    int                sourceId,
		                                    int&               nextId,
		                                    std::unordered_map<int, std::string>& sourceMap);
		// Inserts "#define X" lines after the #version directive in a single shader section.
		static std::string InjectDefines(const std::string& sectionSrc,
		                                  const std::vector<std::string>& defines);
		std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);
		void Compile(std::unordered_map<GLenum, std::string> shaderSources);
	};

	class OpenGLComputeShader : public ComputeShader
	{
	public:
		OpenGLComputeShader(const std::string& filepath);
		~OpenGLComputeShader();

		void Bind() const override;
		void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) const override;

		void SetUint(const std::string& name, uint32_t value) override;
		void SetInt(const std::string& name, int value) override;
		void SetMat4(const std::string& name, const glm::mat4& value) override;

		const std::string& GetName() const override { return m_Name; }

	private:
		GLint GetUniformLocation(const std::string& name) const;

		std::string m_Name;
		uint32_t m_RendererID = 0;
		mutable std::unordered_map<std::string, GLint> m_UniformCache;
	};
}