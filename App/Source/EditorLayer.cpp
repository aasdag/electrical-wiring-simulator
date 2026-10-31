#include "EditorLayer.h"

#include "Core/Application.h"
#include "TestLayer.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <cctype>
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
    DrawText("Electrical Wiring Simulator", 32, 55, 20, DARKGRAY);
}

void EditorLayer::onImGuiRender()
{
    renderMenuBar();
    renderWorkspace();
    renderFileDialog();
    renderConfirmations();
}

void EditorLayer::requestAction(PendingAction action)
{
    if (m_dirty)
    {
        m_pending = action;
        ImGui::OpenPopup("Unsaved changes##ews");
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

void EditorLayer::renderWorkspace()
{
    ImGui::SetNextWindowPos(ImVec2(24, 95), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(470, 280), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Project details")) { ImGui::End(); return; }
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
    ImGui::OpenPopup("Project explorer##ews");
}

void EditorLayer::saveProject(const fs::path& path)
{
    if (FileHandler::save(path, m_project, m_error))
    {
        m_projectPath = path;
        m_dirty = false;
        m_dialog = DialogMode::None;
        refreshEntries();
        ImGui::CloseCurrentPopup();
    }
}

void EditorLayer::openProject(const fs::path& path)
{
    ProjectDocument loaded;
    if (FileHandler::load(path, loaded, m_error))
    {
        m_project = std::move(loaded);
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
    ImGui::SetNextWindowSize(ImVec2(760, 530), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Project explorer##ews", nullptr, ImGuiWindowFlags_NoCollapse)) return;

    const bool saving = m_dialog == DialogMode::Save;
    ImGui::TextColored(ImVec4(0.49f, 0.74f, 1, 1), "%s", saving ? "SAVE PROJECT" : "OPEN PROJECT");
    ImGui::SameLine();
    ImGui::TextDisabled("  Electrical Wiring Simulator (.ews)");
    ImGui::Separator();
    if (ImGui::Button("Up")) navigate(m_directory.parent_path());
    ImGui::SameLine();
    if (ImGui::Button("Home"))
    {
        std::error_code ec;
        auto home = fs::current_path(ec);
        if (!ec) navigate(home);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refreshEntries();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_directory.string().c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search projects and folders...", m_search.data(), m_search.size());
    ImGui::Separator();

    ImGui::BeginChild("Folders", ImVec2(150, -105), true);
    ImGui::TextDisabled("LOCATIONS");
    if (ImGui::Selectable("Current folder")) refreshEntries();
    if (ImGui::Selectable("Parent folder")) navigate(m_directory.parent_path());
    if (!m_projectPath.empty() && ImGui::Selectable("Project folder")) navigate(m_projectPath.parent_path());
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Files", ImVec2(0, -105), true);
    ImGui::Columns(2, "filecols", true);
    ImGui::SetColumnWidth(0, 360);
    ImGui::TextDisabled("Name"); ImGui::NextColumn();
    ImGui::TextDisabled("Type"); ImGui::NextColumn();
    ImGui::Separator();
    fs::path folderToOpen;
    fs::path fileToOpen;
    for (const auto& entry : m_entries)
    {
        std::error_code ec;
        const bool folder = entry.is_directory(ec);
        const auto filename = entry.path().filename().string();
        if (!containsInsensitive(filename, m_search.data())) continue;
        ImGui::PushID(filename.c_str());
        const std::string display = std::string(folder ? "[DIR]  " : "[EWS]  ") + filename;
        const bool selected = filename == m_selectedFile;
        if (ImGui::Selectable(display.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns))
        {
            if (folder && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) folderToOpen = entry.path();
            else if (!folder)
            {
                m_selectedFile = filename;
                copyText(m_fileName.data(), m_fileName.size(), filename);
                if (!saving && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) fileToOpen = entry.path();
            }
        }
        ImGui::NextColumn();
        ImGui::TextDisabled("%s", folder ? "Folder" : "EWS Project");
        ImGui::NextColumn();
        ImGui::PopID();
    }
    ImGui::Columns(1);
    ImGui::EndChild();
    if (!folderToOpen.empty()) navigate(folderToOpen);
    if (!fileToOpen.empty()) openProject(fileToOpen);
    ImGui::Separator();
    ImGui::SetNextItemWidth(-130);
    ImGui::InputTextWithHint("##filename", "Project filename (.ews)", m_fileName.data(), m_fileName.size());
    ImGui::SameLine();
    if (ImGui::Button(saving ? "Save" : "Open", ImVec2(80, 0)))
    {
        fs::path name(m_fileName.data());
        if (name.empty() || name.has_parent_path() || name.filename() == "." || name.filename() == "..")
            m_error = "Enter a valid filename without directory separators.";
        else
        {
            if (saving && name.extension().empty()) name += FileHandler::extension;
            if (name.extension() != FileHandler::extension) m_error = "Select an .ews project file.";
            else
            {
                const auto target = m_directory / name;
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
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0)))
    {
        m_dialog = DialogMode::None;
        ImGui::CloseCurrentPopup();
    }
    if (!m_error.empty()) ImGui::TextColored(ImVec4(1, .45f, .4f, 1), "%s", m_error.c_str());

    bool savedFromOverwrite = false;
    if (ImGui::BeginPopupModal("Overwrite project##ews", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("This file already exists. Replace it?");
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
}

void EditorLayer::renderConfirmations()
{
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
