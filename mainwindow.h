#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTreeWidgetItem>

#include "workspace.hpp"
#include "project.hpp"
#include "ganttry_graphics.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


#define MAX_RECENT 20


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    bool eventFilter(QObject *object, QEvent *event);
    void resizeEvent(QResizeEvent* event);

    void updateTaskFromInput();
    void populate_template_combobox();
    void save_workspace();
    void load_workspace(QString filename);
    void save_project(ganttry::Project & project);
    void load_project(ganttry::Project & project, QString filename);
    void refresh_workspace_tree();
    void project_changed();
    void add_open_recent(const QString & pathName);

private slots:

    void on_taskSelectionChanged_triggered(int old_row_id, int new_row_id);
    void on_newRow_triggered();
    void on_newDependency_triggered();

    void on_nameLineEdit_editingFinished();

    void on_unitsForecastLineEdit_editingFinished();

    void on_unitsDoneLineEdit_editingFinished();

    void on_descriptionTextEdit_textChanged();

    void on_tempateComboBox_currentIndexChanged(int index);

    void on_workspaceActionNew_triggered();

    void on_workspaceActionSave_triggered();

    void on_projectActionNew_triggered();

    void on_workspaceActionTemplates_triggered();

    void on_workspaceActionLoad_triggered();

    void on_workspaceActionSaveAs_triggered();

    void on_projectActionSave_triggered();

    void on_workspaceTreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);

    void on_workspaceTreeWidget_itemChanged(QTreeWidgetItem *item, int column);

    void on_zoomSlider_valueChanged(int value);

private:
    Ui::MainWindow *ui;
    std::unique_ptr<ganttry::Workspace> workspace;
    ganttry::NamesGraphicsScene names_scene;
    ganttry::DatesGraphicsScene dates_scene;
    ganttry::GanttGraphicsScene gantt_scene;
    bool displaying = false;
};
#endif // MAINWINDOW_H
