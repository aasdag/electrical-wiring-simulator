#include "EditorLayer.h"

#include "Core/Application.h"
#include "TestLayer.h"

#include <imgui.h>
#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
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

    ImVec2 resolveWireEndpoint(const ProjectDocument& project, const WireEndpoint& endpoint)
    {
        if (endpoint.componentId != 0)
        {
            const auto it = std::find_if(project.components.begin(), project.components.end(),
                [&](const ElectricalComponent& c) { return c.id() == endpoint.componentId; });
            if (it != project.components.end())
            {
                const float radians = it->rotation() * (3.14159265f / 180.0f);
                const float distance = endpoint.terminal * it->halfLengthPx();
                return ImVec2(it->x() + distance * std::cos(radians),
                              it->y() + distance * std::sin(radians));
            }
        }
        return ImVec2(endpoint.x, endpoint.y);
    }

    WirePoint toPoint(ImVec2 point) { return {point.x, point.y}; }
    ImVec2 toScreen(ImVec2 origin, WirePoint point)
    {
        return ImVec2(origin.x + point.x, origin.y + point.y);
    }

    bool collinear(WirePoint a, WirePoint b, WirePoint c)
    {
        return (a.x == b.x && b.x == c.x) || (a.y == b.y && b.y == c.y);
    }

    void simplifyWirePath(std::vector<WirePoint>& path)
    {
        std::vector<WirePoint> simplified;
        simplified.reserve(path.size());
        for (const WirePoint point : path)
        {
            if (!simplified.empty() && simplified.back() == point) continue;
            while (simplified.size() >= 2 &&
                   collinear(simplified[simplified.size() - 2], simplified.back(), point))
                simplified.pop_back();
            simplified.push_back(point);
        }
        path = std::move(simplified);
    }

    void appendWirePoint(std::vector<WirePoint>& path, WirePoint target, bool simplify = true)
    {
        if (path.empty()) { path.push_back(target); return; }
        const WirePoint previous = path.back();
        if (previous == target) return;
        if (previous.x != target.x && previous.y != target.y)
            path.push_back({target.x, previous.y}); // horizontal first, then vertical
        path.push_back(target);
        if (simplify) simplifyWirePath(path);
    }

    std::vector<WirePoint> wirePath(const ProjectDocument& project, const Wire& wire)
    {
        // Keep explicitly added points, including points on a straight segment.
        // Otherwise an inserted point would disappear before the user could move it.
        std::vector<WirePoint> result{toPoint(resolveWireEndpoint(project, wire.start()))};
        for (const WirePoint& bend : wire.bends()) appendWirePoint(result, bend, false);
        appendWirePoint(result, toPoint(resolveWireEndpoint(project, wire.end())), false);
        return result;
    }

    std::vector<WirePoint> routeDraft(const ProjectDocument& project,
                                     const WireEndpoint& start,
                                     const std::vector<WirePoint>& bends,
                                     WirePoint target)
    {
        std::vector<WirePoint> result{toPoint(resolveWireEndpoint(project, start))};
        for (const WirePoint& bend : bends) appendWirePoint(result, bend);
        appendWirePoint(result, target);
        return result;
    }

    std::vector<WirePoint> interiorPoints(const std::vector<WirePoint>& path)
    {
        if (path.size() <= 2) return {};
        return std::vector<WirePoint>(path.begin() + 1, path.end() - 1);
    }

    void drawWirePath(ImDrawList* draw, ImVec2 origin,
                      const std::vector<WirePoint>& path, ImU32 color, float thickness)
    {
        for (std::size_t i = 1; i < path.size(); ++i)
            draw->AddLine(toScreen(origin, path[i - 1]),
                          toScreen(origin, path[i]), color, thickness);
    }

    float pointSegmentDistanceSquared(ImVec2 p, ImVec2 a, ImVec2 b)
    {
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float lenSq = dx * dx + dy * dy;
        const float t = lenSq > 0.0f
            ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq, 0.0f, 1.0f)
            : 0.0f;
        const float ex = p.x - (a.x + t * dx), ey = p.y - (a.y + t * dy);
        return ex * ex + ey * ey;
    }

    // A single-pole light-switch symbol in canvas coordinates, rotated around its center.
    void drawLightSwitch(ImDrawList* draw, ImVec2 center,
                         const ElectricalComponent& component, bool preview, bool selected = false,
                         bool closed = false, bool resizeHandle = false)
    {
        const ImU32 stroke = preview ? IM_COL32(90, 186, 255, 160)
                                   : closed ? IM_COL32(95, 225, 161, 255)
                                            : IM_COL32(148, 217, 255, 255);
        const ImU32 fill = preview ? IM_COL32(90, 186, 255, 110)
                                 : closed ? IM_COL32(35, 160, 106, 255)
                                          : IM_COL32(45, 117, 177, 255);
        const float radians = component.rotation() * (3.14159265f / 180.0f);
        const float cs = std::cos(radians), sn = std::sin(radians);
        const auto point = [&](float x, float y) {
            return ImVec2(center.x + x * cs - y * sn,
                          center.y + x * sn + y * cs);
        };

        // The switch mechanism has a fixed 2-cell contact spacing.
        // Only the wire stubs to the outer terminals change with terminal span.
        const float contactOffset = static_cast<float>(ElectricalComponent::gridSize);
        const float halfLength = component.halfLengthPx();
        draw->AddLine(point(-halfLength, 0), point(-contactOffset, 0), stroke, 2.2f);
        draw->AddLine(point(contactOffset, 0), point(halfLength, 0), stroke, 2.2f);
        draw->AddCircleFilled(point(-halfLength, 0), 3.0f, stroke);
        draw->AddCircleFilled(point(halfLength, 0), 3.0f, stroke);
        draw->AddCircleFilled(point(-contactOffset, 0), 4.3f, fill);
        draw->AddCircle(point(-contactOffset, 0), 4.3f, stroke, 0, 1.8f);
        draw->AddCircleFilled(point(contactOffset, 0), 4.3f, fill);
        draw->AddCircle(point(contactOffset, 0), 4.3f, stroke, 0, 1.8f);
        draw->AddLine(point(-contactOffset, 0),
                      closed ? point(contactOffset, 0) : point(contactOffset - 8.0f, -16.0f),
                      stroke, 2.4f);

        if (selected)
        {
            const float outline = halfLength + 8.0f;
            draw->AddQuad(point(-outline, -24), point(outline, -24),
                          point(outline, 24), point(-outline, 24),
                          IM_COL32(88, 191, 255, 255), 2.0f);
            if (resizeHandle)
            {
                const ImVec2 handle = point(halfLength + 9.0f, 0);
                draw->AddRectFilled(ImVec2(handle.x - 6, handle.y - 6),
                                    ImVec2(handle.x + 6, handle.y + 6),
                                    IM_COL32(88, 191, 255, 255), 2.0f);
            }
        }

        const std::string label = component.label().empty()
            ? "S" + std::to_string(component.id()) : component.label();
        const float labelWidth = ImGui::CalcTextSize(label.c_str()).x;
        draw->AddText(ImVec2(center.x - labelWidth * 0.5f, center.y + 16.0f),
                      stroke, label.c_str());
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
    // Escape and right-click cancel the current tool/mode, not the application.
    if (m_dialog == DialogMode::None &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
         ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
    {
        if (m_wireCornerDrag) cancelWireCornerDrag();
        else if (m_componentResize) cancelComponentResize();
        else if (m_componentDrag) cancelComponentDrag();
        else if (m_simulationMode) setSimulationMode(false);
        else if (m_wireTool) cancelWirePlacement();
        else if (m_placementTool) m_placementTool.reset();
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape) && m_selectedWireCornerIndex)
            m_selectedWireCornerIndex.reset();
        else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_selectedComponentId.reset();
    }

    renderMenuBar();
    renderToolbox();
    renderCanvas();
    renderWorkspace();
    renderPropertiesPanel();
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
    cancelComponentDrag();
    cancelComponentResize();
    cancelWireCornerDrag();
    cancelWirePlacement();
    m_pending = PendingAction::None;
    switch (action)
    {
        case PendingAction::New:
            m_project = {};
            m_projectPath.clear();
            m_dirty = false;
            copyText(m_name.data(), m_name.size(), m_project.name);
            m_notes.fill('\0');
            setSimulationMode(false);
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
        ImGui::TextDisabled(m_simulationMode ? "SIMULATION" : "TOOLBOX");
        ImGui::SameLine(100.0f);
        const bool placing = m_placementTool.has_value();
        ImGui::BeginDisabled(m_simulationMode);
        ImGui::PushStyleColor(ImGuiCol_Button, placing ? ImVec4(0.14f, 0.37f, 0.58f, 1.0f)
                                                       : ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
        if (ImGui::Button("Light switch", ImVec2(146, 34)))
        {
            if (placing) m_placementTool.reset();
            else
            {
                cancelWirePlacement();
                m_placementTool = ElectricalComponentType::LightSwitch;
                m_selectedWireId.reset();
                m_selectedWireCornerIndex.reset();
            }
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(m_wireTool ? "Wire [active]" : "Wire", ImVec2(112, 34)))
        {
            if (m_wireTool) cancelWirePlacement();
            else
            {
                m_placementTool.reset();
                m_wireTool = true;
                m_selectedComponentId.reset();
                m_selectedWireId.reset();
                m_selectedWireCornerIndex.reset();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, m_simulationMode
            ? ImVec4(0.58f, 0.23f, 0.23f, 1.0f) : ImVec4(0.11f, 0.39f, 0.29f, 1.0f));
        if (ImGui::Button(m_simulationMode ? "Stop simulation" : "Start simulation", ImVec2(166, 34)))
            setSimulationMode(!m_simulationMode);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_simulationMode
            ? "Click a switch to toggle  |  Esc / right-click to stop"
            : m_wireTool ? (m_wireStart ? "Click corners  |  Terminal or double-click: finish"
                                      : "Click start point  |  Esc to cancel")
            : placing ? "Click to place  |  Esc / right-click to cancel"
                      : "Select a tool to begin");
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
    const float rightPanelWidth = (m_selectedComponentId || m_selectedWireId) ? 314.0f : 0.0f;
    const float width = std::max(110.0f, viewport->Size.x - 326.0f - rightPanelWidth);
    const float height = std::max(110.0f, viewport->Size.y - (top - viewport->Pos.y) - 12.0f);
    ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.11f, 0.16f, 1.0f));
    if (ImGui::Begin("##DesignCanvas", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        ImGui::TextColored(m_simulationMode ? ImVec4(0.37f, 0.88f, 0.62f, 1.0f)
                                           : ImVec4(0.63f, 0.77f, 0.89f, 1.0f),
                           m_simulationMode ? "SIMULATION MODE" : "DESIGN CANVAS");
        ImGui::SameLine();
        ImGui::TextDisabled(m_simulationMode ? "  Click a switch to toggle" :
                            m_wireTool ? "  Click start and end points  |  Grid: 20 px" :
                                         "  Grid: 20 px  |  Drag a component to move");
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

            // Every routed polyline remains one Wire, including all its corners.
            for (const Wire& wire : m_project.wires)
            {
                const auto path = wirePath(m_project, wire);
                const bool selected = m_selectedWireId == wire.id();
                const ImU32 color = selected ? IM_COL32(104, 216, 255, 255)
                                             : IM_COL32(223, 177, 103, 255);
                drawWirePath(draw, origin, path, color, selected ? 3.5f : 2.4f);
                if (!path.empty())
                {
                    draw->AddCircleFilled(toScreen(origin, path.front()), 3.0f, color);
                    draw->AddCircleFilled(toScreen(origin, path.back()), 3.0f, color);
                }
                if (selected && !m_simulationMode && !m_wireTool)
                {
                    for (std::size_t i = 1; i + 1 < path.size(); ++i)
                    {
                        const ImVec2 p = toScreen(origin, path[i]);
                        const bool active = m_selectedWireCornerIndex == i;
                        const float halfSize = active ? 7.0f : 5.0f;
                        draw->AddRectFilled(ImVec2(p.x - halfSize, p.y - halfSize),
                                            ImVec2(p.x + halfSize, p.y + halfSize),
                                            active ? IM_COL32(255, 205, 100, 255)
                                                   : IM_COL32(104, 216, 255, 255), 2.0f);
                    }
                }
            }

            // Prefer nearby switch terminals; other endpoints snap to the grid.
            const auto snapEndpoint = [&](ImVec2 mouse) {
                const float grid = static_cast<float>(ElectricalComponent::gridSize);
                WireEndpoint result;
                result.x = std::clamp(std::round((mouse.x - origin.x) / grid) * grid,
                                      0.0f, std::floor(size.x / grid) * grid);
                result.y = std::clamp(std::round((mouse.y - origin.y) / grid) * grid,
                                      0.0f, std::floor(size.y / grid) * grid);
                float closestSq = 12.0f * 12.0f;
                for (const ElectricalComponent& component : m_project.components)
                {
                    for (const int side : {-1, 1})
                    {
                        const WireEndpoint candidate{component.x(), component.y(), component.id(), side};
                        const ImVec2 terminal = resolveWireEndpoint(m_project, candidate);
                        const float dx = mouse.x - origin.x - terminal.x;
                        const float dy = mouse.y - origin.y - terminal.y;
                        const float distSq = dx * dx + dy * dy;
                        if (distSq < closestSq)
                        {
                            closestSq = distSq;
                            result.x = terminal.x;
                            result.y = terminal.y;
                            result.componentId = component.id();
                            result.terminal = side;
                        }
                    }
                }
                return result;
            };

            if (m_wireTool && !m_simulationMode && m_dialog == DialogMode::None && hovered)
            {
                const WireEndpoint endpoint = snapEndpoint(ImGui::GetMousePos());
                const WirePoint target{endpoint.x, endpoint.y};
                const ImVec2 pos = toScreen(origin, target);
                draw->AddCircleFilled(pos, 5.0f, IM_COL32(105, 230, 172, 255));
                if (m_wireStart)
                {
                    const auto path = routeDraft(m_project, *m_wireStart, m_wireBends, target);
                    drawWirePath(draw, origin, path, IM_COL32(105, 230, 172, 190), 2.0f);
                }
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                // Process on mouse-down to distinguish a double-click from a waypoint.
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (!m_wireStart)
                        m_wireStart = endpoint;
                    else
                    {
                        const WirePoint start = toPoint(resolveWireEndpoint(m_project, *m_wireStart));
                        const bool finish = endpoint.componentId != 0 ||
                                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                        if (finish)
                        {
                            auto path = routeDraft(m_project, *m_wireStart, m_wireBends, target);
                            if (path.size() >= 2 && path.front() != path.back())
                            {
                                auto bends = interiorPoints(path);
                                if (bends.size() <= Wire::maxBends)
                                {
                                    m_project.addWire(*m_wireStart, endpoint);
                                    m_project.wires.back().setBends(std::move(bends));
                                    m_selectedWireId = m_project.wires.back().id();
                                    m_selectedWireCornerIndex.reset();
                                    m_selectedComponentId.reset();
                                    m_dirty = true;
                                }
                            }
                            m_wireStart.reset();
                            m_wireBends.clear();
                        }
                        else if (target != start)
                        {
                            auto path = routeDraft(m_project, *m_wireStart, m_wireBends, target);
                            if (path.size() >= 2)
                            {
                                auto bends = std::vector<WirePoint>(path.begin() + 1, path.end());
                                if (bends.size() <= Wire::maxBends)
                                    m_wireBends = std::move(bends);
                            }
                        }
                    }
                }
            }

            // Clicking an existing corner selects that point, not just its wire.
            // It also arms dragging, with movement only after the drag threshold.
            bool cornerGrabbed = false;
            if (!m_simulationMode && !m_wireTool && !m_placementTool &&
                m_dialog == DialogMode::None && hovered &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                for (auto wire = m_project.wires.rbegin(); wire != m_project.wires.rend(); ++wire)
                {
                    const auto path = wirePath(m_project, *wire);
                    for (std::size_t i = 1; i + 1 < path.size(); ++i)
                    {
                        const ImVec2 p = toScreen(origin, path[i]);
                        if (std::abs(mouse.x - p.x) <= 9.0f &&
                            std::abs(mouse.y - p.y) <= 9.0f)
                        {
                            m_selectedWireId = wire->id();
                            m_selectedWireCornerIndex = i;
                            m_selectedComponentId.reset();
                            m_wireCornerDrag = WireCornerDrag{
                                wire->id(), i, wire->bends(), path, m_dirty
                            };
                            cornerGrabbed = true;
                            break;
                        }
                    }
                    if (cornerGrabbed) break;
                }
            }

            // Double-click a segment to add a point without creating a new wire.
            bool addedCorner = false;
            if (!cornerGrabbed && !m_simulationMode && !m_wireTool && !m_placementTool &&
                m_dialog == DialogMode::None && hovered && m_selectedWireId &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                auto wire = std::find_if(m_project.wires.begin(), m_project.wires.end(),
                    [&](const Wire& w) { return w.id() == *m_selectedWireId; });
                if (wire != m_project.wires.end())
                {
                    auto path = wirePath(m_project, *wire);
                    if (path.size() >= 2 && path.size() - 2 < Wire::maxBends)
                    {
                        const ImVec2 mouse = ImGui::GetMousePos();
                        const ImVec2 local(mouse.x - origin.x, mouse.y - origin.y);
                        const float grid = static_cast<float>(ElectricalComponent::gridSize);
                        for (std::size_t i = 1; i < path.size(); ++i)
                        {
                            const WirePoint a = path[i - 1], b = path[i];
                            if (pointSegmentDistanceSquared(local, ImVec2(a.x, a.y),
                                    ImVec2(b.x, b.y)) > 9.0f * 9.0f)
                                continue;
                            WirePoint point;
                            if (a.y == b.y)
                            {
                                point.x = std::clamp(std::round(local.x / grid) * grid,
                                                     std::min(a.x, b.x), std::max(a.x, b.x));
                                point.y = a.y;
                            }
                            else if (a.x == b.x)
                            {
                                point.x = a.x;
                                point.y = std::clamp(std::round(local.y / grid) * grid,
                                                     std::min(a.y, b.y), std::max(a.y, b.y));
                            }
                            else continue;

                            if (point == a || point == b) continue;
                            path.insert(path.begin() + static_cast<std::ptrdiff_t>(i), point);
                            wire->setBends(interiorPoints(path));
                            m_selectedWireCornerIndex = i;
                            m_dirty = true;
                            addedCorner = true;
                            break;
                        }
                    }
                }
            }

            if (m_wireCornerDrag)
            {
                const auto wire = std::find_if(m_project.wires.begin(), m_project.wires.end(),
                    [&](const Wire& w) { return w.id() == m_wireCornerDrag->id; });
                if (wire == m_project.wires.end())
                    m_wireCornerDrag.reset();
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    if (wire->bends() == m_wireCornerDrag->originalBends)
                        m_dirty = m_wireCornerDrag->wasDirty;
                    m_wireCornerDrag.reset();
                }
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f))
                {
                    const float grid = static_cast<float>(ElectricalComponent::gridSize);
                    const ImVec2 mouse = ImGui::GetMousePos();
                    const WirePoint target{
                        std::clamp(std::round((mouse.x - origin.x) / grid) * grid,
                                   0.0f, std::floor(size.x / grid) * grid),
                        std::clamp(std::round((mouse.y - origin.y) / grid) * grid,
                                   0.0f, std::floor(size.y / grid) * grid)
                    };
                    auto path = m_wireCornerDrag->originalPath;
                    path[m_wireCornerDrag->cornerIndex] = target;
                    std::vector<WirePoint> orthogonal{path.front()};
                    for (std::size_t i = 1; i < path.size(); ++i)
                        appendWirePoint(orthogonal, path[i], false);
                    auto bends = interiorPoints(orthogonal);
                    if (bends.size() <= Wire::maxBends && bends != wire->bends())
                    {
                        wire->setBends(std::move(bends));
                        m_selectedWireCornerIndex.reset();
                        const auto current = wirePath(m_project, *wire);
                        for (std::size_t i = 1; i + 1 < current.size(); ++i)
                        {
                            if (current[i] == target)
                            {
                                m_selectedWireCornerIndex = i;
                                break;
                            }
                        }
                        m_dirty = true;
                    }
                }
            }

            // The square at the positive terminal resizes symmetrically in grid steps.
            bool resizeStarted = false;
            if (!addedCorner && !cornerGrabbed && !m_wireCornerDrag &&
                !m_simulationMode && !m_wireTool && !m_placementTool && m_dialog == DialogMode::None &&
                hovered && m_selectedComponentId &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const auto selected = std::find_if(m_project.components.begin(),
                    m_project.components.end(), [&](const ElectricalComponent& component) {
                        return component.id() == *m_selectedComponentId;
                    });
                if (selected != m_project.components.end())
                {
                    const float angle = selected->rotation() * (3.14159265f / 180.0f);
                    const float halfLength = selected->halfLengthPx() + 9.0f;
                    const ImVec2 handle(origin.x + selected->x() + halfLength * std::cos(angle),
                                        origin.y + selected->y() + halfLength * std::sin(angle));
                    const ImVec2 mouse = ImGui::GetMousePos();
                    if (std::abs(mouse.x - handle.x) <= 9.0f &&
                        std::abs(mouse.y - handle.y) <= 9.0f)
                    {
                        m_componentResize = ComponentResize{
                            selected->id(), selected->lengthCells(), m_dirty
                        };
                        resizeStarted = true;
                    }
                }
            }

            if (m_componentResize)
            {
                auto target = std::find_if(m_project.components.begin(), m_project.components.end(),
                    [&](const ElectricalComponent& component) {
                        return component.id() == m_componentResize->id;
                    });
                if (target == m_project.components.end())
                    m_componentResize.reset();
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    if (target->lengthCells() == m_componentResize->startLengthCells)
                        m_dirty = m_componentResize->wasDirty;
                    m_componentResize.reset();
                }
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f))
                {
                    const ImVec2 mouse = ImGui::GetMousePos();
                    const float dx = mouse.x - origin.x - target->x();
                    const float dy = mouse.y - origin.y - target->y();
                    const float angle = target->rotation() * (3.14159265f / 180.0f);
                    const float localX = dx * std::cos(angle) + dy * std::sin(angle);
                    const int lengthCells = std::clamp(
                        static_cast<int>(std::round(localX / ElectricalComponent::gridSize)) * 2,
                        ElectricalComponent::minLengthCells, ElectricalComponent::maxLengthCells);
                    if (lengthCells != target->lengthCells())
                    {
                        target->setLengthCells(lengthCells);
                        m_dirty = true;
                    }
                }
            }

            // Select on mouse-down in edit mode; simulation continues to toggle on click.
            if (!addedCorner && !cornerGrabbed && !m_wireCornerDrag &&
                !resizeStarted && !m_componentResize && !m_wireTool && !m_placementTool &&
                m_dialog == DialogMode::None &&
                ((m_simulationMode && clicked) ||
                 (!m_simulationMode && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))))
            {
                m_selectedComponentId.reset();
                m_selectedWireId.reset();
                m_selectedWireCornerIndex.reset();
                const ImVec2 mouse = ImGui::GetMousePos();
                for (auto it = m_project.components.rbegin(); it != m_project.components.rend(); ++it)
                {
                    const float dx = mouse.x - origin.x - it->x();
                    const float dy = mouse.y - origin.y - it->y();
                    const float radians = it->rotation() * (3.14159265f / 180.0f);
                    const float cs = std::cos(radians), sn = std::sin(radians);
                    const float localX = dx * cs + dy * sn;
                    const float localY = -dx * sn + dy * cs;
                    if (std::abs(localX) <= it->halfLengthPx() + 8.0f &&
                        std::abs(localY) <= 24.0f)
                    {
                        selectComponent(*it);
                        if (m_simulationMode)
                        {
                            if (!m_closedSwitches.erase(it->id()))
                                m_closedSwitches.insert(it->id());
                        }
                        else
                        {
                            // Preserve the cursor's grab point to avoid jumping.
                            m_componentDrag = ComponentDrag{
                                it->id(), it->x(), it->y(), dx, dy, m_dirty
                            };
                        }
                        break;
                    }
                }
            }

            if (m_componentDrag)
            {
                auto it = std::find_if(m_project.components.begin(), m_project.components.end(),
                    [&](const ElectricalComponent& component) {
                        return component.id() == m_componentDrag->id;
                    });
                if (it == m_project.components.end())
                {
                    m_componentDrag.reset();
                }
                else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    if (it->x() == m_componentDrag->startX &&
                        it->y() == m_componentDrag->startY)
                        m_dirty = m_componentDrag->wasDirty;
                    m_componentDrag.reset();
                }
                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                         ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f) &&
                         size.x >= 80.0f && size.y >= 80.0f)
                {
                    const ImVec2 mouse = ImGui::GetMousePos();
                    const auto snap = [](float value) {
                        return std::round(value / 20.0f) * 20.0f;
                    };
                    const float maxX = std::floor((size.x - 40.0f) / 20.0f) * 20.0f;
                    const float maxY = std::floor((size.y - 30.0f) / 20.0f) * 20.0f;
                    const float x = std::clamp(
                        snap(mouse.x - origin.x - m_componentDrag->offsetX), 40.0f, maxX);
                    const float y = std::clamp(
                        snap(mouse.y - origin.y - m_componentDrag->offsetY), 40.0f, maxY);
                    if (x != it->x() || y != it->y())
                    {
                        it->setPosition(x, y);
                        m_dirty = true;
                    }
                }
            }

            // When no component was hit, allow selecting a line near the cursor.
            if (!addedCorner && !cornerGrabbed && !m_wireCornerDrag &&
                !m_simulationMode && !m_wireTool && !m_placementTool &&
                m_dialog == DialogMode::None && hovered &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_selectedComponentId)
            {
                m_selectedWireCornerIndex.reset();
                const ImVec2 mouse = ImGui::GetMousePos();
                const ImVec2 local(mouse.x - origin.x, mouse.y - origin.y);
                for (auto it = m_project.wires.rbegin(); it != m_project.wires.rend(); ++it)
                {
                    const auto path = wirePath(m_project, *it);
                    bool hit = false;
                    for (std::size_t i = 1; i < path.size(); ++i)
                        if (pointSegmentDistanceSquared(local,
                                ImVec2(path[i - 1].x, path[i - 1].y),
                                ImVec2(path[i].x, path[i].y)) <= 9.0f * 9.0f)
                        {
                            hit = true;
                            break;
                        }
                    if (hit)
                    {
                        m_selectedWireId = it->id();
                        break;
                    }
                }
            }

            for (const ElectricalComponent& component : m_project.components)
            {
                if (component.type() == ElectricalComponentType::LightSwitch)
                    drawLightSwitch(draw, ImVec2(origin.x + component.x(),
                                                 origin.y + component.y()), component, false,
                                    m_selectedComponentId == component.id(),
                                    m_simulationMode && m_closedSwitches.contains(component.id()),
                                    !m_simulationMode && !m_placementTool);
            }

            if (hovered && !m_simulationMode && m_placementTool.has_value() &&
                size.x >= 80.0f && size.y >= 80.0f)
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                const float x = std::clamp(std::round((mouse.x - origin.x) / 20.0f) * 20.0f,
                                           40.0f, size.x - 40.0f);
                const float y = std::clamp(std::round((mouse.y - origin.y) / 20.0f) * 20.0f,
                                           30.0f, size.y - 30.0f);
                const ElectricalComponent preview(m_project.nextComponentId, *m_placementTool, x, y);
                drawLightSwitch(draw, ImVec2(origin.x + x, origin.y + y), preview, true);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (clicked)
                {
                    m_project.addComponent(*m_placementTool, x, y);
                    selectComponent(m_project.components.back());
                    m_dirty = true;
                }
            }
            draw->PopClipRect();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void EditorLayer::cancelComponentResize()
{
    if (!m_componentResize) return;

    const auto it = std::find_if(m_project.components.begin(), m_project.components.end(),
        [&](const ElectricalComponent& component) {
            return component.id() == m_componentResize->id;
        });
    if (it != m_project.components.end())
        it->setLengthCells(m_componentResize->startLengthCells);

    m_dirty = m_componentResize->wasDirty;
    m_componentResize.reset();
}

void EditorLayer::cancelComponentDrag()
{
    if (!m_componentDrag) return;

    const auto it = std::find_if(m_project.components.begin(), m_project.components.end(),
        [&](const ElectricalComponent& component) {
            return component.id() == m_componentDrag->id;
        });
    if (it != m_project.components.end())
        it->setPosition(m_componentDrag->startX, m_componentDrag->startY);

    m_dirty = m_componentDrag->wasDirty;
    m_componentDrag.reset();
}

void EditorLayer::cancelWireCornerDrag()
{
    if (!m_wireCornerDrag) return;
    const auto wire = std::find_if(m_project.wires.begin(), m_project.wires.end(),
        [&](const Wire& w) { return w.id() == m_wireCornerDrag->id; });
    if (wire != m_project.wires.end())
        wire->setBends(m_wireCornerDrag->originalBends);
    m_dirty = m_wireCornerDrag->wasDirty;
    m_selectedWireCornerIndex = m_wireCornerDrag->cornerIndex;
    m_wireCornerDrag.reset();
}

void EditorLayer::cancelWirePlacement()
{
    m_wireStart.reset();
    m_wireBends.clear();
    m_wireTool = false;
}

void EditorLayer::setSimulationMode(bool enabled)
{
    cancelWireCornerDrag();
    cancelWirePlacement();
    m_selectedWireId.reset();
    m_selectedWireCornerIndex.reset();
    cancelComponentDrag();
    cancelComponentResize();
    m_simulationMode = enabled;
    m_closedSwitches.clear(); // Simulation states are transient and never saved.
    m_placementTool.reset();
    m_selectedComponentId.reset();
}

void EditorLayer::selectComponent(const ElectricalComponent& component)
{
    m_selectedComponentId = component.id();
    m_selectedWireCornerIndex.reset();
    copyText(m_componentLabel.data(), m_componentLabel.size(), component.label());
}

void EditorLayer::renderPropertiesPanel()
{
    if (!m_selectedComponentId && !m_selectedWireId) return;
    if (m_selectedWireId)
    {
        const auto it = std::find_if(m_project.wires.begin(), m_project.wires.end(),
            [&](const Wire& wire) { return wire.id() == *m_selectedWireId; });
        if (it == m_project.wires.end())
        {
            m_selectedWireId.reset();
            m_selectedWireCornerIndex.reset();
            return;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float top = viewport->Pos.y + ImGui::GetFrameHeight() + 74.0f;
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - 302.0f, top), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(290.0f, std::max(240.0f,
            viewport->Size.y - (top - viewport->Pos.y) - 12.0f)), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.085f, 0.12f, 0.17f, 1.0f));
        if (ImGui::Begin("Properties##wire", nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
        {
            auto path = wirePath(m_project, *it);
            if (m_selectedWireCornerIndex &&
                (*m_selectedWireCornerIndex == 0 ||
                 *m_selectedWireCornerIndex + 1 >= path.size()))
                m_selectedWireCornerIndex.reset();

            if (m_selectedWireCornerIndex)
            {
                const std::size_t pointIndex = *m_selectedWireCornerIndex;
                const WirePoint point = path[pointIndex];
                ImGui::TextColored(ImVec4(0.96f, 0.76f, 0.43f, 1.0f), "BEND POINT PROPERTIES");
                ImGui::Separator();
                ImGui::TextDisabled("Wire ID: %llu", static_cast<unsigned long long>(it->id()));
                ImGui::Text("X: %.0f px  (%.0f cells)", point.x,
                            point.x / ElectricalComponent::gridSize);
                ImGui::Text("Y: %.0f px  (%.0f cells)", point.y,
                            point.y / ElectricalComponent::gridSize);
                ImGui::TextDisabled("Drag the highlighted point to reposition it.");
                ImGui::Spacing();

                ImGui::BeginDisabled(m_simulationMode);
                if (ImGui::Button("Delete bend point"))
                {
                    const WirePoint removed = path[pointIndex];
                    path.erase(path.begin() + static_cast<std::ptrdiff_t>(pointIndex));
                    std::vector<WirePoint> routed{path.front()};
                    for (std::size_t i = 1; i < path.size(); ++i)
                    {
                        const WirePoint previous = routed.back();
                        const WirePoint next = path[i];
                        if (previous.x != next.x && previous.y != next.y)
                        {
                            // A perpendicular connection still needs a bend.
                            // Choose the other elbow instead of restoring this one.
                            const WirePoint horizontalFirst{next.x, previous.y};
                            const WirePoint verticalFirst{previous.x, next.y};
                            routed.push_back(horizontalFirst == removed
                                ? verticalFirst : horizontalFirst);
                        }
                        if (routed.back() != next) routed.push_back(next);
                    }
                    auto bends = interiorPoints(routed);
                    if (bends.size() <= Wire::maxBends)
                    {
                        it->setBends(std::move(bends));
                        m_selectedWireCornerIndex.reset();
                        m_dirty = true;
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Back to wire")) m_selectedWireCornerIndex.reset();
                ImGui::TextDisabled("The wire and conductor count are unchanged.");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.46f, 0.73f, 1.0f, 1.0f), "WIRE PROPERTIES");
                ImGui::Separator();
                ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(it->id()));
                ImGui::BeginDisabled(m_simulationMode);
                int conductors = it->conductors();
                if (ImGui::InputInt("Conductors", &conductors))
                {
                    it->setConductors(std::clamp(conductors, Wire::minConductors, Wire::maxConductors));
                    m_dirty = true;
                }
                ImGui::EndDisabled();
                ImGui::TextDisabled("1 to 32 conductors. Metadata only for now.");
                ImGui::Spacing();
                ImGui::Text("Bend points: %zu", path.size() >= 2 ? path.size() - 2 : 0);
                ImGui::TextWrapped("Click a blue bend point to edit or delete it. Double-click a segment to add one.");
                const ImVec2 a = resolveWireEndpoint(m_project, it->start());
                const ImVec2 b = resolveWireEndpoint(m_project, it->end());
                ImGui::Separator();
                ImGui::TextDisabled("Start: (%.0f, %.0f)", a.x, a.y);
                ImGui::TextDisabled("End:   (%.0f, %.0f)", b.x, b.y);
                if (ImGui::Button("Deselect"))
                {
                    m_selectedWireId.reset();
                    m_selectedWireCornerIndex.reset();
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    const auto it = std::find_if(m_project.components.begin(), m_project.components.end(),
        [&](const ElectricalComponent& component) { return component.id() == *m_selectedComponentId; });
    if (it == m_project.components.end())
    {
        m_selectedComponentId.reset();
        return;
    }

    ElectricalComponent& component = *it;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float top = viewport->Pos.y + ImGui::GetFrameHeight() + 74.0f;
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - 302.0f, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(290.0f, std::max(240.0f,
        viewport->Size.y - (top - viewport->Pos.y) - 12.0f)), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.085f, 0.12f, 0.17f, 1.0f));
    if (ImGui::Begin("Properties##component", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextColored(ImVec4(0.46f, 0.73f, 1.0f, 1.0f), "COMPONENT PROPERTIES");
        ImGui::Separator();
        ImGui::TextUnformatted("Light switch");
        ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(component.id()));
        ImGui::Spacing();
        if (m_simulationMode)
        {
            const bool closed = m_closedSwitches.contains(component.id());
            ImGui::TextColored(closed ? ImVec4(0.37f, 0.88f, 0.62f, 1.0f)
                                    : ImVec4(0.96f, 0.72f, 0.42f, 1.0f),
                               "Contact: %s", closed ? "CLOSED" : "OPEN");
            ImGui::TextDisabled("Click the switch to change its state.");
            ImGui::Separator();
        }

        ImGui::BeginDisabled(m_simulationMode);
        if (ImGui::InputText("Name", m_componentLabel.data(), m_componentLabel.size()))
        {
            component.setLabel(m_componentLabel.data());
            m_dirty = true;
        }

        // Edit position in grid cells, preventing free-pixel coordinates from
        // moving the switch contacts away from their grid intersections.
        constexpr int grid = ElectricalComponent::gridSize;
        constexpr int maxCell = 1000000 / grid;
        int xCell = static_cast<int>(std::round(component.x() / grid));
        int yCell = static_cast<int>(std::round(component.y() / grid));
        bool moved = ImGui::DragInt("X (cells)", &xCell, 0.2f, 0, maxCell);
        moved |= ImGui::DragInt("Y (cells)", &yCell, 0.2f, 0, maxCell);
        if (moved)
        {
            component.setPosition(static_cast<float>(std::clamp(xCell, 0, maxCell) * grid),
                                  static_cast<float>(std::clamp(yCell, 0, maxCell) * grid));
            m_dirty = true;
        }

        const char* lengths[] = { "4 cells (80 px)", "6 cells (120 px)", "8 cells (160 px)",
                                  "10 cells (200 px)", "12 cells (240 px)", "14 cells (280 px)",
                                  "16 cells (320 px)" };
        int lengthIndex = (component.lengthCells() - ElectricalComponent::minLengthCells) / 2;
        if (ImGui::Combo("Terminal span", &lengthIndex, lengths, 7))
        {
            component.setLengthCells(ElectricalComponent::minLengthCells + lengthIndex * 2);
            m_dirty = true;
        }
        ImGui::TextDisabled("Inner contacts: fixed 2-cell spacing.");
        ImGui::TextDisabled("Terminal span adjusts wire stubs only.");

        const char* rotations[] = { "0 deg", "90 deg", "180 deg", "270 deg" };
        int rotationIndex = component.rotation() / 90;
        if (ImGui::Combo("Rotation", &rotationIndex, rotations, 4))
        {
            component.setRotation(rotationIndex * 90);
            m_dirty = true;
        }

        ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("%s", m_simulationMode ? "Switch state is not saved."
                                                  : "Changes are saved with the project.");
        if (ImGui::Button("Deselect")) m_selectedComponentId.reset();
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
    ImGui::BeginDisabled(m_simulationMode);
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
    ImGui::EndDisabled();
    ImGui::TextDisabled("Components: %zu", m_project.components.size());
    ImGui::TextDisabled("Wires: %zu", m_project.wires.size());
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
    cancelWireCornerDrag();
    cancelWirePlacement();
    cancelComponentDrag();
    cancelComponentResize();
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
    cancelWireCornerDrag();
    cancelWirePlacement();
    cancelComponentDrag();
    cancelComponentResize();
    ProjectDocument loaded;
    if (FileHandler::load(path, loaded, m_error))
    {
        m_project = std::move(loaded);
        setSimulationMode(false);
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
