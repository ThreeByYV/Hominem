#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "Hominem/Core/Log.h"

namespace Hominem::FileUtils {

	inline std::string ReadTextFile(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			HMN_CORE_ERROR("FileUtils::ReadTextFile: cannot open '{}'", path.string());
			return {};
		}
		std::ostringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	inline bool Exists(const std::filesystem::path& path)
	{
		return std::filesystem::exists(path);
	}

}
