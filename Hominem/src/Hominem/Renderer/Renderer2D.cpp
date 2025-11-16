#include "hmnpch.h"
#include "Renderer2D.h"
#include "glad/glad.h"

#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "RenderCommand.h"

#include "glm/gtc/matrix_transform.hpp"
#include "Hominem/Utils/Renderer.h"

#include "MSDFData.h"

namespace Hominem {

	Renderer2DStorage* Renderer2D::s_Data = nullptr;

	namespace {
		const glm::vec2 s_FullTextureCoords[4] = {
			{ 0.0f, 0.0f }, // BL
			{ 1.0f, 0.0f }, // BR
			{ 1.0f, 1.0f }, // TR
			{ 0.0f, 1.0f }  // TL
		};
	}

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DStorage();
		s_Data->QuadVertexArray = VertexArray::Create();

		s_Data->ShaderLibrary = std::make_shared<ShaderLibrary>();

		// Quads
		float vertices[5 * 4] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,  // 0: Bottom-left
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // 1: Bottom-right  
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,  // 2: Top-right
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f   // 3: Top-left
		};

		// Store the default version once
		memcpy(s_Data->DefaultQuadVertices, vertices, sizeof(vertices));
		memcpy(s_Data->TempQuadVertices, vertices, sizeof(vertices));

		s_Data->VertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));

		BufferLayout layout = {
			{ ShaderDataType::Float3, "a_Pos" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		s_Data->VertexBuffer->SetLayout(layout);
		s_Data->QuadVertexArray->AddVertexBuffer(s_Data->VertexBuffer); //todo may need to leverage Unbind more, since there could be bad associations to the bind VAO


		unsigned int indices[6] = {
			0, 1, 2,  // First triangle (bottom-left, bottom-right, top-right)
			2, 3, 0   // Second triangle (top-right, top-left, bottom-left)
		};

		s_Data->IndexBuffer = IndexBuffer::Create(indices, ARRAY_SIZE_IN_ELEMENTS(indices));
		s_Data->QuadVertexArray->SetIndexBuffer(s_Data->IndexBuffer);

		s_Data->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t whiteTextureData = 0xffffffff;
		s_Data->WhiteTexture->SetData(&whiteTextureData, sizeof(uint32_t));

		s_Data->TextureShader = s_Data->ShaderLibrary->Load("src/Hominem/Resources/Shaders/texture.glsl");
		s_Data->DefaultShader = s_Data->ShaderLibrary->Get("texture");
		s_Data->OverrideShader = nullptr;

		s_Data->TextureShader->Bind();
		s_Data->TextureShader->SetInt("u_Texture", 0);

		//Text
		s_Data->TextVertexArray = VertexArray::Create();
		s_Data->TextVertexBuffer = VertexBuffer::Create(s_Data->MaxVertices * sizeof(TextVertex));
		s_Data->TextVertexArray->SetIndexBuffer(s_Data->IndexBuffer); //layout of the indicies is the same as a normal quad i.e. can reuse index buffer


		s_Data->TextVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" },
			{ ShaderDataType::Float2, "a_TexCoord" },
			});

		s_Data->TextVertexArray->AddVertexBuffer(s_Data->TextVertexBuffer);

		s_Data->TextShader = Shader::Create("src/Hominem/Resources/Shaders/text.glsl");
	}

	void Renderer2D::Shutdown()
	{
		delete s_Data; //need to destroy any vertex arrays in the GPU while it's still in OpenGL context
	}

	void Renderer2D::EndScene()
	{
		if (!s_Data) return;

		if (s_Data->QuadVertexArray)
			s_Data->QuadVertexArray->Unbind();

		Shader::UnbindAll();
		Texture::UnbindAll();
	}


	Ref<ShaderLibrary> Renderer2D::GetShaderLibrary()
	{
		return s_Data->ShaderLibrary;
	}

	void Renderer2D::UploadQuadUVs(const glm::vec2 texCoords[4])
	{
		// Copy position data
		memcpy(s_Data->TempQuadVertices,
			s_Data->DefaultQuadVertices,
			sizeof(s_Data->DefaultQuadVertices));

		// Overwrite UVs only  
		// BL = 0, BR = 1, TR = 2, TL = 3
		s_Data->TempQuadVertices[3] = texCoords[0].x;
		s_Data->TempQuadVertices[4] = texCoords[0].y;

		s_Data->TempQuadVertices[8] = texCoords[1].x;
		s_Data->TempQuadVertices[9] = texCoords[1].y;

		s_Data->TempQuadVertices[13] = texCoords[2].x;
		s_Data->TempQuadVertices[14] = texCoords[2].y;

		s_Data->TempQuadVertices[18] = texCoords[3].x;
		s_Data->TempQuadVertices[19] = texCoords[3].y;

		// Upload to VBO
		s_Data->VertexBuffer->SetData(s_Data->TempQuadVertices,
			sizeof(s_Data->TempQuadVertices));
	}

	void Renderer2D::DrawTexturedQuadInternal(
		const glm::vec3& position,
		const glm::vec2& size,
		const glm::vec4& color,
		const Ref<Texture2D>& texture,
		const glm::vec2 texCoords[4])
	{
		UploadQuadUVs(texCoords);

		s_Data->TextureShader->Bind();
		s_Data->TextureShader->SetFloat4("u_Color", color);

		texture->Bind();

		glm::mat4 transform =
			glm::translate(glm::mat4(1.0f), position) *
			glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		s_Data->TextureShader->SetMat4("u_Transform", transform);

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
	}


	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		// Bind white texture i.e in the shader u_Color will be equal to 1 if not bound
		DrawTexturedQuadInternal(
			position,
			size,
			color,
			s_Data->WhiteTexture,
			s_FullTextureCoords
		);
	}

	void Renderer2D::DrawQuad(const Quad& q)
	{

		Ref<Shader> shader = SelectShader(
			q.Shader,               // meshShader / per-object shader
			s_Data->OverrideShader, // global override if set
			s_Data->DefaultShader   // fallback (e.g. TextureShader)
		);

		shader->Bind();
		//shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);

		const glm::mat4 T =
			q.Transform != glm::mat4(1.0f)
			? q.Transform
			: glm::translate(glm::mat4(1.0f), q.Position) *
			glm::scale(glm::mat4(1.0f), { q.Size.x, q.Size.y, 1.0f });

		shader->SetMat4("u_Transform", T);
		shader->SetFloat4("u_Color", q.Color);

		(q.Texture ? q.Texture : s_Data->WhiteTexture)->Bind();

		s_Data->QuadVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data->QuadVertexArray);

	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture)
	{
		DrawTexturedQuadInternal(
			position,
			size,
			glm::vec4(1.0f),     // no tint
			texture,
			s_FullTextureCoords  // full texture UVs
		);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
		const Ref<SubTexture2D>& subtexture, const glm::vec4& tint)
	{
		const glm::vec2* texCoords = subtexture->GetTexCoords();
		const auto texture = subtexture->GetTexture();

		DrawTexturedQuadInternal(
			position,
			size,
			tint,
			texture,
			texCoords // atlas UVs
		);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
		const Ref<SubTexture2D>& subtexture, const glm::vec4& tint)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, subtexture, tint);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
		const Ref<Texture2D>& texture, const glm::vec4& tint)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tint);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
		const Ref<Texture2D>& texture, const glm::vec4& tint)
	{
		DrawTexturedQuadInternal(
			position,
			size,
			tint,
			texture,
			s_FullTextureCoords
		);
	}

	void Renderer2D::DrawString(const std::string& string, Ref<Font> font, const glm::mat4& transform, const glm::vec4& color)
	{
		const auto& fontGeometry = font->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		auto fontAtlas = font->GetAtlasTexture();

		//bind atlas texture and text shader
		fontAtlas->Bind(0);
		s_Data->TextShader->Bind();

		s_Data->TextShader->SetMat4("u_ViewProjection", s_Data->ViewProjectionMatrix);
		s_Data->TextShader->SetMat4("u_Transform", transform);
		s_Data->TextShader->SetInt("u_FontAtlas", 0); // texture slot is 0 

		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		double x = 0.0;
		double y = 0.0;

		//needed to calc the size of each pixel in the atlas
		float texelWidth = 1.0f / fontAtlas->GetWidth();
		float texelHeight = 1.0f / fontAtlas->GetHeight();

		for (size_t i = 0; i < string.size(); i++)
		{
			char character = string[i];

			auto glyph = fontGeometry.getGlyph(character);

			if (!glyph)
			{
				glyph = fontGeometry.getGlyph('?');
			}

			if (!glyph)
			{
				HMN_CORE_ERROR("Font does not have glyph for '{0}' or fallback '?'", character);
				continue;
			}

			//calc the geometry below
			double al, ab, ar, at;
			glyph->getQuadAtlasBounds(al, ab, ar, at);
			//texture coords needed to send to the frag shader
			glm::vec2 texCoordMin((float)al, (float)ab);
			glm::vec2 texCoordMax((float)ar, (float)at);

			double pl, pb, pr, pt;
			glyph->getQuadPlaneBounds(pl, pb, pr, pt);
			glm::vec2 quadMin((float)pl, (float)pb);
			glm::vec2 quadMax((float)pr, (float)pt);
			quadMin *= fsScale, quadMax *= fsScale;
			quadMin += glm::vec2(x, y);
			quadMax += glm::vec2(x, y);

			texCoordMin *= glm::vec2(texelWidth, texelHeight);
			texCoordMax *= glm::vec2(texelWidth, texelHeight);

			//text rendering below

		   // Create and render this letter vertice based on values from MSDF calcs
			TextVertex vertices[4] = {
				{ glm::vec3(quadMin.x, quadMin.y, 0.0f), color, texCoordMin },
				{ glm::vec3(quadMax.x, quadMin.y, 0.0f), color, {texCoordMax.x, texCoordMin.y} },
				{ glm::vec3(quadMax.x, quadMax.y, 0.0f), color, texCoordMax },
				{ glm::vec3(quadMin.x, quadMax.y, 0.0f), color, {texCoordMin.x, texCoordMax.y} }
			};

			s_Data->TextVertexBuffer->SetData(vertices, sizeof(vertices));
			s_Data->TextVertexArray->Bind();
			RenderCommand::DrawIndexed(s_Data->TextVertexArray);

			double advance = glyph->getAdvance();
			if (i < string.size() - 1)
			{
				char nextCharacter = string[i + 1];
				fontGeometry.getAdvance(advance, character, nextCharacter);
			}

			float kerningOffset = 1.0f; //this allows space between each character of the string
			x += fsScale * advance + kerningOffset;
		}
	}
}
