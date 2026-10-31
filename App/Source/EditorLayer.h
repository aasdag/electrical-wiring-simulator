#pragma once

#include "Core/Layer.h"
#include "FileHandler.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <unordered_set>

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
    enum class SchematicView { SingleLine, DetailedSymbols };

    struct WireCornerDrag
    {
        std::uint64_t id;
        std::size_t cornerIndex;
        std::vector<WirePoint> originalBends;
        std::vector<WirePoint> originalPath;
        bool wasDirty;
    };

    struct ComponentResize
    {
        std::uint64_t id;
        int startLengthCells;
        bool wasDirty;
    };

    struct ComponentDrag
    {
        std::uint64_t id;
        float startX;
        float startY;
        float offsetX;
        float offsetY;
        bool wasDirty;
    };

    void cancelComponentDrag();
    void cancelComponentResize();
    void cancelWireCornerDrag();
    void renderMenuBar();
    void renderToolbox();
    void renderCanvas();
    void renderWorkspace();
    void renderPropertiesPanel();
    void renderFileDialog();
    void renderConfirmations();
    void showDialog(DialogMode mode);
    void navigate(const std::filesystem::path& path);
    void refreshEntries();
    void saveProject(const std::filesystem::path& path);
    void openProject(const std::filesystem::path& path);
    void requestAction(PendingAction action);
    void performAction(PendingAction action);
    void selectComponent(const ElectricalComponent& component);
    void setSimulationMode(bool enabled);
    void cancelWirePlacement();

    ProjectDocument m_project;
    std::optional<ElectricalComponentType> m_placementTool;
    bool m_wireTool = false;
    std::optional<WireEndpoint> m_wireStart;
    std::vector<WirePoint> m_wireBends;
    std::optional<std::uint64_t> m_selectedWireId;
    std::optional<std::size_t> m_selectedWireCornerIndex;
    std::optional<WireCornerDrag> m_wireCornerDrag;
    std::optional<std::uint64_t> m_selectedComponentId;
    std::optional<ComponentDrag> m_componentDrag;
    std::optional<ComponentResize> m_componentResize;
    SchematicView m_schematicView = SchematicView::SingleLine;
    bool m_simulationMode = false;
    std::unordered_set<std::uint64_t> m_closedSwitches;
    std::array<char, 64> m_componentLabel{};
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
    bool m_openFileDialog = false;
    bool m_openUnsavedDialog = false;
    DialogMode m_dialog = DialogMode::None;
    PendingAction m_pending = PendingAction::None;
};
