#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Texture.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


class InfiniteBackgroundActor : public Hominem::Actor
{
public:
	float TileWidth     = 20.f;  // world width of one image repeat
	float NaturalHeight = 12.f;  // world height the image occupies
	float ZDepth        = -5.f;
	float SkyExtent     = 200.f; // how far above the image the sky extends (world units)

	InfiniteBackgroundActor(Hominem::Ref<Hominem::Texture2D> texture, float tileWidth = 20.f, float naturalHeight = 12.f)
		: m_Texture(std::move(texture))
	{
		TileWidth     = tileWidth;
		NaturalHeight = naturalHeight;
	}

	void OnCreate() override
	{
		if (m_Texture)
		{
			m_Texture->SetWrapS(Hominem::TextureWrap::Repeat);
			m_Texture->SetWrapT(Hominem::TextureWrap::ClampToEdge);
		}
	}

	void OnBuildRenderFrame(Hominem::RenderFrame& frame) override
	{
		if (!m_Texture || !m_Scene) return;

		float camX = m_Scene->GetCameraPosition().x;

		constexpr float k_SideBuffer = 1.5f;
		float quadW  = TileWidth * (2.f * k_SideBuffer + 1.f);
		float uvMinX = (camX - quadW * 0.5f) / TileWidth;
		float uvMaxX = (camX + quadW * 0.5f) / TileWidth;

		float imageTopY = NaturalHeight * 0.5f;
		float skyMidY   = imageTopY + SkyExtent * 0.5f;

		Hominem::QuadDraw sky;
		sky.transform = glm::translate(glm::mat4(1.f), glm::vec3(camX, skyMidY, ZDepth))
		              * glm::scale(glm::mat4(1.f), glm::vec3(quadW, SkyExtent, 1.f));
		sky.texture = m_Texture;
		sky.uvMin   = { uvMinX, 0.999f };
		sky.uvMax   = { uvMaxX, 1.f };
		frame.quads.push_back(std::move(sky));

		Hominem::QuadDraw bg;
		bg.transform = glm::translate(glm::mat4(1.f), glm::vec3(camX, 0.f, ZDepth))
		             * glm::scale(glm::mat4(1.f), glm::vec3(quadW, NaturalHeight, 1.f));
		bg.texture = m_Texture;
		bg.uvMin   = { uvMinX, 0.f };
		bg.uvMax   = { uvMaxX, 1.f };
		frame.quads.push_back(std::move(bg));
	}

private:
	Hominem::Ref<Hominem::Texture2D> m_Texture;
};
