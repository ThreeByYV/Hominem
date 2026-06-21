#include "hmnpch.h"
#include "CutsceneLayer.h"
#include "GameLayer.h"

#include "Hominem/Core/Application.h"
#include "Hominem/Core/Input.h"
#include "Hominem/Core/KeyCodes.h"
#include "Hominem/Core/MouseButtonCodes.h"
#include "Hominem/Utils/MathUtils.h"
#include "Hominem/Cinematics/CutsceneLoader.h"
#include "Game/Actors/SceneActor.h"
#include "Game/Actors/SilhouetteCharacterActor.h"
#include "Game/CutscenePreload.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>
#include "Hominem/ImGui/UIHelpers.h"

using namespace Hominem;

CutsceneLayer::CutsceneLayer()
	: Layer("Cutscene")
{
}

std::string CutsceneLayer::CutscenePath()
{
	// In Debug, read/write the source copy so editor saves persist across runs.
	//todo: client shouldn't have to do this should be handled in engine
#ifdef HMN_SOURCE_RESOURCES_PATH
	return std::string(HMN_SOURCE_RESOURCES_PATH) + "/Cutscenes/intro.json";
#else
	return "Resources/Cutscenes/intro.json";
#endif
}

void CutsceneLayer::OnAttach()
{
	m_Scene = CreateRef<Scene>();

	auto& window = Application::Get().GetWindow();
	const float aspect = (float)window.GetWidth() / (float)window.GetHeight();
	m_Scene->GetCamera().SetPerspective(m_CamFOV, aspect, 0.1f, 1000.f);
	m_Scene->OnViewportResize(window.GetWidth(), window.GetHeight());

	m_Scene->SetClearColor({ 0.f, 0.f, 0.f, 1.f });

	auto& dl = m_Scene->GetDirectionalLight();
	dl.Direction       = glm::normalize(glm::vec3{ -0.2f, -1.f, -0.4f });
	dl.Color           = { 0.877f, 0.365f, 0.232f };
	dl.AmbientIntensity = 0.74f;
	dl.DiffuseIntensity = 2.95f;

	// 3D set - the burning town the intro plays against. LoadingLayer guarantees
	// CutscenePreload has already finished by the time we get here.
	if (auto mesh = CutscenePreload::TryGet())
		m_Set = &m_Scene->SpawnActor<SceneActor>(std::move(mesh));
	ApplyFraming();

	// Seed default fire patches so the procedural fire is visible without having
	// to open the F4 panel; tune/add more there. Each gets a distinct hashed seed
	// so the FBM noise doesn't scroll in lockstep across patches.
	auto hashSeed = [](int32_t i) -> float
	{
		return (float)(PCG_Hash(i) % 10000u) * 0.01f; // 0..100
	};

	m_FireInstances.push_back({ { -53.95f, 6.05f, -1.55f }, { 0.f, 90.f, 0.f }, { 20.f, 20.f }, 1.5f, 0.6f, hashSeed(0) });
	m_FireInstances.push_back({ { -54.65f, 6.30f, -4.05f }, { 0.f, 90.f, 0.f }, { 6.f,  5.f  }, 1.5f, 0.6f, hashSeed(1) });
	m_FireInstances.push_back({ { -56.55f, 2.50f, -6.30f }, { 0.f, 90.f, 0.f }, { 6.f,  5.f  }, 1.5f, 0.6f, hashSeed(2) });

	// Smoke plumes drift above each fire patch - bigger, slower, alpha-blended.
	m_SmokeInstances.push_back({ { -53.95f,  6.90f, -1.55f }, { 0.f, 90.f, 0.f }, { 28.f, 24.f }, 0.6f, 0.15f, 28.82f });
	m_SmokeInstances.push_back({ { -54.65f, 12.00f, -4.05f }, { 0.f, 90.f, 0.f }, { 16.f, 16.f }, 0.6f, 0.15f, 74.66f });
	m_SmokeInstances.push_back({ { -56.55f,  9.00f, -6.30f }, { 0.f, 90.f, 0.f }, { 16.f, 16.f }, 0.6f, 0.15f, 3.04f  });

	// Background-load the FBX (Assimp parse is multi-second) so OnAttach never stalls;
	// SpawnCinematicCharacters runs from OnUpdate once both loads complete.
	m_RunMeshLoad  = SkinnedMesh::LoadAsync(k_RunMesh);
	m_IdleMeshLoad = SkinnedMesh::LoadAsync(k_IdleMesh);

	// HDR night-sky backdrop.
	m_Skybox = Skybox::CreateFromEquirectEXR(k_SkyboxEXR);
	m_Scene->SetSkybox(m_Skybox, m_SkyIntensity);

	m_Ctx.cutscene = &m_Cutscene;
	m_Ctx.scene    = m_Scene.get();

	const std::string path = CutscenePath();
	if (!CutsceneLoader::LoadFromFile(path, m_Cutscene))
		HMN_CORE_WARN("CutsceneLayer: failed to load '{}'", path);

	m_Editor.SavePath = path;
	m_Cutscene.SetOnFinished([this]
	{
		GameLayer::s_SkipIntro = true;
		m_TransitionRequested  = true;
	});
	m_Cutscene.Play();
}

void CutsceneLayer::OnDetach()
{
	m_RunMeshLoad.Detach();
	m_IdleMeshLoad.Detach();
	m_Scene.reset();
}

void CutsceneLayer::SpawnCinematicCharacters(Ref<SkinnedMesh> runMesh, Ref<SkinnedMesh> idleMesh)
{
	using namespace glm;

	// Camera is at (-58.94, 1.26, -0.23) looking in +X. Right = +Z. Ground = Y=0.
	// All runners share one Running.fbx instance; center girl + foreground arm share Idle.fbx.

	struct RunnerDef { vec3 pos; float rotY; vec3 scale; float speed; };
	const RunnerDef runners[] = {
		{ { -41.50f, 0.00f, -0.45f }, radians( 80.0f), { 1.f, 1.f, 1.f }, 1.00f },
		{ { -41.45f, 0.00f,  3.85f }, radians(100.0f), { 3.f, 3.f, 3.f }, 1.20f },
		{ { -44.20f, 0.00f,  8.80f }, radians( 54.5f), { 3.f, 3.f, 3.f }, 0.90f },
		{ { -35.55f,-0.75f,  6.75f }, radians( 75.5f), { 3.f, 3.f, 3.f }, 1.10f },
		{ { -31.00f, 0.00f, -0.15f }, radians( 87.0f), { 1.f, 1.f, 1.f }, 1.30f },
	};
	// Running.fbx/Idle.fbx are authored in centimetres, but aiProcess_GlobalScale now
	// converts them to metres at import, so a scale of 1.0 already reads as ~1.8 m tall.
	// The girl/arm use slightly larger scales purely for foreground framing.

	// Midground runners fleeing down the street - rendered with the normal lit shader
	// (Silhouette = false) so the fire lights pick them out, not flat black.
	for (int i = 0; i < k_RunnerCount; i++)
	{
		auto& actor = m_Scene->SpawnActor<SilhouetteCharacterActor>(runMesh);
		actor.Position   = runners[i].pos;
		actor.Rotation   = { 0.f, runners[i].rotY, 0.f };
		actor.Scale      = runners[i].scale;
		actor.AnimSpeed  = runners[i].speed;
		actor.Silhouette = false;
		m_Runners[i]     = &actor;
	}

	// fire.png foreground pair: girl seen from behind, centre-left and close; adult at the
	// right edge reaching toward her. Both flat-black. Arm angles tuned live via F3.
	m_CenterGirl = &m_Scene->SpawnActor<SilhouetteCharacterActor>(idleMesh);
	m_CenterGirl->Position        = { -58.2f, 0.05f, -0.75f };
	m_CenterGirl->Rotation        = { 0.f, radians(115.f), 0.f };
	m_CenterGirl->Scale           = { 1.9f, 1.9f, 1.9f };
	m_CenterGirl->RightArmDeg     = { 73.f, -5.f, 156.f }; // reach toward the adult on the right
	m_CenterGirl->RightForeArmDeg = { 0.f, 0.f, 23.f };

	m_ForegroundArm = &m_Scene->SpawnActor<SilhouetteCharacterActor>(idleMesh);
	m_ForegroundArm->Position        = { -58.65f, 1.05f, 2.25f };
	m_ForegroundArm->Rotation        = { 0.f, radians(90.f), 0.f };
	m_ForegroundArm->Scale           = { 2.6f, 2.6f, 2.6f };
	m_ForegroundArm->RightArmDeg     = { 0.f, 0.f, 43.f };
	m_ForegroundArm->LeftArmDeg      = { 55.f, -176.f, -23.f }; // reach back toward the girl
	m_ForegroundArm->RightForeArmDeg = { 0.f, 0.f, 2.f };
}

void CutsceneLayer::ApplyFraming()
{
	if (!m_Scene) return;
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

	// Don't let the free-fly camera react while ImGui has the input - dragging a slider
	// or typing in a panel (even the F2 panel's own widgets) shouldn't fly the camera.
	// Only mouse/keyboard over the open viewport drive it.
	const ImGuiIO& io = ImGui::GetIO();

	const auto [mx, my] = Input::GetMousePosition();
	const glm::vec2 mouse = { mx, my };
	const bool lmbHeld = Input::IsMouseButtonPressed(HMN_MOUSE_BUTTON_LEFT) && !io.WantCaptureMouse;

	if (lmbHeld && !m_FlyLooking)
	{
		// Just grabbed the view: derive yaw/pitch from the current look direction
		// so the camera doesn't snap to a different orientation on first move.
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

	// Suppress WASD/Q/E flight while a panel/text field has keyboard focus.
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
	if (m_Scene) m_DebugFly.OnUpdate(ts, *m_Scene);
	m_FireTime  += ts;
	m_SmokeTime += ts;

	// Spawn silhouette characters once both background mesh loads complete.
	if (!m_CharactersSpawned)
	{
		auto runMesh  = m_RunMeshLoad.TryGet();
		auto idleMesh = m_IdleMeshLoad.TryGet();
		if (runMesh && idleMesh)
		{
			SpawnCinematicCharacters(runMesh, idleMesh);
			m_CharactersSpawned = true;
		}
	}

	m_Cutscene.OnUpdate(ts, m_Ctx);
	m_Scene->OnUpdate(ts);

	// Hand off to gameplay when the cutscene ends (unless we're tuning it in the
	// editor), or immediately if the player skipped with Esc.
	const bool wantGame = m_ForceExit || (!m_EditMode && !m_ShowFraming && !m_ShowFire && !m_ShowSmoke && !m_DebugFly.Enabled && m_TransitionRequested);
	if (wantGame && !m_Transitioning)
	{
		m_Transitioning = true;
		TransitionTo<GameLayer>();
	}
}

void CutsceneLayer::OnBuildRenderFrame(RenderFrame& frame)
{
	// Sync live-tweakable values (F2 panel) to the scene before building.
	m_Scene->SetSkybox(m_Skybox, m_SkyIntensity);

	// Keep the scene viewport on the live window size - the OnAttach seed can be stale,
	// which would render the scene into a sub-region of the backbuffer.
	auto& window = Application::Get().GetWindow();
	const uint32_t winW = window.GetWidth();
	const uint32_t winH = window.GetHeight();
	if (winW > 0 && winH > 0)
		m_Scene->OnViewportResize(winW, winH);

	Layer::OnBuildRenderFrame(frame); // scene settings + actors collected

	// Warm fire-toned point lights down the street. They also keep the Forward+ light
	// cull running, which the static set's light grid depends on (see Renderer3D).
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

	// Procedural fire quads
	for (const auto& fi : m_FireInstances)
	{
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

	// Procedural smoke quads
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

	// The fade cue stops drawing the same frame it requests the cut, so the scene would
	// flash back for one frame - hold solid black ourselves from here on.
	if (m_TransitionRequested)
	{
		QuadDraw q;
		q.transform = glm::scale(glm::mat4(1.f), { 20.f, 20.f, 1.f });
		q.color     = { 0.f, 0.f, 0.f, 1.f };
		frame.quads.push_back(std::move(q));
	}
}

void CutsceneLayer::OnImGuiRender()
{
	if (m_EditMode)
		m_Editor.OnImGui(m_Cutscene, m_Ctx);

	if (m_ShowCharacters)
		CharactersImGui();

	if (m_ShowFire)
		FireImGui();

	if (m_ShowSmoke)
		SmokeImGui();

	if (m_ShowFraming)
		UI::Window("Cutscene Set Framing (F2)", [&] {
			UI::TextDisabled("Hold LMB + WASD/Q-E to fly (Shift = faster)");
			UI::Drag("Fly Speed", m_FlySpeed, 0.1f, 0.1f, 50.f);
			UI::Separator();
			bool changed = false;
			changed |= UI::Drag    ("Cam FOV",       m_CamFOV,    0.5f, 10.f, 120.f);
			changed |= UI::DragVec3("Cam Pos",       m_CamPos,    0.05f);
			changed |= UI::DragVec3("Cam Target",    m_CamTarget, 0.05f);
			UI::Separator();
			changed |= UI::DragVec3("Set Pos",       m_SetPos,    0.05f);
			changed |= UI::DragVec3("Set Scale",     m_SetScale,  0.01f, 0.01f, 100.f);
			changed |= UI::DragVec3("Set Rot (deg)", m_SetRotDeg, 0.5f);
			if (changed)
				ApplyFraming();

			UI::Separator();
			UI::Text("Directional Light");
			auto& dl = m_Scene->GetDirectionalLight();
			UI::EditDirectionalLight(dl);
			UI::Separator();
			UI::Text("Skybox");
			UI::Drag("Sky Intensity", m_SkyIntensity, 0.02f, 0.f, 10.f);

			if (UI::Button("Log values (bake into defaults)"))
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
		});
}

bool CutsceneLayer::OnWindowResize(WindowResizeEvent& e)
{
	if (m_Scene && e.GetWidth() > 0 && e.GetHeight() > 0)
		m_Scene->OnViewportResize(e.GetWidth(), e.GetHeight());
	return false;
}

bool CutsceneLayer::OnKeyPressed(KeyPressedEvent& e)
{
	if (e.GetRepeatCount() > 0)
		return false;

	switch (e.GetKeyCode())
	{
		case HMN_KEY_ESCAPE: // skip straight to gameplay
			m_ForceExit = true;
			return true;

		case HMN_KEY_F1: // toggle the authoring editor
			m_EditMode = !m_EditMode;
			if (!m_EditMode)
				m_Cutscene.Play(); // leaving the editor: run it through for real
			return true;

		case HMN_KEY_F2: // toggle the set-framing panel (also pauses auto-exit)
			m_ShowFraming = !m_ShowFraming;
			return true;

		case HMN_KEY_F3: // toggle the silhouette character tweaker
			m_ShowCharacters = !m_ShowCharacters;
			return true;

		case HMN_KEY_F4: // toggle the fire-placement panel
			m_ShowFire = !m_ShowFire;
			return true;

		case HMN_KEY_F5: // toggle the smoke-placement panel
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

void CutsceneLayer::CharactersImGui()
{
	UI::Window("Silhouette Characters (F3)", [&] {
		UI::TextDisabled("Tweak positions/scales then press Log to bake values into code.");

		auto charWidget = [](const char* label, SilhouetteCharacterActor* a)
		{
			if (!a) return;
			if (!UI::Collapsible(label)) return;

			UI::WithID(label, [&] {
				UI::EditTransform(*a, 0.05f);
				UI::Drag    ("Anim Speed", a->AnimSpeed, 0.01f, 0.f, 5.f);
				UI::Checkbox("Silhouette (flat black)", a->Silhouette);
				UI::TextDisabled("Arm pose (deg) - for the hand-hold");
				UI::DragVec3("R Arm",     a->RightArmDeg,     1.f, -180.f, 180.f);
				UI::DragVec3("L Arm",     a->LeftArmDeg,      1.f, -180.f, 180.f);
				UI::DragVec3("R ForeArm", a->RightForeArmDeg, 1.f, -180.f, 180.f);
				UI::DragVec3("L ForeArm", a->LeftForeArmDeg,  1.f, -180.f, 180.f);
			});
		};

		for (int i = 0; i < k_RunnerCount; i++)
		{
			char label[32];
			std::snprintf(label, sizeof(label), "Runner %d", i + 1);
			charWidget(label, m_Runners[i]);
		}
		charWidget("Center Girl",    m_CenterGirl);
		charWidget("Foreground Arm", m_ForegroundArm);

		UI::Separator();
		if (UI::Button("Log all values (bake into SpawnCinematicCharacters)"))
		{
			for (int i = 0; i < k_RunnerCount; i++)
			{
				if (!m_Runners[i]) continue;
				auto* a = m_Runners[i];
				HMN_CORE_INFO("Runner {}: pos=({:.3f},{:.3f},{:.3f})  rotY={:.1f}deg  scale=({:.2f},{:.2f},{:.2f})  speed={:.2f}",
					i + 1,
					a->Position.x, a->Position.y, a->Position.z,
					glm::degrees(a->Rotation.y),
					a->Scale.x, a->Scale.y, a->Scale.z,
					a->AnimSpeed);
			}
			auto logChar = [](const char* name, SilhouetteCharacterActor* a)
			{
				if (!a) return;
				HMN_CORE_INFO("{}: pos=({:.3f},{:.3f},{:.3f})  rotY={:.1f}deg  scale=({:.3f},{:.3f},{:.3f})  speed={:.2f}\n"
					"    RArm=({:.0f},{:.0f},{:.0f}) LArm=({:.0f},{:.0f},{:.0f}) RFore=({:.0f},{:.0f},{:.0f}) LFore=({:.0f},{:.0f},{:.0f})",
					name,
					a->Position.x, a->Position.y, a->Position.z,
					glm::degrees(a->Rotation.y),
					a->Scale.x, a->Scale.y, a->Scale.z,
					a->AnimSpeed,
					a->RightArmDeg.x, a->RightArmDeg.y, a->RightArmDeg.z,
					a->LeftArmDeg.x,  a->LeftArmDeg.y,  a->LeftArmDeg.z,
					a->RightForeArmDeg.x, a->RightForeArmDeg.y, a->RightForeArmDeg.z,
					a->LeftForeArmDeg.x,  a->LeftForeArmDeg.y,  a->LeftForeArmDeg.z);
			};
			logChar("CenterGirl",    m_CenterGirl);
			logChar("ForegroundArm", m_ForegroundArm);
		}
	});
}

void CutsceneLayer::FireImGui()
{
	UI::Window("Fire Quads (F4)", [&] {
		UI::TextDisabled("Procedural fire patches placed in the 3D set (fire_quad.glsl).");

		if (UI::Button("Add Fire"))
			m_FireInstances.emplace_back();

		int removeIndex = -1;
		for (int i = 0; i < (int)m_FireInstances.size(); i++)
		{
			auto& fi = m_FireInstances[i];

			char label[32];
			std::snprintf(label, sizeof(label), "Fire %d", i + 1);
			if (!UI::Collapsible(label)) continue;

			UI::WithID(i, [&] {
				UI::DragVec3("Pos",          fi.pos,    0.05f);
				UI::DragVec3("Rot (deg)",    fi.rotDeg, 1.f, -180.f, 180.f);
				UI::DragVec2("Size",         fi.size,   0.05f, 0.1f, 50.f);
				UI::Drag    ("Intensity",    fi.intensity,   0.05f, 0.f, 10.f);
				UI::Drag    ("Scroll Speed", fi.scrollSpeed, 0.05f, 0.f, 10.f);
				UI::Drag    ("Seed",         fi.seed,        0.1f);
				if (UI::Button("Remove")) removeIndex = i;
			});
		}

		if (removeIndex >= 0)
			m_FireInstances.erase(m_FireInstances.begin() + removeIndex);

		UI::Separator();
		if (UI::Button("Log values (bake into defaults)"))
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
		UI::TextDisabled("Procedural smoke plumes placed in the 3D set (smoke_quad.glsl).");

		if (UI::Button("Add Smoke"))
			m_SmokeInstances.emplace_back();

		int removeIndex = -1;
		for (int i = 0; i < (int)m_SmokeInstances.size(); i++)
		{
			auto& si = m_SmokeInstances[i];

			char label[32];
			std::snprintf(label, sizeof(label), "Smoke %d", i + 1);
			if (!UI::Collapsible(label)) continue;

			UI::WithID(i, [&] {
				UI::DragVec3("Pos",          si.pos,    0.05f);
				UI::DragVec3("Rot (deg)",    si.rotDeg, 1.f, -180.f, 180.f);
				UI::DragVec2("Size",         si.size,   0.1f, 0.1f, 100.f);
				UI::Drag    ("Opacity",      si.opacity,     0.02f, 0.f, 1.f);
				UI::Drag    ("Scroll Speed", si.scrollSpeed, 0.02f, 0.f, 5.f);
				UI::Drag    ("Seed",         si.seed,        0.1f);
				if (UI::Button("Remove")) removeIndex = i;
			});
		}

		if (removeIndex >= 0)
			m_SmokeInstances.erase(m_SmokeInstances.begin() + removeIndex);

		UI::Separator();
		if (UI::Button("Log values (bake into defaults)"))
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
