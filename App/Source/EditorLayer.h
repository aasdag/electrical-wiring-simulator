#pragma once

#include "Core/Layer.h"
#include "FileHandler.h"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

class EditorLayer final : public Core::Layer
{
public:
    EditorLayer();
    ~EditorLayer() override = default;

    void onUpdate(float timeStep) override;
    void onRender() override;
    void onImGuiRender() override;

private:
    enum class DialogMode { None, Open, Save };
    enum class PendingAction { None, New, Open, Exit };

    void renderMenuBar();
    void renderWorkspace();
    void renderFileDialog();
    void renderConfirmations();
    void showDialog(DialogMode mode);
    void navigate(const std::filesystem::path& path);
    void refreshEntries();
    void saveProject(const std::filesystem::path& path);
    void openProject(const std::filesystem::path& path);
    void requestAction(PendingAction action);
    void performAction(PendingAction action);

    ProjectDocument m_project;
    std::filesystem::path m_projectPath;
    std::filesystem::path m_directory;
    std::vector<std::filesystem::directory_entry> m_entries;
    std::string m_selectedFile;
    std::array<char, 256> m_fileName{};
    std::array<char, 128> m_search{};
    std::array<char, 128> m_name{};
    std::array<char, 2048> m_notes{};
    std::string m_error;
    bool m_dirty = false;
    bool m_overwriteRequested = false;
    DialogMode m_dialog = DialogMode::None;
    PendingAction m_pending = PendingAction::None;
};
