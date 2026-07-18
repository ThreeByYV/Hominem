#pragma once

#include "Texture.h"


namespace Hominem {

	struct MSDFData;

	class Font : public RefCounted
	{
	public:
		Font(const std::filesystem::path& font);
		~Font() = default;

		MSDFData* GetMSDFData() const { return m_Data; }
		Ref<Texture2D> GetAtlasTexture() const { return m_AtlasTexture; }

		/// Lays out the string and returns its width in the same normalised text units
		/// Renderer2D::DrawString uses. Multiply by your text scale for world-space width;
		/// halve it to centre the string.
		float MeasureWidth(const std::string& text) const;

		static Ref<Font> GetDefaultFont();
	private:
		MSDFData* m_Data;
		Ref<Texture2D> m_AtlasTexture;
	};
}