#pragma once
#include "model/Project.h"
#include <filesystem>
#include <string>
namespace forge
{
[[nodiscard]] std::string projectJson(const Project &);
Result exportJson(const Project &, const std::filesystem::path &);
} // namespace forge
