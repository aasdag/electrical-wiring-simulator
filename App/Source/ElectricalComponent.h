#pragma once

#include <cstdint>

// Positions are in canvas coordinates (pixels), independent of the UI window position.
enum class ElectricalComponentType : int
{
    LightSwitch = 1
};

class ElectricalComponent
{
public:
    ElectricalComponent(std::uint64_t id, ElectricalComponentType type, float x, float y)
        : m_id(id), m_type(type), m_x(x), m_y(y) {}

    [[nodiscard]] std::uint64_t id() const { return m_id; }
    [[nodiscard]] ElectricalComponentType type() const { return m_type; }
    [[nodiscard]] float x() const { return m_x; }
    [[nodiscard]] float y() const { return m_y; }

private:
    std::uint64_t m_id;
    ElectricalComponentType m_type;
    float m_x;
    float m_y;
};
