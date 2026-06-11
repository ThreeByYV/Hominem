#pragma once

#include <cstdint>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <type_traits>
#include <vector>

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

	inline std::filesystem::file_time_type LastWriteTime(const std::filesystem::path& path)
	{
		std::error_code ec;
		auto t = std::filesystem::last_write_time(path, ec);
		return ec ? std::filesystem::file_time_type{} : t;
	}

	// --- binary serialization primitives (for cache files etc.) ---

	/// Write a single trivially-copyable value.
	template<typename T>
	void WriteValue(std::ostream& os, const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "WriteValue needs a trivially-copyable type");
		os.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	/// Read a single trivially-copyable value. Returns false on stream failure.
	template<typename T>
	bool ReadValue(std::istream& is, T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "ReadValue needs a trivially-copyable type");
		is.read(reinterpret_cast<char*>(&value), sizeof(T));
		return static_cast<bool>(is);
	}

	/// Write a contiguous array of trivially-copyable values (no length prefix).
	template<typename T>
	void WriteArray(std::ostream& os, const std::vector<T>& vec)
	{
		static_assert(std::is_trivially_copyable_v<T>, "WriteArray needs a trivially-copyable type");
		if (!vec.empty())
			os.write(reinterpret_cast<const char*>(vec.data()), vec.size() * sizeof(T));
	}

	/// Read `count` trivially-copyable values into `vec`. Returns false on stream failure.
	template<typename T>
	bool ReadArray(std::istream& is, std::vector<T>& vec, size_t count)
	{
		static_assert(std::is_trivially_copyable_v<T>, "ReadArray needs a trivially-copyable type");
		vec.resize(count);
		if (count > 0)
			is.read(reinterpret_cast<char*>(vec.data()), count * sizeof(T));
		return static_cast<bool>(is);
	}

	/// Write a length-prefixed string: [uint32 len][bytes].
	inline void WriteString(std::ostream& os, const std::string& s)
	{
		WriteValue(os, static_cast<uint32_t>(s.size()));
		if (!s.empty()) os.write(s.data(), s.size());
	}

	/// Read a length-prefixed string written by WriteString.
	inline std::string ReadString(std::istream& is)
	{
		uint32_t len = 0;
		if (!ReadValue(is, len) || len == 0) return {};
		std::string s(len, '\0');
		is.read(s.data(), len);
		return s;
	}

}
