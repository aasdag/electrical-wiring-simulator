#pragma once

#include <stdint.h>

#include "Core/Layer.h"

class TestLayer : public Core::Layer
{
public:
	TestLayer();
	virtual ~TestLayer();

	virtual void onUpdate(float ts) override;
	virtual void onRender() override;
private:
	float m_Time = 0.0f;
};
