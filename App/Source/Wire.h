#pragma once

#include <cstdint>
#include <utility>
#include <vector>

// Attached endpoints retain their component ID. Terminal 0 with a component ID
// is a logical single-line anchor, not an actual electrical contact. +/-1 are
// the physical terminals used in multi-line views.
struct WireEndpoint
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint64_t componentId = 0;
    int terminal = 0; // 0 = free grid point or logical anchor; +/-1 = physical terminal.
};

struct WirePoint
{
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const WirePoint&) const = default;
};

class Wire
{
public:
    static constexpr int minConductors = 1;
    static constexpr int maxConductors = 32;
    static constexpr std::size_t maxBends = 128;

    Wire(std::uint64_t id, WireEndpoint start, WireEndpoint end, int conductors = 1,
         std::vector<WirePoint> bends = {})
        : m_id(id), m_start(start), m_end(end), m_conductors(conductors),
          m_bends(std::move(bends)) {}

    [[nodiscard]] std::uint64_t id() const { return m_id; }
    [[nodiscard]] const WireEndpoint& start() const { return m_start; }
    [[nodiscard]] const WireEndpoint& end() const { return m_end; }
    [[nodiscard]] int conductors() const { return m_conductors; }
    [[nodiscard]] const std::vector<WirePoint>& bends() const { return m_bends; }

    void setBends(std::vector<WirePoint> bends) { m_bends = std::move(bends); }

    void setConductors(int count)
    {
        if (count >= minConductors && count <= maxConductors) m_conductors = count;
    }

private:
    std::uint64_t m_id;
    WireEndpoint m_start;
    WireEndpoint m_end;
    int m_conductors;
    std::vector<WirePoint> m_bends;
};
