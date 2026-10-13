#pragma once
#include "model/Project.h"
#include <filesystem>
namespace forge
{
Result saveProject(const Project &, const std::filesystem::path &);
Result loadProject(const std::filesystem::path &, Project &);
} // namespace forge
