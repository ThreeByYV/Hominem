#pragma once
#include <string>
#include <vector>

namespace Hominem {

/**
 * @brief Maps URI prefixes (e.g. @c engine://, @c game://) to filesystem roots.
 *
 * Keeps engine and client assets separate so multiple games can share the engine
 * without shipping or duplicating its internal resources.
 *
 * @note Path resolution only — file copying is handled by CMake/Premake post-build.
 */
class VFS
{
public:
    static void        Mount(const std::string& name, const std::string& rootPath);
    static std::string Resolve(const std::string& uri);

private:
    struct MountPoint { std::string name; std::string root; };
    static std::vector<MountPoint> s_Mounts;
};

}
