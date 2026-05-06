#pragma once

#include "Hominem/Scene/Actor.h"
#include "Hominem/Scene/Scene.h"
#include "Hominem/Renderer/Renderer2D.h"
#include "Hominem/Renderer/Texture.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/// Infinitely tiling background.
/// - Scrolls horizontally with the camera (GL_REPEAT on S).
/// - Sky extends upward infinitely: a second quad above the image samples
///   UV.y = 1.0 everywhere — with GL_CLAMP_TO_EDGE that is always the top
///   edge row of pixels, so the sky colour bleeds up with zero artifacts.
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

	void OnDraw2D() override
	{
		if (!m_Texture || !m_Scene) return;

		float camX = m_Scene->GetCameraPosition().x;

		constexpr float k_SideBuffer = 1.5f;
		float quadW  = TileWidth * (2.f * k_SideBuffer + 1.f);
		float uvMinX = (camX - quadW * 0.5f) / TileWidth;
		float uvMaxX = (camX + quadW * 0.5f) / TileWidth;

		// Sky extension — UV.y uses a tiny non-zero range to avoid degenerate LOD.
		float imageTopY = NaturalHeight * 0.5f;
		float skyMidY   = imageTopY + SkyExtent * 0.5f;
		{
			glm::mat4 t =
				glm::translate(glm::mat4(1.f), glm::vec3(camX, skyMidY, ZDepth))
				* glm::scale(glm::mat4(1.f), glm::vec3(quadW, SkyExtent, 1.f));

			Hominem::Renderer2D::DrawQuad(t, m_Texture, { uvMinX, 0.999f }, { uvMaxX, 1.f });
		}

		// Background image at its natural size, UV.y in [0, 1].
		{
			glm::mat4 t =
				glm::translate(glm::mat4(1.f), glm::vec3(camX, 0.f, ZDepth))
				* glm::scale(glm::mat4(1.f), glm::vec3(quadW, NaturalHeight, 1.f));

			Hominem::Renderer2D::DrawQuad(t, m_Texture, { uvMinX, 0.f }, { uvMaxX, 1.f });
		}
	}

private:
	Hominem::Ref<Hominem::Texture2D> m_Texture;
};
