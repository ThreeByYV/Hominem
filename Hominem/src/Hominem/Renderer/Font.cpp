#include "hmnpch.h"
#include "Font.h"

#undef INFINITE //windows has already has this macro which causes issues with msdf's version, remove it here

#include "msdf-atlas-gen.h"
#include "msdfgen.h"
#include "Texture.h"

#include "MSDFData.h"

namespace Hominem {

	/*template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
	static Ref<Texture2D> CreateAndCacheAtlas(const std::string& fontName, float fontSize, const std::vector<msdf_atlas::GlyphGeometry>& glyphs,
		const msdf_atlas::FontGeometry& fontGeometry, uint32_t width, uint32_t height)
	{
		msdf_atlas::GeneratorAttributes attributes;
		attributes.config.overlapSupport = true;
		attributes.scanlinePass = true;

		msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width, height);
		generator.setAttributes(attributes);
		generator.setThreadCount(8);
		generator.generate(glyphs.data(), (int)glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

		TextureSpecification spec;
		spec.Width = bitmap.width;
		spec.Height = bitmap.height;
		spec.Format = ImageFormat::RGB8;
		spec.GenerateMips = false;

		Ref<Texture2D> texture = Texture2D::Create(spec);
		texture->SetData((void*)bitmap.pixels, bitmap.width * bitmap.height * 3);
		return texture;
		*/

	Font::Font(const std::filesystem::path& filepath)
		: m_Data(new MSDFData())
	{
		//todo finish this 
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		HMN_CORE_ASSERT(ft, "Freetype failed to initialize!");

		std::string fileString = filepath.string();

		msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());
		
		if (!font)
		{
			HMN_CORE_ERROR("Failed to load font: {}", fileString);
			return;
		}
	}

	Ref<Font> Font::GetDefaultFont()
	{
		static Ref<Font> DefaultFont;
		if (!DefaultFont)
		{
			DefaultFont = CreateRef<Font>("src/Hominem/Resources/Fonts/OpenSans-Regular.ttf");
		}

		return DefaultFont;
	}
}