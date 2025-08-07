//#pragma once
//
//#include <glm/glm.hpp>
//
//
//namespace Hominem {
//
//	class Shader
//	{
//	public:
//		Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
//		~Shader();
//
//		void Bind() const;
//		void Unbind() const;
//
//		void UploadUniformMat4(const glm::mat4& matrix);
//
//		uint32_t CreateComputeShader(const std::filesystem::path& path);
//		uint32_t ReloadComputeShader(uint32_t shaderHandle, const std::filesystem::path& path);
//
//		uint32_t CreateBaseShaders(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
//		uint32_t ReloadBaseShaders(uint32_t shaderHandle, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
//
//	private:
//		static std::string ReadFile(const std::filesystem::path& path);
//		uint32_t m_RendererID;
//	};
//}
