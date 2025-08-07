//#pragma once
//
//#include <glm/glm.hpp>
//
//namespace Hominem {
//
//	class RendererAPI
//	{
//	public:
//		enum class API
//		{
//			None = 0; OpenGL = 1; Vulkan = 2;
//		};
//	};
//	public:
//		virtual void SetClearColor(const glm::vec4& color) = 0;
//		virtual void Clear() = 0;
//
//		virtual void DrawIndexed(const std::shared_ptr<unsigned int>& vertexArray) = 0;
//		inline static GetAPI() { return s_API; }
//private:
//	static API s_API;
//}