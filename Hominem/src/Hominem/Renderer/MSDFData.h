#pragma once

#include <vector>

#undef INFINITE //windows has already has this macro which causes issues with msdf's version, remove it here

#include "msdf-atlas-gen.h"
#include "msdfgen.h"

namespace Hominem {

	struct MSDFData
	{
		std::vector<msdf_atlas::GlyphGeometry> Glyphs;
		msdf_atlas::FontGeometry FontGeometry;
	};
}