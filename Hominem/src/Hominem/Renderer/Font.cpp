#include "hmnpch.h"
#include "Font.h"

#undef INFINITE //windows has already has this macro which causes issues with msdf's version, remove it here
#include "msdf-atlas-gen.h"
#include "msdfgen.h"

namespace Hominem {

	Font::Font(const std::filesystem::path& filepath)
	{
		msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
		
		std::string outputPath = "src/Hominem/Resources/Fonts/output.png";

		if (ft)
		{
			std::string fileString = filepath.string();

			msdfgen::FontHandle* font = msdfgen::loadFont(ft, fileString.c_str());

			if (font)
			{
				msdfgen::Shape shape;
					
				if (msdfgen::loadGlyph(shape, font, 'H'))
				{
					shape.normalize();         //max  angle
					msdfgen::edgeColoringSimple(shape, 3.0);

									// image's width & height
					msdfgen::Bitmap<float, 3> msdf(32, 32);

											// range		 scale	  translation
					msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, msdfgen::Vector2(4.0, 4.0));
						
					msdfgen::savePng(msdf, outputPath.c_str());
				}
				
				msdfgen::destroyFont(font);
			}
			msdfgen::deinitializeFreetype(ft);
		}
	}
}