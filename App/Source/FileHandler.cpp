#include "FileHandler.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <fstream>
#include <iomanip>
#include <system_error>
#include <utility>

namespace
{
    bool valid(const ProjectDocument& project)
    {
        return !project.name.empty() && project.name.size() <= 127 && project.notes.size() <= 2047;
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
        stream << "EWS_PROJECT 1\n" << std::quoted(project.name) << '\n'
               << std::quoted(project.notes) << '\n';
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

    // Keep the original file untouched if replacement fails.
    std::error_code ec;
#ifdef _WIN32
    // std::filesystem::rename does not replace an existing destination on Windows.
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
    if (ec || size > 64 * 1024)
    {
        error = "Project is missing, inaccessible or too large.";
        return false;
    }

    std::ifstream stream(path, std::ios::binary);
    std::string magic;
    ProjectDocument parsed;
    if (!stream || !std::getline(stream, magic) || magic != "EWS_PROJECT 1")
    {
        error = "Not a supported EWS project (version 1).";
        return false;
    }
    if (!(stream >> std::quoted(parsed.name) >> std::quoted(parsed.notes)) || !valid(parsed))
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
