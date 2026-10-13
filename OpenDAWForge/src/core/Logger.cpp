#include "Logger.h"
#include <algorithm>
namespace forge
{
bool Logger::push(std::string_view message) noexcept
{
    std::array<char, 192> entry{};
    std::copy_n(message.begin(), std::min(message.size(), entry.size() - 1), entry.begin());
    return queue_.push(entry);
}
} // namespace forge
