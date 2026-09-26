#include "hmnpch.h"
#include "Hominem/Renderer/RHI/SharedResources.h"

#include "Platform/OpenGL/OpenGLSharedResources.h"

namespace Hominem {

std::unique_ptr<SharedResources> SharedResources::Create()
{
    return std::make_unique<OpenGLSharedResources>();
}

std::array<uint8_t, 8> SharedResources::GetDeviceLUID()
{
    return OpenGLSharedResources::GetDeviceLUID();
}

std::string SharedResources::GetDeviceName()
{
    return OpenGLSharedResources::GetDeviceName();
}

bool SharedResources::IsInteropSupported()
{
    return OpenGLSharedResources::IsInteropSupported();
}

}
