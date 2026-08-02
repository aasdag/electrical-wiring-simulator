#include "TestLayer.h"
#include "EditorLayer.h"
#include "Core/Application.h"

#include <raylib.h>
#include <print>

TestLayer::TestLayer()
{
    std::println("Created new AppLayer!");
}

TestLayer::~TestLayer()
{
}

void TestLayer::onUpdate(float timeStep)
{
	m_Time += timeStep;

	if (IsKeyDown(KEY_ONE))
	{
        std::println("{}", "Transitioning to EditorLayer");
	    transitionTo<EditorLayer>();
	}
}

void TestLayer::onRender()
{
    DrawText("TestLayer active", 100, 100, 20, LIME);
}
