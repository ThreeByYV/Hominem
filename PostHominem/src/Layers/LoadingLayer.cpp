#include "hmnpch.h"
#include "LoadingLayer.h"
#include "GameLayer.h"

#include "Hominem/Renderer/Font.h"
#include "Hominem/Utils/Transform.h"
#include "Cinematics/IntroCutscene.h"
#include "Game/Player.h"

#include <glm/gtc/matrix_transform.hpp>
#include <random>

using namespace Hominem;

LoadingLayer::LoadingLayer()
	: Layer("Loading")
{
}

void LoadingLayer::OnAttach()
{
	m_Font = Font::GetDefaultFont();

	std::mt19937 rng(std::random_device{}());
	m_QuoteIndex = std::uniform_int_distribution<int>(0, (int)k_Quotes.size() - 1)(rng);

	AssetManager::LoadAsync<StaticMesh>(IntroCutscene::k_SetMesh,
		[this](const AssetHandle<StaticMesh>& h) { m_SetMesh = h; OnAssetLoaded(); });

	AssetManager::LoadAsync<SkinnedMesh>(Player::k_MeshPath,
		[this](const AssetHandle<SkinnedMesh>& h) { m_PlayerMesh = h; OnAssetLoaded(); });

	AssetManager::LoadAsync<SoundBuffer>(GameLayer::k_MusicPath,
		[this](const AssetHandle<SoundBuffer>& h) { m_GameMusic = h; OnAssetLoaded(); });

	// Intro cutscene's silhouette runner/idle meshes -- without this, CutsceneLayer kicks off
	// these same loads itself on attach and the characters visibly pop in once they finish.
	AssetManager::LoadAsync<SkinnedMesh>(IntroCutscene::k_RunMesh,
		[this](const AssetHandle<SkinnedMesh>& h) { m_RunMesh = h; OnAssetLoaded(); });

	AssetManager::LoadAsync<SkinnedMesh>(IntroCutscene::k_IdleMesh,
		[this](const AssetHandle<SkinnedMesh>& h) { m_IdleMesh = h; OnAssetLoaded(); });
}

void LoadingLayer::OnUpdate(Timestep ts)
{
	m_PulseT += static_cast<float>(ts);
}

void LoadingLayer::OnAssetLoaded()
{
	if (--m_PendingLoads == 0 && !m_Transitioning)
	{
		m_Transitioning = true;
		TransitionTo<GameLayer>();
	}
}

void LoadingLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	SetFullscreenClear(frame, { 0.f, 0.f, 0.f, 1.f });

	if (!m_Font || frame.viewportHeight == 0)
		return;

	// No Scene here to derive a view-projection from, so build a screen-space ortho
	// directly for the 2D text.
	const float aspect = (float)frame.viewportWidth / (float)frame.viewportHeight;
	frame.viewProjection2D = glm::ortho(-aspect, aspect, -1.f, 1.f, -1.f, 1.f);

	const std::string text(k_Quotes[m_QuoteIndex]);

	constexpr float kTextScale    = 0.1f;
	constexpr float kBottomMargin = 0.15f;

	// For centering, measure the longest line (quotes may contain \n).
	float maxLineWidth = 0.f;
	{
		size_t prev = 0, pos = 0;
		while ((pos = text.find('\n', prev)) != std::string::npos)
		{
			maxLineWidth = std::max(maxLineWidth, m_Font->MeasureWidth(text.substr(prev, pos - prev)));
			prev = pos + 1;
		}
		maxLineWidth = std::max(maxLineWidth, m_Font->MeasureWidth(text.substr(prev)));
	}
	const float halfWidth = maxLineWidth * 0.5f * kTextScale;

	// Raise the block so the bottom line always sits at the same distance from the screen edge,
	// regardless of how many lines the quote has.
	constexpr float kLineSpacing = 1.3f; // must match DrawStringMultiline default
	int lineCount = 1;
	for (char c : text) if (c == '\n') ++lineCount;
	const float blockY = -1.f + kBottomMargin + (lineCount - 1) * kTextScale * kLineSpacing;

	// Slow breathing fade on text here
	const float pulse = 0.3f + 0.5f * (0.5f + 0.5f * std::sin(m_PulseT * 1.5f));

	glm::mat4 transform = Transform::PosScale({ -halfWidth, blockY, 0.f }, glm::vec3(kTextScale)).ToMatrix();
	frame.texts.push_back({ text, m_Font, transform, { 1.f, 1.f, 1.f, pulse } });
}
