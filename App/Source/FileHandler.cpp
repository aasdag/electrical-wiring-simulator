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
    constexpr std::uintmax_t maxProjectSize = 256 * 1024;

    bool valid(const ProjectDocument& project)
    {
        if (project.name.empty() || project.name.size() > 127 ||
            project.notes.size() > 2047 || project.components.size() > maxComponents)
            return false;

        std::unordered_set<std::uint64_t> ids;
        for (const ElectricalComponent& component : project.components)
        {
            if (component.type() != ElectricalComponentType::LightSwitch ||
                component.id() == 0 || component.id() == std::numeric_limits<std::uint64_t>::max() ||
                !std::isfinite(component.x()) || !std::isfinite(component.y()) ||
                component.x() < 0 || component.y() < 0 ||
                component.x() > 1000000 || component.y() > 1000000 ||
                !ids.insert(component.id()).second)
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

        stream << "EWS_PROJECT 2\n" << std::quoted(project.name) << '\n'
               << std::quoted(project.notes) << '\n'
               << project.components.size() << '\n'
               << std::setprecision(std::numeric_limits<float>::max_digits10);
        for (const ElectricalComponent& component : project.components)
        {
            stream << component.id() << ' ' << static_cast<int>(component.type()) << ' '
                   << component.x() << ' ' << component.y() << '\n';
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
        (magic != "EWS_PROJECT 1" && magic != "EWS_PROJECT 2"))
    {
        error = "Not a supported EWS project.";
        return false;
    }

    if (!(stream >> std::quoted(parsed.name) >> std::quoted(parsed.notes)))
    {
        error = "Project name or notes are invalid.";
        return false;
    }

    if (magic == "EWS_PROJECT 2")
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
                type != static_cast<int>(ElectricalComponentType::LightSwitch))
            {
                error = "Invalid electrical component data.";
                return false;
            }

            parsed.components.emplace_back(id, static_cast<ElectricalComponentType>(type), x, y);
            if (id < std::numeric_limits<std::uint64_t>::max())
                parsed.nextComponentId = std::max(parsed.nextComponentId, id + 1);
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
