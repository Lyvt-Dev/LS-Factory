#pragma once
#include <string>
#include <utility>
namespace forge
{
struct Result
{
    bool ok = true;
    std::string error;
    explicit operator bool() const noexcept
    {
        return ok;
    }
    static Result failure(std::string message)
    {
        return {false, std::move(message)};
    }
};
} // namespace forge
