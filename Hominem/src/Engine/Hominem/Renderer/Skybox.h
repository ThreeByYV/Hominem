#pragma once

#include "Hominem/Core/Core.h"
#include <string>

namespace Hominem {

	class Skybox : public RefCounted
	{
	public:
		~Skybox();

		/// Load an equirectangular HDR panorama from a `.exr` file on disk.
		static Ref<Skybox> CreateFromEquirectEXR(const std::string& path);

		uint32_t GetRendererID() const { return m_RendererID; }
		uint32_t GetWidth()      const { return m_Width; }
		uint32_t GetHeight()     const { return m_Height; }

		void Bind(uint32_t slot = 0) const;

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Width = 0, m_Height = 0;
	};

}
