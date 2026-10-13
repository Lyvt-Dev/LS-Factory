#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
namespace forge
{
// Exactly one producer and one consumer; one cell is reserved to distinguish full from empty.
template <class T, std::size_t Capacity> class SpscQueue
{
    static_assert(Capacity > 1 && std::is_trivially_copyable_v<T>);

  public:
    bool push(const T &value) noexcept
    {
        const auto w = write_.load(std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire))
            return false;
        data_[w] = value;
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T &value) noexcept
    {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire))
            return false;
        value = data_[r];
        read_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

  private:
    std::array<T, Capacity> data_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};
} // namespace forge
