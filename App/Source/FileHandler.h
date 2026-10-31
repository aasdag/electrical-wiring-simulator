#pragma once

#include <filesystem>
#include <string>

struct ProjectDocument
{
    std::string name = "Untitled";
    std::string notes;
};

class FileHandler
{
public:
    static constexpr const char* extension = ".ews";
    static bool save(const std::filesystem::path& path, const ProjectDocument& project, std::string& error);
    static bool load(const std::filesystem::path& path, ProjectDocument& project, std::string& error);
};
