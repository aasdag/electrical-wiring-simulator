#include "FileHandler.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::size_t maxComponents = 1000;
    constexpr std::size_t maxWires = 1000;
    constexpr std::uintmax_t maxProjectSize = 512 * 1024;

    bool valid(const ProjectDocument& project)
    {
        if (project.name.empty() || project.name.size() > 127 ||
            project.notes.size() > 2047 || project.components.size() > maxComponents ||
            project.wires.size() > maxWires)
            return false;

        std::unordered_set<std::uint64_t> ids;
        for (const ElectricalComponent& component : project.components)
        {
            if ((component.type() != ElectricalComponentType::LightSwitch &&
             component.type() != ElectricalComponentType::Lamp) ||
                component.id() == 0 || component.id() == std::numeric_limits<std::uint64_t>::max() ||
                !std::isfinite(component.x()) || !std::isfinite(component.y()) ||
                component.x() < 0 || component.y() < 0 ||
                component.x() > 1000000 || component.y() > 1000000 ||
                component.label().size() > 63 ||
                !ElectricalComponent::validLength(component.lengthCells()) ||
                (component.rotation() != 0 && component.rotation() != 90 &&
                 component.rotation() != 180 && component.rotation() != 270) ||
                !ids.insert(component.id()).second)
                return false;
        }
        std::unordered_set<std::uint64_t> wireIds;
        const auto validEndpoint = [&](const WireEndpoint& endpoint) {
            if (!std::isfinite(endpoint.x) || !std::isfinite(endpoint.y) ||
                endpoint.x < 0.0f || endpoint.y < 0.0f ||
                endpoint.x > 1000000.0f || endpoint.y > 1000000.0f)
                return false;
            if (endpoint.componentId == 0)
                return endpoint.terminal == 0;
            if (endpoint.terminal != -1 && endpoint.terminal != 1)
                return false;
            return std::any_of(project.components.begin(), project.components.end(),
                [&](const ElectricalComponent& c) { return c.id() == endpoint.componentId; });
        };
        for (const Wire& wire : project.wires)
        {
            if (wire.id() == 0 || wire.id() == std::numeric_limits<std::uint64_t>::max() ||
                wire.conductors() < Wire::minConductors ||
                wire.conductors() > Wire::maxConductors ||
                wire.bends().size() > Wire::maxBends ||
                !validEndpoint(wire.start()) || !validEndpoint(wire.end()) ||
                !wireIds.insert(wire.id()).second)
                return false;
            for (const WirePoint& bend : wire.bends())
                if (!std::isfinite(bend.x) || !std::isfinite(bend.y) ||
                    bend.x < 0.0f || bend.y < 0.0f ||
                    bend.x > 1000000.0f || bend.y > 1000000.0f)
                    return false;
            // A wire must connect two different points or terminals.
            const auto& a = wire.start();
            const auto& b = wire.end();
            if (a.componentId && a.componentId == b.componentId && a.terminal == b.terminal)
                return false;
            if (!a.componentId && !b.componentId && a.x == b.x && a.y == b.y)
                return false;
        }
        return true;
    }
}

bool FileHandler::save(const std::filesystem::path& path, const ProjectDocument& project, std::string& error)
{
    error.clear();
    if (path.extension() != extension || !valid(project))
    {
        error = "Invalid project filename or data.";
        return false;
    }

    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "Could not create project file.";
            return false;
        }

        stream << "EWS_PROJECT 6\n" << std::quoted(project.name) << '\n'
               << std::quoted(project.notes) << '\n'
               << project.components.size() << '\n'
               << std::setprecision(std::numeric_limits<float>::max_digits10);
        for (const ElectricalComponent& component : project.components)
        {
            stream << component.id() << ' ' << static_cast<int>(component.type()) << ' '
                   << component.x() << ' ' << component.y() << ' '
                   << component.rotation() << ' ' << std::quoted(component.label()) << ' '
                   << component.lengthCells() << '\n';
        }
        stream << project.wires.size() << '\n';
        for (const Wire& wire : project.wires)
        {
            const auto& a = wire.start();
            const auto& b = wire.end();
            stream << wire.id() << ' ' << wire.conductors() << ' '
                   << a.x << ' ' << a.y << ' ' << a.componentId << ' ' << a.terminal << ' '
                   << b.x << ' ' << b.y << ' ' << b.componentId << ' ' << b.terminal << ' '
                   << wire.bends().size();
            for (const WirePoint& bend : wire.bends())
                stream << ' ' << bend.x << ' ' << bend.y;
            stream << '\n';
        }
        stream.flush();
        if (!stream)
        {
            error = "Failed writing project file.";
            stream.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        stream.close();
        if (!stream)
        {
            error = "Failed closing project file.";
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    std::error_code ec;
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    std::filesystem::rename(temporary, path, ec);
#endif
    if (ec)
    {
        error = "Could not replace project file: " + ec.message();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

bool FileHandler::load(const std::filesystem::path& path, ProjectDocument& project, std::string& error)
{
    error.clear();
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > maxProjectSize)
    {
        error = "Project is missing, inaccessible or too large.";
        return false;
    }

    std::ifstream stream(path, std::ios::binary);
    std::string magic;
    ProjectDocument parsed;
    if (!stream || !std::getline(stream, magic) ||
        (magic != "EWS_PROJECT 1" && magic != "EWS_PROJECT 2" &&
         magic != "EWS_PROJECT 3" && magic != "EWS_PROJECT 4" &&
         magic != "EWS_PROJECT 5" && magic != "EWS_PROJECT 6"))
    {
        error = "Not a supported EWS project.";
        return false;
    }

    if (!(stream >> std::quoted(parsed.name) >> std::quoted(parsed.notes)))
    {
        error = "Project name or notes are invalid.";
        return false;
    }

    if (magic == "EWS_PROJECT 2" || magic == "EWS_PROJECT 3" ||
        magic == "EWS_PROJECT 4" || magic == "EWS_PROJECT 5" ||
        magic == "EWS_PROJECT 6")
    {
        std::size_t count = 0;
        if (!(stream >> count) || count > maxComponents)
        {
            error = "Invalid number of project components.";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            std::uint64_t id = 0;
            int type = 0;
            float x = 0, y = 0;
            if (!(stream >> id >> type >> x >> y) ||
                (type != static_cast<int>(ElectricalComponentType::LightSwitch) &&
                 type != static_cast<int>(ElectricalComponentType::Lamp)))
            {
                error = "Invalid electrical component data.";
                return false;
            }

            int rotation = 0;
            std::string label;
            if ((magic == "EWS_PROJECT 3" || magic == "EWS_PROJECT 4" ||
                 magic == "EWS_PROJECT 5" || magic == "EWS_PROJECT 6") &&
                !(stream >> rotation >> std::quoted(label)))
            {
                error = "Invalid component properties.";
                return false;
            }
            int lengthCells = ElectricalComponent::defaultLengthCells;
            if ((magic == "EWS_PROJECT 4" || magic == "EWS_PROJECT 5" ||
                 magic == "EWS_PROJECT 6") && !(stream >> lengthCells))
            {
                error = "Invalid component length.";
                return false;
            }
            parsed.components.emplace_back(id, static_cast<ElectricalComponentType>(type),
                                           x, y, std::move(label), rotation, lengthCells);
            if (id < std::numeric_limits<std::uint64_t>::max())
                parsed.nextComponentId = std::max(parsed.nextComponentId, id + 1);
        }
    }

    if (magic == "EWS_PROJECT 5" || magic == "EWS_PROJECT 6")
    {
        std::size_t count = 0;
        if (!(stream >> count) || count > maxWires)
        {
            error = "Invalid number of wires.";
            return false;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            std::uint64_t id = 0;
            int conductors = 0;
            WireEndpoint start, end;
            if (!(stream >> id >> conductors
                         >> start.x >> start.y >> start.componentId >> start.terminal
                         >> end.x >> end.y >> end.componentId >> end.terminal))
            {
                error = "Invalid wire data.";
                return false;
            }
            std::vector<WirePoint> bends;
            if (magic == "EWS_PROJECT 6")
            {
                std::size_t bendCount = 0;
                if (!(stream >> bendCount) || bendCount > Wire::maxBends)
                {
                    error = "Invalid number of wire corners.";
                    return false;
                }
                bends.reserve(bendCount);
                for (std::size_t j = 0; j < bendCount; ++j)
                {
                    WirePoint bend;
                    if (!(stream >> bend.x >> bend.y))
                    {
                        error = "Invalid wire corner.";
                        return false;
                    }
                    bends.push_back(bend);
                }
            }
            parsed.wires.emplace_back(id, start, end, conductors, std::move(bends));
            if (id < std::numeric_limits<std::uint64_t>::max())
                parsed.nextWireId = std::max(parsed.nextWireId, id + 1);
        }
    }

    if (!valid(parsed))
    {
        error = "Project data is incomplete or invalid.";
        return false;
    }

    stream >> std::ws;
    if (stream.peek() != std::char_traits<char>::eof())
    {
        error = "Unexpected trailing project data.";
        return false;
    }

    project = std::move(parsed);
    return true;
}
