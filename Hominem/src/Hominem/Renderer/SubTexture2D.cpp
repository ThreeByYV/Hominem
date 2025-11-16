#include "hmnpch.h"

#include "SubTexture2D.h"

namespace Hominem {

	SubTexture2D::SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max)
		: m_Texture(texture)
	{
		m_TexCoords[0] = { min.x, min.y }; //bottom left corner
		m_TexCoords[1] = { max.x, min.y }; //bottom right corner
		m_TexCoords[2] = { max.x, max.y }; //top right corner
		m_TexCoords[3] = { min.x, max.y }; //top left corner
	}

	//helper func, this will derive the min and max from the coords needed for creating a sub region from the spritesheet
	Ref<SubTexture2D> SubTexture2D::CreateFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2 spriteSize)
	{
		glm::vec2 min = { (coords.x * spriteSize.x) / texture->GetWidth(), (coords.y * spriteSize.y) / texture->GetHeight() }; //bottom left corner
		glm::vec2 max = { ((coords.x + 1) * spriteSize.x) / texture->GetWidth(), (coords.y * spriteSize.y) / texture->GetHeight() }; //top right corner
	
		return CreateRef<SubTexture2D>(texture, min, max);
	}
}