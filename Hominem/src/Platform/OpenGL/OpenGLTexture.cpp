#include "hmnpch.h"

#include "OpenGLTexture.h"
#include "stb_image.h"
#include <glad/glad.h>

namespace Hominem {

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		:m_Path(path)
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		HMN_CORE_ASSERT(data, "Failed to load image!");

		//convert stbi_image signed int back to unsigned int
		m_Width = width; 
		m_Height = height;

		GLenum interalFormat = 0;  //How OpenGL will see the format
		GLenum dataFormat = 0;

		if (channels == 4) {
			interalFormat = GL_RGBA8;  
			dataFormat = GL_RGBA;
		}
		else if (channels == 3) {
			interalFormat = GL_RGB8;
			dataFormat = GL_RGB;
		}
		else if (channels == 1) {
			interalFormat = GL_R8;
			dataFormat = GL_RED;
		}

		HMN_CORE_ASSERT(interalFormat != 0 && dataFormat != 0, "Image format not supported!");

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, interalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}
}