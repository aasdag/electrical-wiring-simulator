#pragma once

#include <cstdint>
#include <string>
#include <utility>

// Positions are in canvas coordinates (pixels), independent of the UI window position.
enum class ElectricalComponentType : int
{
    LightSwitch = 1,
    Lamp = 2,
    SupplyInput = 3
};

enum class SupplySystem : int { IT = 1, TN = 2, TT = 3, DC = 4 };

struct SupplyInputSettings
{
    SupplySystem system = SupplySystem::IT;
    int voltage = 230;
    int frequency = 50;
    int phases = 1;
    bool neutral = false;
    bool protectiveEarth = true;
    bool showDescription = false;

    bool operator==(const SupplyInputSettings&) const = default;
};

[[nodiscard]] inline bool validSupplySettings(const SupplyInputSettings& settings)
{
    const int system = static_cast<int>(settings.system);
    if (system < 1 || system > 4 || settings.voltage < 1 || settings.voltage > 1000)
        return false;
    if (settings.system == SupplySystem::DC)
        return settings.frequency == 0 && settings.phases == 1 && !settings.neutral;
    return settings.frequency >= 1 && settings.frequency <= 400 &&
           (settings.phases == 1 || settings.phases == 3);
}

class ElectricalComponent
{
public:
    static constexpr int gridSize = 20;
    static constexpr int defaultLengthCells = 4;
    static constexpr int minLengthCells = 4;
    static constexpr int maxLengthCells = 16;

    // Even cell counts keep both terminals on grid nodes when the center is on-grid.
    [[nodiscard]] static constexpr bool validLength(int cells)
    {
        return cells >= minLengthCells && cells <= maxLengthCells && cells % 2 == 0;
    }
    ElectricalComponent(std::uint64_t id, ElectricalComponentType type, float x, float y,
                        std::string label = {}, int rotation = 0,
                        int lengthCells = defaultLengthCells,
                        SupplyInputSettings supply = {})
        : m_id(id), m_type(type), m_x(x), m_y(y),
          m_label(std::move(label)), m_rotation(rotation), m_lengthCells(lengthCells),
          m_supply(supply) {}

    [[nodiscard]] std::uint64_t id() const { return m_id; }
    [[nodiscard]] ElectricalComponentType type() const { return m_type; }
    [[nodiscard]] float x() const { return m_x; }
    [[nodiscard]] float y() const { return m_y; }
    [[nodiscard]] const std::string& label() const { return m_label; }
    [[nodiscard]] int rotation() const { return m_rotation; }
    [[nodiscard]] int lengthCells() const { return m_lengthCells; }
    [[nodiscard]] float halfLengthPx() const { return m_lengthCells * gridSize * 0.5f; }
    [[nodiscard]] const SupplyInputSettings& supplySettings() const { return m_supply; }

    void setPosition(float x, float y) { m_x = x; m_y = y; }
    void setLabel(std::string label) { m_label = std::move(label); }
    void setRotation(int rotation) { m_rotation = rotation; }
    void setLengthCells(int cells) { if (validLength(cells)) m_lengthCells = cells; }
    void setSupplySettings(const SupplyInputSettings& settings)
    {
        if (validSupplySettings(settings)) m_supply = settings;
    }

private:
    std::uint64_t m_id;
    ElectricalComponentType m_type;
    float m_x;
    float m_y;
    std::string m_label;
    int m_rotation;
    int m_lengthCells;
    SupplyInputSettings m_supply;
};
