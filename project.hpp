
#pragma once

#include <vector>
#include <set>
#include <string>
#include <functional>
#include <variant>

#include <QDateTime>

#include "types.hpp"
#include "workspace.hpp"

namespace ganttry
{

using nixtime = std::uint64_t; // seconds since epoch
using nixtime_diff = std::int64_t; // seconds since epoch

enum DependencyType {
    BeginAfter,
    BeginWith,
    EndBefore,
    EndWith,
};

struct Project;
class Workspace;
struct TaskTemplate;

struct Dependency
{
    DependencyType type;
    TaskID task_id;
};

class Task_Base
{
    Project     & project              ;
    TaskID        id                   ;
    std::string   name                 ;
    std::string   description          ;
    float         unit_count_forecast  ;
    float         units_done_count     ;
    std::uint64_t unixtime_start_offset;

    // dependencies
    std::vector<Dependency> parent_tasks  ;
    std::vector<Dependency> children_tasks;

    // HR

public:
    Task_Base( Project & project
             , TaskID id
             , std::string name
             , std::string description
             , float unit_count_forecast
             , float units_done_count
             );

    inline Task_Base & operator=(const Task_Base & other)
    {
        if (&project != &other.project)
            throw new std::exception();
        this->id                  = other.id;
        this->name                = other.name;
        this->description         = other.description;
        this->unit_count_forecast = other.unit_count_forecast;
        this->units_done_count    = other.units_done_count;
        this->parent_tasks        = other.parent_tasks;
        this->children_tasks      = other.children_tasks;
        return *this;
    }

    inline auto & get_project              () const { return project              ; }
    inline auto & get_id                   () const { return id                   ; }
    inline auto & get_name                 () const { return name                 ; }
    inline auto & get_description          () const { return description          ; }
    inline auto & get_unit_count_forecast  () const { return unit_count_forecast  ; }
    inline auto & get_units_done_count     () const { return units_done_count     ; }
    virtual inline nixtime_diff get_unixtime_start_offset() const { return unixtime_start_offset; }
    inline auto & get_parent_tasks         () const { return parent_tasks         ; }
    inline auto & get_children_tasks       () const { return children_tasks       ; }

    void set_id         (TaskID      v);
    void set_name       (std::string v);
    void set_description(std::string v);

    void set_unit_count_forecast(float forecast);
    void set_units_done_count(float done);
    virtual void set_unixtime_start_offset(std::uint64_t v);

    //std::uint64_t unixtime_start() const;

    virtual inline nixtime_diff get_unixtime_end_offset() const { return unixtime_start_offset + duration_in_seconds(); }
    virtual nixtime_diff duration_in_seconds() const;
    bool add_child_task(DependencyType d, Task_Base & t);
    bool add_parent_task(DependencyType d, Task_Base & t);
    bool remove_parent_task(TaskID task_id);
    bool remove_child_task (TaskID task_id);
    void recalculate_start_offset();
    void children_recalculate_start_offset();
    bool find_descendent(TaskID id);

    inline virtual int get_template_id() const { return -1; }
    inline virtual bool is_recursive() const { return false; }
    inline virtual bool is_relative() const { return true; }
    inline virtual Project * get_child() { return nullptr; }
    virtual void set_template_id(int) {}; // do nothing
    virtual float duration_in_days() const = 0;
    virtual bool contains(const Project * const proj) const = 0;
    virtual std::string to_json(TaskID tid) const = 0;
    virtual std::string get_full_display_name() const = 0;
};

class Task_TimePoint : public Task_Base
{
    nixtime time_point;

public:
    inline Task_TimePoint( Project & project
                  , TaskID id
                  , std::string name
                  , std::string description
                  , std::uint64_t time
                  )
        : Task_Base(project, id, name, description, 0, 0)
        , time_point(time)
    {}

    inline nixtime get_time_point() { return time_point; }
    void set_time_point(nixtime t);

    virtual inline float duration_in_days() const override { return 0; }
    virtual inline bool contains(const Project * const ) const override { return false; }
    virtual std::string to_json(TaskID tid) const override;
    virtual inline std::string get_full_display_name() const override { return get_name(); }

    virtual nixtime_diff get_unixtime_start_offset() const override;
    virtual inline nixtime_diff get_unixtime_end_offset() const override { return get_unixtime_start_offset(); }
    inline virtual bool is_relative() const override { return false; }
};

class Task_Templated : public Task_Base
{
    int template_id;

public:
    inline Task_Templated( Project & project
                         , TaskID id
                         , std::string name
                         , std::string description
                         , float unit_count_forecast
                         , float units_done_count
                         , int template_id_
                         )
        : Task_Base(project, id, name, description, unit_count_forecast, units_done_count)
        , template_id(template_id_)
    {}
    inline Task_Templated & operator=(const Task_Templated & other)
    {
        Task_Base::operator=(other);
        this->template_id = other.template_id;
        return *this;
    }
    virtual void set_template_id(int v) override;
    inline virtual int get_template_id() const override { return template_id; }

    virtual float duration_in_days() const override;
    inline virtual bool contains(const Project * const) const override { return false; }
    virtual std::string to_json(TaskID tid) const override;
    virtual std::string get_full_display_name() const override;

};

class Task_SubProject : public Task_Base
{
    Project & child;

public:
    inline Task_SubProject( Project & project
                          , TaskID id
                          , std::string name
                          , std::string description
                          , float unit_count_forecast
                          , float units_done_count
                          , Project & p
                          )
        : Task_Base(project, id, name, description, unit_count_forecast, units_done_count)
        , child(p)
    {}

    inline virtual bool is_recursive() const override { return true; }
    inline virtual Project * get_child() override { return &child; }
    virtual float duration_in_days() const override;
    virtual nixtime_diff duration_in_seconds() const override;
    virtual bool contains(const Project * const p) const override;
    virtual std::string to_json(TaskID tid) const override;
    virtual std::string get_full_display_name() const override;
};

struct Project
{
    bool changed = true;
    std::string filename;

    std::string name = "New project";
    Workspace & workspace;
    std::map<TaskID,std::unique_ptr<Task_Base>> tasks;
    int zoom = 2;
    TaskID next_task_id = 1;

    inline Project(Workspace & w, nixtime unixtime_start)
        : workspace(w)
    {
        tasks[0] = std::make_unique<Task_TimePoint>(*this, 0, "Start", "Project beginning", unixtime_start);
    }

    inline nixtime get_unixtime_start() { return ((ganttry::Task_TimePoint*)(this->tasks[0].get()))->get_time_point(); }
    inline nixtime get_unixtime_end() { return get_unixtime_start() + duration_in_seconds(); }

    inline nixtime get_unixtime_earliest()
    {
        return get_unixtime_start() + get_unixtime_earliest_offset();
    }
    inline nixtime get_unixtime_earliest_offset()
    {
        nixtime_diff offset = std::numeric_limits<nixtime_diff>::max();
        for (const auto & task_pair : tasks)
            offset = std::min(offset, task_pair.second->get_unixtime_start_offset());
        return offset;
    }
    inline nixtime get_unixtime_latest()
    {
        nixtime_diff offset = std::numeric_limits<nixtime_diff>::lowest();
        for (const auto & task_pair : tasks)
            offset = std::max(offset, task_pair.second->get_unixtime_end_offset());
        return this->get_unixtime_start() + offset;
    }

    inline void set_unixtime_start(std::uint64_t s_since_epoch)
    {
        ((Task_TimePoint*)tasks[0].get())->set_time_point(s_since_epoch);
    }

    inline Task_Base * find_task(TaskID id) const
    {
        auto it = tasks.find(id);
        if (it == tasks.end())
            return nullptr;
        else
            return &*it->second;
    }

    inline TaskID add_task(int template_id, std::string name, float unit_count_forecast, float units_done_count)
    {
        auto [it,b] = tasks.insert({next_task_id , std::make_unique<Task_Templated>(*this, -1, name, "", unit_count_forecast, units_done_count, template_id)});
        if(!b)
            return -1;
        it->second->set_id(it->first);
        ++next_task_id;
        changed = true;
        return it->first;
    }
    inline TaskID add_time_point(std::string name)
    {
        auto [it,b] = tasks.insert({next_task_id , std::make_unique<Task_TimePoint>(*this, -1, name, "", get_unixtime_end())});
        if(!b)
            return -1;
        it->second->set_id(it->first);
        ++next_task_id;
        changed = true;
        return it->first;
    }
    inline TaskID add_subproject(Project & child)
    {
        // check for circular dependency
        if (child.contains(this))
            return -1;

        auto [it,b] = tasks.insert({next_task_id , std::make_unique<Task_SubProject>(*this, -1, "", "", 1, 0, child)});
        if(!b)
            return -1;
        it->second->set_id(it->first);
        ++next_task_id;
        changed = true;
        return it->first;
    }

    inline bool contains(const Project * const proj) const
    {
        if (proj == this)
            return true;
        for (const auto & p : tasks)
            if (p.second->contains(proj))
                return true;
        return false;
    }

    inline void add_task(std::unique_ptr<Task_Base> && t)
    {
        tasks[t->get_id()] = std::move(t);
    }

    inline void recalculate_start_offsets()
    {
        for (auto & p : tasks)
            p.second->recalculate_start_offset();
    }

    inline nixtime_diff duration_in_seconds() const
    {
        nixtime_diff end_offset = 0;
        for (auto & task : tasks)
            end_offset = std::max(end_offset, task.second->get_unixtime_end_offset());
        return end_offset;
    }

    TaskTemplate & get_task_template(TemplateID id);
    std::map<uint64_t, TaskTemplate> & get_task_templates();
    Workspace & get_workspace();
    bool remove_task(TaskID id);
};

} // namespace
