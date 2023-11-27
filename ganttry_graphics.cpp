
#include <algorithm>
#include <cmath>
#include <tuple>

#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QDateTime>
#include <QTime>
#include <QList>
#include <QScrollBar>
#include <QPainterPath>
#include <QVector2D>
#include <QTransform>
#include <QMessageBox>

#include "ganttry_graphics.hpp"

namespace ganttry
{

void NamesGraphicsScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu(event->widget());
    QAction * action_add_task = menu.addAction("Add Task");
    QAction * action_add_time_point = menu.addAction("Add Time point");
    QMenu * action_menu_project = menu.addMenu("Add Project as Task");
    QAction * action_delete_task = menu.addAction("Delete Task");
    std::vector<std::pair<QAction*,Project&>> projects_actions;
    projects_actions.reserve(project->workspace.get_projects().size());
    for(auto & proj : project->workspace.get_projects())
        if ( ! proj->contains(this->project))
            projects_actions.emplace_back(action_menu_project->addAction(QString::fromStdString(proj->name)), *proj);

    int row_id, dummy;
    std::tie(row_id, dummy) = gantt_scene->get_item_id(event->scenePos().y());
    // en/disable delete task
    TaskID task_id = [&]() -> TaskID
        {
            bool enable_it = (row_id != -1 && rows_info()[row_id].project_tree_path.size() == 1);
            action_delete_task->setEnabled(enable_it);
            if (enable_it)
                return rows_info()[row_id].task->get_id();
            else
                return -1;
        }();

    if (task_id == 0)
        action_delete_task->setEnabled(false);

    auto action = menu.exec(event->screenPos());
    if (action == action_add_task)
    {
        [[maybe_unused]] int id = project->add_task(0, "New task", 1, 0);
        emit newRow();
        gantt_scene->updateSelection(rows_info_.size()-1);
    }
    else if (action == action_add_time_point)
    {
        [[maybe_unused]] int id = project->add_time_point("New time point");
        emit newRow();
        gantt_scene->updateSelection(rows_info_.size()-1);
    }
    else if (action == action_delete_task)
    {
        project->remove_task(task_id);
        gantt_scene->unselect_row();
        redraw();
        gantt_scene->redraw();
    }
    else // something in submenu
    {
        for(size_t i=0 ; i<projects_actions.size() ; i++)
            if (action == projects_actions[i].first)
            {
                [[maybe_unused]] TaskID id = project->add_subproject(projects_actions[i].second);
                emit newRow();
                gantt_scene->updateSelection(rows_info_.size()-1);
                break;
            }
    }
}
void DatesGraphicsScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
}
void GanttGraphicsScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
}

void NamesGraphicsScene::redraw()
{
    static const int indent = 20;

    clear();
    invalidate(0,0,width(),height());
    setSceneRect(0,0,1,1);
    selection_rect = nullptr;

    rows_info_.clear();

    if (project->tasks.empty())
        return;

    int width = this->views()[0]->viewport()->width() - 1;
    qreal total_height(0);

    // text
    std::function<void(ganttry::Project &, int, std::vector<std::tuple<TaskID,Project*>>, uint64_t)> redraw_project;
    redraw_project = [&](ganttry::Project & project, int depth, std::vector<std::tuple<TaskID,Project*>> project_tree_path, std::uint64_t base_start_time)
        {
            for (auto & task : project.tasks)
            {
                std::uint64_t nx_start_time = base_start_time + task.second->get_unixtime_start_offset();
                std::uint64_t nx_end_time   = nx_start_time + task.second->duration_in_seconds();

                QFont font = this->font();
                if (depth > 0) // subtasks with smaller font
                    font.setPointSize(font.pointSize()-2);
                QGraphicsTextItem * item = this->addText(QString::fromStdString(task.second->get_full_display_name()), font);
                item->setPos(depth*indent, total_height);
                rows_info_.push_back(row_info{(int)item->boundingRect().height(), depth, project_tree_path, task.second.get(), nx_start_time, nx_end_time});
                if (depth > 0) // gray subtasks
                    this->addRect(0, total_height, width, (int)item->boundingRect().height(), QPen(QColor(0,0,0,20)),QBrush(QColor(0,0,0,20)));
                total_height += item->boundingRect().height();

                if (task.second->is_recursive())
                {
                    Project & subproject = *task.second->get_child();
                    std::vector<std::tuple<TaskID,Project*>> project_tree_path_copy = project_tree_path;
                    project_tree_path_copy.push_back({task.second->get_id(), &subproject});
                    redraw_project(*task.second->get_child(), depth+1, project_tree_path_copy, base_start_time + task.second->get_unixtime_start_offset());
                }
            }
        };
    redraw_project(*project, 0, {{0,project}}, project->get_unixtime_start());

    //horizontal lines
    {
        setSceneRect(0, 0, width, total_height);
        int total_height = 0;
        for (const auto & row_info : rows_info_)
        {
            this->addLine(0, total_height, width, total_height, QPen(QColor(200,200,200,255)));
            total_height += row_info.height;
        }
        this->addLine(0, total_height, width, total_height, QPen(QColor(200,200,200,255)));
    }

    // selection
    if (gantt_scene->get_selected_row_id() != -1)
    {
        int total_height = std::accumulate(rows_info_.begin(), rows_info_.begin()+gantt_scene->get_selected_row_id(), 0, [](int v, const row_info & left){ return v+left.height; });

        // update selection
        //if (selection_rect) {
        //    this->removeItem(selection_rect);
        //    selection_rect = nullptr;
        //}
        selection_rect = this->addRect(0, total_height, width, rows_info_[gantt_scene->get_selected_row_id()].height, QPen(QColor(0,0,0,55)),QBrush(QColor(0,0,0,55)));
    }
}
void NamesGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    // get point coords
    Qt::MouseButtons buttons = mouseEvent->buttons();
    QPointF scene_point = [&]()
        {
            if (buttons.testFlag(Qt::MouseButton::LeftButton))
                return mouseEvent->buttonDownScenePos(Qt::MouseButton::LeftButton);
            else if (buttons.testFlag(Qt::MouseButton::RightButton))
                return mouseEvent->buttonDownScenePos(Qt::MouseButton::RightButton);
            else
                return QPointF{0.0, 0.0};
        }();

    if (scene_point.x() == 0 && scene_point.y() == 0)
        return;

    gantt_scene->row_left_clicked(scene_point.y());
}
void NamesGraphicsScene::update_selected_row(int row_id, int total_height)
{
    if (selection_rect) {
        this->removeItem(selection_rect);
        selection_rect = nullptr;
    }

    if (row_id >= 0 && row_id < (int)rows_info_.size())
        selection_rect = this->addRect(0, total_height-rows_info_[row_id].height, this->views()[0]->viewport()->width()-1, rows_info_[row_id].height, QPen(QColor(0,0,0,55)),QBrush(QColor(0,0,0,55)));
}

template<typename T>
std::vector<T> operator+(const std::vector<T> & v, const T & t)
{
    std::vector<T> v2 = v;
    v2.push_back(t);
    return v2;
}

void DatesGraphicsScene::redraw()
{
    clear();
    invalidate(0,0,width(),height());
    setSceneRect(0,0,1,1);
    col_widths_.clear();

    if (project->tasks.empty())
        return;

    qreal total_width(0);

    QDateTime start_datetime = QDateTime::fromSecsSinceEpoch(project->get_unixtime_start());
    QDateTime   end_datetime = QDateTime::fromSecsSinceEpoch(project->get_unixtime_end());

    if (project->zoom == 2)
    {
        for (auto day = start_datetime.date() ; day <= end_datetime.date() ; day = day.addDays(1))
        {
            QGraphicsTextItem * item = this->addText(day.toString("yyyy-MM-dd"));
            item->setRotation(270.0);
            item->setPos(total_width, this->views()[0]->viewport()->height());

            col_widths_.push_back(item->boundingRect().height());
            total_width += item->boundingRect().height();
        }
    }
    else if (project->zoom == 1)
    {
        QDateTime hour(start_datetime.date(), QTime(start_datetime.time().hour(), 0, 0));
        QDateTime latest_hour(end_datetime.date(), QTime(end_datetime.time().hour(), 0, 0));
        for ( ; hour <= latest_hour ; hour = hour.addSecs(3600))
        {
            QGraphicsTextItem * item = this->addText(hour.toString("MM-dd HH:mm"));
            item->setRotation(270.0);
            item->setPos(total_width, this->views()[0]->viewport()->height());

            col_widths_.push_back(item->boundingRect().height());
            total_width += item->boundingRect().height();
        }
    }

    {
        // vertical lines
        int height = this->views()[0]->viewport()->height() - 1;
        setSceneRect(0, 0, total_width, height);
        int total_width = 0;
        for (const auto & width : column_widths())
        {
            this->addLine(total_width, 0, total_width, height, QPen(QColor(200,200,200,255)));
            total_width += width;
        }
        this->addLine(total_width, 0, total_width, height, QPen(QColor(200,200,200,255)));
    }
}

std::uint32_t GanttGraphicsScene::get_pixel_coord(nixtime t) const
{
    QDateTime project_begin_date;
    project_begin_date.setSecsSinceEpoch(project->get_unixtime_start());

    QDateTime task_begin;
    task_begin.setSecsSinceEpoch(t);

    if (project->zoom == 2)
    {
        int begin_day_idx = project_begin_date.daysTo(task_begin);

        return std::accumulate(dates_scene.column_widths().begin(), dates_scene.column_widths().begin()+begin_day_idx, 0)
               + dates_scene.column_widths()[begin_day_idx] * QDateTime(task_begin.date(), QTime(0,0,0)).secsTo(task_begin) / 86400
             ;
    }
    else if (project->zoom == 1)
    {
        int begin_day_idx = project_begin_date.secsTo(task_begin) / 3600;

        return std::accumulate(dates_scene.column_widths().begin(), dates_scene.column_widths().begin()+begin_day_idx, 0)
              + dates_scene.column_widths()[begin_day_idx] * QDateTime(task_begin.date(), QTime(task_begin.time().hour(),0,0)).secsTo(task_begin) / 3600
            ;
    }
    return 0;
}

std::tuple<std::uint32_t,std::uint32_t> GanttGraphicsScene::get_bar_pixel_coords(const row_info & info) const
{
    QDateTime project_begin_date;
    project_begin_date.setSecsSinceEpoch(project->get_unixtime_start());

    QDateTime task_begin;
    QDateTime task_end;
    task_begin.setSecsSinceEpoch(info.unixime_start);
    task_end  .setSecsSinceEpoch(info.unixime_end  );

    if (project->zoom == 2)
    {
        int begin_day_idx = project_begin_date.daysTo(task_begin);
        int   end_day_idx = project_begin_date.daysTo(task_end);

        return {  std::accumulate(dates_scene.column_widths().begin(), dates_scene.column_widths().begin()+begin_day_idx, 0)
                 + dates_scene.column_widths()[begin_day_idx] * QDateTime(task_begin.date(), QTime(0,0,0)).secsTo(task_begin) / 86400
               , std::accumulate(dates_scene.column_widths().begin(), dates_scene.column_widths().begin()+end_day_idx, 0)
                 + dates_scene.column_widths()[end_day_idx] * QDateTime(task_end.date(), QTime(0,0,0)).secsTo(task_end) / 86400
               };
    }
    else if (project->zoom == 1)
    {
        int begin_day_idx = project_begin_date.secsTo(task_begin) / 3600;
        int   end_day_idx = project_begin_date.secsTo(task_end) / 3600;

        return {  std::accumulate(dates_scene.column_widths().begin(), dates_scene.column_widths().begin()+begin_day_idx, 0)
                 + dates_scene.column_widths()[begin_day_idx] * QDateTime(task_begin.date(), QTime(task_begin.time().hour(),0,0)).secsTo(task_begin) / 3600
               , std::accumulate(dates_scene.column_widths().begin(), dates_scene.column_widths().begin()+end_day_idx, 0)
                 + dates_scene.column_widths()[end_day_idx] * QDateTime(task_end.date(), QTime(task_end.time().hour(),0,0)).secsTo(task_end) / 3600
               };
    }
    return {};
}

void GanttGraphicsScene::redraw_vlines()
{
    redraw();
}
void GanttGraphicsScene::rotate_45(QGraphicsRectItem * item)
{
    QPointF offset = item->sceneBoundingRect().center();
    QTransform transform;
    transform.translate(offset.x(),offset.y());
    transform.rotate(45);
    transform.translate(-offset.x(),-offset.y());
    item->setTransform(transform);
    this->update();
}
void GanttGraphicsScene::redraw()
{
    clear();
    invalidate(0,0,width(),height());
    setSceneRect(0, 0
                ,std::max(dates_scene.views()[0]->viewport()->width (), (int)dates_scene.width() )
                ,std::max(names_scene.views()[0]->viewport()->height(), (int)names_scene.height())
                );
    selection_rect = nullptr;
    drag_path = nullptr;
    highlighted_from = nullptr;
    highlighted_to   = nullptr;

    int width = std::max((int)dates_scene.width(), (int)this->width()) - 1;

    // draw grid
    if (names_scene.rows_info().size() > 0)
    {
        //horizontal lines
        int total_height = 0;
        for (const auto & row_info : names_scene.rows_info())
        {
            this->addLine(0, total_height, width, total_height, QPen(QColor(200,200,200,255)));

            // gray subtasks
            if (row_info.project_tree_path.size() > 1)
                this->addRect(0, total_height, width, row_info.height, QPen(QColor(0,0,0,20)),QBrush(QColor(0,0,0,20)));

            total_height += row_info.height;
        }
        this->addLine(0, total_height, width, total_height, QPen(QColor(200,200,200,255)));
    }
    qreal total_width(0);
    if (dates_scene.column_widths().size() > 0)
    {
        // vertical lines
        int height = std::max((int)names_scene.height(), (int)this->height()) - 1;
        for (const auto & width : dates_scene.column_widths())
        {
            this->addLine(total_width, 0, total_width, height, QPen(QColor(200,200,200,255)));
            total_width += width;
        }
        this->addLine(total_width, 0, total_width, height, QPen(QColor(200,200,200,255)));
    }

    // for dependency arrows, later
    // indices should match that of project->tasks
    std::vector<std::tuple<int,int,int>> bars_coords; // y,x1,x2

    // draw bars
    qreal total_height(0);
    for (const row_info & info : names_scene.rows_info())
    {
        // project starting point
        if (info.task->is_relative() == false)
        {
            auto pixel_pos = get_pixel_coord(info.unixime_start);
            QGraphicsRectItem * item = this->addRect(pixel_pos-4, total_height+info.height/2 - 4, 8, 8, QPen(QColor(0,0,0,255)), QBrush(QColor(0,0,0,255*(info.task->get_id() == 0))));
            rotate_45(item);
            bars_coords.push_back({total_height+info.height/2, pixel_pos, pixel_pos});
            total_height += info.height;
            continue;
        }

        auto [bar_pixel_begin, bar_pixel_end] = get_bar_pixel_coords(info);
        [[maybe_unused]] QGraphicsRectItem * item;
        if (info.task->is_recursive())
            item = this->addRect(bar_pixel_begin, total_height+info.height/2-2, bar_pixel_end-bar_pixel_begin, 4, QPen(QColor(0,0,0,0)), QBrush(QColor(0,0,0,255)));
        else
            item = this->addRect(bar_pixel_begin, total_height+4              , bar_pixel_end-bar_pixel_begin, info.height-8, QPen(QColor(0,0,0,0)), QBrush(QColor(100,149,237,255)));
        bars_coords.push_back({total_height+info.height/2, bar_pixel_begin, bar_pixel_end});
        total_height += info.height;
    }

    // dependency arrows
    int i=0;
    QPen normal_pen(QColor(255,0,0,128));
    normal_pen.setWidth(2);
    QPen highlight_pen(QColor(100,237,149,255));
    highlight_pen.setWidth(3);
    std::function<void(std::vector<std::tuple<TaskID,Project*>>)> draw_arrows;
    draw_arrows = [&](std::vector<std::tuple<TaskID,Project*>> project_tree_path)
        {
            auto [task_id,proj] = project_tree_path.back();

            for (const auto & p : proj->tasks)
            {
                for (const auto & dependency : p.second->get_children_tasks())
                {
                    auto it = std::find_if(names_scene.rows_info().begin(), names_scene.rows_info().end(), [&dependency,&project_tree_path](const row_info & ri)
                        {
                            return project_tree_path == ri.project_tree_path  &&  ri.task != nullptr  &&  dependency.task_id == ri.task->get_id();
                        });
                    if (it == names_scene.rows_info().end())
                        continue;
                    auto task_idx = std::distance(names_scene.rows_info().begin(), it);

                    QPen & pen = [&]() ->QPen& {
                            return (proj == highlighted_dependency.proj
                                && p.first == highlighted_dependency.parent_task_id
                                && dependency.task_id == highlighted_dependency.child_task_id
                                ) ? highlight_pen : normal_pen;
                        }();

                    QGraphicsPathItem * arrow = nullptr;
                    if (dependency.type == ganttry::DependencyType::BeginAfter)
                        arrow = this->draw_arrow(QPointF(std::get<2>(bars_coords[i       ]), std::get<0>(bars_coords[i       ]))
                                                ,QPointF(std::get<1>(bars_coords[task_idx]), std::get<0>(bars_coords[task_idx]))
                                                ,pen);
                    else if (dependency.type == ganttry::DependencyType::BeginWith)
                        arrow = this->draw_arrow(QPointF(std::get<1>(bars_coords[i       ]), std::get<0>(bars_coords[i       ]))
                                                ,QPointF(std::get<1>(bars_coords[task_idx]), std::get<0>(bars_coords[task_idx]))
                                                ,pen);
                    else if (dependency.type == ganttry::DependencyType::EndBefore)
                        arrow = this->draw_arrow(QPointF(std::get<1>(bars_coords[i       ]), std::get<0>(bars_coords[i       ]))
                                                ,QPointF(std::get<2>(bars_coords[task_idx]), std::get<0>(bars_coords[task_idx]))
                                                ,pen);
                    else if (dependency.type == ganttry::DependencyType::EndWith)
                        arrow = this->draw_arrow(QPointF(std::get<2>(bars_coords[i       ]), std::get<0>(bars_coords[i       ]))
                                                ,QPointF(std::get<2>(bars_coords[task_idx]), std::get<0>(bars_coords[task_idx]))
                                                ,pen);
                    if (arrow)
                        arrow->setFlag(QGraphicsItem::GraphicsItemFlag::ItemIsSelectable);
                }
                i++;
                if (p.second->is_recursive())
                    draw_arrows(project_tree_path + std::make_tuple(p.second->get_id(), p.second->get_child()));
            }
        };
    draw_arrows({{0,project}});


    // selection
    if (selected_row_id != -1)
    {
        int total_height = std::accumulate(names_scene.rows_info().begin(), names_scene.rows_info().begin()+selected_row_id, 0, [](int v, const row_info & left){ return v+left.height; });

        // update selection
        //if (selection_rect) {
        //    this->removeItem(selection_rect);
        //    selection_rect = nullptr;
        //}
        selection_rect = this->addRect(0, total_height, width, names_scene.rows_info()[selected_row_id].height, QPen(QColor(0,0,0,55)),QBrush(QColor(0,0,0,55)));
    }
}

void GanttGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    // check mouse button
    Qt::MouseButtons buttons = mouseEvent->buttons();
    if (buttons.testFlag(Qt::MouseButton::LeftButton))
    {
        // get point coords
        QPointF scene_point = mouseEvent->buttonDownScenePos(Qt::MouseButton::LeftButton);
        if (scene_point.x() == 0 && scene_point.y() == 0)
            return;
        row_left_clicked(scene_point.y());
    }
    else if (buttons.testFlag(Qt::MouseButton::RightButton))
    {
        // get point coords
        QPointF scene_point = mouseEvent->buttonDownScenePos(Qt::MouseButton::RightButton);
        if (scene_point.x() == 0 && scene_point.y() == 0)
            return;
        // check the item is not in a subproject
        // get tasks
        int task_1_id;
        int dummy;
        std::tie(task_1_id,dummy) = get_item_id(scene_point.y());
        if (task_1_id == -1)
            return;
        const ganttry::row_info & info = names_scene.rows_info()[task_1_id];
        if (info.project_tree_path.size() == 1)
        {
            is_dragging = true;
            highlighted_from = create_highlight_bar(scene_point.y());
        }
    }
}

void GanttGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if ( ! is_dragging)
        return;

    is_dragging = false;

    // cleanup arrow and selected bars
    if (drag_path) {
        this->removeItem(drag_path);
        free(drag_path);
        drag_path = nullptr;
    }
    if (highlighted_from) {
        this->removeItem(highlighted_from);
        free(highlighted_from);
        highlighted_from = nullptr;
    }
    if (highlighted_to) {
        this->removeItem(highlighted_to);
        free(highlighted_to);
        highlighted_to = nullptr;
    }

    // get point coords
    QPointF down_pos = mouseEvent->buttonDownScenePos(Qt::MouseButton::RightButton);
    QPointF up_pos = mouseEvent->scenePos();

    // guard against creating items over the border that would expande the scene
    if (up_pos.x() < 0 || up_pos.y() < 0)
        return;
    if (up_pos.x() >= this->width()-1 || up_pos.y() >= this->height()-1)
        return;
    // get point coords
    int task_down_id;
    int task_up_id;
    int dummy;
    std::tie(task_down_id,dummy) = get_item_id(down_pos.y());
    std::tie(task_up_id,dummy) = get_item_id(up_pos.y());
    // check both tasks are valid
    if ((task_down_id == -1) | (task_up_id == -1))
        return;
    // check we're not pointing from and to the same object
    if (task_down_id == task_up_id)
        return;
    // check the item is not a subproject
    const ganttry::row_info & info = names_scene.rows_info()[task_up_id];
    if (info.project_tree_path.size() > 1)
        return;

    QMenu menu(this->views()[0]);
    auto action_begin_after = menu.addAction("Begin after");
    auto action_begin_with  = menu.addAction("Begin with");
    auto action_end_before  = menu.addAction("End before");
    auto action_end_with    = menu.addAction("End with");
    auto action = menu.exec(mouseEvent->screenPos());

    // get tasks
    std::tie(task_down_id,dummy) = get_item_id(down_pos.y());
    std::tie(task_up_id,dummy) = get_item_id(up_pos.y());

    ganttry::Task_Base & task_down = *names_scene.rows_info()[task_down_id].task;
    ganttry::Task_Base & task_up   = *names_scene.rows_info()[task_up_id].task;

    if (task_up.find_descendent(task_down.get_id()))
    {
        QMessageBox::warning(this->views()[0], "No", "Circular dependency");
        return;
    }

    auto [got_action, dep] = [&]() -> std::tuple<bool,DependencyType>
        {
                 if (action == action_begin_after) return {true, DependencyType::BeginAfter};
            else if (action == action_end_before ) return {true, DependencyType::EndBefore };
            else if (action == action_end_with   ) return {true, DependencyType::EndWith   };
            else if (action == action_begin_with ) return {true, DependencyType::BeginWith };
            return {false,DependencyType::BeginAfter};
        }();

    if (got_action && task_down.add_child_task(dep, task_up) && task_up.add_parent_task(dep, task_down))
    {
        project->changed = true;
        emit newDependency();
    }
}

void GanttGraphicsScene::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if ( ! is_dragging)
        return;

    Qt::MouseButtons buttons = mouseEvent->buttons();
    if ( ! buttons.testFlag(Qt::MouseButton::RightButton))
        return;

    // get point coords
    QPointF scene_down_pos = mouseEvent->buttonDownScenePos(Qt::MouseButton::RightButton);
    QPointF scene_up_pos = mouseEvent->scenePos();

    scroll_if_needed(scene_up_pos);

    // remove old highlight and arrow
    if (highlighted_to) {
        this->removeItem(highlighted_to);
        free(highlighted_to);
        highlighted_to = nullptr;
    }
    if (highlighted_from)
        this->removeItem(highlighted_from);
    if (drag_path) {
        this->removeItem(drag_path);
        free(drag_path);
        drag_path = nullptr;
    }

    // guard against creating items over the border that would expande the scene
    if (scene_up_pos.x() < 0 || scene_up_pos.y() < 0)
        return;
    if (scene_up_pos.x() >= this->width()-1 || scene_up_pos.y() >= this->height()-1)
        return;

    // get point coords
    int task_down_id;
    int task_up_id;
    int dummy;
    std::tie(task_down_id,dummy) = get_item_id(scene_down_pos.y());
    std::tie(task_up_id,dummy) = get_item_id(scene_up_pos.y());
    // check we're not pointing from and to the same object
    if (task_down_id == task_up_id)
        return;
    // check the item is not IN a subproject
    const ganttry::row_info & info = names_scene.rows_info()[task_up_id];
    if (info.project_tree_path.size() > 1)
        return;
    // check the item is not a time point
    if ( ! info.task || ! info.task->is_relative())
        return;

    // new highlight
    highlighted_to = create_highlight_bar(scene_up_pos.y());
    this->addItem(highlighted_to);
    this->addItem(highlighted_from); // readd removed origin highlight

    // draw the drag arrow
    QPen pen(QColor(0,0,0,100));
    pen.setWidth(3);
    QGraphicsPathItem * new_drag_path = draw_arrow(scene_down_pos, scene_up_pos, pen);
    if (new_drag_path)
        drag_path = new_drag_path;
}

std::vector<std::string> split(std::string str, std::string delim)
{
    std::vector<std::string> result;
    while(str.size())
    {
        auto pos = str.find(delim);
        if (pos != std::string::npos)
        {
            result.push_back(str);
            break;
        }
        result.push_back(str.substr(0, pos));
        str = str.substr(pos+1);
    }
    return result;
}

QGraphicsRectItem * GanttGraphicsScene::create_highlight_bar(float scene_y)
{
    auto [row_id,total_height] = get_item_id(scene_y);
    if (row_id == -1 || row_id >= (int)names_scene.rows_info().size())
        return nullptr;

    const row_info & info = names_scene.rows_info()[row_id];
    auto [bar_pixel_begin, bar_pixel_end] = get_bar_pixel_coords(info);

    QGraphicsRectItem * result = new QGraphicsRectItem(bar_pixel_begin, total_height-info.height+4, bar_pixel_end-bar_pixel_begin, info.height-8);
    result->setPen(QPen(QColor(0,0,0,255)));
    result->setBrush(QBrush(QColor(237,149,100,255)));
    return result;
}

QGraphicsPathItem * GanttGraphicsScene::draw_arrow(QPointF from, QPointF to, QPen pen)
{
    // guard against creating items over the border that would expande the scene
    if ( to.x() < 3 || to.x() > width ()-3
       ||to.y() < 3 || to.y() > height()-3)
    {
        return nullptr;
    }

    QPainterPath path(QPointF(from.x(), from.y()));
    path.lineTo(to.x(), to.y());

    QTransform rotation1 = QTransform().rotate(30);
    QTransform rotation2 = QTransform().rotate(-30);
    QVector2D vec1(rotation1.map(QPointF(from.x()-to.x(), from.y()-to.y())));
    QVector2D vec2(rotation2.map(QPointF(from.x()-to.x(), from.y()-to.y())));

    vec1.normalize();
    vec1 *= 10.0;
    vec2.normalize();
    vec2 *= 10.0;

    if (  to.x()+vec1.x() > 3 && to.x()+vec1.x() < width ()-4
       && to.y()+vec1.y() > 3 && to.y()+vec1.y() < height()-4)
    {
        path.lineTo(to.x()+vec1.x(), to.y()+vec1.y());
    }
    if (  to.x()+vec2.x() > 3 && to.x()+vec2.x() < width ()-4
       && to.y()+vec2.y() > 3 && to.y()+vec2.y() < height()-4)
    {
        path.moveTo(to.x(), to.y());
        path.lineTo(to.x()+vec2.x(), to.y()+vec2.y());
    }

    return this->addPath(path, pen);
}

void GanttGraphicsScene::scroll_if_needed(QPointF scene_up_pos)
{
    auto & view = *this->views()[0];
    QPointF scene_coords_top_left_corner     = view.mapToScene(0,0);
    QPointF scene_coords_bottom_right_corner = view.mapToScene(view.viewport()->width(), view.viewport()->height());
    if (scene_up_pos.x() > scene_coords_bottom_right_corner.x())
    {
        auto diff = scene_up_pos.x() - scene_coords_bottom_right_corner.x();
        is_dragging = false;
        view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->value() + 1+diff/50);
        is_dragging = true;
    }
    else if (scene_up_pos.x() < scene_coords_top_left_corner.x())
    {
        auto diff = scene_coords_top_left_corner.x() - scene_up_pos.x();
        is_dragging = false;
        view.horizontalScrollBar()->setValue(view.horizontalScrollBar()->value() - (1+diff/50));
        is_dragging = true;
    }

    if (scene_up_pos.y() > scene_coords_bottom_right_corner.y())
    {
        auto diff = scene_up_pos.y() - scene_coords_bottom_right_corner.y();
        is_dragging = false;
        view.verticalScrollBar()->setValue(view.verticalScrollBar()->value() + 1+diff/50);
        is_dragging = true;
    }
    else if (scene_up_pos.y() < scene_coords_top_left_corner.y())
    {
        auto diff = scene_coords_top_left_corner.y() - scene_up_pos.y();
        is_dragging = false;
        view.verticalScrollBar()->setValue(view.verticalScrollBar()->value() - (1+diff/50));
        is_dragging = true;
    }
}

std::tuple<int,int> GanttGraphicsScene::get_item_id(int scene_y)
{
    // get line/item
    int row_id = 0;
    int total_height = 0;
    for ( ; row_id<(int)names_scene.rows_info().size() ; row_id++)
    {
        total_height += names_scene.rows_info()[row_id].height;
        if (scene_y <= total_height)
            break;
    }
    if (row_id < (int)names_scene.rows_info().size())
        return {row_id,total_height};
    else
        return {-1,0};
}

void GanttGraphicsScene::row_left_clicked(int scene_y)
{
    auto [row_id,total_height] = get_item_id(scene_y);
    updateSelection(row_id, total_height);
}

void GanttGraphicsScene::updateSelection(int row_id)
{
    int total_height = std::accumulate(names_scene.rows_info().begin(), names_scene.rows_info().begin()+row_id+1, 0, [](int v, const row_info & right) { return v+right.height; });
    updateSelection(row_id, total_height);
}

void GanttGraphicsScene::updateSelection(int row_id, int total_height)
{
    // update selection
    if (selection_rect) {
        this->removeItem(selection_rect);
        selection_rect = nullptr;
    }

    if (row_id >= (int)names_scene.rows_info().size())
        row_id = -1;
    else if (row_id >= 0)
    {
        auto sel_width = std::max(this->views()[0]->viewport()->width(), (int)this->width()) - 1;
        selection_rect = this->addRect(0, total_height-names_scene.rows_info()[row_id].height, sel_width, names_scene.rows_info()[row_id].height, QPen(QColor(0,0,0,55)),QBrush(QColor(0,0,0,55)));
    }

    names_scene.update_selected_row(row_id, total_height);
    bool selection_changed = selected_row_id != (int)row_id;
    selected_row_id = row_id;
    if (selection_changed)
        emit selectionChanged(selected_row_id, (int)row_id);
}

void GanttGraphicsScene::unselect_row()
{
    if (selected_row_id != -1)
        emit selectionChanged(selected_row_id, -1);
    selected_row_id = -1;
}

}
