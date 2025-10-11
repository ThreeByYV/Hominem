#include "hmnpch.h"
#include "Font.h"

#undef INFINITE //windows has already has this macro which causes issues with msdf's version, remove it here

#include "msdf-atlas-gen.h"
#include "msdfgen.h"
#include "Texture.h"

#include "MSDFData.h"

namespace Hominem {

    Font::Font(const std::filesystem::path& filepath)
        : m_Data(new MSDFData())
    {
        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        HMN_CORE_ASSERT(ft, "Freetype failed to initialize!");

        std::string fileString = filepath.string();
        msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());

        if (!font)
        {
            HMN_CORE_ERROR("Failed to load font: {}", fileString);
            msdfgen::deinitializeFreetype(ft);
            return;
        }

        // Create character set of the ASCII characters
        msdf_atlas::Charset charset;
        for (uint32_t c = 32; c < 128; c++)
            charset.add(c);

        // Loading all glyphs from the charset
        double fontScale = 1.0;
        m_Data->FontGeometry = msdf_atlas::FontGeometry(&m_Data->Glyphs);
        int glyphsLoaded = m_Data->FontGeometry.loadCharset(font, fontScale, charset);
       
        HMN_CORE_INFO("Loaded {} glyphs from font", glyphsLoaded);

        // Generate atlas
        double maxCornerAngle = 3.0;

        for (msdf_atlas::GlyphGeometry& glyph : m_Data->Glyphs)
            glyph.edgeColoring(msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);

        msdf_atlas::TightAtlasPacker packer;
        packer.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::POWER_OF_TWO_SQUARE);
        packer.setMinimumScale(32.0);
        packer.setPixelRange(2.0); //frag shader is same value
        packer.setMiterLimit(1.0);
        packer.pack(m_Data->Glyphs.data(), (int)m_Data->Glyphs.size());

        int width = 0, height = 0;
        packer.getDimensions(width, height);

        // Generate MSDF atlas
        msdf_atlas::GeneratorAttributes attributes;
        attributes.config.overlapSupport = true;
        attributes.scanlinePass = true;

        msdf_atlas::ImmediateAtlasGenerator<float, 3, msdf_atlas::msdfGenerator, msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 3>> generator(width, height);
        generator.setAttributes(attributes);
        generator.setThreadCount(8);
        generator.generate(m_Data->Glyphs.data(), (int)m_Data->Glyphs.size());

        msdfgen::BitmapConstRef<msdf_atlas::byte, 3> bitmap = (msdfgen::BitmapConstRef<msdf_atlas::byte, 3>)generator.atlasStorage();

        // Create texture to act as a spritesheet for all the alphabetical letters
        m_AtlasTexture = Texture2D::Create(bitmap.width, bitmap.height, TextureFormat::RGB8);
        m_AtlasTexture->SetData((void*)bitmap.pixels, bitmap.width * bitmap.height * 3);

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
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