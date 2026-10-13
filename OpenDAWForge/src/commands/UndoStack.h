#pragma once
#include "model/Project.h"
#include <functional>
#include <vector>
namespace forge
{
class UndoStack
{
  public:
    Result execute(Project &project, std::string description, const std::function<void(Project &)> &edit);
    bool undo(Project &project);
    bool redo(Project &project);
    void clear()
    {
        past_.clear();
        future_.clear();
    }
    [[nodiscard]] bool canUndo() const noexcept
    {
        return !past_.empty();
    }
    [[nodiscard]] bool canRedo() const noexcept
    {
        return !future_.empty();
    }

  private:
    struct Entry
    {
        std::string description;
        Project before, after;
    };
    std::vector<Entry> past_, future_;
    static constexpr std::size_t historyLimit = 100;
};
} // namespace forge
