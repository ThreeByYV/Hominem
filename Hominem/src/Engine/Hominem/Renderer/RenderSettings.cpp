#include "hmnpch.h"
#include "RenderSettings.h"
#include "RenderThread.h"
#include "Renderer2D.h"
#include "Renderer3D.h"

namespace Hominem {

	void RenderSettings::RequestShaderReload()
	{
		RenderThread::QueueUpload([]
		{
			Renderer2D::GetShaderLibrary()->ReloadAll();
			Renderer3D::GetShaderLibrary()->ReloadAll();
			Renderer3D::ReloadVariants();
		});
	}

}
