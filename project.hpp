
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
    inline auto & get_unixtime_start_offset() const { return unixtime_start_offset; }
    inline auto & get_parent_tasks         () const { return parent_tasks         ; }
    inline auto & get_children_tasks       () const { return children_tasks       ; }

    void set_id         (TaskID      v);
    void set_name       (std::string v);
    void set_description(std::string v);

    void set_unit_count_forecast(float forecast);
    void set_units_done_count(float done);
    void set_unixtime_start_offset(std::uint64_t v);

    //std::uint64_t unixtime_start() const;

    inline uint64_t unixtime_end_offset() const { return unixtime_start_offset + duration_in_seconds(); }
    virtual uint64_t duration_in_seconds() const;
    bool add_child_task(DependencyType d, Task_Base & t);
    void recalculate_start_offset();
    void children_recalculate_start_offset();
    bool find_descendent(TaskID id);

    inline virtual int get_template_id() const { return -1; }
    inline virtual bool is_recursive() const { return false; }
    inline virtual Project * get_child() { return nullptr; }
    virtual void set_template_id(int) {}; // do nothing
    virtual float duration_in_days() const = 0;
    virtual bool contains(const Project * const proj) const = 0;
    virtual std::string to_json(TaskID tid) const = 0;
    virtual std::string get_full_display_name() const = 0;
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
    virtual uint64_t duration_in_seconds() const override;
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
    TaskID next_task_id = 0;
    std::uint64_t unixtime_start;

    inline Project(Workspace & w, std::uint64_t unixtime_start_)
        : workspace(w)
        , unixtime_start(unixtime_start_)
    {}

    inline std::uint64_t get_unixtime_start() const { return unixtime_start; }
    inline std::uint64_t get_unixtime_end() const { return unixtime_start + duration_in_seconds(); }

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
        tasks.insert({t->get_id(), std::move(t)});
    }

    inline void recalculate_start_offsets()
    {
        for (auto & p : tasks)
            p.second->recalculate_start_offset();
    }

    inline std::uint64_t duration_in_seconds() const
    {
        std::uint64_t end_offset = 0;
        for (auto & task : tasks)
            end_offset = std::max(end_offset, task.second->unixtime_end_offset());
        return end_offset;
    }

    TaskTemplate & get_task_template(TemplateID id);
    std::map<uint64_t, TaskTemplate> & get_task_templates();
    Workspace & get_workspace();
    bool remove_task(TaskID id);
};

} // namespace
