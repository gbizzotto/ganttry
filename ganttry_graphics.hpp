#ifndef GANTTRY_GRAPHICS_HPP
#define GANTTRY_GRAPHICS_HPP

#include <memory>
#include <tuple>
#include <vector>

#include <QGraphicsScene>
#include <QGraphicsRectItem>

#include "project.hpp"

namespace ganttry
{

class GanttGraphicsScene;

struct row_info
{
    int height;
    int depth;
    std::vector<std::tuple<TaskID,Project*>> project_tree_path;
    Task_Base * task;
    std::uint64_t unixime_start;
    std::uint64_t unixime_end;
};

struct DependencyHighlight
{
    TaskID parent_task_id;
    TaskID child_task_id;
    Project * proj;
};
inline bool operator!=(const DependencyHighlight & left, const DependencyHighlight & right)
{
    return false
        || left.parent_task_id != right.parent_task_id
        || left.child_task_id  != right.child_task_id
        || left.proj           != right.proj
        ;
}

class DatesGraphicsScene : public QGraphicsScene
{
    Project * project;
    std::vector<int> col_widths_;

public:
    inline DatesGraphicsScene(Project & p)
        : project(&p)
    {}

    void redraw();
    inline const std::vector<int> & column_widths() const { return col_widths_; }
    inline void set_project(Project * p) { project = p; }
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
};

class NamesGraphicsScene : public QGraphicsScene
{
    Q_OBJECT;

    Project * project;
    DatesGraphicsScene & dates_scene;
    GanttGraphicsScene * gantt_scene;
    std::vector<row_info> rows_info_;
    QGraphicsRectItem * selection_rect = nullptr;

public:
    inline NamesGraphicsScene(Project & p, DatesGraphicsScene & dates_scene_)
        : project(&p)
        , dates_scene(dates_scene_)
    {}

    void set_gantt_scene(GanttGraphicsScene * g) { gantt_scene = g; }

    void redraw();
    inline const std::vector<row_info> & rows_info() const { return rows_info_; }
    inline const row_info & get_row_info(std::uint64_t idx) { return rows_info_[idx]; }
    void update_selected_row(int row, int total_height);
    inline void set_project(Project * p) { project = p; }

    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;

signals:
    void newRow();
};

class GanttGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

    Project * project;
    int selected_row_id = -1;
    NamesGraphicsScene & names_scene;
    DatesGraphicsScene & dates_scene;
    QGraphicsRectItem * selection_rect = nullptr;

    bool is_dragging = false;
    QGraphicsPathItem * drag_path = nullptr;
    QGraphicsRectItem * highlighted_from = nullptr;
    QGraphicsRectItem * highlighted_to  = nullptr;

    DependencyHighlight highlighted_dependency;

public:
    inline GanttGraphicsScene(Project & p, NamesGraphicsScene & names_, DatesGraphicsScene & dates_)
        : project(&p)
        , names_scene(names_)
        , dates_scene(dates_)
    {
        names_scene.set_gantt_scene(this);
    }

    inline bool set_highlighted_dependency(decltype(highlighted_dependency) d)
    {
        bool result = highlighted_dependency != d;
        highlighted_dependency = d;
        return result;
    }

    void redraw();
    void redraw_vlines();
    void row_left_clicked(int y);
    void updateSelection(int row_id);
    void updateSelection(int row_id, int total_height);
    void scroll_if_needed(QPointF scene_up_pos);
    QGraphicsPathItem * draw_arrow(QPointF from, QPointF to, QPen pen);
    QGraphicsRectItem * create_highlight_bar(float scene_y);
    std::tuple<int,int> get_item_id(int scene_y);
    int get_selected_row_id() const { return selected_row_id; }
    void unselect_row();
    std::tuple<std::uint32_t,uint32_t> get_bar_pixel_coords(const row_info & info);
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent) override;

    inline void set_project(Project * p) { project = p; }

signals:
    void selectionChanged(int old_row_id, int new_row_id);
    void newDependency();
};

} // namespace

#endif // GANTTRY_GRAPHICS_HPP
