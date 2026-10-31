#pragma once

#include "ElectricalComponent.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ProjectDocument
{
    std::string name = "Untitled";
    std::string notes;
    std::vector<ElectricalComponent> components;
    std::uint64_t nextComponentId = 1;

    void addComponent(ElectricalComponentType type, float x, float y)
    {
        components.emplace_back(nextComponentId++, type, x, y);
    }
};

class FileHandler
{
public:
    static constexpr const char* extension = ".ews";
    static bool save(const std::filesystem::path& path, const ProjectDocument& project, std::string& error);
    static bool load(const std::filesystem::path& path, ProjectDocument& project, std::string& error);
};
