#pragma once
#include "SpscQueue.h"
#include <array>
#include <string_view>
namespace forge
{
class Logger
{
  public:
    bool push(std::string_view message) noexcept;
    bool pop(std::array<char, 192> &message) noexcept
    {
        return queue_.pop(message);
    }

  private:
    SpscQueue<std::array<char, 192>, 128> queue_;
};
} // namespace forge
