#include "hmnpch.h"
#include "IntroCutscene.h"

#include "Layers/GameLayer.h"
#include "Game/Actors/SilhouetteCharacterActor.h"
#include "Hominem/Assets/AssetLoaders.h"
#include "Hominem/ImGui/UI.h"
#include "Hominem/Utils/MathUtils.h"

#include <imgui.h>
#include <glm/glm.hpp>

using namespace Hominem;

namespace {

struct IntroChars
{
	static constexpr int k_RunnerCount = 5;
	std::array<SilhouetteCharacterActor*, k_RunnerCount> runners{};
	SilhouetteCharacterActor* centerGirl    = nullptr;
	SilhouetteCharacterActor* foregroundArm = nullptr;
};

static std::string IntroCutscenePath()
{
#ifdef HMN_SOURCE_RESOURCES_PATH
	return std::string(HMN_SOURCE_RESOURCES_PATH) + "/Cutscenes/intro.json";
#else
	return "Resources/Cutscenes/intro.json";
#endif
}

static void SpawnIntroCharacters(Scene& scene, std::shared_ptr<IntroChars> chars)
{
	using namespace glm;

	Ref<SkinnedMesh> runMesh, idleMesh;
	if (auto r = AssetManager::Load<SkinnedMesh>(IntroCutscene::k_RunMesh))   runMesh  = r->Get();
	if (auto r = AssetManager::Load<SkinnedMesh>(IntroCutscene::k_IdleMesh)) idleMesh = r->Get();
	if (!runMesh || !idleMesh) return;

	struct RunnerDef { vec3 pos; float rotY; vec3 scale; float speed; };
	const RunnerDef runners[] = {
		{ { -41.50f, 0.00f, -0.45f }, radians( 80.0f), { 1.f, 1.f, 1.f }, 1.00f },
		{ { -41.45f, 0.00f,  3.85f }, radians(100.0f), { 3.f, 3.f, 3.f }, 1.20f },
		{ { -44.20f, 0.00f,  8.80f }, radians( 54.5f), { 3.f, 3.f, 3.f }, 0.90f },
		{ { -35.55f,-0.75f,  6.75f }, radians( 75.5f), { 3.f, 3.f, 3.f }, 1.10f },
		{ { -31.00f, 0.00f, -0.15f }, radians( 87.0f), { 1.f, 1.f, 1.f }, 1.30f },
	};

	for (int i = 0; i < IntroChars::k_RunnerCount; i++)
	{
		auto& actor = scene.SpawnActor<SilhouetteCharacterActor>(runMesh);
		actor.Position   = runners[i].pos;
		actor.Rotation   = { 0.f, runners[i].rotY, 0.f };
		actor.Scale      = runners[i].scale;
		actor.AnimSpeed  = runners[i].speed;
		actor.Silhouette = false;
		chars->runners[i] = &actor;
	}

	chars->centerGirl = &scene.SpawnActor<SilhouetteCharacterActor>(idleMesh);
	chars->centerGirl->Position        = { -58.2f, 0.05f, -0.75f };
	chars->centerGirl->Rotation        = { 0.f, glm::radians(115.f), 0.f };
	chars->centerGirl->Scale           = { 1.9f, 1.9f, 1.9f };
	chars->centerGirl->RightArmDeg     = { 73.f, -5.f, 156.f };
	chars->centerGirl->RightForeArmDeg = { 0.f, 0.f, 23.f };

	chars->foregroundArm = &scene.SpawnActor<SilhouetteCharacterActor>(idleMesh);
	chars->foregroundArm->Position        = { -58.65f, 1.05f, 2.25f };
	chars->foregroundArm->Rotation        = { 0.f, glm::radians(90.f), 0.f };
	chars->foregroundArm->Scale           = { 2.6f, 2.6f, 2.6f };
	chars->foregroundArm->RightArmDeg     = { 0.f, 0.f, 43.f };
	chars->foregroundArm->LeftArmDeg      = { 55.f, -176.f, -23.f };
	chars->foregroundArm->RightForeArmDeg = { 0.f, 0.f, 2.f };
}

static void CharactersImGui(std::shared_ptr<IntroChars> chars)
{
	UI::Window("Silhouette Characters (F3)", [&chars] {
		ImGui::TextDisabled("Tweak positions/scales then press Log to bake values into code.");

		auto charWidget = [](const char* label, SilhouetteCharacterActor* a)
		{
			if (!a) return;
			if (!ImGui::CollapsingHeader(label)) return;

			ImGui::PushID(label);
			UI::EditTransform(*a, 0.05f);
			ImGui::DragFloat ("Anim Speed", &a->AnimSpeed, 0.01f, 0.f, 5.f);
			ImGui::Checkbox  ("Silhouette (flat black)", &a->Silhouette);
			ImGui::TextDisabled("Arm pose (deg) - for the hand-hold");
			ImGui::DragFloat3("R Arm",     &a->RightArmDeg.x,     1.f, -180.f, 180.f);
			ImGui::DragFloat3("L Arm",     &a->LeftArmDeg.x,      1.f, -180.f, 180.f);
			ImGui::DragFloat3("R ForeArm", &a->RightForeArmDeg.x, 1.f, -180.f, 180.f);
			ImGui::DragFloat3("L ForeArm", &a->LeftForeArmDeg.x,  1.f, -180.f, 180.f);
			ImGui::PopID();
		};

		for (int i = 0; i < IntroChars::k_RunnerCount; i++)
		{
			char label[32];
			std::snprintf(label, sizeof(label), "Runner %d", i + 1);
			charWidget(label, chars->runners[i]);
		}
		charWidget("Center Girl",    chars->centerGirl);
		charWidget("Foreground Arm", chars->foregroundArm);

		ImGui::Separator();
		if (ImGui::Button("Log all values (bake into SpawnIntroCharacters)"))
		{
			for (int i = 0; i < IntroChars::k_RunnerCount; i++)
			{
				auto* a = chars->runners[i];
				if (!a) continue;
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
			logChar("CenterGirl",    chars->centerGirl);
			logChar("ForegroundArm", chars->foregroundArm);
		}
	});
}

} // namespace

CutsceneDesc IntroCutscene::Make()
{
	auto fireSeed = [](int32_t i) {
		return (float)(PCG_Hash(i) % 10000u) * 0.01f;
	};
	auto chars = std::make_shared<IntroChars>();

	return CutsceneDesc {
		.assets = {
			.setMesh      = k_SetMesh,
			.skybox       = "game://Textures/satara_night_2k.exr",
			.cutsceneJson = IntroCutscenePath()
		},
		.camera = {
			.fov    = 120.f,
			.pos    = { -58.957f, 1.294f, -0.667f },
			.target = { -57.966f, 1.166f, -0.631f }
		},
		.set = {
			.pos    = { 0.f, 0.f, 0.f },
			.scale  = { 2.f, 2.f, 2.f },
			.rotDeg = { 1.0f, -1.0f, -4.5f }
		},
		.light = {
			.dir      = glm::normalize(glm::vec3{ -0.2f, -1.f, -0.4f }),
			.color    = { 0.877f, 0.365f, 0.232f },
			.ambient  = 0.74f,
			.diffuse  = 2.95f
		},
		.fire = {
			FireInstance{ { -53.95f, 6.05f, -1.55f }, { 0.f, 90.f, 0.f }, { 20.f, 20.f }, 1.5f, 0.6f, fireSeed(0) },
			FireInstance{ { -54.65f, 6.30f, -4.05f }, { 0.f, 90.f, 0.f }, {  6.f,  5.f }, 1.5f, 0.6f, fireSeed(1) },
			FireInstance{ { -56.55f, 2.50f, -6.30f }, { 0.f, 90.f, 0.f }, {  6.f,  5.f }, 1.5f, 0.6f, fireSeed(2) }
		},
		.smoke = {
			SmokeInstance{ { -53.95f,  6.90f, -1.55f }, { 0.f, 90.f, 0.f }, { 28.f, 24.f }, 0.6f, 0.15f, 28.82f },
			SmokeInstance{ { -54.65f, 12.00f, -4.05f }, { 0.f, 90.f, 0.f }, { 16.f, 16.f }, 0.6f, 0.15f, 74.66f },
			SmokeInstance{ { -56.55f,  9.00f, -6.30f }, { 0.f, 90.f, 0.f }, { 16.f, 16.f }, 0.6f, 0.15f,  3.04f }
		},
		.spawnMeshPaths    = { k_RunMesh, k_IdleMesh },
		.onSpawnActors     = [chars](Scene& scene) { SpawnIntroCharacters(scene, chars); },
		.onImGuiCharacters = [chars]()             { CharactersImGui(chars); },
		.onComplete        = [] { GameLayer::s_SkipIntro = true; },
		.nextLayer         = [] { return std::make_unique<GameLayer>(); }
	};
}
