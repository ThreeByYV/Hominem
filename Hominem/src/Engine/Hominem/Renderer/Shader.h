#pragma once

#include "Hominem/Core/Core.h"
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Hominem {

	class Shader : public RefCounted
	{
	public:
		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
		virtual void SetFloat(const std::string& name, float value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
		virtual void SetInt(const std::string& name, int value) = 0;

		virtual int  GetUniformLocation(const std::string& name) const = 0;

		// By-location variants — call after GetUniformLocation to avoid name lookup at submit time.
		virtual void SetIntLoc   (int loc, int v)               = 0;
		virtual void SetFloatLoc (int loc, float v)             = 0;
		virtual void SetFloat3Loc(int loc, const glm::vec3& v)  = 0;
		virtual void SetFloat4Loc(int loc, const glm::vec4& v)  = 0;
		virtual void SetMat4Loc  (int loc, const glm::mat4& v)  = 0;

		virtual const std::string& GetName() const = 0;

		virtual void Reload() = 0;

		static Ref<Shader> Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		static Ref<Shader> Create(const std::string& name, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
		static Ref<Shader> Create(const std::string& filepath);
		// Compile with extra #define flags injected after each #version line.
		static Ref<Shader> Create(const std::string& filepath, const std::vector<std::string>& defines);

		static void UnbindAll();
	};

	// Compute-only shader. Dispatched via Dispatch() rather than used in a draw call.
	class ComputeShader : public RefCounted
	{
	public:
		virtual ~ComputeShader() = default;

		virtual void Bind() const = 0;
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) const = 0;

		virtual void SetUint(const std::string& name, uint32_t value) = 0;
		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;

		virtual const std::string& GetName() const = 0;

		static Ref<ComputeShader> Create(const std::string& filepath);
	};

	class ShaderLibrary : public RefCounted
	{
	public:
		void Add(const Ref<Shader>& shader);
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Reload(const std::string& name);
		void ReloadAll();

		Ref<Shader> Get(const std::string& name);

		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);

		bool Exists(const std::string& name) const;
	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}