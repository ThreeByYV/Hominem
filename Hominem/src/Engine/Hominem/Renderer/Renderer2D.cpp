#include "hmnpch.h"
#include "Renderer2D.h"
#include "Hominem/Core/Core.h"

#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "RenderCommand.h"
#include "StorageBuffer.h"

#include "glm/gtc/matrix_transform.hpp"
#include "Hominem/Utils/Renderer.h"

#include "MSDFData.h"

namespace Hominem {

	Renderer2DStorage* Renderer2D::s_Data = nullptr;

	namespace {
		const glm::vec2 s_FullUVMin = { 0.0f, 0.0f };
		const glm::vec2 s_FullUVMax = { 1.0f, 1.0f };

		// Mirrors the layout expected by glDrawArraysIndirect
		struct DrawArraysIndirectCommand
		{
			uint32_t count;         // vertex count  (quadCount * 6)
			uint32_t instanceCount; // always 1
			uint32_t first;         // first vertex  (firstQuad * 6)
			uint32_t baseInstance;  // always 0
		};
	}

	void Renderer2D::Init()
	{
		s_Data = new Renderer2DStorage();
		s_Data->ShaderLibrary = CreateRef<ShaderLibrary>();

		s_Data->QuadBatch.reserve(Renderer2DStorage::MaxQuads);

		s_Data->QuadSSBO = StorageBuffer::Create(
			sizeof(QuadSSBOData) * Renderer2DStorage::MaxQuads);

		// One DrawArraysIndirectCommand per texture group, worst case = MaxQuads
		s_Data->IndirectBuffer = DrawIndirectBuffer::Create(
			sizeof(DrawArraysIndirectCommand) * Renderer2DStorage::MaxQuads);

		s_Data->BatchPVPShader = s_Data->ShaderLibrary->Load("engine://Shaders/texture_batch.glsl");

		// as fallback use the unit-quad VAO for custom-shader draws
		s_Data->QuadVertexArray = VertexArray::Create();

		float unitQuad[5 * 4] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};

		s_Data->QuadVBO = VertexBuffer::Create(unitQuad, sizeof(unitQuad));
		s_Data->QuadVBO->SetLayout({
			{ ShaderDataType::Float3, "a_Pos" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});
		s_Data->QuadVertexArray->AddVertexBuffer(s_Data->QuadVBO);

		uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };
		s_Data->QuadIBO = IndexBuffer::Create(indices, ARRAY_SIZE_IN_ELEMENTS(indices));
		s_Data->QuadVertexArray->SetIndexBuffer(s_Data->QuadIBO);

		// Shared resources
		s_Data->WhiteTexture = Texture2D::Create(1, 1);
		uint32_t white = 0xffffffff;
		s_Data->WhiteTexture->SetData(&white, sizeof(uint32_t));
		s_Data->WhiteTexture->QueueUpload();

		s_Data->TextureShader = s_Data->ShaderLibrary->Load("engine://Shaders/texture.glsl");
		s_Data->DefaultShader = s_Data->ShaderLibrary->Get("texture");
		s_Data->OverrideShader = nullptr;

		s_Data->TextureShader->Bind();
		s_Data->TextureShader->SetInt("u_Texture", 0);

		// Text
		s_Data->TextVertexArray  = VertexArray::Create();
		s_Data->TextVertexBuffer = VertexBuffer::Create(Renderer2DStorage::MaxQuads * sizeof(TextVertex));
		s_Data->TextVertexArray->SetIndexBuffer(s_Data->QuadIBO);
		s_Data->TextVertexBuffer->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color"    },
			{ ShaderDataType::Float2, "a_TexCoord" },
		});
		s_Data->TextVertexArray->AddVertexBuffer(s_Data->TextVertexBuffer);
		s_Data->TextShader = Shader::Create("engine://Shaders/text.glsl");
	}

	void Renderer2D::Shutdown()
	{
		// Ref<> destructors release GPU buffers
		delete s_Data;
	}

	void Renderer2D::BeginScene(OrthographicCamera& camera)
	{
		s_Data->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
		s_Data->QuadBatch.clear();
		s_Data->TextureGroups.clear();

		s_Data->TextureShader->Bind();
		s_Data->TextureShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
	}

	void Renderer2D::BeginScene(Camera& camera, const glm::mat4& transform)
	{
		glm::mat4 viewProj = camera.GetProjectionMatrix() * glm::inverse(transform);
		s_Data->ViewProjectionMatrix = viewProj;
		s_Data->QuadBatch.clear();
		s_Data->TextureGroups.clear();

		s_Data->TextureShader->Bind();
		s_Data->TextureShader->SetMat4("u_ViewProjection", viewProj);
	}

	void Renderer2D::BeginScene(const glm::mat4& viewProjection)
	{
		s_Data->ViewProjectionMatrix = viewProjection;
		s_Data->QuadBatch.clear();
		s_Data->TextureGroups.clear();

		s_Data->TextureShader->Bind();
		s_Data->TextureShader->SetMat4("u_ViewProjection", viewProjection);
	}

	void Renderer2D::EndScene()
	{
		if (!s_Data) return;
		Flush();
		Shader::UnbindAll();
		Texture::UnbindAll();
	}

	void Renderer2D::Flush()
	{
		HMN_PROFILE_FUNCTION();
		if (s_Data->QuadBatch.empty()) return;

		s_Data->QuadSSBO->SetData(
			s_Data->QuadBatch.data(),
			static_cast<uint32_t>(sizeof(QuadSSBOData) * s_Data->QuadBatch.size()));

		std::vector<DrawArraysIndirectCommand> cmds;
		cmds.reserve(s_Data->TextureGroups.size());
		for (const auto& group : s_Data->TextureGroups)
			cmds.push_back({ group.QuadCount * 6, 1, group.FirstQuad * 6, 0 });

		s_Data->IndirectBuffer->SetData(cmds.data(),
			static_cast<uint32_t>(sizeof(DrawArraysIndirectCommand) * cmds.size()));

		Ref<Shader> shader = s_Data->OverrideShader ? s_Data->OverrideShader : s_Data->BatchPVPShader;
		shader->Bind();
		shader->SetMat4("u_ViewProjection", s_Data->ViewProjectionMatrix);
		shader->SetInt("u_Texture", 0);

		s_Data->QuadSSBO->BindBase(0);
		s_Data->IndirectBuffer->Bind();
		RenderCommand::BindEmptyVAO();

		for (size_t i = 0; i < s_Data->TextureGroups.size(); i++)
		{
			auto& texRef = s_Data->TextureGroups[i].Texture;
			(texRef ? texRef : s_Data->WhiteTexture)->Bind();
			RenderCommand::DrawArraysIndirect(
				static_cast<uint32_t>(i * sizeof(DrawArraysIndirectCommand)));
		}

		RenderCommand::UnbindVAO();
		s_Data->IndirectBuffer->Unbind();

		s_Data->QuadBatch.clear();
		s_Data->TextureGroups.clear();
	}

	void Renderer2D::PushQuad(const glm::mat4& transform, const glm::vec4& color,
	                          const glm::vec2& uvMin, const glm::vec2& uvMax,
	                          const Ref<Texture2D>& texture)
	{
		const Ref<Texture2D>& tex = texture ? texture : s_Data->WhiteTexture;

		if (s_Data->QuadBatch.size() >= Renderer2DStorage::MaxQuads)
			Flush();

		// Extend current group or start a new one when texture changes
		if (s_Data->TextureGroups.empty() || s_Data->TextureGroups.back().Texture != tex)
			s_Data->TextureGroups.push_back({ tex, static_cast<uint32_t>(s_Data->QuadBatch.size()), 0 });

		s_Data->TextureGroups.back().QuadCount++;
		s_Data->QuadBatch.push_back({ transform, color, uvMin, uvMax });
	}

	Ref<ShaderLibrary> Renderer2D::GetShaderLibrary()
	{
		return s_Data->ShaderLibrary;
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		glm::mat4 T = glm::translate(glm::mat4(1.0f), position)
		            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		PushQuad(T, color, s_FullUVMin, s_FullUVMax, s_Data->WhiteTexture);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color)
	{
		PushQuad(transform, color, s_FullUVMin, s_FullUVMax, s_Data->WhiteTexture);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture)
	{
		PushQuad(transform, glm::vec4(1.0f), s_FullUVMin, s_FullUVMax, texture);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec4& tint)
	{
		PushQuad(transform, tint, s_FullUVMin, s_FullUVMax, texture);
	}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture,
	                          const glm::vec2& uvMin, const glm::vec2& uvMax)
	{
		PushQuad(transform, glm::vec4(1.f), uvMin, uvMax, texture);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture)
	{
		glm::mat4 T = glm::translate(glm::mat4(1.0f), position)
		            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		PushQuad(T, glm::vec4(1.0f), s_FullUVMin, s_FullUVMax, texture);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
		const Ref<Texture2D>& texture, const glm::vec4& tint)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, texture, tint);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
		const Ref<Texture2D>& texture, const glm::vec4& tint)
	{
		glm::mat4 T = glm::translate(glm::mat4(1.0f), position)
		            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		PushQuad(T, tint, s_FullUVMin, s_FullUVMax, texture);
	}

	void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
		const Ref<SubTexture2D>& subtexture, const glm::vec4& tint)
	{
		DrawQuad({ position.x, position.y, 0.0f }, size, subtexture, tint);
	}

	void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
		const Ref<SubTexture2D>& subtexture, const glm::vec4& tint)
	{
		const glm::vec2* coords = subtexture->GetTexCoords();
		glm::mat4 T = glm::translate(glm::mat4(1.0f), position)
		            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
		// coords[0]=BL, coords[2]=TR — axis-aligned atlas rect
		PushQuad(T, tint, coords[0], coords[2], subtexture->GetTexture());
	}

	void Renderer2D::DrawQuad(const Quad& q)
	{
		// Custom per-quad shader: flush the batch and draw immediately
		// (custom shaders may not have the SSBO interface)
		if (q.Shader)
		{
			Flush();

			const glm::mat4 T = q.Transform != glm::mat4(1.0f)
				? q.Transform
				: glm::translate(glm::mat4(1.0f), q.Position)
				* glm::scale(glm::mat4(1.0f), { q.Size.x, q.Size.y, 1.0f });

			q.Shader->Bind();
			q.Shader->SetMat4("u_ViewProjection", s_Data->ViewProjectionMatrix);
			q.Shader->SetMat4("u_Transform", T);
			q.Shader->SetFloat4("u_Color", q.Color);
			(q.Texture ? q.Texture : s_Data->WhiteTexture)->Bind();
			s_Data->QuadVertexArray->Bind();
			RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
			return;
		}

		const glm::mat4 T = q.Transform != glm::mat4(1.0f)
			? q.Transform
			: glm::translate(glm::mat4(1.0f), q.Position)
			* glm::scale(glm::mat4(1.0f), { q.Size.x, q.Size.y, 1.0f });

		PushQuad(T, q.Color, s_FullUVMin, s_FullUVMax, q.Texture);
	}

	void Renderer2D::DrawString(const std::string& string, Ref<Font> font, const glm::mat4& transform, const glm::vec4& color)
	{
		const auto& fontGeometry = font->GetMSDFData()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();
		auto fontAtlas = font->GetAtlasTexture();

		fontAtlas->Bind(0);
		s_Data->TextShader->Bind();

		s_Data->TextShader->SetMat4("u_ViewProjection", s_Data->ViewProjectionMatrix);
		s_Data->TextShader->SetMat4("u_Transform", transform);
		s_Data->TextShader->SetInt("u_FontAtlas", 0);

		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		double x = 0.0;
		double y = 0.0;

		float texelWidth  = 1.0f / fontAtlas->GetWidth();
		float texelHeight = 1.0f / fontAtlas->GetHeight();

		for (size_t i = 0; i < string.size(); i++)
		{
			char character = string[i];
			auto glyph = fontGeometry.getGlyph(character);

			if (!glyph) glyph = fontGeometry.getGlyph('?');
			if (!glyph)
			{
				HMN_CORE_ERROR("Font does not have glyph for '{0}' or fallback '?'", character);
				continue;
			}

			double al, ab, ar, at;
			glyph->getQuadAtlasBounds(al, ab, ar, at);
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
				fontGeometry.getAdvance(advance, character, string[i + 1]);

			x += fsScale * advance;
		}
	}
}
