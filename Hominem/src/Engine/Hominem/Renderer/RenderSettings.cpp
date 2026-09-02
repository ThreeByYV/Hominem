#include "hmnpch.h"
#include "RenderSettings.h"
#include "RenderThread.h"
#include "Renderer2D.h"
#include "Renderer3D.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

namespace Hominem {

namespace {

using Type = RenderSettings::Setting::Type;

enum Flags : uint8_t
{
	None    = 0,
	Persist = 1 << 0, // written to the config file
	Derived = 1 << 1, // recomputed every launch, so editing it does nothing
};

struct Entry
{
	const char* name;
	void*       addr;
	Type        type;
	uint8_t     flags;
};

// Adding a setting means adding a row here; the config file, the command line, the startup
// log and the capture notes all come off this one table.
#define HMN_SETTING(field, type, flags) { #field, &RenderSettings::field, Type::type, flags }

const Entry s_Entries[] = {
	// Debug overlays stay out of the config file. One left on at exit coming back next
	// launch reads as a rendering bug rather than as a setting.
	HMN_SETTING(DrawNormals,      Bool,  None),
	HMN_SETTING(NormalLength,     Float, None),
	HMN_SETTING(DrawAABB,         Bool,  None),
	HMN_SETTING(DrawBoneWeights,  Bool,  None),
	HMN_SETTING(DisplayBoneIndex, Int,   None),
	HMN_SETTING(DebugHeatmap,     Bool,  None),
	HMN_SETTING(DDGIDebug,        Bool,  None),
	HMN_SETTING(TAADebugView,     Int,   None),

	HMN_SETTING(ToonShading, Bool, Persist),
	HMN_SETTING(AreaLights,  Bool, Persist),

	HMN_SETTING(StrictGLErrors,         Bool, Persist),
	HMN_SETTING(RayTracing,             Bool, Persist),
	HMN_SETTING(RayTracingOnIntegrated, Bool, Persist),

	HMN_SETTING(TAA,            Bool,  Persist),
	HMN_SETTING(TAAVelocity,    Bool,  Persist),
	HMN_SETTING(TAADilation,    Bool,  Persist),
	HMN_SETTING(TAAFeedbackMin, Float, Persist),
	HMN_SETTING(TAAFeedbackMax, Float, Persist),
	HMN_SETTING(TAAJitterScale, Float, Persist),

	HMN_SETTING(RecommendedRenderScale, Float, Derived),
};

#undef HMN_SETTING

std::string ValueToString(const Entry& e)
{
	switch (e.type)
	{
		case Type::Bool: return *static_cast<bool*>(e.addr) ? "1" : "0";
		case Type::Int:  return std::to_string(*static_cast<int*>(e.addr));
		case Type::Float:
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%g", *static_cast<float*>(e.addr));
			return buf;
		}
	}
	return {};
}

// The settings are all constant-initialised, so they hold their compiled-in values by the
// time this runs. Lets LogAll and SaveTo talk about what actually changed. Re-taken by
// CaptureDefaults once the game has stated its own baseline.
std::vector<std::string> s_Defaults = []
{
	std::vector<std::string> defaults;
	defaults.reserve(std::size(s_Entries));
	for (const Entry& e : s_Entries)
		defaults.push_back(ValueToString(e));
	return defaults;
}();

std::string ToLower(std::string_view s)
{
	std::string out(s);
	std::transform(out.begin(), out.end(), out.begin(),
		[](unsigned char c) { return (char)std::tolower(c); });
	return out;
}

std::string_view Trim(std::string_view s)
{
	const auto first = s.find_first_not_of(" \t\r\n");
	if (first == std::string_view::npos) return {};
	return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

const Entry* Find(std::string_view name)
{
	const std::string wanted = ToLower(name);
	for (const Entry& e : s_Entries)
		if (ToLower(e.name) == wanted)
			return &e;
	return nullptr;
}

bool Assign(const Entry& e, std::string_view value)
{
	if (value.empty()) return false;

	if (e.type == Type::Bool)
	{
		const std::string v = ToLower(value);
		if (v == "1" || v == "true"  || v == "on")  { *static_cast<bool*>(e.addr) = true;  return true; }
		if (v == "0" || v == "false" || v == "off") { *static_cast<bool*>(e.addr) = false; return true; }
		return false;
	}

	const std::string text = std::string(value);
	char* end = nullptr;

	if (e.type == Type::Int)
	{
		const long parsed = std::strtol(text.c_str(), &end, 10);
		if (end == text.c_str() || *end != '\0') return false;
		*static_cast<int*>(e.addr) = (int)parsed;
		return true;
	}

	const float parsed = std::strtof(text.c_str(), &end);
	if (end == text.c_str() || *end != '\0') return false;
	*static_cast<float*>(e.addr) = parsed;
	return true;
}

}

	void RenderSettings::RequestShaderReload()
	{
		RenderThread::QueueUpload([]
		{
			Renderer2D::GetShaderLibrary()->ReloadAll();
			Renderer3D::GetShaderLibrary()->ReloadAll();
			Renderer3D::ReloadVariants();
		});
	}

	bool RenderSettings::Set(std::string_view name, std::string_view value)
	{
		const Entry* entry = Find(name);
		if (!entry)
		{
			HMN_CORE_WARN("RenderSettings: unknown setting '{}'", std::string(name));
			return false;
		}

		if (!Assign(*entry, value))
		{
			HMN_CORE_WARN("RenderSettings: '{}' is not a valid value for {}",
				std::string(value), entry->name);
			return false;
		}
		return true;
	}

	std::string RenderSettings::Describe()
	{
		std::string out;
		for (const Entry& e : s_Entries)
		{
			out += e.name;
			out += '=';
			out += ValueToString(e);
			out += '\n';
		}
		return out;
	}

	void RenderSettings::LogAll()
	{
		std::string changed;
		for (size_t i = 0; i < std::size(s_Entries); i++)
		{
			const std::string value = ValueToString(s_Entries[i]);
			if (value == s_Defaults[i]) continue;

			if (!changed.empty()) changed += ' ';
			changed += s_Entries[i].name;
			changed += '=';
			changed += value;
		}

		if (changed.empty())
			HMN_CORE_INFO("RenderSettings: all defaults");
		else
			HMN_CORE_INFO("RenderSettings: {}", changed);
	}

	void RenderSettings::ApplyArgs(int argc, char** argv)
	{
		for (int i = 1; i < argc; i++)
		{
			std::string_view arg = argv[i];
			if (arg.size() < 2 || arg[0] != '-') continue;
			arg.remove_prefix(arg[1] == '-' ? 2 : 1);

			const size_t eq = arg.find('=');
			if (eq == std::string_view::npos) continue;

			Set(Trim(arg.substr(0, eq)), Trim(arg.substr(eq + 1)));
		}
	}

	void RenderSettings::LoadFrom(const std::string& path)
	{
		std::ifstream file(path);
		if (!file.is_open()) return;

		uint32_t applied = 0;
		std::string line;
		while (std::getline(file, line))
		{
			const size_t comment = line.find_first_of("#;");
			if (comment != std::string::npos) line.resize(comment);

			const size_t eq = line.find('=');
			if (eq == std::string::npos) continue;

			if (Set(Trim(std::string_view(line).substr(0, eq)),
			        Trim(std::string_view(line).substr(eq + 1))))
				applied++;
		}

		HMN_CORE_INFO("RenderSettings: applied {} from {}", applied, path);
	}

	void RenderSettings::SaveTo(const std::string& path)
	{
		std::ofstream file(path, std::ios::trunc);
		if (!file.is_open())
		{
			HMN_CORE_WARN("RenderSettings: cannot write '{}'", path);
			return;
		}

		for (const Entry& e : s_Entries)
			if (e.flags & Persist)
				file << e.name << '=' << ValueToString(e) << '\n';
	}

	std::vector<RenderSettings::Setting> RenderSettings::Enumerate()
	{
		std::vector<Setting> out;
		out.reserve(std::size(s_Entries));

		for (size_t i = 0; i < std::size(s_Entries); i++)
			out.push_back({ s_Entries[i].name, s_Entries[i].addr, s_Entries[i].type,
			                ValueToString(s_Entries[i]) == s_Defaults[i],
			                (s_Entries[i].flags & Derived) != 0 });

		return out;
	}

	void RenderSettings::ResetToDefaults()
	{
		for (size_t i = 0; i < std::size(s_Entries); i++)
			Assign(s_Entries[i], s_Defaults[i]);
	}

	void RenderSettings::CaptureDefaults()
	{
		for (size_t i = 0; i < std::size(s_Entries); i++)
			s_Defaults[i] = ValueToString(s_Entries[i]);
	}

}
