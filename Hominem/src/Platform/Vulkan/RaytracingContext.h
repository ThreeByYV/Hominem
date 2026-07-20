#pragma once

#include "VulkanCore.h"

namespace Hominem {

class VulkanRenderer;
class VulkanShaderLibrary;

struct RaytracingContext
{
    VulkanRenderer&            renderer;      // device / allocator / frame index / frame deletion queue
    VkCommandBuffer            cmd;
    VkAccelerationStructureKHR tlas;          // the scene
    VkDeviceAddress            instanceGeom;  // per-instance { vtxAddr, idxAddr, baseColor }
    VkDeviceAddress            scene;         // GPUSceneData (lights + camera)
    VulkanShaderLibrary&       shaders;       // shared compile cache
};

}
