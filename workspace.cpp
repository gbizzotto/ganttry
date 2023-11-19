
#include "workspace.hpp"
#include "project.hpp"

namespace ganttry
{

Project * Workspace::get_project_by_filename(std::string filename)
{
    for (auto & proj : projects)
        if (proj->filename == filename)
            return proj.get();
    return nullptr;
}

} // namespace
