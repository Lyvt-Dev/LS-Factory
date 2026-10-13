#include "UndoStack.h"
namespace forge
{
Result UndoStack::execute(Project &project, std::string description,
                          const std::function<void(Project &)> &edit)
{
    Project candidate = project;
    edit(candidate);
    auto result = candidate.validate();
    if (!result)
        return result;
    if (candidate == project)
        return {};
    past_.push_back({std::move(description), project, candidate});
    if (past_.size() > historyLimit)
        past_.erase(past_.begin());
    future_.clear();
    project = std::move(candidate);
    return {};
}
bool UndoStack::undo(Project &p)
{
    if (past_.empty())
        return false;
    auto e = std::move(past_.back());
    past_.pop_back();
    p = e.before;
    future_.push_back(std::move(e));
    return true;
}
bool UndoStack::redo(Project &p)
{
    if (future_.empty())
        return false;
    auto e = std::move(future_.back());
    future_.pop_back();
    p = e.after;
    past_.push_back(std::move(e));
    return true;
}
} // namespace forge
