#pragma once

#include <cstdint>
#include <string>
#include <utility>

// Positions are in canvas coordinates (pixels), independent of the UI window position.
enum class ElectricalComponentType : int
{
    LightSwitch = 1
};

class ElectricalComponent
{
public:
    ElectricalComponent(std::uint64_t id, ElectricalComponentType type, float x, float y,
                        std::string label = {}, int rotation = 0)
        : m_id(id), m_type(type), m_x(x), m_y(y),
          m_label(std::move(label)), m_rotation(rotation) {}

    [[nodiscard]] std::uint64_t id() const { return m_id; }
    [[nodiscard]] ElectricalComponentType type() const { return m_type; }
    [[nodiscard]] float x() const { return m_x; }
    [[nodiscard]] float y() const { return m_y; }
    [[nodiscard]] const std::string& label() const { return m_label; }
    [[nodiscard]] int rotation() const { return m_rotation; }

    void setPosition(float x, float y) { m_x = x; m_y = y; }
    void setLabel(std::string label) { m_label = std::move(label); }
    void setRotation(int rotation) { m_rotation = rotation; }

private:
    std::uint64_t m_id;
    ElectricalComponentType m_type;
    float m_x;
    float m_y;
    std::string m_label;
    int m_rotation;
};
