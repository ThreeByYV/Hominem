#pragma once

#include "Hominem/Core/Layer.h"
#include "Hominem/Renderer/Font.h"

#include <array>
#include <string_view>

/// @brief Shown right after the menu while CutscenePreload finishes importing assets
class LoadingLayer : public Hominem::Layer
{
public:
	LoadingLayer();

	void OnAttach()                                  override;
	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;

private:
	static constexpr std::array<std::string_view, 6> k_Quotes = {{
		"\"Ah, to the lord who dwells in the sky that granted my prayers,\n"
		"I make one final wish - Let her rest in peace\"  - Malice Mizer",

		"\"Oh my sweet corpse, inside me this love is weighing me down\"  - Malice Mizer",

		"I whispered, \"At least until I wake from this dream,\n"
		"just a while more\"  - Malice Mizer",

		"\"Nothing changed in this sky that carries our thoughts on the clouds,\n"
		"nothing has changed\"  - Malice Mizer",

		"\"The only way to deal with an unfree world is to become so absolutely free\n"
		"that your very existence is an act of rebellion\"  - Albert Camus",

		"\"What does not destroy me, makes me stronger.\"  - Nietzsche",
	}};

	Hominem::Ref<Hominem::Font> m_Font;
	int   m_QuoteIndex    = 0;
	float m_PulseT        = 0.f;
	bool  m_Transitioning = false;
};
