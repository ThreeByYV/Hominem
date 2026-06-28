#include "hmnpch.h"
#include "CutsceneLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/MouseButtonCodes.h"
#include "Hominem/Utils/MathUtils.h"
#include "Hominem/Utils/Transform.h"
#include "Hominem/Cinematics/CutsceneLoader.h"
#include "Game/Actors/SceneActor.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>
#include "Hominem/ImGui/UI.h"

using namespace Hominem;

CutsceneLayer::CutsceneLayer(CutsceneDesc desc)
	: Layer("Cutscene"), m_Desc(std::move(desc))
{
}

void CutsceneLayer::OnAttach()
{
	m_Scene = CreateRef<Scene>();

	// Seed live-tweakable values from desc
	m_CamFOV       = m_Desc.camera.fov;
	m_CamPos       = m_Desc.camera.pos;
	m_CamTarget    = m_Desc.camera.target;
	m_SetPos       = m_Desc.set.pos;
	m_SetScale     = m_Desc.set.scale;
	m_SetRotDeg    = m_Desc.set.rotDeg;
	m_SkyIntensity = m_Desc.light.skyIntensity;

	auto& window = Application::Get().GetWindow();
	const float aspect = (float)window.GetWidth() / (float)window.GetHeight();
	m_Scene->GetCamera().SetPerspective(m_CamFOV, aspect, 0.1f, 1000.f);
	m_Scene->OnViewportResize(window.GetWidth(), window.GetHeight());
	m_Scene->SetClearColor({ 0.f, 0.f, 0.f, 1.f });

	auto& dl = m_Scene->GetDirectionalLight();
	dl.Direction        = glm::normalize(m_Desc.light.dir);
	dl.Color            = m_Desc.light.color;
	dl.AmbientIntensity = m_Desc.light.ambient;
	dl.DiffuseIntensity = m_Desc.light.diffuse;

	if (!m_Desc.assets.setMesh.empty())
	{
		// 3D set (guaranteed in cache from LoadingLayer preload; sync is a cache hit)
		if (auto r = AssetManager::Load<StaticMesh>(m_Desc.assets.setMesh))
			m_Set = &m_Scene->SpawnActor<SceneActor>(r->Get());
	}
	ApplyFraming();

	// Skybox
	if (!m_Desc.assets.skybox.empty())
	{
		if (auto r = AssetManager::Load<Skybox>(m_Desc.assets.skybox))
			m_Skybox = r->Get();
		m_Scene->SetSkybox(m_Skybox, m_SkyIntensity);
	}

	// Procedural effects — copy from desc so F4/F5 panels can edit the live list
	m_FireInstances  = m_Desc.fire;
	m_SmokeInstances = m_Desc.smoke;

	// Async character meshes
	for (const auto& path : m_Desc.spawnMeshPaths)
		m_SpawnMeshHandles.push_back(AssetManager::LoadAsync<SkinnedMesh>(path));

	// Cutscene cue list
	m_Ctx.cutscene = &m_Cutscene;
	m_Ctx.scene    = m_Scene.get();

	if (!m_Desc.assets.cutsceneJson.empty())
	{
		if (!CutsceneLoader::LoadFromFile(m_Desc.assets.cutsceneJson, m_Cutscene))
			HMN_CORE_WARN("CutsceneLayer: failed to load '{}'", m_Desc.assets.cutsceneJson);
		m_Editor.SavePath = m_Desc.assets.cutsceneJson;
	}

	m_Cutscene.SetOnFinished([this]
	{
		if (m_Desc.onComplete) m_Desc.onComplete();
		m_TransitionRequested = true;
	});
	m_Cutscene.Play();
}

void CutsceneLayer::OnDetach()
{
	m_SpawnMeshHandles.clear();
	m_Scene.reset();
}

void CutsceneLayer::ApplyFraming()
{
	m_Scene->GetCamera().SetPerspectiveFOV(m_CamFOV);
	m_Scene->GetCameraPosition() = m_CamPos;
	m_Scene->GetCameraFront()    = glm::normalize(m_CamTarget - m_CamPos);
	if (m_Set)
	{
		m_Set->Position = m_SetPos;
		m_Set->Scale    = m_SetScale;
		m_Set->Rotation = glm::radians(m_SetRotDeg);
	}
}

void CutsceneLayer::UpdateFreeFlyCamera(Timestep ts)
{
	if (!m_ShowFraming)
	{
		m_FlyLooking = false;
		return;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const auto [mx, my] = Input::GetMousePosition();
	const glm::vec2 mouse = { mx, my };
	const bool lmbHeld = Input::IsMouseButtonPressed(HMN_MOUSE_BUTTON_LEFT) && !io.WantCaptureMouse;

	if (lmbHeld && !m_FlyLooking)
	{
		const glm::vec3 front = glm::normalize(m_CamTarget - m_CamPos);
		m_FlyYaw    = std::atan2(front.z, front.x);
		m_FlyPitch  = std::asin(glm::clamp(front.y, -1.f, 1.f));
		m_LastMouse = mouse;
		m_FlyLooking = true;
	}
	else if (!lmbHeld)
	{
		m_FlyLooking = false;
	}

	glm::vec3 front = glm::normalize(m_CamTarget - m_CamPos);

	if (m_FlyLooking)
	{
		const glm::vec2 delta = mouse - m_LastMouse;
		constexpr float kLookSpeed = 0.0025f;
		m_FlyYaw   += delta.x * kLookSpeed;
		m_FlyPitch -= delta.y * kLookSpeed;
		m_FlyPitch  = glm::clamp(m_FlyPitch, glm::radians(-89.f), glm::radians(89.f));
		front = glm::normalize(glm::vec3(
			std::cos(m_FlyYaw) * std::cos(m_FlyPitch),
			std::sin(m_FlyPitch),
			std::sin(m_FlyYaw) * std::cos(m_FlyPitch)));
	}
	m_LastMouse = mouse;

	const glm::vec3 right = glm::normalize(glm::cross(front, { 0.f, 1.f, 0.f }));
	const glm::vec3 up    = { 0.f, 1.f, 0.f };
	const float speed = m_FlySpeed * (Input::IsKeyPressed(HMN_KEY_LEFT_SHIFT) ? 3.f : 1.f) * (float)ts;

	bool moved = false;
	if (!io.WantCaptureKeyboard)
	{
		if (Input::IsKeyPressed(HMN_KEY_W)) { m_CamPos += front * speed; moved = true; }
		if (Input::IsKeyPressed(HMN_KEY_S)) { m_CamPos -= front * speed; moved = true; }
		if (Input::IsKeyPressed(HMN_KEY_A)) { m_CamPos -= right * speed; moved = true; }
		if (Input::IsKeyPressed(HMN_KEY_D)) { m_CamPos += right * speed; moved = true; }
		if (Input::IsKeyPressed(HMN_KEY_E)) { m_CamPos += up    * speed; moved = true; }
		if (Input::IsKeyPressed(HMN_KEY_Q)) { m_CamPos -= up    * speed; moved = true; }
	}

	if (m_FlyLooking || moved)
	{
		m_CamTarget = m_CamPos + front;
		ApplyFraming();
	}
}

void CutsceneLayer::OnUpdate(Timestep ts)
{
	UpdateFreeFlyCamera(ts);
	m_DebugFly.OnUpdate(ts, *m_Scene);
	m_FireTime  += ts;
	m_SmokeTime += ts;

	// Spawn custom actors once all spawnMeshPaths are in cache
	if (!m_CharactersSpawned && m_Desc.onSpawnActors && !m_SpawnMeshHandles.empty())
	{
		const bool allReady = std::all_of(m_SpawnMeshHandles.begin(), m_SpawnMeshHandles.end(),
		                                  [](const auto& h) { return h.IsLoaded(); });
		if (allReady)
		{
			m_Desc.onSpawnActors(*m_Scene);
			m_CharactersSpawned = true;
		}
	}

	m_Cutscene.OnUpdate(ts, m_Ctx);
	m_Scene->OnUpdate(ts);

	const bool wantNext = m_ForceExit ||
	    (!m_EditMode && !m_ShowFraming && !m_ShowFire && !m_ShowSmoke && !m_DebugFly.Enabled && m_TransitionRequested);
	if (wantNext && !m_Transitioning)
	{
		m_Transitioning = true;
		if (m_Desc.nextLayer)
			QueueTransition(m_Desc.nextLayer());
	}
}

void CutsceneLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	m_Scene->SetSkybox(m_Skybox, m_SkyIntensity);

	auto& window = Application::Get().GetWindow();
	const uint32_t winW = window.GetWidth();
	const uint32_t winH = window.GetHeight();
	if (winW > 0 && winH > 0)
		m_Scene->OnViewportResize(winW, winH);

	Layer::OnBuildRenderFrame(frame);

	// Warm fire-toned point lights
	{
		const glm::vec3 fire = { 1.0f, 0.45f, 0.15f };
		auto fireLight = [&](glm::vec3 pos, float intensity, float radius)
		{
			Light l;
			l.Position  = pos;
			l.Color     = fire;
			l.Intensity = intensity;
			l.Radius    = radius;
			l.Type      = LightType::Point;
			frame.lights.push_back(l);
		};
		fireLight({ -50.f, 2.0f, 0.f }, 12.f, 14.f);
		fireLight({ -42.f, 2.0f, 1.f }, 14.f, 14.f);
		fireLight({ -36.f, 2.5f, -1.f }, 16.f, 16.f);
	}

	for (const auto& fi : m_FireInstances)
	{
		// rotDeg is XYZ-tweakable via the F4 editor — kept on eulerAngleYXZ (not Transform's
		// quat-based XYZ composition) so multi-axis edits don't silently change orientation.
		FireQuadDraw fq;
		fq.transform = glm::translate(glm::mat4(1.f), fi.pos)
		             * glm::eulerAngleYXZ(glm::radians(fi.rotDeg.y), glm::radians(fi.rotDeg.x), glm::radians(fi.rotDeg.z))
		             * glm::scale(glm::mat4(1.f), { fi.size.x, fi.size.y, 1.f });
		fq.intensity   = fi.intensity;
		fq.scrollSpeed = fi.scrollSpeed;
		fq.time        = m_FireTime;
		fq.seed        = fi.seed;
		frame.fireQuads.push_back(fq);
	}

	for (const auto& si : m_SmokeInstances)
	{
		SmokeQuadDraw sq;
		sq.transform = glm::translate(glm::mat4(1.f), si.pos)
		             * glm::eulerAngleYXZ(glm::radians(si.rotDeg.y), glm::radians(si.rotDeg.x), glm::radians(si.rotDeg.z))
		             * glm::scale(glm::mat4(1.f), { si.size.x, si.size.y, 1.f });
		sq.opacity     = si.opacity;
		sq.scrollSpeed = si.scrollSpeed;
		sq.time        = m_SmokeTime;
		sq.seed        = si.seed;
		frame.smokeQuads.push_back(sq);
	}

	m_Cutscene.BuildRenderFrame(frame);

	if (m_TransitionRequested)
	{
		QuadDraw q;
		q.transform = Transform::Scale2D(20.f, 20.f).ToMatrix();
		q.color     = { 0.f, 0.f, 0.f, 1.f };
		frame.quads.push_back(std::move(q));
	}
}

void CutsceneLayer::OnImGuiRender()
{
	if (m_EditMode)
		m_Editor.OnImGui(m_Cutscene, m_Ctx);

	if (m_ShowCharacters && m_Desc.onImGuiCharacters)
		m_Desc.onImGuiCharacters();

	if (m_ShowFire)
		FireImGui();

	if (m_ShowSmoke)
		SmokeImGui();

	if (m_ShowFraming)
		UI::Window("Cutscene Set Framing (F2)", [&] {
			ImGui::TextDisabled("Hold LMB + WASD/Q-E to fly (Shift = faster)");
			ImGui::DragFloat("Fly Speed", &m_FlySpeed, 0.1f, 0.1f, 50.f);
			ImGui::Separator();
			bool changed = false;
			changed |= ImGui::DragFloat ("Cam FOV",       &m_CamFOV,       0.5f, 10.f, 120.f);
			changed |= ImGui::DragFloat3("Cam Pos",       &m_CamPos.x,     0.05f);
			changed |= ImGui::DragFloat3("Cam Target",    &m_CamTarget.x,  0.05f);
			ImGui::Separator();
			changed |= ImGui::DragFloat3("Set Pos",       &m_SetPos.x,     0.05f);
			changed |= ImGui::DragFloat3("Set Scale",     &m_SetScale.x,   0.01f, 0.01f, 100.f);
			changed |= ImGui::DragFloat3("Set Rot (deg)", &m_SetRotDeg.x,  0.5f);
			if (changed)
				ApplyFraming();

			ImGui::Separator();
			ImGui::TextUnformatted("Directional Light");
			UI::EditDirectionalLight(m_Scene->GetDirectionalLight());
			ImGui::Separator();
			ImGui::TextUnformatted("Skybox");
			ImGui::DragFloat("Sky Intensity", &m_SkyIntensity, 0.02f, 0.f, 10.f);

			if (ImGui::Button("Log values (bake into CutsceneDesc)"))
			{
				auto& dl = m_Scene->GetDirectionalLight();
				HMN_CORE_INFO("Framing: FOV={:.1f}  CamPos=({:.3f},{:.3f},{:.3f})  CamTarget=({:.3f},{:.3f},{:.3f})  "
				              "SetPos=({:.3f},{:.3f},{:.3f})  SetScale=({:.3f},{:.3f},{:.3f})  SetRot=({:.1f},{:.1f},{:.1f})  "
				              "LightDir=({:.3f},{:.3f},{:.3f})  LightColor=({:.3f},{:.3f},{:.3f})  Ambient={:.2f}  Diffuse={:.2f}",
				              m_CamFOV,
				              m_CamPos.x, m_CamPos.y, m_CamPos.z,
				              m_CamTarget.x, m_CamTarget.y, m_CamTarget.z,
				              m_SetPos.x, m_SetPos.y, m_SetPos.z,
				              m_SetScale.x, m_SetScale.y, m_SetScale.z,
				              m_SetRotDeg.x, m_SetRotDeg.y, m_SetRotDeg.z,
				              dl.Direction.x, dl.Direction.y, dl.Direction.z,
				              dl.Color.x, dl.Color.y, dl.Color.z,
				              dl.AmbientIntensity, dl.DiffuseIntensity);
			}
		});
}

bool CutsceneLayer::OnWindowResize(WindowResizeEvent& e)
{
	if (e.GetWidth() > 0 && e.GetHeight() > 0)
		m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
	return false;
}

bool CutsceneLayer::OnKeyPressed(KeyPressedEvent& e)
{
	if (e.GetRepeatCount() > 0)
		return false;

	switch (e.GetKeyCode())
	{
		case HMN_KEY_ESCAPE:
			m_ForceExit = true;
			return true;

		case HMN_KEY_F1:
			m_EditMode = !m_EditMode;
			if (!m_EditMode)
				m_Cutscene.Play();
			return true;

		case HMN_KEY_F2:
			m_ShowFraming = !m_ShowFraming;
			return true;

		case HMN_KEY_F3:
			if (m_Desc.onImGuiCharacters)
				m_ShowCharacters = !m_ShowCharacters;
			return true;

		case HMN_KEY_F4:
			m_ShowFire = !m_ShowFire;
			return true;

		case HMN_KEY_F5:
			m_ShowSmoke = !m_ShowSmoke;
			return true;

		default:
			return false;
	}
}

void CutsceneLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowResizeEvent>(HMN_BIND_EVENT_FN(CutsceneLayer::OnWindowResize));
	dispatcher.Dispatch<KeyPressedEvent>(HMN_BIND_EVENT_FN(CutsceneLayer::OnKeyPressed));
	dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return m_DebugFly.OnKeyPressed(e); });
}

void CutsceneLayer::FireImGui()
{
	UI::Window("Fire Quads (F4)", [&] {
		ImGui::TextDisabled("Procedural fire patches placed in the 3D set (fire_quad.glsl).");

		if (ImGui::Button("Add Fire"))
			m_FireInstances.emplace_back();

		int removeIndex = -1;
		for (int i = 0; i < (int)m_FireInstances.size(); i++)
		{
			auto& fi = m_FireInstances[i];

			char label[32];
			std::snprintf(label, sizeof(label), "Fire %d", i + 1);
			if (!ImGui::CollapsingHeader(label)) continue;

			ImGui::PushID(i);
			ImGui::DragFloat3("Pos",          &fi.pos.x,    0.05f);
			ImGui::DragFloat3("Rot (deg)",    &fi.rotDeg.x, 1.f, -180.f, 180.f);
			ImGui::DragFloat2("Size",         &fi.size.x,   0.05f, 0.1f, 50.f);
			ImGui::DragFloat ("Intensity",    &fi.intensity,   0.05f, 0.f, 10.f);
			ImGui::DragFloat ("Scroll Speed", &fi.scrollSpeed, 0.05f, 0.f, 10.f);
			ImGui::DragFloat ("Seed",         &fi.seed,        0.1f);
			if (ImGui::Button("Remove")) removeIndex = i;
			ImGui::PopID();
		}

		if (removeIndex >= 0)
			m_FireInstances.erase(m_FireInstances.begin() + removeIndex);

		ImGui::Separator();
		if (ImGui::Button("Log values (bake into CutsceneDesc)"))
		{
			for (int i = 0; i < (int)m_FireInstances.size(); i++)
			{
				const auto& fi = m_FireInstances[i];
				HMN_CORE_INFO("Fire {}: pos=({:.3f},{:.3f},{:.3f})  rot=({:.1f},{:.1f},{:.1f})  size=({:.2f},{:.2f})  "
				              "intensity={:.2f}  scrollSpeed={:.2f}  seed={:.2f}",
				              i + 1,
				              fi.pos.x, fi.pos.y, fi.pos.z,
				              fi.rotDeg.x, fi.rotDeg.y, fi.rotDeg.z,
				              fi.size.x, fi.size.y,
				              fi.intensity, fi.scrollSpeed, fi.seed);
			}
		}
	});
}

void CutsceneLayer::SmokeImGui()
{
	UI::Window("Smoke Quads (F5)", [&] {
		ImGui::TextDisabled("Procedural smoke plumes placed in the 3D set (smoke_quad.glsl).");

		if (ImGui::Button("Add Smoke"))
			m_SmokeInstances.emplace_back();

		int removeIndex = -1;
		for (int i = 0; i < (int)m_SmokeInstances.size(); i++)
		{
			auto& si = m_SmokeInstances[i];

			char label[32];
			std::snprintf(label, sizeof(label), "Smoke %d", i + 1);
			if (!ImGui::CollapsingHeader(label)) continue;

			ImGui::PushID(i);
			ImGui::DragFloat3("Pos",          &si.pos.x,    0.05f);
			ImGui::DragFloat3("Rot (deg)",    &si.rotDeg.x, 1.f, -180.f, 180.f);
			ImGui::DragFloat2("Size",         &si.size.x,   0.1f, 0.1f, 100.f);
			ImGui::DragFloat ("Opacity",      &si.opacity,     0.02f, 0.f, 1.f);
			ImGui::DragFloat ("Scroll Speed", &si.scrollSpeed, 0.02f, 0.f, 5.f);
			ImGui::DragFloat ("Seed",         &si.seed,        0.1f);
			if (ImGui::Button("Remove")) removeIndex = i;
			ImGui::PopID();
		}

		if (removeIndex >= 0)
			m_SmokeInstances.erase(m_SmokeInstances.begin() + removeIndex);

		ImGui::Separator();
		if (ImGui::Button("Log values (bake into CutsceneDesc)"))
		{
			for (int i = 0; i < (int)m_SmokeInstances.size(); i++)
			{
				const auto& si = m_SmokeInstances[i];
				HMN_CORE_INFO("Smoke {}: pos=({:.3f},{:.3f},{:.3f})  rot=({:.1f},{:.1f},{:.1f})  size=({:.2f},{:.2f})  "
				              "opacity={:.2f}  scrollSpeed={:.2f}  seed={:.2f}",
				              i + 1,
				              si.pos.x, si.pos.y, si.pos.z,
				              si.rotDeg.x, si.rotDeg.y, si.rotDeg.z,
				              si.size.x, si.size.y,
				              si.opacity, si.scrollSpeed, si.seed);
			}
		}
	});
}
