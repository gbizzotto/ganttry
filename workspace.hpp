
#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <map>

#include "types.hpp"
#include "project.hpp"

namespace ganttry
{

struct Project;

struct TaskTemplate
{
    uint64_t id;
    std::string name;
    std::string description;
    std::string units;
    float       default_UDM;
    float       average_UDM;
    float       default_material_cost_per_unit;
    float       average_material_cost_per_unit;
    float       default_manpower_cost_per_unit;
    float       average_manpower_cost_per_unit;
    bool use_avg = true;

    inline void set_id(TaskID v) { id = v; }
};

class Workspace
{
    bool changed = false;
    std::string filename;

    std::string name = "Default workspace";
    std::map<uint64_t,TaskTemplate> task_templates;
    std::vector<std::unique_ptr<Project>> projects;
    size_t current_project_idx = 0;
    uint64_t next_task_template_id = 0;

public:
    inline Workspace() {
        task_templates.insert({0, {0, "One per month" , "", "Units", 1.0/21 , 0, 0, 0.0, 2100.0       , 0.0, false}});
        task_templates.insert({1, {0, "One per week"  , "", "Units", 1.0/ 5 , 0, 0, 0.0,  500.0       , 0.0, false}});
        task_templates.insert({2, {0, "One per day"   , "", "Units", 1      , 0, 0, 0.0,  100.0/1     , 0.0, false}});
        task_templates.insert({3, {0, "One per hour"  , "", "Units", 8      , 0, 0, 0.0,  100.0/8     , 0.0, false}});
        task_templates.insert({4, {0, "One per minute", "", "Units", 8*60   , 0, 0, 0.0,  100.0/(8*60), 0.0, false}});
        add_new_project();
    }
    inline TaskTemplate & add_task_template( std::string name
                                           , std::string desc
                                           , std::string unit
                                           , float default_UDM
                                           , float average_UDM
                                           , float default_material_cost_per_unit
                                           , float average_material_cost_per_unit
                                           , float default_manpower_cost_per_unit
                                           , float average_manpower_cost_per_unit
                                           , bool use_avg
                                           )
    {
        typename decltype(task_templates)::iterator it;
        bool b;
        do {
            std::tie(it,b) = task_templates.insert({next_task_template_id, {next_task_template_id, name, desc, unit, default_UDM, average_UDM, default_material_cost_per_unit, average_material_cost_per_unit, default_manpower_cost_per_unit, average_manpower_cost_per_unit, use_avg}});
            ++next_task_template_id;
        } while (!b);
        return it->second;
    }

    inline Project & add_new_project()
    {
        projects.push_back(std::make_unique<Project>(*this, QDateTime::currentDateTime().currentSecsSinceEpoch()));
        current_project_idx = projects.size() - 1;
        changed = true;
        return *projects.back();
    }
    inline void add_project(std::unique_ptr<Project> & p)
    {
        projects.push_back(std::move(p));
    }

    inline Project & get_current_project() { return *projects[current_project_idx]; }
    inline size_t get_current_project_idx() const { return current_project_idx; }
    inline void set_current_project_idx(size_t idx) { current_project_idx = idx; }
    inline uint64_t get_next_task_template_id() const { return next_task_template_id; }
    inline void set_next_task_template_id(uint64_t n) { next_task_template_id = n; }

    inline TaskTemplate & get_task_template(TemplateID id) { return task_templates[id]; }

    inline void reset()
    {
        name = "";
        task_templates.clear();
        projects.clear();
        changed = false;
    }

    Project * get_project_by_filename(std::string filename);

    inline const std::string & get_name    () const { return name    ; }
    inline const std::string & get_filename() const { return filename; }
    inline const auto & get_task_templates() const { return task_templates; }
    inline       auto & get_task_templates()       { return task_templates; }
    inline const auto & get_projects      () const { return projects; }
    inline       bool   get_changed       () const { return changed; }
    inline void set_filename     (std::string  f) { filename = f            ; changed = true; }
    inline void set_name         (std::string  n) { name     = n            ; changed = true; }
    inline void add_task_template(TaskTemplate t) { task_templates[t.id] = std::move(t); changed = true; }
    inline void set_changed      (bool         b) { changed  = b; }

};

} // namespace
