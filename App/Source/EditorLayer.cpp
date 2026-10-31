#include "EditorLayer.h"

#include "Core/Application.h"
#include "TestLayer.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <system_error>

namespace fs = std::filesystem;

namespace
{
    void copyText(char* target, std::size_t capacity, const std::string& source)
    {
        std::snprintf(target, capacity, "%s", source.c_str());
    }

    bool containsInsensitive(const std::string& text, const std::string& query)
    {
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        std::string a = text, b = query;
        std::transform(a.begin(), a.end(), a.begin(), lower);
        std::transform(b.begin(), b.end(), b.begin(), lower);
        return a.find(b) != std::string::npos;
    }

    // Small vector icons keep the explorer independent of an icon-font dependency.
    void drawExplorerIcon(ImVec2 position, bool folder)
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float x = position.x, y = position.y;
        if (folder)
        {
            draw->AddRectFilled(ImVec2(x + 2, y + 3), ImVec2(x + 12, y + 10),
                                IM_COL32(245, 191, 100, 255), 3.0f);
            draw->AddRectFilled(ImVec2(x + 2, y + 7), ImVec2(x + 23, y + 21),
                                IM_COL32(236, 171, 72, 255), 4.0f);
            draw->AddLine(ImVec2(x + 4, y + 10), ImVec2(x + 21, y + 10),
                          IM_COL32(255, 214, 148, 190));
        }
        else
        {
            draw->AddRectFilled(ImVec2(x + 4, y + 2), ImVec2(x + 21, y + 22),
                                IM_COL32(60, 143, 225, 255), 3.0f);
            draw->AddTriangleFilled(ImVec2(x + 15, y + 2), ImVec2(x + 21, y + 8),
                                    ImVec2(x + 15, y + 8), IM_COL32(161, 210, 255, 255));
            draw->AddLine(ImVec2(x + 8, y + 13), ImVec2(x + 17, y + 13),
                          IM_COL32(230, 245, 255, 230), 1.5f);
            draw->AddLine(ImVec2(x + 8, y + 17), ImVec2(x + 16, y + 17),
                          IM_COL32(230, 245, 255, 180), 1.5f);
        }
    }

    // A small schematic-style single-pole light switch. Both saved and preview symbols
    // are drawn from the same model coordinates.
    void drawLightSwitch(ImDrawList* draw, ImVec2 center, std::uint64_t id, bool preview)
    {
        const ImU32 stroke = preview ? IM_COL32(90, 186, 255, 160) : IM_COL32(148, 217, 255, 255);
        const ImU32 fill = preview ? IM_COL32(90, 186, 255, 110) : IM_COL32(45, 117, 177, 255);
        const float x = center.x, y = center.y;
        draw->AddLine(ImVec2(x - 34, y), ImVec2(x - 16, y), stroke, 2.2f);
        draw->AddLine(ImVec2(x + 16, y), ImVec2(x + 34, y), stroke, 2.2f);
        draw->AddCircleFilled(ImVec2(x - 14, y), 4.3f, fill);
        draw->AddCircle(ImVec2(x - 14, y), 4.3f, stroke, 0, 1.8f);
        draw->AddCircleFilled(ImVec2(x + 14, y), 4.3f, fill);
        draw->AddCircle(ImVec2(x + 14, y), 4.3f, stroke, 0, 1.8f);
        draw->AddLine(ImVec2(x - 11, y - 3), ImVec2(x + 7, y - 16), stroke, 2.4f);
        const std::string label = "S" + std::to_string(id);
        draw->AddText(ImVec2(x - 12, y + 12), stroke, label.c_str());
    }
}

EditorLayer::EditorLayer()
{
    std::error_code ec;
    m_directory = fs::current_path(ec);
    if (ec) m_directory = ".";
    copyText(m_name.data(), m_name.size(), m_project.name);
    refreshEntries();
}

void EditorLayer::onUpdate(float)
{
    // Retain the existing layer transition shortcut.
    if (!ImGui::GetIO().WantCaptureKeyboard && IsKeyPressed(KEY_TWO))
        transitionTo<TestLayer>();
}

void EditorLayer::onRender()
{
    // The canvas and component symbols are rendered by ImGui in onImGuiRender().
}

void EditorLayer::onImGuiRender()
{
    renderMenuBar();
    renderToolbox();
    renderCanvas();
    renderWorkspace();
    renderFileDialog();
    renderConfirmations();
}

void EditorLayer::requestAction(PendingAction action)
{
    if (m_dirty)
    {
        m_pending = action;
        m_openUnsavedDialog = true;
    }
    else performAction(action);
}

void EditorLayer::performAction(PendingAction action)
{
    m_pending = PendingAction::None;
    switch (action)
    {
        case PendingAction::New:
            m_project = {};
            m_projectPath.clear();
            m_dirty = false;
            copyText(m_name.data(), m_name.size(), m_project.name);
            m_notes.fill('\0');
            m_placementTool.reset();
            break;
        case PendingAction::Open: showDialog(DialogMode::Open); break;
        case PendingAction::Exit: Core::Application::get().stop(); break;
        case PendingAction::None: break;
    }
}

void EditorLayer::renderMenuBar()
{
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New", "Ctrl+N")) requestAction(PendingAction::New);
        if (ImGui::MenuItem("Open...", "Ctrl+O")) requestAction(PendingAction::Open);
        if (ImGui::MenuItem("Save", "Ctrl+S"))
        {
            if (m_projectPath.empty()) showDialog(DialogMode::Save);
            else saveProject(m_projectPath);
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) showDialog(DialogMode::Save);
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) requestAction(PendingAction::Exit);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    const auto& io = ImGui::GetIO();
    if (!io.WantTextInput && io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_N, false)) requestAction(PendingAction::New);
        if (ImGui::IsKeyPressed(ImGuiKey_O, false)) requestAction(PendingAction::Open);
        if (ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (io.KeyShift || m_projectPath.empty()) showDialog(DialogMode::Save);
            else saveProject(m_projectPath);
        }
    }
}

void EditorLayer::renderToolbox()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float top = viewport->Pos.y + ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 62.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.085f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 12));
    if (ImGui::Begin("##ComponentToolbox", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        if (m_dialog == DialogMode::None && ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_placementTool.reset();

        ImGui::TextDisabled("TOOLBOX");
        ImGui::SameLine(90.0f);
        const bool placing = m_placementTool.has_value();
        ImGui::PushStyleColor(ImGuiCol_Button, placing ? ImVec4(0.14f, 0.37f, 0.58f, 1.0f)
                                                       : ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
        if (ImGui::Button("Light switch", ImVec2(146, 34)))
        {
            if (placing) m_placementTool.reset();
            else m_placementTool = ElectricalComponentType::LightSwitch;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", placing ? "Click on canvas to place  |  Esc to finish"
                                          : "Select a component to start placing");
        ImGui::SameLine();
        ImGui::TextDisabled("  %zu placed", m_project.components.size());
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void EditorLayer::renderCanvas()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float top = viewport->Pos.y + ImGui::GetFrameHeight() + 74.0f;
    const float left = viewport->Pos.x + 314.0f;
    const float width = std::max(110.0f, viewport->Size.x - 326.0f);
    const float height = std::max(110.0f, viewport->Size.y - (top - viewport->Pos.y) - 12.0f);
    ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.11f, 0.16f, 1.0f));
    if (ImGui::Begin("##DesignCanvas", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        ImGui::TextColored(ImVec4(0.63f, 0.77f, 0.89f, 1.0f), "DESIGN CANVAS");
        ImGui::SameLine();
        ImGui::TextDisabled("  Grid: 20 px");
        ImGui::Separator();

        const ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x > 4.0f && size.y > 4.0f)
        {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 end(origin.x + size.x, origin.y + size.y);
            const bool clicked = ImGui::InvisibleButton("##CanvasSurface", size);
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(origin, end, IM_COL32(21, 29, 42, 255));
            draw->PushClipRect(origin, end, true);
            for (float x = 0; x < size.x; x += 20.0f)
                draw->AddLine(ImVec2(origin.x + x, origin.y),
                              ImVec2(origin.x + x, end.y), IM_COL32(57, 70, 88, 120));
            for (float y = 0; y < size.y; y += 20.0f)
                draw->AddLine(ImVec2(origin.x, origin.y + y),
                              ImVec2(end.x, origin.y + y), IM_COL32(57, 70, 88, 120));

            for (const ElectricalComponent& component : m_project.components)
            {
                if (component.type() == ElectricalComponentType::LightSwitch)
                    drawLightSwitch(draw, ImVec2(origin.x + component.x(),
                                                 origin.y + component.y()), component.id(), false);
            }

            if (hovered && m_placementTool.has_value() && size.x >= 80.0f && size.y >= 80.0f)
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                const float x = std::clamp(std::round((mouse.x - origin.x) / 20.0f) * 20.0f,
                                           40.0f, size.x - 40.0f);
                const float y = std::clamp(std::round((mouse.y - origin.y) / 20.0f) * 20.0f,
                                           30.0f, size.y - 30.0f);
                drawLightSwitch(draw, ImVec2(origin.x + x, origin.y + y),
                                m_project.nextComponentId, true);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (clicked)
                {
                    m_project.addComponent(*m_placementTool, x, y);
                    m_dirty = true;
                }
            }
            draw->PopClipRect();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void EditorLayer::renderWorkspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float top = viewport->Pos.y + ImGui::GetFrameHeight() + 74.0f;
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 12.0f, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(290.0f, std::max(240.0f,
        viewport->Size.y - (top - viewport->Pos.y) - 12.0f)), ImGuiCond_Always);
    if (!ImGui::Begin("Project details", nullptr,
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) { ImGui::End(); return; }
    ImGui::TextDisabled("PROJECT");
    ImGui::SameLine();
    if (m_dirty) ImGui::TextColored(ImVec4(0.96f, 0.69f, 0.28f, 1), "Unsaved changes");
    else ImGui::TextDisabled("Saved");
    if (ImGui::InputText("Name", m_name.data(), m_name.size()))
    {
        m_project.name = m_name.data();
        m_dirty = true;
    }
    if (ImGui::InputTextMultiline("Notes", m_notes.data(), m_notes.size(), ImVec2(-1, 100)))
    {
        m_project.notes = m_notes.data();
        m_dirty = true;
    }
    ImGui::TextDisabled("Components: %zu", m_project.components.size());
    ImGui::TextDisabled("File: %s", m_projectPath.empty() ? "Not saved" : m_projectPath.string().c_str());
    if (ImGui::Button("Open project...")) requestAction(PendingAction::Open);
    ImGui::SameLine();
    if (ImGui::Button("Save project"))
    {
        if (m_projectPath.empty()) showDialog(DialogMode::Save);
        else saveProject(m_projectPath);
    }
    if (!m_error.empty()) ImGui::TextWrapped("%s", m_error.c_str());
    ImGui::End();
}

void EditorLayer::refreshEntries()
{
    m_entries.clear();
    std::error_code ec;
    fs::directory_iterator it(m_directory, fs::directory_options::skip_permission_denied, ec);
    if (ec) { m_error = "Cannot read folder: " + ec.message(); return; }
    for (const auto& entry : it)
    {
        std::error_code typeError;
        if (entry.is_directory(typeError) || (!typeError && entry.is_regular_file(typeError)
            && entry.path().extension() == FileHandler::extension))
            m_entries.push_back(entry);
    }
    std::sort(m_entries.begin(), m_entries.end(), [](const auto& a, const auto& b) {
        std::error_code ea, eb;
        const bool da = a.is_directory(ea), db = b.is_directory(eb);
        if (da != db) return da;
        return a.path().filename().string() < b.path().filename().string();
    });
}

void EditorLayer::navigate(const fs::path& path)
{
    std::error_code ec;
    if (!fs::is_directory(path, ec)) { m_error = "Folder does not exist or is inaccessible."; return; }
    m_directory = fs::weakly_canonical(path, ec);
    if (ec) m_directory = path;
    m_selectedFile.clear();
    m_search.fill('\0');
    m_error.clear();
    refreshEntries();
}

void EditorLayer::showDialog(DialogMode mode)
{
    m_dialog = mode;
    m_error.clear();
    m_selectedFile.clear();
    m_overwriteRequested = false;
    copyText(m_fileName.data(), m_fileName.size(),
             mode == DialogMode::Save ? (m_projectPath.empty() ? m_project.name + FileHandler::extension
                                                  : m_projectPath.filename().string()) : "");
    if (!m_projectPath.empty()) navigate(m_projectPath.parent_path());
    else refreshEntries();
    m_openFileDialog = true;
}

void EditorLayer::saveProject(const fs::path& path)
{
    if (FileHandler::save(path, m_project, m_error))
    {
        m_projectPath = path;
        m_dirty = false;
        const bool fromDialog = m_dialog != DialogMode::None;
        m_dialog = DialogMode::None;
        refreshEntries();
        if (fromDialog) ImGui::CloseCurrentPopup();
    }
}

void EditorLayer::openProject(const fs::path& path)
{
    ProjectDocument loaded;
    if (FileHandler::load(path, loaded, m_error))
    {
        m_project = std::move(loaded);
        m_placementTool.reset();
        m_projectPath = path;
        m_dirty = false;
        copyText(m_name.data(), m_name.size(), m_project.name);
        copyText(m_notes.data(), m_notes.size(), m_project.notes);
        m_dialog = DialogMode::None;
        ImGui::CloseCurrentPopup();
    }
}

void EditorLayer::renderFileDialog()
{
    if (m_dialog == DialogMode::None) return;
    if (m_openFileDialog)
    {
        ImGui::OpenPopup("Project explorer##ews");
        m_openFileDialog = false;
    }

    const bool saving = m_dialog == DialogMode::Save;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float dialogWidth = std::min(960.0f, std::max(280.0f, viewport->WorkSize.x - 32.0f));
    const float dialogHeight = std::min(620.0f, std::max(340.0f, viewport->WorkSize.y - 32.0f));
    ImGui::SetNextWindowSize(ImVec2(dialogWidth, dialogHeight), ImGuiCond_Always);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Scoped colors: the rest of the editor keeps its own ImGui appearance.
    const ImVec4 accent(0.32f, 0.68f, 1.0f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,         ImVec4(0.065f, 0.083f, 0.120f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,         ImVec4(0.092f, 0.115f, 0.158f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,            ImVec4(0.92f, 0.95f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,    ImVec4(0.59f, 0.67f, 0.77f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,          ImVec4(0.20f, 0.25f, 0.33f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Separator,       ImVec4(0.20f, 0.25f, 0.33f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,         ImVec4(0.13f, 0.17f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.17f, 0.22f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.19f, 0.26f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4(0.16f, 0.20f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   ImVec4(0.23f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,    ImVec4(0.25f, 0.37f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Header,          ImVec4(0.17f, 0.33f, 0.48f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4(0.18f, 0.30f, 0.42f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,    ImVec4(0.20f, 0.40f, 0.57f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,   ImVec4(0.12f, 0.16f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg,      ImVec4(0.092f, 0.115f, 0.158f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt,   ImVec4(0.11f, 0.135f, 0.18f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    if (!ImGui::BeginPopupModal("Project explorer##ews", nullptr,
                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::PopStyleVar(6);
        ImGui::PopStyleColor(18);
        return;
    }

    ImGui::TextColored(accent, "ELECTRICAL WIRING SIMULATOR");
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted(saving ? "Save project" : "Open project");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("  .ews");
    ImGui::Separator();

    if (ImGui::Button("Up")) navigate(m_directory.parent_path());
    ImGui::SameLine();
    if (ImGui::Button("Home"))
    {
        std::error_code ec;
        const auto home = fs::current_path(ec);
        if (!ec) navigate(home);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refreshEntries();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##explorerSearch", "Search projects and folders...", m_search.data(), m_search.size());

    ImGui::TextDisabled("LOCATION");
    ImGui::SameLine();
    ImGui::TextUnformatted(m_directory.string().c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", m_directory.string().c_str());

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const bool compact = contentWidth < 430.0f;
    const float footerHeight = compact ? 145.0f : 110.0f;
    ImGui::BeginChild("ExplorerContent", ImVec2(0, -footerHeight), false);

    if (ImGui::GetContentRegionAvail().x > 640.0f)
    {
        ImGui::BeginChild("QuickAccess", ImVec2(190, 0), true);
        ImGui::TextColored(accent, "QUICK ACCESS");
        ImGui::Separator();
        if (ImGui::Selectable("Working directory"))
        {
            std::error_code ec;
            const auto working = fs::current_path(ec);
            if (!ec) navigate(working);
        }
        if (ImGui::Selectable("Parent folder")) navigate(m_directory.parent_path());
        if (!m_projectPath.empty() && ImGui::Selectable("Current project"))
            navigate(m_projectPath.parent_path());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("TIP");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Double-click folders to browse. Only .ews projects are shown.");
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    ImGui::BeginChild("ProjectList", ImVec2(0, 0), true);
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
                                          | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp
                                          | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##entries", 2, tableFlags, ImVec2(0, 0)))
    {
        ImGui::TableSetupColumn("NAME", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("TYPE", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        fs::path folderToOpen;
        fs::path fileToOpen;
        int visibleCount = 0;
        for (const auto& entry : m_entries)
        {
            std::error_code ec;
            const bool folder = entry.is_directory(ec);
            const std::string filename = entry.path().filename().string();
            if (!containsInsensitive(filename, m_search.data())) continue;
            ++visibleCount;

            ImGui::TableNextRow(ImGuiTableRowFlags_None, 40.0f);
            ImGui::TableSetColumnIndex(0);
            const float nameColumnWidth = ImGui::GetContentRegionAvail().x;
            ImGui::PushID(filename.c_str());
            const bool selected = filename == m_selectedFile;
            if (ImGui::Selectable("##entry", selected,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
                    ImVec2(0, 34.0f)))
            {
                if (folder && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    folderToOpen = entry.path();
                else if (!folder)
                {
                    m_selectedFile = filename;
                    copyText(m_fileName.data(), m_fileName.size(), filename);
                    if (!saving && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        fileToOpen = entry.path();
                }
            }
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 rowMin = ImGui::GetItemRectMin();
            drawExplorerIcon(ImVec2(rowMin.x + 7.0f, rowMin.y + 5.0f), folder);

            // Clip long filenames in the first column rather than covering TYPE.
            ImGui::PushClipRect(rowMin, ImVec2(rowMin.x + nameColumnWidth - 9.0f,
                                              rowMin.y + 34.0f), true);
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(rowMin.x + 40.0f, rowMin.y + 9.0f),
                IM_COL32(234, 242, 252, 255), filename.c_str());
            ImGui::PopClipRect();
            if (hovered) ImGui::SetTooltip("%s", filename.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", folder ? "Folder" : "EWS project");
            ImGui::PopID();
        }
        if (visibleCount == 0)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("No matching projects or folders.");
        }
        ImGui::EndTable();

        // Navigation occurs after the table to avoid invalidating its row iteration.
        if (!folderToOpen.empty()) navigate(folderToOpen);
        if (!fileToOpen.empty()) openProject(fileToOpen);
    }
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("FILE NAME");
    const float buttonWidth = 94.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float available = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(compact ? -1.0f : std::max(90.0f, available - 2.0f * buttonWidth - 2.0f * spacing));
    ImGui::InputTextWithHint("##filename", "Project filename (.ews)", m_fileName.data(), m_fileName.size());
    if (compact)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, available - 2.0f * buttonWidth - spacing));
    else
        ImGui::SameLine();

    // The primary action has its own accent without changing the secondary button.
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.16f, 0.44f, 0.79f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.21f, 0.54f, 0.90f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.12f, 0.38f, 0.70f, 1.0f));
    if (ImGui::Button(saving ? "Save" : "Open", ImVec2(buttonWidth, 0)))
    {
        fs::path name(m_fileName.data());
        if (name.empty() || name.has_parent_path() || name.filename() == "." || name.filename() == "..")
            m_error = "Enter a valid filename without directory separators.";
        else
        {
            if (saving && name.extension().empty()) name += FileHandler::extension;
            if (name.extension() != FileHandler::extension)
                m_error = "Select an .ews project file.";
            else
            {
                const fs::path target = m_directory / name;
                std::error_code ec;
                if (saving && fs::exists(target, ec))
                {
                    m_overwriteRequested = true;
                    ImGui::OpenPopup("Overwrite project##ews");
                }
                else if (saving) saveProject(target);
                else openProject(target);
            }
        }
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
    {
        m_dialog = DialogMode::None;
        ImGui::CloseCurrentPopup();
    }
    if (!m_error.empty())
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.57f, 0.51f, 1.0f), "%s", m_error.c_str());
        ImGui::PopTextWrapPos();
    }

    bool savedFromOverwrite = false;
    if (ImGui::BeginPopupModal("Overwrite project##ews", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("This project already exists. Replace it?");
        if (ImGui::Button("Replace"))
        {
            fs::path name(m_fileName.data());
            if (name.extension().empty()) name += FileHandler::extension;
            saveProject(m_directory / name);
            savedFromOverwrite = m_dialog == DialogMode::None;
            m_overwriteRequested = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep existing"))
        {
            m_overwriteRequested = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (savedFromOverwrite) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    ImGui::PopStyleVar(6);
    ImGui::PopStyleColor(18);
}

void EditorLayer::renderConfirmations()
{
    if (m_openUnsavedDialog)
    {
        ImGui::OpenPopup("Unsaved changes##ews");
        m_openUnsavedDialog = false;
    }
    if (ImGui::BeginPopupModal("Unsaved changes##ews", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("You have unsaved changes. Discard them and continue?");
        if (ImGui::Button("Discard changes"))
        {
            const auto action = m_pending;
            ImGui::CloseCurrentPopup();
            performAction(action);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
