#pragma once

#include <glm/glm.hpp>
#include "VertexArray.h"

namespace Hominem {

	enum class BlendMode { Alpha = 0, Additive };

	/// Full rasterizer/output-merger state for one render pass.
	/// Use RenderCommand::SetPipelineState() at the start of every pass, never rely on inherited state.
	struct PipelineState
	{
		bool      blend      = false;
		BlendMode blendMode  = BlendMode::Alpha;
		bool      depthTest  = true;
		bool      depthWrite = true;
		bool      cullFace   = true;
		bool      scissor    = false;

		static constexpr PipelineState DepthTestWriteCull()    { return {}; }
		static constexpr PipelineState NoDepthNoCull()         { return { false, BlendMode::Alpha,    false, false, false, false }; }
		static constexpr PipelineState AlphaBlendDepthTest()   { return { true,  BlendMode::Alpha,    true,  false, false, false }; }
		static constexpr PipelineState AdditiveBlendDepthTest(){ return { true,  BlendMode::Additive, true,  false, false, false }; }
		static constexpr PipelineState AlphaBlendNoDepth()     { return { true,  BlendMode::Alpha,    false, false, true,  false }; }
	};

	// Base/OpenGL* split (here and across Shader/Texture/Buffer/etc.) exists to keep Platform/OpenGL
	// headers out of Renderer/ headers, not to prepare for a swappable Vulkan backend, Vulkan is a
	// fixed-role compute/RT coprocessor in this engine, never an alternative to these classes.
	class RendererAPI
	{
	public:
		virtual void Init()      = 0;
		virtual void Shutdown() {}

		/// Sets all rasterizer/blend/depth state at once. Call at the start of every pass.
		virtual void ApplyState(const PipelineState& s) = 0;

		/// Sets the GL viewport rectangle.
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

		/// Sets the color written by the next Clear() call.
		virtual void SetClearColor(const glm::vec4& color) = 0;

		/// Clears the color and depth buffers.
		virtual void Clear() = 0;

		/// Draws the indexed geometry in the given vertex array.
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) = 0;

		/// Draws indexCount indices from the given vertex array as GL_LINES.
		virtual void DrawIndexedLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount) = 0;

		/// Enables or disables the depth test.
		virtual void SetDepthTestEnabled(bool enabled) = 0;

		/// Enables or disables writing to the depth buffer (depth test can stay enabled).
		virtual void SetDepthWriteEnabled(bool enabled) = 0;

		/// Selects the colour blend equation for subsequent draws.
		virtual void SetBlendMode(BlendMode mode) = 0;

		/// Enables or disables back-face culling.
		virtual void SetCullFaceEnabled(bool enabled) = 0;

		/// Enables or disables the scissor test.
		virtual void SetScissorEnabled(bool enabled) = 0;

		/// Binds the engine-owned empty VAO (required by OpenGL before any draw call).
		virtual void BindEmptyVAO() = 0;

		/// Unbinds the currently bound VAO.
		virtual void UnbindVAO() = 0;

		/// Issues an indirect draw using parameters from the currently bound DrawIndirectBuffer at the given byte offset.
		virtual void DrawArraysIndirect(uint32_t offset) = 0;

		/// Draws a fullscreen triangle using gl_VertexID — no VBO needed. Use for post-process passes.
		virtual void DrawFullscreenTriangle() = 0;

		/// Draws a unit quad (-0.5..0.5 in XY, Z=0) using gl_VertexID — no VBO needed.
		virtual void DrawUnitQuad() = 0;

		/// Draws count vertices as points with the given pixel size.
		virtual void DrawPoints(uint32_t count, float pointSize) = 0;

		/// Draws count patches for tessellation.
		virtual void DrawPatches(uint32_t count, uint32_t patchVertices = 1) = 0;

		/// Toggles wireframe polygon mode.
		virtual void SetWireframe(bool enabled) = 0;

		/// Binds a texture to the given texture unit.
		virtual void BindTexture(uint32_t slot, uint32_t id) = 0;

		/// Binds a raw VAO by id (0 unbinds).
		virtual void BindVertexArray(uint32_t vaoID) = 0;

		/// Draws indexCount indices starting at indexByteOffset, offsetting all vertex indices by baseVertex.
		virtual void DrawElementsBaseVertex(uint32_t indexCount, uint32_t indexByteOffset, int32_t baseVertex) = 0;

		/// Overwrites part of a raw GL buffer's contents (glNamedBufferSubData).
		virtual void UpdateBufferSubData(uint32_t bufferID, const void* data, uint32_t size, uint32_t offset) = 0;

		/// Binds a raw GL buffer as a shader storage buffer at the given binding slot.
		virtual void BindShaderStorageBufferBase(uint32_t bufferID, uint32_t slot) = 0;

		/// Binds a raw GL buffer as the draw-indirect buffer (0 unbinds).
		virtual void BindDrawIndirectBuffer(uint32_t bufferID) = 0;

		/// Issues a multi-draw-elements-indirect call reading drawCount commands starting at byteOffset
		/// in the currently bound draw-indirect buffer, each stride bytes apart.
		virtual void MultiDrawElementsIndirect(uint32_t byteOffset, uint32_t drawCount, uint32_t stride) = 0;

		virtual const char* GetGPUVendor()   const = 0;
		virtual const char* GetGPURenderer() const = 0;

		/// Temporary framebuffer management for off-screen passes (convolution, bake, LUT).
		virtual uint32_t GenFramebuffer()                                    = 0;
		virtual void     BindFramebuffer(uint32_t id)                        = 0;
		virtual void     UnbindFramebuffer()                                 = 0;
		virtual void     DeleteFramebuffer(uint32_t id)                      = 0;
		virtual void     AttachCubeFace(uint32_t cubeID, int face, int mip)  = 0;
		virtual void     Attach2DTexture(uint32_t texID)                     = 0;

		/// Renderbuffer management for depth attachments on temporary framebuffers.
		virtual uint32_t GenRenderbuffer()                                   = 0;
		virtual void     BindRenderbuffer(uint32_t id)                       = 0;
		virtual void     RenderbufferDepth(uint32_t width, uint32_t height)  = 0;
		virtual void     AttachRenderbuffer(uint32_t rboID)                  = 0;
		virtual void     DeleteRenderbuffer(uint32_t id)                     = 0;

	};

}
