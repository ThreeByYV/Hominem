#include "hmnpch.h"
#include "OpenGLSharedResources.h"

#include <glad/glad.h>
#include <cstring>

namespace Hominem {

// GL_EXT_memory_object / GL_EXT_memory_object_win32 / GL_EXT_semaphore / GL_EXT_semaphore_win32
// Not present in the pre-built GLAD - load manually after GL context is current.

static constexpr GLenum k_HandleTypeOpaqueWin32 = 0x9587u;
static constexpr GLenum k_LayoutShaderReadOnly  = 0x9591u; // GL_LAYOUT_SHADER_READ_ONLY_EXT
static constexpr GLenum k_LayoutGeneral         = 0x958Du;
static constexpr GLenum k_DeviceLUIDEXT         = 0x9599u;

using PFN_glGetUnsignedBytevEXT          = void (APIENTRY*)(GLenum, GLubyte*);
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

static bool HasGLExtension(const char* name)
{
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; i++)
    {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (ext && std::strcmp(ext, name) == 0)
            return true;
    }
    return false;
}

bool OpenGLSharedResources::IsInteropSupported()
{
    static constexpr const char* required[] = {
        "GL_EXT_memory_object", "GL_EXT_memory_object_win32",
        "GL_EXT_semaphore",     "GL_EXT_semaphore_win32",
    };

    bool ok = true;
    for (const char* ext : required)
    {
        if (!HasGLExtension(ext))
        {
            HMN_CORE_WARN("OpenGLSharedResources: missing {} - GL/VK interop unavailable", ext);
            ok = false;
        }
    }
    return ok;
}

std::array<uint8_t, 8> OpenGLSharedResources::GetDeviceLUID()
{
    // Non-indexed get: DEVICE_LUID_EXT is a plain pname in EXT_external_objects_win32,
    // unlike DEVICE_UUID_EXT which is indexed by device. Querying it through the indexed
    // entry point is GL_INVALID_ENUM - NVIDIA answers anyway, Intel rejects it and the
    // LUID comes back zeroed, silently dropping the adapter match to the name fallback.
    auto pfn = (PFN_glGetUnsignedBytevEXT)wglGetProcAddress("glGetUnsignedBytevEXT");
    if (!pfn || !HasGLExtension("GL_EXT_memory_object_win32"))
    {
        HMN_CORE_WARN("OpenGLSharedResources: GL_EXT_memory_object_win32 not exposed by driver, falling back to name match");
        return {};
    }

    std::array<uint8_t, 8> luid{};
    pfn(k_DeviceLUIDEXT, luid.data());
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
    m_Texture = ImportSharedImage({ memHandle, memSize, w, h, /*generalLayout=*/false });
}

uint32_t OpenGLSharedResources::ImportSharedImage(const SharedImageDesc& desc)
{
    LoadProcs();

    if (!desc.memHandle || desc.memSize == 0 || desc.width == 0 || desc.height == 0)
        return 0;

    // Vulkan's limits are its own; a shared image can be legal there and too big here.
    // Without this the storage call just fails and leaves an incomplete texture that
    // samples as black, which looks like a broken technique rather than a size problem.
    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if ((GLint)desc.width > maxTexSize || (GLint)desc.height > maxTexSize)
    {
        HMN_CORE_ERROR("Shared image {0}x{1} exceeds GL_MAX_TEXTURE_SIZE ({2})",
                       desc.width, desc.height, maxTexSize);
        return 0;
    }

    ImportedImage img;
    img.layout = desc.generalLayout ? k_LayoutGeneral : k_LayoutShaderReadOnly;

    pfn_CreateMemoryObjects(1, &img.memObject);
    pfn_ImportMemoryWin32(img.memObject, (GLuint64)desc.memSize, k_HandleTypeOpaqueWin32, desc.memHandle);

    while (glGetError() != GL_NO_ERROR) {}
    glCreateTextures(GL_TEXTURE_2D, 1, &img.texture);
    pfn_TextureStorageMem2D(img.texture, 1, GL_RGBA16F, (GLsizei)desc.width, (GLsizei)desc.height,
                            img.memObject, 0);
    if (const GLenum err = glGetError(); err != GL_NO_ERROR)
    {
        HMN_CORE_ERROR("Shared image {0}x{1} storage failed (GL error 0x{2:X})",
                       desc.width, desc.height, err);
        glDeleteTextures(1, &img.texture);
        pfn_DeleteMemoryObjects(1, &img.memObject);
        return 0;
    }

    glTextureParameteri(img.texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(img.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(img.texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(img.texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Keep the first import addressable through m_MemObject so Destroy's original
    // teardown order (draw image last) is unchanged.
    if (m_Images.empty()) m_MemObject = img.memObject;

    m_Images.push_back(img);
    RebuildSyncLists();
    return img.texture;
}

void OpenGLSharedResources::ReleaseSharedImage(uint32_t texID)
{
    for (auto it = m_Images.begin(); it != m_Images.end(); ++it)
    {
        if (it->texture != texID) continue;

        glDeleteTextures(1, &it->texture);
        pfn_DeleteMemoryObjects(1, &it->memObject);
        if (m_MemObject == it->memObject) m_MemObject = 0;
        if (m_Texture   == texID)         m_Texture   = 0;
        m_Images.erase(it);
        RebuildSyncLists();
        return;
    }
}

void OpenGLSharedResources::RebuildSyncLists()
{
    m_SyncTextures.clear();
    m_SyncLayouts.clear();
    m_SyncTextures.reserve(m_Images.size());
    m_SyncLayouts.reserve(m_Images.size());
    for (const auto& img : m_Images)
    {
        m_SyncTextures.push_back(img.texture);
        m_SyncLayouts.push_back(img.layout);
    }
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
    if (m_SyncTextures.empty()) return;
    pfn_WaitSemaphore(m_Semaphores[frameIdx], 0, nullptr,
                      (GLuint)m_SyncTextures.size(), m_SyncTextures.data(), m_SyncLayouts.data());
}

void OpenGLSharedResources::SignalGLDone()
{
    if (m_SyncTextures.empty()) return;
    pfn_SignalSemaphore(m_GLDoneSemaphore, 0, nullptr,
                        (GLuint)m_SyncTextures.size(), m_SyncTextures.data(), m_SyncLayouts.data());
}

void OpenGLSharedResources::Destroy()
{
    for (auto& img : m_Images)
    {
        glDeleteTextures(1, &img.texture);
        pfn_DeleteMemoryObjects(1, &img.memObject);
    }
    m_Images.clear();
    RebuildSyncLists();
    m_Texture   = 0;
    m_MemObject = 0;
    for (auto& sem : m_Semaphores)
        if (sem) { pfn_DeleteSemaphores(1, &sem); sem = 0; }
    if (m_GLDoneSemaphore) { pfn_DeleteSemaphores(1, &m_GLDoneSemaphore); m_GLDoneSemaphore = 0; }
}

}
