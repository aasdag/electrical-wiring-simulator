#include "EditorLayer.h"

#include <imgui.h>
#include <print>
#include <raylib.h>

void EditorLayer::onUpdate(float timeStep)
{
   //m_time += timeStep;
   if(IsKeyDown(KEY_TWO))
   {
        transitionTo(VoidLayer);
   }
}

void EditorLayer::onRender()
{
    DrawText("EditorLayer active", 100, 100, 20, RED);
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