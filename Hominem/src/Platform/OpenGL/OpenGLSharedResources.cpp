#include "hmnpch.h"
#include "OpenGLSharedResources.h"

#include <glad/glad.h>

namespace Hominem {

// GL_EXT_memory_object / GL_EXT_memory_object_win32 / GL_EXT_semaphore / GL_EXT_semaphore_win32
// Not present in the pre-built GLAD — load manually after GL context is current.

static constexpr GLenum k_HandleTypeOpaqueWin32 = 0x9587u;
static constexpr GLenum k_LayoutShaderReadOnly  = 0x9509u;
static constexpr GLenum k_DeviceLUIDEXT         = 0x9599u;

using PFN_glGetUnsignedBytei_vEXT        = void (APIENTRY*)(GLenum, GLuint, GLubyte*);
using PFN_glCreateMemoryObjectsEXT       = void (APIENTRY*)(GLsizei, GLuint*);
using PFN_glDeleteMemoryObjectsEXT       = void (APIENTRY*)(GLsizei, const GLuint*);
using PFN_glImportMemoryWin32HandleEXT   = void (APIENTRY*)(GLuint, GLuint64, GLenum, void*);
using PFN_glTextureStorageMem2DEXT       = void (APIENTRY*)(GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLuint, GLuint64);
using PFN_glGenSemaphoresEXT             = void (APIENTRY*)(GLsizei, GLuint*);
using PFN_glDeleteSemaphoresEXT          = void (APIENTRY*)(GLsizei, const GLuint*);
using PFN_glImportSemaphoreWin32HandleEXT = void (APIENTRY*)(GLuint, GLenum, void*);
using PFN_glWaitSemaphoreEXT             = void (APIENTRY*)(GLuint, GLuint, const GLuint*, GLuint, const GLuint*, const GLenum*);
using PFN_glSignalSemaphoreEXT           = void (APIENTRY*)(GLuint, GLuint, const GLuint*, GLuint, const GLuint*, const GLenum*);

static PFN_glGetUnsignedBytei_vEXT        pfn_GetUnsignedBytei_v   = nullptr;
static PFN_glCreateMemoryObjectsEXT        pfn_CreateMemoryObjects  = nullptr;
static PFN_glDeleteMemoryObjectsEXT        pfn_DeleteMemoryObjects  = nullptr;
static PFN_glImportMemoryWin32HandleEXT    pfn_ImportMemoryWin32    = nullptr;
static PFN_glTextureStorageMem2DEXT        pfn_TextureStorageMem2D  = nullptr;
static PFN_glGenSemaphoresEXT              pfn_GenSemaphores        = nullptr;
static PFN_glDeleteSemaphoresEXT           pfn_DeleteSemaphores     = nullptr;
static PFN_glImportSemaphoreWin32HandleEXT pfn_ImportSemaphoreWin32 = nullptr;
static PFN_glWaitSemaphoreEXT              pfn_WaitSemaphore        = nullptr;
static PFN_glSignalSemaphoreEXT            pfn_SignalSemaphore      = nullptr;

static void LoadProcs()
{
    static bool s_Loaded = false;
    if (s_Loaded) return;
    s_Loaded = true;

    auto get = [](const char* name) -> void*
    {
        void* p = (void*)wglGetProcAddress(name);
        HMN_CORE_ASSERT(p, "GL shared-resource proc not found: {0}", name);
        return p;
    };

    pfn_GetUnsignedBytei_v   = (PFN_glGetUnsignedBytei_vEXT)        get("glGetUnsignedBytei_vEXT");
    pfn_CreateMemoryObjects  = (PFN_glCreateMemoryObjectsEXT)        get("glCreateMemoryObjectsEXT");
    pfn_DeleteMemoryObjects  = (PFN_glDeleteMemoryObjectsEXT)        get("glDeleteMemoryObjectsEXT");
    pfn_ImportMemoryWin32    = (PFN_glImportMemoryWin32HandleEXT)    get("glImportMemoryWin32HandleEXT");
    pfn_TextureStorageMem2D  = (PFN_glTextureStorageMem2DEXT)        get("glTextureStorageMem2DEXT");
    pfn_GenSemaphores        = (PFN_glGenSemaphoresEXT)              get("glGenSemaphoresEXT");
    pfn_DeleteSemaphores     = (PFN_glDeleteSemaphoresEXT)           get("glDeleteSemaphoresEXT");
    pfn_ImportSemaphoreWin32 = (PFN_glImportSemaphoreWin32HandleEXT) get("glImportSemaphoreWin32HandleEXT");
    pfn_WaitSemaphore        = (PFN_glWaitSemaphoreEXT)              get("glWaitSemaphoreEXT");
    pfn_SignalSemaphore      = (PFN_glSignalSemaphoreEXT)            get("glSignalSemaphoreEXT");
}

std::array<uint8_t, 8> OpenGLSharedResources::GetDeviceLUID()
{
    auto pfn = (PFN_glGetUnsignedBytei_vEXT)wglGetProcAddress("glGetUnsignedBytei_vEXT");
    if (!pfn)
    {
        HMN_CORE_WARN("OpenGLSharedResources: GL_EXT_memory_object not exposed by driver, falling back to name match");
        return {};
    }

    std::array<uint8_t, 8> luid{};
    pfn(k_DeviceLUIDEXT, 0, luid.data());
    return luid;
}

std::string OpenGLSharedResources::GetDeviceName()
{
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    return renderer ? renderer : "";
}

void OpenGLSharedResources::ImportSharedTexture(HANDLE memHandle, uint64_t memSize,
                                                uint32_t w, uint32_t h)
{
    LoadProcs();

    pfn_CreateMemoryObjects(1, &m_MemObject);
    pfn_ImportMemoryWin32(m_MemObject, (GLuint64)memSize, k_HandleTypeOpaqueWin32, memHandle);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_Texture);
    pfn_TextureStorageMem2D(m_Texture, 1, GL_RGBA16F, (GLsizei)w, (GLsizei)h, m_MemObject, 0);
    glTextureParameteri(m_Texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_Texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_Texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_Texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void OpenGLSharedResources::ImportSemaphore(uint32_t frameIdx, HANDLE semHandle)
{
    LoadProcs();

    pfn_GenSemaphores(1, &m_Semaphores[frameIdx]);
    pfn_ImportSemaphoreWin32(m_Semaphores[frameIdx], k_HandleTypeOpaqueWin32, semHandle);
}

void OpenGLSharedResources::ImportGLDoneSemaphore(HANDLE semHandle)
{
    LoadProcs();

    pfn_GenSemaphores(1, &m_GLDoneSemaphore);
    pfn_ImportSemaphoreWin32(m_GLDoneSemaphore, k_HandleTypeOpaqueWin32, semHandle);
}

void OpenGLSharedResources::WaitSemaphore(uint32_t frameIdx)
{
    const GLenum layout = k_LayoutShaderReadOnly;
    pfn_WaitSemaphore(m_Semaphores[frameIdx], 0, nullptr, 1, &m_Texture, &layout);
}

void OpenGLSharedResources::SignalGLDone()
{
    const GLenum layout = k_LayoutShaderReadOnly;
    pfn_SignalSemaphore(m_GLDoneSemaphore, 0, nullptr, 1, &m_Texture, &layout);
}

void OpenGLSharedResources::Destroy()
{
    if (m_Texture)   { glDeleteTextures(1, &m_Texture);          m_Texture   = 0; }
    if (m_MemObject) { pfn_DeleteMemoryObjects(1, &m_MemObject); m_MemObject = 0; }
    for (auto& sem : m_Semaphores)
        if (sem) { pfn_DeleteSemaphores(1, &sem); sem = 0; }
    if (m_GLDoneSemaphore) { pfn_DeleteSemaphores(1, &m_GLDoneSemaphore); m_GLDoneSemaphore = 0; }
}

}
