#include "hmnpch.h"
#include "Hominem/Cinematics/CutsceneLoader.h"

#include "Hominem/Cinematics/Cues/FadeCue.h"
#include "Hominem/Cinematics/Cues/SpriteCue.h"
#include "Hominem/Cinematics/Cues/CameraCue.h"
#include "Hominem/Renderer/Texture.h"
#include "Hominem/Core/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace Hominem {

	using json = nlohmann::json;

	namespace {

		glm::vec3 ReadVec3(const json& j, const char* key, glm::vec3 def)
		{
			if (j.contains(key) && j[key].is_array() && j[key].size() >= 3)
				return { j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>() };
			return def;
		}

		glm::vec2 ReadVec2(const json& j, const char* key, glm::vec2 def)
		{
			if (j.contains(key) && j[key].is_array() && j[key].size() >= 2)
				return { j[key][0].get<float>(), j[key][1].get<float>() };
			return def;
		}

		glm::vec4 ReadVec4(const json& j, const char* key, glm::vec4 def)
		{
			if (j.contains(key) && j[key].is_array() && j[key].size() >= 4)
				return { j[key][0].get<float>(), j[key][1].get<float>(),
				         j[key][2].get<float>(), j[key][3].get<float>() };
			return def;
		}

	}

	bool CutsceneLoader::LoadFromFile(const std::string& path, Cutscene& cs)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			HMN_CORE_ERROR("CutsceneLoader: could not open '{}'", path);
			return false;
		}

		json root;
		try
		{
			file >> root;
		}
		catch (const std::exception& e)
		{
			HMN_CORE_ERROR("CutsceneLoader: parse error in '{}': {}", path, e.what());
			return false;
		}

		cs.Clear();

		if (!root.contains("cues") || !root["cues"].is_array())
			return true; // empty but valid

		for (const auto& jc : root["cues"])
		{
			const std::string type  = jc.value("type", std::string());
			const float       start = jc.value("start", 0.f);
			const float       dur   = jc.value("duration", 0.f);

			if (type == "fade")
			{
				const glm::vec3 color = ReadVec3(jc, "color", { 0.f, 0.f, 0.f });
				const float     from  = jc.value("from", 0.f);
				const float     to    = jc.value("to", 1.f);
				cs.Add<FadeCue>(color, from, to).At(start).For(dur);
			}
			else if (type == "sprite")
			{
				const std::string tex     = jc.value("texture", std::string());
				const glm::vec3   pos     = ReadVec3(jc, "pos",  { 0.f, 0.f, -0.5f });
				const glm::vec2   size    = ReadVec2(jc, "size", { 1.f, 1.f });
				const glm::vec4   tint    = ReadVec4(jc, "tint", { 1.f, 1.f, 1.f, 1.f });
				const bool        flicker = jc.value("flicker", false);

				auto sprite = CreateScope<SpriteCue>(
					tex.empty() ? Ref<Texture2D>(nullptr) : Texture2D::Create(tex),
					pos, size, tint, flicker);
				sprite->TexPath   = tex;
				sprite->StartTime = start;
				sprite->Duration  = dur;
				cs.AddCue(std::move(sprite));
			}
			else if (type == "camera")
			{
				const glm::vec3 fromPos   = ReadVec3(jc, "from_pos",   { 0.f, 0.f, 0.f });
				const glm::vec3 toPos     = ReadVec3(jc, "to_pos",     { 0.f, 0.f, 0.f });
				const float     fromOrtho = jc.value("from_ortho", 2.f);
				const float     toOrtho   = jc.value("to_ortho",   2.f);
				CameraCue& cam = cs.Add<CameraCue>(fromPos, toPos, fromOrtho, toOrtho);
				cam.FromBone = jc.value("from_bone", std::string());
				cam.ToBone   = jc.value("to_bone",   std::string());
				cam.At(start).For(dur);
			}
			else
			{
				HMN_CORE_WARN("CutsceneLoader: unknown cue type '{}' (skipped)", type);
			}
		}

		return true;
	}

	bool CutsceneLoader::SaveToFile(const std::string& path, const Cutscene& cs)
	{
		json root;
		json cues = json::array();

		for (size_t i = 0; i < cs.CueCount(); ++i)
		{
			const Cue& c = cs.GetCue(i);

			json jc;
			jc["type"]     = c.TypeName();
			jc["start"]    = c.StartTime;
			jc["duration"] = c.Duration;

			if (const auto* f = dynamic_cast<const FadeCue*>(&c))
			{
				jc["color"] = { f->Color.x, f->Color.y, f->Color.z };
				jc["from"]  = f->From;
				jc["to"]    = f->To;
			}
			else if (const auto* s = dynamic_cast<const SpriteCue*>(&c))
			{
				jc["texture"] = s->TexPath;
				jc["pos"]     = { s->Pos.x, s->Pos.y, s->Pos.z };
				jc["size"]    = { s->Size.x, s->Size.y };
				jc["tint"]    = { s->Tint.x, s->Tint.y, s->Tint.z, s->Tint.w };
				jc["flicker"] = s->Flicker;
			}
			else if (const auto* cam = dynamic_cast<const CameraCue*>(&c))
			{
				jc["from_pos"]   = { cam->FromPos.x,  cam->FromPos.y,  cam->FromPos.z  };
				jc["to_pos"]     = { cam->ToPos.x,    cam->ToPos.y,    cam->ToPos.z    };
				jc["from_ortho"] = cam->FromOrthoSize;
				jc["to_ortho"]   = cam->ToOrthoSize;
				if (!cam->FromBone.empty()) jc["from_bone"] = cam->FromBone;
				if (!cam->ToBone.empty())   jc["to_bone"]   = cam->ToBone;
			}

			cues.push_back(std::move(jc));
		}

		root["cues"] = std::move(cues);

		try
		{
			const std::filesystem::path fp(path);
			if (fp.has_parent_path())
				std::filesystem::create_directories(fp.parent_path());
		}
		catch (const std::exception&) { /* fall through; ofstream will report */ }

		std::ofstream file(path);
		if (!file.is_open())
		{
			HMN_CORE_ERROR("CutsceneLoader: could not write '{}'", path);
			return false;
		}

		file << root.dump(2);
		HMN_CORE_INFO("CutsceneLoader: saved '{}'", path);
		return true;
	}

}
