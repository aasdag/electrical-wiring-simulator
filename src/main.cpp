#include <iostream>
#include <expected>
#include <print>

#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 400, "First window");

    rlImGuiSetup(true);

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;


    while(!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
    #pragma region imgui
	rlImGuiBegin();
    #pragma endregion
        


        ImGui::Begin("Test");

        if(ImGui::Button("My button"))
        {
            std::println("{}", "'My button' clicked");
        }

        ImGui::End();

    #pragma region imgui
	rlImGuiEnd();
    #pragma endregion
        EndDrawing();
    }

    rlImGuiShutdown();

    CloseWindow();
    
    return 0;
}
