#pragma once

#include "Core/Layer.h"

class EditorLayer final : public Core::Layer
{
public:
    EditorLayer() = default;
    ~EditorLayer() override = default;

    void onUpdate(float timeStep) override;
    void onRender() override;
    void onImGuiRender() override;
};