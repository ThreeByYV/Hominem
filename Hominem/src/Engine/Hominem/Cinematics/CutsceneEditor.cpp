#include "hmnpch.h"
#include "Hominem/Cinematics/CutsceneEditor.h"

#include "Hominem/Cinematics/Cutscene.h"
#include "Hominem/Cinematics/CutsceneLoader.h"
#include "Hominem/Cinematics/Cues/FadeCue.h"
#include "Hominem/Cinematics/Cues/SpriteCue.h"
#include "Hominem/Cinematics/Cues/CameraCue.h"
#include "Hominem/Renderer/Texture.h"

#include <imgui.h>
#include <algorithm>
#include <string>
#include <cstring>

namespace Hominem {

	void CutsceneEditor::OnImGui(Cutscene& cs, CutsceneContext& ctx)
	{
		if (!Open)
			return;

		ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Cutscene Editor", &Open))
		{
			ImGui::End();
			return;
		}

		const float duration = std::max(cs.Duration(), 0.001f);

		if (ImGui::Button("Restart")) cs.Play();
		ImGui::SameLine();
		if (cs.IsPlaying())
		{
			if (ImGui::Button("Pause")) cs.Stop();
		}
		else
		{
			if (ImGui::Button("Play")) cs.Resume();
		}
		ImGui::SameLine();
		ImGui::Text("%.2f / %.2f s", cs.Time(), cs.Duration());

		// Scrub: pause and preview at the chosen time.
		float t = cs.Time();
		if (ImGui::SliderFloat("Scrub", &t, 0.f, duration))
		{
			cs.Stop();
			cs.PreviewAt(t);
		}

		ImGui::SliderFloat("Zoom (px/s)", &m_PixelsPerSecond, 20.f, 240.f, "%.0f");

		// Timeline
		ImGui::TextUnformatted("Timeline  (drag a block to move it)");
		{
			ImDrawList*  dl     = ImGui::GetWindowDrawList();
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const float  width  = ImGui::GetContentRegionAvail().x;
			const float  pps    = m_PixelsPerSecond;
			const float  rowH   = 22.f;
			const float  gap    = 4.f;
			const int    n      = (int)cs.CueCount();
			const float  bodyH  = std::max(n * (rowH + gap) + 8.f, rowH + 8.f);

			dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + bodyH),
			                  IM_COL32(28, 28, 32, 255));

			// One-second gridlines.
			for (int s = 0; s <= (int)duration + 1; ++s)
			{
				const float x = origin.x + s * pps;
				if (x > origin.x + width) break;
				dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + bodyH), IM_COL32(55, 55, 60, 255));
			}

			for (int i = 0; i < n; ++i)
			{
				Cue&        c  = cs.GetCue(i);
				const float y  = origin.y + 4.f + i * (rowH + gap);
				const float x0 = origin.x + c.StartTime * pps;
				const float w  = std::max(c.Duration * pps, 8.f);

				const ImVec2 a(x0, y), b(x0 + w, y + rowH);

				ImGui::SetCursorScreenPos(a);
				ImGui::InvisibleButton(("##blk" + std::to_string(i)).c_str(), ImVec2(w, rowH));
				const bool activeDrag = ImGui::IsItemActive();
				if (activeDrag)
				{
					c.StartTime += ImGui::GetIO().MouseDelta.x / pps;
					c.StartTime  = std::max(c.StartTime, 0.f);
				}

				const ImU32 col = activeDrag ? IM_COL32(120, 165, 250, 255)
				                             : IM_COL32(78, 120, 200, 255);
				dl->AddRectFilled(a, b, col, 3.f);
				dl->AddRect(a, b, IM_COL32(20, 20, 24, 255), 3.f);
				dl->AddText(ImVec2(a.x + 5, a.y + 3), IM_COL32(255, 255, 255, 255), c.TypeName());
			}

			// Playhead.
			const float px = origin.x + cs.Time() * pps;
			dl->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + bodyH), IM_COL32(250, 80, 80, 255), 2.f);

			ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + bodyH + 6.f));
			ImGui::Dummy(ImVec2(width, 0.f));
		}

		// Per-cue inspector
		ImGui::SeparatorText("Cues");
		for (int i = 0; i < (int)cs.CueCount(); ++i)
		{
			ImGui::PushID(i);
			Cue& c = cs.GetCue(i);

			if (ImGui::TreeNodeEx("node", ImGuiTreeNodeFlags_DefaultOpen, "%d  %s", i, c.TypeName()))
			{
				ImGui::DragFloat("Start",    &c.StartTime, 0.05f, 0.f, 10000.f, "%.2f");
				ImGui::DragFloat("Duration", &c.Duration,  0.05f, 0.f, 10000.f, "%.2f");

				if (auto* f = dynamic_cast<FadeCue*>(&c))
				{
					ImGui::ColorEdit3("Color", &f->Color.x);
					ImGui::DragFloat("From",   &f->From, 0.01f, 0.f, 1.f);
					ImGui::DragFloat("To",     &f->To,   0.01f, 0.f, 1.f);
				}
				else if (auto* s = dynamic_cast<SpriteCue*>(&c))
				{
					ImGui::DragFloat3("Pos",  &s->Pos.x,  0.05f);
					ImGui::DragFloat2("Size", &s->Size.x, 0.05f, 0.f, 100.f);
					ImGui::ColorEdit4("Tint", &s->Tint.x);
					ImGui::Checkbox("Flicker", &s->Flicker);
					ImGui::TextDisabled("tex: %s", s->TexPath.c_str());
				}
				else if (auto* cam = dynamic_cast<CameraCue*>(&c))
				{
					ImGui::DragFloat3("From Pos",   &cam->FromPos.x, 0.05f);
					ImGui::DragFloat3("To Pos",     &cam->ToPos.x,   0.05f);
					ImGui::DragFloat("From Ortho",  &cam->FromOrthoSize, 0.01f, 0.01f, 50.f);
					ImGui::DragFloat("To Ortho",    &cam->ToOrthoSize,   0.01f, 0.01f, 50.f);
					char fromBoneBuf[64]{}, toBoneBuf[64]{};
					std::strncpy(fromBoneBuf, cam->FromBone.c_str(), 63);
					std::strncpy(toBoneBuf,   cam->ToBone.c_str(),   63);
					if (ImGui::InputText("From Bone", fromBoneBuf, sizeof(fromBoneBuf)))
						cam->FromBone = fromBoneBuf;
					if (ImGui::InputText("To Bone",   toBoneBuf,   sizeof(toBoneBuf)))
						cam->ToBone = toBoneBuf;
					ImGui::TextDisabled("(blank = use Pos; bone name overrides Pos if found)");
				}

				if (ImGui::SmallButton("Remove"))
				{
					cs.RemoveCue((size_t)i);
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// Add / Save
		ImGui::Separator();
		if (ImGui::Button("Add Fade"))
			cs.Add<FadeCue>(glm::vec3(0.f), 0.f, 1.f).At(cs.Duration()).For(1.f);
		ImGui::SameLine();
		if (ImGui::Button("Add Sprite"))
		{
			auto& sp = cs.Add<SpriteCue>(Texture2D::Create("Resources/Textures/fire.png"),
			                             glm::vec3(0.f, 0.f, -0.5f), glm::vec2(2.f, 2.f));
			sp.TexPath = "Resources/Textures/fire.png";
			sp.At(cs.Duration()).For(2.f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Camera"))
			cs.Add<CameraCue>(glm::vec3(0.f), glm::vec3(0.f), 2.f, 2.f).At(cs.Duration()).For(1.f);

		ImGui::Separator();
		if (ImGui::Button("Save") && !SavePath.empty())
			CutsceneLoader::SaveToFile(SavePath, cs);
		ImGui::SameLine();
		ImGui::TextDisabled("%s", SavePath.empty() ? "(no save path)" : SavePath.c_str());

		ImGui::End();

		// When paused, keep the rendered preview in sync with edits/scrubbing.
		// TickCues applies camera/scene effects (e.g. CameraCue position) without
		// advancing the clock or firing enter/exit side effects.
		if (!cs.IsPlaying())
		{
			cs.PreviewAt(cs.Time());
			cs.TickCues(ctx);
		}
	}

}
