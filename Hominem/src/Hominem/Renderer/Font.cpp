#include "hmnpch.h"
#include "Font.h"

#undef INFINITE //windows has already has this macro which causes issues with msdf's version 
#include "msdf-atlas-gen.h"
#include "msdfgen.h"

namespace Hominem {

	Font::Font(const std::filesystem::path& filepath)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();

		if (ft)
		{
			std::string fileString = filepath.string();

			msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());

			if (font)
			{
				msdfgen::Shape shape;

				std::array<char, 8> name = { 'H', 'O', 'M', 'I', 'N', 'E', 'M' };
				
				for (uint32_t i = 0; i < name.size(); i++)
				{
					if (msdfgen::loadGlyph(shape, font, name[i]))
					{
						shape.normalize();         //max  angle
						msdfgen::edgeColoringSimple(shape, 3.0);

										// image's width & height
						msdfgen::Bitmap<float, 3> msdf(32, 32);

											 // range		 scale	 translation
						msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, msdfgen::Vector2(4.0, 4.0));
						msdfgen::savePng(msdf, "output.png");
					}
				}
				msdfgen::destroyFont(font);
			}
			msdfgen::deinitializeFreetype(ft);
		}
	}
}