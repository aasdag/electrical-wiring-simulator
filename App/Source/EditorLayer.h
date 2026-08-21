#pragma once

#include "Core/Layer.h"

class EditorLayer final : public Core::Layer
{
public:
    EditorLayer();
    ~EditorLayer() override = default;

    void onUpdate(float timeStep) override;
    void onRender() override;
    void onImGuiRender() override;

private:
    void renderMenuBar();
    void renderToolbar();
    void renderToolPanel();
    void renderPropertiesPanel();
    void renderStatusBar();
private:
    float m_time = 0.0f;
};