//#pragma once
//
//#include "RendererAPI.h"
//
//namespace Hominem {
//
//	class RenderCommand
//	{
//	public:
//		inline static void SetClearColor(const glm::vec4& color) 
//		{
//			s_RendererAPI->SetClearColor(color);
//		}
//
//		inline static void Clear()
//		{
//			s_RendererAPI->Clear();
//		}
//
//		inline static void DrawIndexed(const std::shared_ptr<unsigned int>& vertexArray)
//		{
//			s_RendererAPI->DrawIndexed(vertexArray);
//		}
//	};
//	private:
//		static RendererAPI* s_RendererAPI;
//}