#pragma once

#include <cstdint>

// A free point is stored in canvas pixels. Attached endpoints keep a stable
// component ID and terminal side, and resolve against the component at draw time.
struct WireEndpoint
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint64_t componentId = 0;
    int terminal = 0; // -1: negative terminal; +1: positive terminal; 0: free grid point.
};

class Wire
{
public:
    static constexpr int minConductors = 1;
    static constexpr int maxConductors = 32;

    Wire(std::uint64_t id, WireEndpoint start, WireEndpoint end, int conductors = 1)
        : m_id(id), m_start(start), m_end(end), m_conductors(conductors) {}

    [[nodiscard]] std::uint64_t id() const { return m_id; }
    [[nodiscard]] const WireEndpoint& start() const { return m_start; }
    [[nodiscard]] const WireEndpoint& end() const { return m_end; }
    [[nodiscard]] int conductors() const { return m_conductors; }

    void setConductors(int count)
    {
        if (count >= minConductors && count <= maxConductors) m_conductors = count;
    }

private:
    std::uint64_t m_id;
    WireEndpoint m_start;
    WireEndpoint m_end;
    int m_conductors;
};
