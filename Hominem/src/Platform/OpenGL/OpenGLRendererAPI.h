#pragma once

#include "Hominem/Renderer/RendererAPI.h"

namespace Hominem {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		 void SetClearColor(const glm::vec4& color) override;
	     void Clear() override;

		 void DrawIndexed(const std::shared_ptr<unsigned int>& vertexArray) override;
	};
}
