#include "ComponentRenderer.h"

#include <imgui.h>

#include <cmath>
#include <string>

namespace
{
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

    // IEC-style lamp: a circle with a diagonal cross. Its symbol stays fixed at
    // one grid cell radius; only the terminal leads vary with the configured span.
    void drawLamp(ImDrawList* draw, ImVec2 center,
                  const ElectricalComponent& component, bool preview,
                  bool selected = false, bool resizeHandle = false)
    {
        const ImU32 stroke = preview ? IM_COL32(90, 186, 255, 160)
                                     : IM_COL32(148, 217, 255, 255);
        const float angle = component.rotation() * (3.14159265f / 180.0f);
        const float cs = std::cos(angle), sn = std::sin(angle);
        const auto point = [&](float x, float y) {
            return ImVec2(center.x + x * cs - y * sn,
                          center.y + x * sn + y * cs);
        };

        const float radius = static_cast<float>(ElectricalComponent::gridSize);
        const float halfLength = component.halfLengthPx();
        const float diagonal = radius * 0.70710678f;
        draw->AddLine(point(-halfLength, 0), point(-radius, 0), stroke, 2.2f);
        draw->AddLine(point(radius, 0), point(halfLength, 0), stroke, 2.2f);
        draw->AddCircle(center, radius, stroke, 0, 2.2f);
        draw->AddLine(point(-diagonal, -diagonal), point(diagonal, diagonal), stroke, 2.0f);
        draw->AddLine(point(-diagonal, diagonal), point(diagonal, -diagonal), stroke, 2.0f);
        draw->AddCircleFilled(point(-halfLength, 0), 3.0f, stroke);
        draw->AddCircleFilled(point(halfLength, 0), 3.0f, stroke);

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
            ? "H" + std::to_string(component.id()) : component.label();
        const float labelWidth = ImGui::CalcTextSize(label.c_str()).x;
        draw->AddText(ImVec2(center.x - labelWidth * 0.5f, center.y + 24.0f),
                      stroke, label.c_str());
    }

    void drawSupplyInput(ImDrawList* draw, ImVec2 center,
                         const ElectricalComponent& component, bool preview,
                         bool selected = false, bool resizeHandle = false,
                         bool multiLine = false)
    {
        const ImU32 stroke = preview ? IM_COL32(90, 186, 255, 160)
                                     : IM_COL32(148, 217, 255, 255);
        const float angle = component.rotation() * (3.14159265f / 180.0f);
        const float cs = std::cos(angle), sn = std::sin(angle);
        const auto point = [&](float x, float y) {
            return ImVec2(center.x + x * cs - y * sn,
                          center.y + x * sn + y * cs);
        };

        const float grid = static_cast<float>(ElectricalComponent::gridSize);
        const float halfLength = component.halfLengthPx();
        const ImVec2 topLeft = point(-2.0f * grid, -grid);
        const ImVec2 topRight = point(0.0f, -grid);
        const ImVec2 bottomRight = point(0.0f, grid);
        const ImVec2 bottomLeft = point(-2.0f * grid, grid);
        draw->AddQuadFilled(topLeft, topRight, bottomRight, bottomLeft,
                            IM_COL32(32, 48, 68, preview ? 145 : 255));
        draw->AddQuad(topLeft, topRight, bottomRight, bottomLeft, stroke, 2.2f);
        draw->AddLine(point(0.0f, 0.0f), point(halfLength, 0.0f), stroke, 2.2f);
        draw->AddCircleFilled(point(halfLength, 0.0f), 3.0f, stroke);

        const ImVec2 labelCenter = point(-grid, 0.0f);
        const ImVec2 labelSize = ImGui::CalcTextSize("IN");
        draw->AddText(ImVec2(labelCenter.x - labelSize.x * 0.5f,
                             labelCenter.y - labelSize.y * 0.5f), stroke, "IN");
        const auto& settings = component.supplySettings();
        if (settings.showDescription || multiLine)
        {
            const char* systems[] = { "IT", "TN", "TT", "DC" };
            const std::string details = std::to_string(settings.voltage) + " V " +
                systems[static_cast<int>(settings.system) - 1];
            const ImVec2 textSize = ImGui::CalcTextSize(details.c_str());
            draw->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y + 26.0f),
                          stroke, details.c_str());
        }
        if (multiLine)
        {
            // Informational conductor names, not individually connectable ports.
            const char* names = settings.system == SupplySystem::DC ? "+ / -" :
                settings.phases == 3
                    ? (settings.neutral ? "L1 L2 L3 N" : "L1 L2 L3")
                    : (settings.neutral ? "L N" : "L1 L2");
            std::string conductors = names;
            if (settings.protectiveEarth) conductors += " PE";
            const ImVec2 size = ImGui::CalcTextSize(conductors.c_str());
            draw->AddText(ImVec2(center.x - size.x * 0.5f, center.y + 44.0f),
                          stroke, conductors.c_str());
        }
        const std::string name = component.label().empty()
            ? "IN" + std::to_string(component.id()) : component.label();
        const ImVec2 nameSize = ImGui::CalcTextSize(name.c_str());
        draw->AddText(ImVec2(center.x - nameSize.x * 0.5f, center.y - 39.0f),
                      stroke, name.c_str());

        if (selected)
        {
            draw->AddQuad(point(-2.0f * grid - 8.0f, -grid - 8.0f),
                          point(halfLength + 8.0f, -grid - 8.0f),
                          point(halfLength + 8.0f, grid + 8.0f),
                          point(-2.0f * grid - 8.0f, grid + 8.0f),
                          IM_COL32(88, 191, 255, 255), 2.0f);
            if (resizeHandle)
            {
                const ImVec2 handle = point(halfLength + 9.0f, 0.0f);
                draw->AddRectFilled(ImVec2(handle.x - 6.0f, handle.y - 6.0f),
                                    ImVec2(handle.x + 6.0f, handle.y + 6.0f),
                                    IM_COL32(88, 191, 255, 255), 2.0f);
            }
        }
    }

    // Installation-plan symbols from the supplied reference. Terminal positions
    // are kept identical to their detailed-symbol counterparts.
    void drawSingleLineDevice(ImDrawList* draw, ImVec2 center,
                              const ElectricalComponent& component, bool preview,
                              bool selected, bool closed, bool resizeHandle)
    {
        const bool isSwitch = component.type() == ElectricalComponentType::LightSwitch;
        const ImU32 stroke = preview ? IM_COL32(90, 186, 255, 160)
                                   : isSwitch && closed ? IM_COL32(95, 225, 161, 255)
                                                        : IM_COL32(148, 217, 255, 255);
        const float radians = component.rotation() * (3.14159265f / 180.0f);
        const float cs = std::cos(radians), sn = std::sin(radians);
        const auto point = [&](float x, float y) {
            return ImVec2(center.x + x * cs - y * sn,
                          center.y + x * sn + y * cs);
        };
        (void)resizeHandle;
        if (isSwitch)
        {
            // The single-line connection stem starts at the bottom of the ring.
            draw->AddCircle(center, 8.0f, stroke, 0, 2.2f);
            draw->AddLine(point(5, -7), point(21, -29), stroke, 2.4f);
            draw->AddLine(point(21, -29), point(29, -25), stroke, 2.4f);
            draw->AddLine(point(0, 8), point(0, ElectricalComponent::gridSize),
                          stroke, 2.2f);
        }
        else
        {
            // Lamp: free-standing X with one logical anchor at its center.
            draw->AddLine(point(-16, -16), point(16, 16), stroke, 2.4f);
            draw->AddLine(point(-16, 16), point(16, -16), stroke, 2.4f);
            draw->AddLine(point(0, 0), point(0, ElectricalComponent::gridSize),
                          stroke, 2.2f);
        }
        // One visible, grid-aligned wire anchor for both incoming and outgoing wires.
        draw->AddCircleFilled(point(0, ElectricalComponent::gridSize), 3.5f, stroke);
        if (selected)
        {
            const float right = isSwitch ? 34.0f : 24.0f;
            const float top = isSwitch ? -34.0f : -24.0f;
            draw->AddQuad(point(-24, top), point(right, top),
                          point(right, 27), point(-24, 27),
                          IM_COL32(88, 191, 255, 255), 2.0f);
        }
        const std::string label = component.label().empty()
            ? std::string(isSwitch ? "S" : "H") + std::to_string(component.id())
            : component.label();
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImVec2(center.x - labelSize.x * 0.5f, center.y + 31.0f),
                      stroke, label.c_str());
    }

}

ImVec2 ComponentRenderer::connectionPoint(const ElectricalComponent& component,
                                          int terminal, SchematicView view)
{
    // The single-line lamp and switch share one visual anchor one grid step
    // below the symbol; terminal 0 remains a logical, not electrical, contact.
    // Rotate the stem with the symbol (multiples of 90 deg remain grid aligned).
    const float radians = component.rotation() * (3.14159265f / 180.0f);
    const float cs = std::cos(radians), sn = std::sin(radians);
    if (view == SchematicView::SingleLine &&
        component.type() != ElectricalComponentType::SupplyInput)
    {
        const float offset = static_cast<float>(ElectricalComponent::gridSize);
        return ImVec2(component.x() - offset * sn,
                      component.y() + offset * cs);
    }
    const float distance = component.type() == ElectricalComponentType::SupplyInput
        ? component.halfLengthPx()
        : terminal == 0 ? 0.0f : terminal * component.halfLengthPx();
    return ImVec2(component.x() + distance * cs,
                  component.y() + distance * sn);
}

void ComponentRenderer::draw(ImDrawList* draw, ImVec2 center,
                             const ElectricalComponent& component, SchematicView view,
                             bool preview, bool selected, bool closed, bool resizeHandle)
{
    if (view == SchematicView::SingleLine &&
        (component.type() == ElectricalComponentType::LightSwitch ||
         component.type() == ElectricalComponentType::Lamp))
        drawSingleLineDevice(draw, center, component, preview, selected, closed, resizeHandle);
    else if (component.type() == ElectricalComponentType::LightSwitch)
        drawLightSwitch(draw, center, component, preview, selected, closed, resizeHandle);
    else if (component.type() == ElectricalComponentType::Lamp)
        drawLamp(draw, center, component, preview, selected, resizeHandle);
    else if (component.type() == ElectricalComponentType::SupplyInput)
        drawSupplyInput(draw, center, component, preview, selected, resizeHandle,
                        view != SchematicView::SingleLine);
}
