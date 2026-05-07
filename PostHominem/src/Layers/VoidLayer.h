#pragma once

#include "Hominem.h"

class VoidLayer : public Hominem::Layer
{
public:
	VoidLayer();

	void OnUpdate(Hominem::Timestep ts)              override;
	void OnBuildRenderFrame(Hominem::RenderFrame& f) override;
	void OnImGuiRender()                             override;
};
