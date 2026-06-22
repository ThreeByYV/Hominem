#include "hmnpch.h"
#include "MenuLayer.h"
#include "LoadingLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/MouseCodes.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/Scene/Actors/SpriteActor.h"
#include "Hominem/Utils/MathUtils.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Hominem/Core/KeyCodes.h"

using namespace Hominem;

MenuLayer::MenuLayer()
	: Layer("Menu")
{
}

void MenuLayer::OnAttach()
{
	m_Scene = CreateRef<Scene>();

	auto& window = Application::Get().GetWindow();
	m_ViewportW  = window.GetWidth();
	m_ViewportH  = window.GetHeight();

	m_Scene->GetCamera().SetOrthographic(k_OrthoSize, -1.f, 1.f);
	m_Scene->OnViewportResize(m_ViewportW, m_ViewportH);
	m_Scene->SetClearColor({ 0.05f, 0.05f, 0.05f, 1.f });

	float aspect = (float)m_ViewportW / (float)m_ViewportH;
	if (const auto result=
			AssetManager::Load<Texture2D>("game://Textures/menu2.png"))
		m_BackgroundHandle = *result;

	m_Background = &m_Scene->SpawnActor<SpriteActor>(
		glm::vec3{ 0.f, 0.f, -0.5f },
		glm::vec3{ k_OrthoSize * aspect, k_OrthoSize, 1.f },
		m_BackgroundHandle.Get());

	m_Font   = Font::GetDefaultFont();
	m_ArrowY = k_Items[0].y;

	m_Music = AssetManager::LoadAsync<SoundBuffer>("game://Sounds/menu_music.mp3");
	if (m_Music.IsLoaded())
		m_MusicHandle = AudioSystem::Get().Play(m_Music, 0.9f, /*loop=*/true);
}

void MenuLayer::OnDetach()
{
	if (m_MusicHandle != InvalidSound)
		AudioSystem::Get().Stop(m_MusicHandle);
	m_Music       = {};
	m_MusicHandle = InvalidSound;
	m_Scene.reset();
	m_Background = nullptr;
}

void MenuLayer::OnUpdate(Timestep ts)
{
	if (m_Music.IsLoaded() && m_MusicHandle == InvalidSound && !m_MusicPaused)
		m_MusicHandle = AudioSystem::Get().Play(m_Music, 0.9f, /*loop=*/true);

	if (Input::IsKeyPressed(HMN_KEY_M))
	{
		if (m_MusicPaused)
		{
			AudioSystem::Get().Resume(m_MusicHandle);
			m_MusicPaused = false;
		}
		else
		{
			AudioSystem::Get().Pause(m_MusicHandle);
			m_MusicPaused = true;
		}
	}

	// Convert mouse pixel coords to world space (camera at origin, ortho).
	float halfH  = k_OrthoSize * 0.5f;
	float halfW  = halfH * ((float)m_ViewportW / (float)m_ViewportH);
	float ndcX   = (Input::GetMouseX() / (float)m_ViewportW) * 2.f - 1.f;
	float ndcY   = 1.f - (Input::GetMouseY() / (float)m_ViewportH) * 2.f;
	float worldX = ndcX * halfW;
	float worldY = ndcY * halfH;

	m_HoveredIndex = -1;
	for (int i = 0; i < (int)k_Items.size(); ++i)
	{
		if (std::abs(worldY - k_Items[i].y) < k_HoverBand
		    && worldX >= k_ArrowX && worldX <= k_ItemX + 0.9f)
		{
			m_HoveredIndex = i;
			break;
		}
	}

	if (m_HoveredIndex >= 0)
		m_ArrowY = Lerp(m_ArrowY, k_Items[m_HoveredIndex].y, k_LerpSpeed * ts.GetSeconds());

	m_Scene->OnUpdate(ts);
}

void MenuLayer::FireMenuAction(int index)
{
	switch (index)
	{
		case 0: // Continue
		case 1: // New Game
			TransitionTo<LoadingLayer>();
			break;
		case 4: // Quit
			Application::Get().Close();
			break;
		default:
			break;
	}
}

void MenuLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	Layer::OnBuildRenderFrame(frame);

	if (!m_Font) return;

	static const glm::vec4 k_TitleColor  = { 0.85f, 0.62f, 0.38f, 1.f };
	static const glm::vec4 k_NormalColor = { 0.75f, 0.75f, 0.75f, 1.f };
	static const glm::vec4 k_HoverLeft   = { 0.95f, 0.55f, 0.25f, 1.f };
	static const glm::vec4 k_ArrowColor  = { 0.85f, 0.62f, 0.38f, 1.f };

	// Title
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), glm::vec3(k_TitleX, k_TitleY, 0.f))
		            * glm::scale(glm::mat4(1.f), glm::vec3(k_TitleScale));
		frame.texts.push_back({ "Hominem", m_Font, t, k_TitleColor });
	}

	// Divider — thin horizontal line with a small diamond centered on it,
	// starting flush with the title's left edge.
	{
		const glm::vec4 dividerColor = { 0.85f, 0.62f, 0.38f, 0.6f };
		const float dividerY    = k_TitleY - k_DividerGap;
		const float lineCenterX = k_TitleX + k_DividerWidth * 0.5f;

		QuadDraw line;
		line.transform = glm::translate(glm::mat4(1.f), glm::vec3(lineCenterX, dividerY, 0.f))
		               * glm::scale(glm::mat4(1.f), glm::vec3(k_DividerWidth, k_DividerHeight, 1.f));
		line.color = dividerColor;
		frame.quads.push_back(line);

		QuadDraw diamond;
		diamond.transform = glm::translate(glm::mat4(1.f), glm::vec3(lineCenterX, dividerY, 0.f))
		                   * glm::rotate(glm::mat4(1.f), glm::radians(45.f), glm::vec3(0.f, 0.f, 1.f))
		                   * glm::scale(glm::mat4(1.f), glm::vec3(k_DiamondSize));
		diamond.color = dividerColor;
		frame.quads.push_back(diamond);
	}

	// Menu items
	for (int i = 0; i < (int)k_Items.size(); ++i)
	{
		const bool hovered = (i == m_HoveredIndex);
		glm::mat4 t = glm::translate(glm::mat4(1.f), glm::vec3(k_ItemX, k_Items[i].y, 0.f))
		            * glm::scale(glm::mat4(1.f), glm::vec3(k_ItemScale));

		TextDraw draw;
		draw.text       = k_Items[i].label;
		draw.font       = m_Font;
		draw.transform  = t;
		draw.color      = hovered ? k_HoverLeft : k_NormalColor;
		draw.colorRight = hovered ? k_NormalColor : glm::vec4(-1.f);
		frame.texts.push_back(draw);
	}

	// Arrow — only shown while something is hovered, lerps to the target item
	if (m_HoveredIndex >= 0)
	{
		glm::mat4 t = glm::translate(glm::mat4(1.f), glm::vec3(k_ArrowX, m_ArrowY, 0.f))
		            * glm::scale(glm::mat4(1.f), glm::vec3(k_ItemScale));
		frame.texts.push_back({ ">", m_Font, t, k_ArrowColor });
	}
}

bool MenuLayer::OnWindowResize(WindowResizeEvent& e)
{
	m_ViewportW = e.GetWidth();
	m_ViewportH = e.GetHeight();

	if (m_Scene)
		m_Scene->OnViewportResize(m_ViewportW, m_ViewportH);

	if (m_Background)
	{
		float aspect = (float)m_ViewportW / (float)m_ViewportH;
		m_Background->Scale = glm::vec3{ k_OrthoSize * aspect, k_OrthoSize, 1.f };
	}

	return false;
}

bool MenuLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
{
	if (e.GetMouseButton() == Mouse::ButtonLeft && m_HoveredIndex >= 0)
	{
		FireMenuAction(m_HoveredIndex);
		return true;
	}
	return false;
}

void MenuLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(MenuLayer::OnWindowResize));
	dispatcher.Dispatch<MouseButtonPressedEvent>(HMN_BIND_EVENT_FN(MenuLayer::OnMouseButtonPressed));
}
