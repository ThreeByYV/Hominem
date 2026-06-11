#pragma once

#include "Hominem/Core/Core.h"

namespace Hominem {

	enum class FramebufferFormat { RGBA8, RGBA16F };

	struct FramebufferSpecification
	{
		uint32_t          Width               = 0;
		uint32_t          Height              = 0;
		FramebufferFormat Format              = FramebufferFormat::RGBA8;
		uint32_t          Samples             = 1;
		bool              SwapChainTarget     = false;
		uint32_t          NumColorAttachments = 1; // MRT: up to 4 same-format attachments
	};

	class Framebuffer : public RefCounted
	{
	public:
		/// Binds this framebuffer as the active draw target.
		virtual void Bind() = 0;

		/// Unbinds this framebuffer, restoring the default framebuffer.
		virtual void Unbind() = 0;

		/// Recreates all attachments at the new dimensions.
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		/// Blits the color attachment to the default framebuffer using a single DSA call.
		virtual void BlitToDefault(uint32_t screenW, uint32_t screenH) = 0;

		/// Returns the GL texture ID of color attachment at index (0 = primary).
		virtual uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const = 0;

		/// Returns the spec this framebuffer was created from.
		virtual const FramebufferSpecification& GetSpecification() const = 0;

		/// Creates a platform-specific framebuffer from the given spec.
		static Ref<Framebuffer> Create(const FramebufferSpecification& spec);
	};

}
