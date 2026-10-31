#pragma once

#include "ElectricalComponent.h"

struct ImDrawList;
struct ImVec2;

// Switching view only changes rendering; never component or wire data.
enum class SchematicView { SingleLine, MultiLine };

namespace ComponentRenderer
{
    // A physical terminal +/-1 or a logical single-line anchor (0).
    ImVec2 connectionPoint(const ElectricalComponent& component, int terminal, SchematicView view);

    void draw(ImDrawList* draw, ImVec2 center, const ElectricalComponent& component,
              SchematicView view, bool preview = false, bool selected = false,
              bool closed = false, bool resizeHandle = false);
}
