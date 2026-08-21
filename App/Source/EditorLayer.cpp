#include "Core/Application.h"
#include "EditorLayer.h"
#include "TestLayer.h"
#include <imgui.h>
#include <print>
#include <raylib.h>

EditorLayer::EditorLayer()
{
    std::println("{}", "Creating new EditorLayer");
}

void EditorLayer::onUpdate(float timeStep)
{
   //m_time += timeStep;
   if(IsKeyDown(KEY_TWO))
   {
        std::println("{}", "Transitioning to 'TestLayer'");
        transitionTo<TestLayer>();
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
    renderMenuBar();
}

void EditorLayer::renderMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {

            }
            if (ImGui::MenuItem("Open"))
            {

            }
            if (ImGui::MenuItem("Save"))
            {

            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {
                Core::Application::get().stop();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorLayer::renderToolbar()
{
}

void EditorLayer::renderToolPanel()
{
}

void EditorLayer::renderPropertiesPanel()
{
}

void EditorLayer::renderStatusBar()
{
}
