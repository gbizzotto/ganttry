
#include <algorithm>
#include <sstream>
#include <map>

#include "project.hpp"

std::string operator+(std::string left, const std::string & right)
{
    return left.append(right);
}

namespace ganttry
{

void Task_Templated::set_template_id(int v)
{
    get_project().changed |= template_id != v;
    template_id = v;
}
float Task_Templated::duration_in_days() const
{
    TaskTemplate & templ = get_project().get_task_template(template_id);
    if (templ.use_avg && templ.average_UDM != 0)
        return (get_unit_count_forecast() - get_units_done_count()) / templ.average_UDM;
    else if (templ.default_UDM != 0)
        return (get_unit_count_forecast() - get_units_done_count()) / templ.default_UDM;
    else return 1;
}
std::string Task_Templated::to_json(TaskID tid) const
{
    std::stringstream ss;
    ss << "{\"id\": " << tid
       << ", \"name\": \""              << this->get_name() << "\""
       << ", \"description\": \""       << this->get_description() << "\""
       << ", \"unit_count_forecast\": " << this->get_unit_count_forecast()
       << ", \"units_done_count\": "    << this->get_units_done_count()
       << ", \"template_id\": "         << this->get_template_id()
       << "}"
       ;
    return ss.str();
}
std::string Task_Templated::get_full_display_name() const
{
    auto task_name = get_name();
    if ( ! task_name.empty())
        return task_name;
    else
        return get_project().workspace.get_task_template(template_id).name;
    //const std::string & task_template = get_project().workspace.get_task_template(template_id).name;
    //auto task_name = get_name();
    //if (task_name != "")
    //    task_name = " < " + task_name;
    //return task_template + task_name;
}

float Task_SubProject::duration_in_days() const
{
    return child.duration_in_seconds() / 86400;
}
bool Task_SubProject::contains(const Project * const p) const
{
    return (&child == p) || child.contains(p);
}
std::string Task_SubProject::to_json(TaskID tid) const
{
    std::stringstream ss;
    ss << "{\"id\": " << tid
       << ", \"name\": \""              << this->get_name() << "\""
       << ", \"description\": \""       << this->get_description() << "\""
       << ", \"unit_count_forecast\": " << this->get_unit_count_forecast()
       << ", \"units_done_count\": "    << this->get_units_done_count()
       << ", \"project_filename\": \""    << this->child.filename << "\""
       << "}"
       ;
    return ss.str();
}
std::string Task_SubProject::get_full_display_name() const
{
    auto name = get_name();
    if ( ! name.empty())
        return name;
    else
        return child.name;

    //return get_name() + ((get_name().size())?" < ":"") + child.name + " < " + get_project().name;
}

std::string Task_TimePoint::to_json(TaskID tid) const
{
    std::stringstream ss;
    ss << "{\"id\": " << tid
       << ", \"name\": \""              << this->get_name() << "\""
       << ", \"description\": \""       << this->get_description() << "\""
       << ", \"time_point\": "          << this->time_point
       << "}"
       ;
    return ss.str();
}

Task_Base::Task_Base( Project & project
                    , TaskID id
                    , std::string name
                    , std::string description
                    , float unit_count_forecast
                    , float units_done_count
                    )
    : project(project)
    , id(id)
    , name(name)
    , description(description)
    , unit_count_forecast(unit_count_forecast)
    , units_done_count(units_done_count)
    , unixtime_start_offset(0)
{}


void Task_Base::set_id(TaskID v)
{
    project.changed |= id != v;
    id = v;
}
void Task_Base::set_name(std::string v)
{
    project.changed |= name != v;
    name = v;
}
void Task_Base::set_description(std::string v)
{
    project.changed |= description != v;
    description = v;
}

void Task_Base::set_unit_count_forecast(float forecast)
{
    project.changed |= unit_count_forecast != forecast;
    this->unit_count_forecast = forecast;
    recalculate_start_offset();
    children_recalculate_start_offset();
}
void Task_Base::set_units_done_count(float done)
{
    project.changed |= units_done_count != done;
    this->units_done_count = done;
    recalculate_start_offset();
    children_recalculate_start_offset();
}
void Task_Base::set_unixtime_start_offset(std::uint64_t v)
{
    project.changed |= unixtime_start_offset != v;
    this->unixtime_start_offset = v;
    children_recalculate_start_offset();
}

void Task_Base::children_recalculate_start_offset()
{
    for (const auto & p : children_tasks)
    {
        auto it = project.tasks.find(p.task_id);
        if (it != project.tasks.end())
            it->second->recalculate_start_offset();
    }
}

bool Task_Base::add_child_task(DependencyType d, Task_Base & child)
{
    bool changed = false;

    // in parent
    auto it_children = std::find_if(children_tasks.begin(), children_tasks.end(), [&](const Dependency & d){ return d.task_id == child.id; });
    if (it_children == children_tasks.end())
    {
        changed = true;
        children_tasks.push_back({d, child.id});
    }
    else if (it_children->type != d)
    {
        changed = true;
        it_children->type = d;
    }

    return changed;
}
bool Task_Base::add_parent_task(DependencyType d, Task_Base & parent)
{
    bool changed = false;

    // in child
    auto it_parents = std::find_if(parent_tasks.begin(), parent_tasks.end(), [&](const Dependency & d){ return d.task_id == id; });
    if (it_parents == parent_tasks.end())
    {
        changed = true;
        parent_tasks.push_back({d, parent.id});
    }
    else if (it_parents->type != d)
    {
        changed = it_parents->type != d;
        it_parents->type = d;
    }

    if (changed)
        recalculate_start_offset();
    return changed;
}
bool Task_Base::remove_parent_task(TaskID task_id)
{
    for (auto it=this->parent_tasks.begin(), end=this->parent_tasks.end() ; it!=end ; ++it)
    {
        auto d = *it;
        if (d.task_id == task_id)
        {
            this->parent_tasks.erase(it);
            this->project.changed = true;
            return true;
        }
    }
    return false;
}
bool Task_Base::remove_child_task(TaskID task_id)
{
    for (auto it=this->children_tasks.begin(), end=this->children_tasks.end() ; it!=end ; ++it)
    {
        auto d = *it;
        if (d.task_id == task_id)
        {
            this->children_tasks.erase(it);
            this->project.changed = true;
            return true;
        }
    }
    return false;
}

uint64_t Task_Base::duration_in_seconds() const
{
    return 86400 * duration_in_days();
}

void Task_Base::recalculate_start_offset()
{
    uint64_t earliest_time = [&]()
        {
            uint64_t earliest_time = 0;
            for (const Dependency & d : parent_tasks)
            {
                auto it = project.tasks.find(d.task_id);
                if (it == project.tasks.end())
                    continue;                
                const auto & task_base = *it->second;
                if (d.type == DependencyType::BeginAfter)
                    earliest_time = std::max(earliest_time, task_base.unixtime_end_offset());
                else if (d.type == DependencyType::BeginWith)
                    earliest_time = std::max(earliest_time, task_base.unixtime_start_offset);
            }
            return earliest_time;
        }();
    uint64_t latest_time = [&]()
        {
            auto latest_time = std::numeric_limits<uint64_t>::max();
            for (const Dependency & d : parent_tasks)
            {
                auto it = project.tasks.find(d.task_id);
                if (it == project.tasks.end())
                    continue;
                const auto & task = *it->second;
                if (d.type == DependencyType::EndBefore)
                    latest_time = std::min(latest_time, task.unixtime_start_offset - this->duration_in_seconds());
                else if (d.type == DependencyType::EndWith)
                    latest_time = std::min(latest_time, task.unixtime_end_offset() - this->duration_in_seconds());
            }
            return latest_time;
        }();

    if (earliest_time == 0 && latest_time == std::numeric_limits<uint64_t>::max())
        set_unixtime_start_offset(0);
    else if (earliest_time == 0)
        set_unixtime_start_offset(latest_time);
    else
        set_unixtime_start_offset(earliest_time);
}

bool Task_Base::find_descendent(TaskID id_)
{
    if (this->id == id_)
        return true;
    return children_tasks.end() != std::find_if(children_tasks.begin(), children_tasks.end(), [&](const Dependency & d)
        {
            auto it = project.tasks.find(d.task_id);
            return d.task_id == id_
                || (it != project.tasks.end() && it->second->find_descendent(id_));
        });
}

TaskTemplate & Project::get_task_template(TemplateID id) { return workspace.get_task_template(id); }
std::map<uint64_t,TaskTemplate> & Project::get_task_templates() { return workspace.get_task_templates(); }
Workspace & Project::get_workspace() { return workspace; }
bool Project::remove_task(TaskID id)
{
    auto it = tasks.find(id);
    if (it == tasks.end())
        return false;
    tasks.erase(it);
    return true;
}

uint64_t Task_SubProject::duration_in_seconds() const
{
    return child.duration_in_seconds();
}

nixtime Task_TimePoint::get_unixtime_start_offset() const
{
    return time_point - get_project().get_unixtime_start();
}


void Task_TimePoint::set_time_point(nixtime t)
{
    get_project().changed |= (time_point != t);
    time_point = t;
}

} // namespace

