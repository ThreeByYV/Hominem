#pragma once

#include "Hominem.h"

class VoidLayer : public Hominem::Layer
{
public:
	VoidLayer();

	void OnUpdate(Hominem::Timestep ts) override;
	void OnImGuiRender() override;
};
