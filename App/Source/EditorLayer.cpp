#include "EditorLayer.h"

#include <imgui.h>
#include <print>
#include <raylib.h>

void EditorLayer::onUpdate(float timeStep)
{
    // Update editor state here.
    // For example:
    // - Update camera
    // - Handle wire drawing
    // - Handle symbol placement
}

void EditorLayer::onRender()
{
    // Draw the electrical plan using Raylib here.
    //
    // Example:
    // DrawTexture(housePlan, 0, 0, WHITE);
    // drawWires();
    // drawSymbols();
}

void EditorLayer::onImGuiRender()
{
    ImGui::Begin("Electrical Wiring Simulator");

    if (ImGui::Button("My button"))
    {
        std::println("'My button' clicked");
    }

    ImGui::End();
}