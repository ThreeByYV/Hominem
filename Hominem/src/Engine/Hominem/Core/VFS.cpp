#include "hmnpch.h"
#include "VFS.h"

namespace Hominem {

std::vector<VFS::MountPoint> VFS::s_Mounts;

void VFS::Mount(const std::string& name, const std::string& rootPath)
{
    std::string root = rootPath;
    if (!root.empty() && root.back() != '/' && root.back() != '\\')
        root += '/';

    s_Mounts.push_back({ name, root });
    HMN_CORE_INFO("VFS: mounted '{}://' → '{}'", name, root);
}

std::string VFS::Resolve(const std::string& uri)
{
    auto sep = uri.find("://");
    if (sep == std::string::npos)
        return uri; // bare path — pass through unchanged

    std::string name     = uri.substr(0, sep);
    std::string relative = uri.substr(sep + 3);

    for (const auto& m : s_Mounts)
    {
        if (m.name == name)
            return m.root + relative;
    }

    HMN_CORE_WARN("VFS: unknown mount '{}' in '{}' — returning as-is", name, uri);
    return uri;
}

} // namespace Hominem
