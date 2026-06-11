#include "hmnpch.h"
#include "Skybox.h"

#include "Hominem/Renderer/RenderThread.h"

#include "tinyexr.h" // declarations only — implementation lives in vendor/tinyexr/tinyexr.cpp
#include <glad/glad.h>

#include <cstdlib>
#include <memory>
#include <vector>

namespace Hominem {

	Ref<Skybox> Skybox::CreateFromEquirectEXR(const std::string& path)
	{
		auto sky = CreateRef<Skybox>();

		// CPU decode — safe on any thread (no GL).
		float*      rgba = nullptr;
		int         w = 0, h = 0;
		const char* err = nullptr;
		const int   ret = LoadEXR(&rgba, &w, &h, path.c_str(), &err);
		if (ret != TINYEXR_SUCCESS || !rgba || w <= 0 || h <= 0)
		{
			HMN_CORE_ERROR("Skybox: failed to load EXR '{}': {}", path, err ? err : "unknown error");
			if (err) FreeEXRErrorMessage(err);
			return sky; // RendererID stays 0 → the skybox pass skips it
		}

		sky->m_Width  = (uint32_t)w;
		sky->m_Height = (uint32_t)h;

		// LoadEXR returns tightly-packed RGBA floats, scanline 0 = top of image.
		auto pixels = std::make_shared<std::vector<float>>(rgba, rgba + (size_t)w * h * 4);
		free(rgba);

		HMN_CORE_INFO("Skybox: loaded EXR '{}' ({}x{})", path, w, h);

		Ref<Skybox> self = sky;
		RenderThread::QueueUpload([self, pixels, w, h]() mutable {
			glGenTextures(1, &self->m_RendererID);
			glBindTexture(GL_TEXTURE_2D, self->m_RendererID);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, pixels->data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);        // wrap around longitude
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // clamp at the poles
			glBindTexture(GL_TEXTURE_2D, 0);
		});

		return sky;
	}

	void Skybox::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}

	Skybox::~Skybox()
	{
		if (!m_RendererID) return;
		if (RenderThread::IsOnRenderThread())
		{
			glDeleteTextures(1, &m_RendererID);
		}
		else
		{
			uint32_t id = m_RendererID;
			RenderThread::QueueUpload([id] { glDeleteTextures(1, &id); });
		}
	}

}
