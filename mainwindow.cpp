#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "edittemplates.h"

#include <string>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QScrollBar>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>
#include <QMouseEvent>
#include <QSettings>

#include "project.hpp"
#include "workspace.hpp"
#include "ganttry_graphics.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , workspace(std::make_unique<ganttry::Workspace>())
    , names_scene(workspace->get_current_project(), dates_scene)
    , dates_scene(workspace->get_current_project())
    , gantt_scene(workspace->get_current_project(), names_scene, dates_scene)
{
    ui->setupUi(this);
    ui->names_view->setScene(&names_scene);
    ui->dates_view->setScene(&dates_scene);
    ui->gantt_view->setScene(&gantt_scene);

    auto dv = new QDoubleValidator();
    ui->unitsForecastLineEdit->setValidator(dv);
    ui->unitsDoneLineEdit->setValidator(dv);

    //ui->workspaceTreeView->setMaximumWidth (ui->names_view->geometry().width());
    //ui->workspaceTreeView->setMaximumHeight(ui->verticalLayout_2->geometry().height());
    //ui->workspaceTreeView->setGeometry(0, 0, ui->names_view->geometry().width(), ui->verticalLayout_2->geometry().height());

    QObject::connect(ui->gantt_view->verticalScrollBar(), SIGNAL(valueChanged(int)), ui->names_view->verticalScrollBar(), SLOT(setValue(int)));
    QObject::connect(ui->names_view->verticalScrollBar(), SIGNAL(valueChanged(int)), ui->gantt_view->verticalScrollBar(), SLOT(setValue(int)));

    QObject::connect(ui->gantt_view->horizontalScrollBar(), SIGNAL(valueChanged(int)), ui->dates_view->horizontalScrollBar(), SLOT(setValue(int)));
    QObject::connect(ui->dates_view->horizontalScrollBar(), SIGNAL(valueChanged(int)), ui->gantt_view->horizontalScrollBar(), SLOT(setValue(int)));

    QObject::connect(&gantt_scene, SIGNAL(selectionChanged(int,int)), this, SLOT(on_taskSelectionChanged_triggered(int,int)));
    QObject::connect(&gantt_scene, SIGNAL(newDependency()), this, SLOT(on_newDependency_triggered()));
    QObject::connect(&names_scene, SIGNAL(newRow()), this, SLOT(on_newRow_triggered()));

    ui->dates_view->installEventFilter(this);
    ui->names_view->installEventFilter(this);
    ui->dragHWidget->installEventFilter(this);
    ui->dragVWidget->installEventFilter(this);

    //ui->workspaceTreeWidget->resize(ui->names_view->geometry().width(), ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height());
    //while(ui->workspaceTreeWidget->geometry().height() != ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height())
    //{
    //    int diff = (ui->workspaceTreeWidget->geometry().height() > (ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height())) ? 1 : -1;
    //    auto tree_height = ui->workspaceTreeWidget->geometry().height();
    //    auto other_height = ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height();
    //    ui->dates_view->resize(ui->dates_view->geometry().width(), ui->dates_view->geometry().height() + diff);
    //    //ui->workspaceTreeWidget->setMinimumHeight(ui->zoomSlider->height() + ui->dates_view->height());
    //}
    //ui->workspaceTreeWidget->setMinimumWidth(ui->names_view->width());

    refresh_workspace_tree();
    populate_template_combobox();

    // load settings
    QSettings settings("ttt", "ganttry");
    int recent_count = settings.beginReadArray("recents");
    for(int i=0 ; i<recent_count ; i++)
    {
        settings.setArrayIndex(i);
        QString path = settings.value("recent").toString();
        bool has_same = false;
        for(int i = ui->menuOpenRecent_2->actions().size() - 1 ; i >= 0 ; i--)
            if (path == ui->menuOpenRecent_2->actions()[i]->text())
            {
                has_same = true;
                break;
            }
        if ( ! has_same)
            ui->menuOpenRecent_2->addAction(path)->setData(path);
    }
    // load most recent workspace
    if (recent_count > 0)
    {
        settings.setArrayIndex(0);
        QString filename = settings.value("recent").toString();
        this->load_workspace(filename);
    }
    settings.endArray();

    //QString path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/ganttry.db";
    //QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE"); //not dbConnection
    //db.setDatabaseName(path);
    //db.open();
    //QSqlQuery query;
    //query.exec("create table person "
    //          "(id integer primary key, "
    //          "firstname varchar(20), "
    //          "lastname varchar(30), "
    //          "age integer)");
    //db.commit();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    static int origin_x = 0;
    static int origin_y = 0;
    static int origin_w = 0;
    static int origin_h = 0;

    if (object == ui->dates_view && event->type() == QEvent::Resize)
    {
        ui->workspaceTreeWidget->resize(ui->names_view->geometry().width(), ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height());
        ui->workspaceTreeWidget->setMinimumHeight(ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height());

        dates_scene.redraw();
        names_scene.redraw();
        gantt_scene.redraw();
    }
    else if (object == ui->names_view && event->type() == QEvent::Resize)
    {
        ui->workspaceTreeWidget->resize(ui->names_view->geometry().width(), ui->zoomSlider->geometry().height() + ui->dates_view->geometry().height());
        ui->workspaceTreeWidget->setMinimumWidth(ui->names_view->geometry().width());

        dates_scene.redraw();
        names_scene.redraw();
        gantt_scene.redraw();
    }
    else if (object == ui->dragVWidget)
    {
        QMouseEvent * e = (QMouseEvent*)event;
        if (event->type() == QEvent::MouseButtonPress) {
            origin_y = e->windowPos().y();
            origin_h = ui->dates_view->minimumHeight();
        } else if (event->type() == QEvent::MouseMove) {
            ui->dates_view->setMinimumHeight(origin_h + e->windowPos().y() - origin_y);
        }
    }
    else if (object == ui->dragHWidget)
    {
        QMouseEvent * e = (QMouseEvent*)event;
        if (event->type() == QEvent::MouseButtonPress) {
            origin_x = e->windowPos().x();
            origin_w = ui->names_view->minimumWidth();
        } else if (event->type() == QEvent::MouseMove) {
            ui->names_view->setMinimumWidth(origin_w + e->windowPos().x() - origin_x);
        }
    }
    return false;
}


void MainWindow::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);

   QTimer::singleShot(1, [&](){
       dates_scene.redraw();
       names_scene.redraw();
       gantt_scene.redraw();
   });
}

void MainWindow::refresh_workspace_tree()
{
    ui->workspaceTreeWidget->clear();

    auto t = new QTreeWidgetItem(QStringList() << QString(workspace->get_changed() ? "*" : "") + QString::fromStdString(workspace->get_name()));
    t->setData(0, Qt::ItemDataRole::UserRole, (int)-1);
    t->setFlags(t->flags() | Qt::ItemIsEditable);
    for (size_t i=0 ; i<workspace->get_projects().size() ; i++)
    {
        auto & p = workspace->get_projects()[i];

        QTreeWidgetItem * child = new QTreeWidgetItem(QStringList() << QString(p->changed ? "*" : "") + QString::fromStdString(p->name));
        child->setData(0, Qt::ItemDataRole::UserRole, (int)i);
        child->setFlags(child->flags() | Qt::ItemIsEditable);
        t->addChild(child);
        if (i == workspace->get_current_project_idx())
        {
            QFont font = child->font(0);
            font.setBold(true);
            child->setFont(0, font);
        }
    }

    ui->workspaceTreeWidget->addTopLevelItem(t);
    t->setExpanded(true);
    ui->workspaceTreeWidget->resizeColumnToContents(0);
}

void MainWindow::on_newRow_triggered()
{
    ui->nameLineEdit->setFocus(Qt::FocusReason::ActiveWindowFocusReason);
    ui->nameLineEdit->selectAll();

    names_scene.redraw();
    dates_scene.redraw();
    gantt_scene.redraw();
}
void MainWindow::on_taskSelectionChanged_triggered(int old_row_id, int new_row_id)
{
    displaying = true;

    ui->taskInfoWidget->setEnabled(new_row_id != -1);

    auto clear = [&]()
    {
        ui->beginLabel->setText("");
        ui->  endLabel->setText("");
        ui->lastsLabel->setText("");
        ui->nameLineEdit->setText("");
        ui->descriptionTextEdit->setText("");
        ui->tempateComboBox->setCurrentText("");
        ui->unitsForecastLineEdit->setText("");
        ui->unitsDoneLineEdit->setText("");
        ui->progressBar->setValue(0);
    };

    if (new_row_id == -1)
    {
        clear();
    }
    else
    {
        const ganttry::row_info & info = names_scene.rows_info()[new_row_id];
        ui->taskInfoWidget->setEnabled(info.project_tree_path.size() == 1);
        if (info.project_tree_path.size() > 1)
        {
            clear();
        }
        else
        {
            ganttry::Task_Base * task = info.task;
            ui->beginLabel->setText(QDateTime::fromSecsSinceEpoch(info.unixime_start).toString("yyyy-MM-dd HH:mm"));
            ui->  endLabel->setText(QDateTime::fromSecsSinceEpoch(info.unixime_end  ).toString("yyyy-MM-dd HH:mm"));
            if (ui->zoomSlider->value() == 2)
                ui->lastsLabel->setText(QString::fromStdString(std::to_string((task->duration_in_seconds()) / 86400.0)) + " days");
            else if (ui->zoomSlider->value() == 1)
                ui->lastsLabel->setText(QString::fromStdString(std::to_string((task->duration_in_seconds()) / 3600.0)) + " hours ");
            ui->         nameLineEdit->       setText(QString::fromStdString(task->get_name       ()));
            ui->  descriptionTextEdit->       setText(QString::fromStdString(task->get_description()));
            ui->unitsForecastLineEdit->       setText(QString::fromStdString(std::to_string(task->get_unit_count_forecast())));
            ui->    unitsDoneLineEdit->       setText(QString::fromStdString(std::to_string(task->get_units_done_count   ())));
            ui->          progressBar->       setValue(100.0 * task->get_units_done_count() / task->get_unit_count_forecast());
            if (dynamic_cast<ganttry::Task_Templated*>(task) != nullptr)
            {
                int idx = ui->tempateComboBox->findData(QVariant::fromValue(dynamic_cast<ganttry::Task_Templated*>(task)->get_template_id()));
                ui->tempateComboBox->setCurrentIndex(idx);
            }
        }
    }

    displaying = false;
    return;
}


void MainWindow::updateTaskFromInput()
{
    if (displaying)
        return;

    int row_id = gantt_scene.get_selected_row_id();
    if (row_id == -1)
        return;

    auto task_it = workspace->get_current_project().tasks.find(row_id);
    if (task_it == workspace->get_current_project().tasks.end())
        return;

    ganttry::Task_Base & task = *task_it->second;

    float units_forecast = std::stof(ui->unitsForecastLineEdit->text().toStdString());
    float units_done     = std::stof(ui->unitsDoneLineEdit    ->text().toStdString());

    if (units_done > units_forecast)
        units_done = units_forecast;

    bool redraw = false
            || task.get_name               () != ui->nameLineEdit->text().toStdString()
            || task.get_unit_count_forecast() != units_forecast
            || task.get_units_done_count   () != units_done
            || ((dynamic_cast<ganttry::Task_Templated*>(&task) != nullptr) && (dynamic_cast<ganttry::Task_Templated*>(&task)->get_template_id () != ui->tempateComboBox->currentData()))
        ;

    bool was_changed = task.get_project().changed;

    task.set_name               (ui->nameLineEdit->text().toStdString());
    task.set_description        (ui->descriptionTextEdit->toPlainText().toStdString());
    task.set_template_id        (ui->tempateComboBox->currentData().toUInt());
    task.set_unit_count_forecast(units_forecast);
    task.set_units_done_count   (units_done);

    ui->beginLabel->setText(QDateTime::fromSecsSinceEpoch(task.get_unixtime_start_offset()).toString("yyyy-MM-dd HH:mm"));
    ui->  endLabel->setText(QDateTime::fromSecsSinceEpoch(task.unixtime_end_offset()).toString("yyyy-MM-dd HH:mm"));
    if (ui->zoomSlider->value() == 2)
        ui->lastsLabel->setText(QString::fromStdString(std::to_string((task.duration_in_seconds()) / 86400)) + " days");
    else if (ui->zoomSlider->value() == 1)
        ui->lastsLabel->setText(QString::fromStdString(std::to_string((task.duration_in_seconds()) / 3600)) + " hours");
    ui->progressBar->setValue(100.0 * task.get_units_done_count() / task.get_unit_count_forecast());

    if (redraw) {
        names_scene.redraw();
        dates_scene.redraw();
        gantt_scene.redraw();
    }

    bool is_changed = task.get_project().changed;
    if (is_changed != was_changed)
        this->refresh_workspace_tree();
}


void MainWindow::on_nameLineEdit_editingFinished()
{
    updateTaskFromInput();
}
void MainWindow::on_unitsForecastLineEdit_editingFinished()
{
    updateTaskFromInput();
}
void MainWindow::on_unitsDoneLineEdit_editingFinished()
{
    updateTaskFromInput();
}
void MainWindow::on_descriptionTextEdit_textChanged()
{
    updateTaskFromInput();
}
void MainWindow::on_tempateComboBox_currentIndexChanged(int index)
{
    updateTaskFromInput();
}

void MainWindow::populate_template_combobox()
{
    displaying = true;

    ui->tempateComboBox->clear();
    for (const auto & t : workspace->get_current_project().get_task_templates())
        ui->tempateComboBox->addItem(QString::fromStdString(t.second.name), QVariant::fromValue(t.first));

    if (gantt_scene.get_selected_row_id() != -1)
    {
        auto task_it = workspace->get_current_project().tasks.find(gantt_scene.get_selected_row_id());
        if (task_it != workspace->get_current_project().tasks.end())
            ui->tempateComboBox->setCurrentIndex(ui->tempateComboBox->findData((task_it->second)->get_template_id()));
    }

    displaying = false;
}

void MainWindow::on_newDependency_triggered()
{
    dates_scene.redraw();
    names_scene.redraw();
    gantt_scene.redraw();
}

void MainWindow::save_workspace()
{
    bool save_projects = false;
    for (const auto & proj_uptr : workspace->get_projects())
        if (proj_uptr->changed)
        {
            QMessageBox::StandardButton choice = QMessageBox::warning(this, "Save workspace", "There are unsaved projects.\nSave All?", QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
            if (choice == QMessageBox::Cancel)
                return;
            save_projects = choice == QMessageBox::Yes;
        }
    if (save_projects)
        for (auto & proj_uptr : workspace->get_projects())
            save_project(*proj_uptr);

    if (workspace->get_filename().empty())
    {
        auto filename = QFileDialog::getSaveFileName(this, "Save Workspace As...", QString("~/") + QString::fromStdString(workspace->get_name()) + ".gtw", "Workspace Files (*.gtw)");
        if (filename == "")
            return;
        workspace->set_filename(filename.toStdString());
    }

    if (workspace->get_name().empty())
        workspace->set_name("Unnamed workspace");

    refresh_workspace_tree();


    std::ofstream out(workspace->get_filename());
    out << "{" << std::endl;
    out << "    \"name\": \"" << workspace->get_name() << "\", " << std::endl;
    out << "    \"current_project_idx\": " << workspace->get_current_project_idx() << ", " << std::endl;
    out << "    \"next_task_template_id\": " << workspace->get_next_task_template_id() << ", " << std::endl;
    out << "    \"templates\": [" << std::endl;
    for (auto it=workspace->get_task_templates().begin(), end=workspace->get_task_templates().end() ; it!=end ; )
    {
        ganttry::TemplateID tid = it->first;
        ganttry::TaskTemplate & templ = it->second;
        out << "        {\"id\": " << tid
            << ", \"name\": \""        << templ.name << "\""
            << ", \"description\": \"" << templ.description << "\""
            << ", \"units\": \""       << templ.units << "\""
            << ", \"default_UDM\": "                    << templ.default_UDM
            << ", \"average_UDM\": "                    << templ.average_UDM
            << ", \"default_material_cost_per_unit\": " << templ.default_material_cost_per_unit
            << ", \"average_material_cost_per_unit\": " << templ.average_material_cost_per_unit
            << ", \"default_manpower_cost_per_unit\": " << templ.default_manpower_cost_per_unit
            << ", \"average_manpower_cost_per_unit\": " << templ.average_manpower_cost_per_unit
            << ", \"use_avg\": " << (templ.use_avg ? "true" : "false")
            << "}";
        it++;
        if (it!=end)
            out << ",";
        out << std::endl;
    }
    out << "    ]," << std::endl;

    out << "    \"projects\": [" << std::endl;
    for (auto it=workspace->get_projects().begin(), end=workspace->get_projects().end() ; it!=end ; )
    {
        ganttry::Project & project = **it;
        if (project.filename.empty())
            continue;
        out << "        \"" << project.filename << "\"";
        it++;
        if (it!=end)
            out << ",";
        out << std::endl;
    }
    out << "    ]" << std::endl;

    out << "}";

    workspace->set_changed(false);
    refresh_workspace_tree();
}

void MainWindow::save_project(ganttry::Project & project)
{
    if (project.filename.empty())
    {
        auto filename = QFileDialog::getSaveFileName(this, "Save as...", QString("~/") + QString::fromStdString(project.name) + ".gtp", "Project Files (*.gtp)");
        if (filename == "")
            return;
        project.filename = filename.toStdString();
    }

    if (project.name.empty())
        project.name = "Unnamed project";

    std::ofstream out(project.filename);
    out << "{" << std::endl;
    out << "    \"name\": \"" << project.name << "\", " << std::endl;
    out << "    \"unixtime_start\": " << project.unixtime_start << ", " << std::endl;
    out << "    \"zoom\": " << project.zoom << ", " << std::endl;
    out << "    \"next_task_id\":" << project.next_task_id << ", " << std::endl;
    out << "    \"tasks\": [" << std::endl;
    for (auto it=project.tasks.begin(), end=project.tasks.end() ; it!=end ; )
    {
        ganttry::TaskID tid = it->first;
        ganttry::Task_Base & task = *it->second;
        out << "        " << task.to_json(tid);
        it++;
        if (it!=end)
            out << ",";
        out << std::endl;
    }
    out << "    ]," << std::endl;

    out << "    \"dependencies\": [" << std::endl;
    size_t count = 0;
    for (auto it=project.tasks.begin(), end=project.tasks.end() ; it!=end ; it++)
    {
        ganttry::TaskID tid = it->first;
        ganttry::Task_Base & task = *it->second;
        for (size_t i=0 ; i<task.get_children_tasks().size() ; i++)
        {
            if (count++ > 0)
                out << "," << std::endl;
            const ganttry::Dependency & dependency = task.get_children_tasks()[i];
            out << "        {\"type\": " << (int)dependency.type
                << ", \"from\": " << tid
                << ", \"to\": " << dependency.task_id
                << "}"
                ;
        }
    }
    out << std::endl << "    ]" << std::endl;

    out << "}";

    project.changed = false;
}

void MainWindow::on_workspaceActionNew_triggered()
{
    if (workspace->get_changed())
    {
        QMessageBox::StandardButton choice = QMessageBox::question(this, "New workspace", "Save current workspace and project?", QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
        if (choice == QMessageBox::Yes){
            save_workspace();
        } else if (choice == QMessageBox::No) {
            // dont save
        } else if (choice == QMessageBox::Cancel) {
            return;
        }
    }
    workspace = std::make_unique<ganttry::Workspace>();
    refresh_workspace_tree();

    dates_scene.set_project(&workspace->get_current_project());
    names_scene.set_project(&workspace->get_current_project());
    gantt_scene.set_project(&workspace->get_current_project());

    dates_scene.redraw();
    names_scene.redraw();
    gantt_scene.redraw();
}


void MainWindow::on_workspaceActionSave_triggered()
{
    save_workspace();
}
void MainWindow::on_workspaceActionSaveAs_triggered()
{
    auto filename = QFileDialog::getSaveFileName(this, "Save as...", QString("~/") + QString::fromStdString(workspace->get_name()) + ".gtw", "Workspace Files (*.gtw)");
    if (filename == "")
        return;
    workspace->set_filename(filename.toStdString());

    save_workspace();
}

void MainWindow::on_workspaceActionLoad_triggered()
{
    QString ok("Workspace Files (*.gtw)");
    auto filename = QFileDialog::getOpenFileName(this, "Open...", QString("~/"), "Workspace Files (*.gtw)", &ok);
    if (filename == "")
        return;

    this->add_open_recent(filename);

    load_workspace(filename);
}

void MainWindow::load_workspace(QString filename)
{
    QString val = [](QString filename)
        {
            QString v;
            QFile file;
            file.setFileName(filename);
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            v = file.readAll();
            file.close();
            return v;
        }(filename);

    if (val == "")
        return;

    workspace->reset();

    QJsonObject doc_obj = QJsonDocument::fromJson(val.toUtf8()).object();
    QString name = doc_obj["name"].toString();
    workspace->set_name(name.toStdString());
    workspace->set_filename(filename.toStdString());
    workspace->set_current_project_idx(doc_obj["current_project_idx"].toInt());
    workspace->set_next_task_template_id(doc_obj["next_task_template_id"].toInt());

    QJsonArray templates = doc_obj.value(QString("templates")).toArray();
    for (int i=0 ; i<templates.size() ; i++)
    {
        ganttry::TaskTemplate t{(uint64_t)templates[i].toObject()["id"                            ].toInt()
                               ,          templates[i].toObject()["name"                          ].toString().toStdString()
                               ,          templates[i].toObject()["description"                   ].toString().toStdString()
                               ,          templates[i].toObject()["units"                         ].toString().toStdString()
                               ,(float)   templates[i].toObject()["default_UDM"                   ].toDouble()
                               ,(float)   templates[i].toObject()["average_UDM"                   ].toDouble()
                               ,(float)   templates[i].toObject()["default_material_cost_per_unit"].toDouble()
                               ,(float)   templates[i].toObject()["average_material_cost_per_unit"].toDouble()
                               ,(float)   templates[i].toObject()["default_manpower_cost_per_unit"].toDouble()
                               ,(float)   templates[i].toObject()["average_manpower_cost_per_unit"].toDouble()
                               ,          templates[i].toObject()["use_avg"                       ].toBool()
                               };
        workspace->add_task_template(t);
    }

    QJsonArray projects = doc_obj.value(QString("projects")).toArray();
    for (int i=0 ; i<projects.size() ; i++)
    {
        auto proj = std::make_unique<ganttry::Project>(*workspace, 0);
        proj->filename = projects[i].toString().toStdString();
        workspace->add_project(proj);
    }
    for (auto & proj : workspace->get_projects())
        load_project(*proj, QString::fromStdString(proj->filename));

    workspace->set_changed(false);

    populate_template_combobox();
    project_changed();
}

void MainWindow::load_project(ganttry::Project & project, QString filename)
{
    QString val = [](QString filename)
        {
            QString v;
            QFile file;
            file.setFileName(filename);
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            v = file.readAll();
            file.close();
            return v;
        }(filename);

    if (val == "")
        return;

    project.filename = filename.toStdString();

    QJsonObject doc_obj = QJsonDocument::fromJson(val.toUtf8()).object();
    project.name           = doc_obj.value(QString("name")).toString().toStdString();
    project.unixtime_start = doc_obj.value(QString("unixtime_start")).toInt();
    project.zoom           = doc_obj.value(QString("zoom")).toInt();
    project.next_task_id   = doc_obj.value(QString("next_task_id")).toInt();

    QJsonArray tasks = doc_obj["tasks"].toArray();
    for (int i=0 ; i<tasks.size() ; i++)
    {
        if (tasks[i].toObject().contains("template_id"))
            project.add_task(
                std::make_unique<ganttry::Task_Templated>
                    ( project
                    , (ganttry::TaskID)tasks[i].toObject()["id"].toInt()
                    , tasks[i].toObject()["name"].toString().toStdString()
                    , tasks[i].toObject()["description"].toString().toStdString()
                    , (float)tasks[i].toObject()["unit_count_forecast"].toDouble()
                    , (float)tasks[i].toObject()["units_done_count"].toDouble()
                    , tasks[i].toObject()["template_id"].toInt()
                    )
                );
        else if (tasks[i].toObject().contains("project_filename"))
        {
            std::string proj_filename = tasks[i].toObject()["project_filename"].toString().toStdString();
            ganttry::Project * proj = workspace->get_project_by_filename(proj_filename);
            if (proj == nullptr)
                continue;
            project.add_task(
                std::make_unique<ganttry::Task_SubProject>
                    ( project
                    , (ganttry::TaskID)tasks[i].toObject()["id"].toInt()
                    , tasks[i].toObject()["name"].toString().toStdString()
                    , tasks[i].toObject()["description"].toString().toStdString()
                    , (float)tasks[i].toObject()["unit_count_forecast"].toDouble()
                    , (float)tasks[i].toObject()["units_done_count"].toDouble()
                    , *proj
                    )
                );
        }
    }
    QJsonArray deps = doc_obj.value(QString("dependencies")).toArray();
    for (int i=0 ; i<deps.size() ; i++)
    {
        ganttry::DependencyType type    = (ganttry::DependencyType) deps[i].toObject()["type"].toInt();
        auto from = project.tasks.find((ganttry::DependencyType) deps[i].toObject()["from"].toInt());
        auto to   = project.tasks.find((ganttry::DependencyType) deps[i].toObject()["to"  ].toInt());
        if (from == project.tasks.end() || to == project.tasks.end())
            continue;
        from->second->add_child_task(type, *to->second);
    }

    project.changed = false;
}

void MainWindow::on_workspaceActionTemplates_triggered()
{
    EditTemplates dialog(*workspace);
    dialog.setWindowTitle("Edit templates");
    dialog.setModal(false);
    dialog.exec();

    refresh_workspace_tree();
    populate_template_combobox();
    dates_scene.redraw();
    names_scene.redraw();
    gantt_scene.redraw();
}

void MainWindow::on_projectActionNew_triggered()
{
    ganttry::Project & project = workspace->add_new_project();

    project_changed();
}

void MainWindow::on_projectActionSave_triggered()
{
    ganttry::Project & project = workspace->get_current_project();
    save_project(project);
    refresh_workspace_tree();
}


void MainWindow::on_workspaceTreeWidget_itemDoubleClicked(QTreeWidgetItem *item, [[maybe_unused]] int column)
{
    int idx = item->data(0, Qt::ItemDataRole::UserRole).toInt();
    if (idx == -1)
        return;
    workspace->set_current_project_idx(idx);

    project_changed();
}

void MainWindow::project_changed()
{
    gantt_scene.unselect_row();

    dates_scene.set_project(&workspace->get_current_project());
    names_scene.set_project(&workspace->get_current_project());
    gantt_scene.set_project(&workspace->get_current_project());

    refresh_workspace_tree();

    dates_scene.redraw();
    names_scene.redraw();
    gantt_scene.redraw();

    displaying = true;
    ui->zoomSlider->setValue(workspace->get_current_project().zoom);
    displaying = false;
}


void MainWindow::on_workspaceTreeWidget_itemChanged(QTreeWidgetItem *item, [[maybe_unused]] int column)
{
    int idx = item->data(0, Qt::ItemDataRole::UserRole).toInt();
    if (idx < -1 || idx >= (int)workspace->get_projects().size())
        return;

    if (idx == -1) {
        workspace->set_name(item->text(0).toStdString());
    } else {
        workspace->get_projects()[idx]->changed |= workspace->get_projects()[idx]->name != item->text(0).toStdString();
        workspace->get_projects()[idx]->name = item->text(0).toStdString();
    }

    refresh_workspace_tree();
}

void MainWindow::on_zoomSlider_valueChanged(int value)
{
    if (displaying)
        return;

    workspace->get_current_project().changed |= workspace->get_current_project().zoom != value;
    workspace->get_current_project().zoom = value;
    refresh_workspace_tree();

    dates_scene.redraw();
    //names_scene.redraw();
    gantt_scene.redraw();

    int row_id = gantt_scene.get_selected_row_id();
    if (row_id == -1)
        return;

    auto it = workspace->get_current_project().tasks.find(row_id);
    if (it == workspace->get_current_project().tasks.end())
        return;

    ganttry::Task_Base & task = *it->second;
    if (value == 2)
        ui->lastsLabel->setText(QString::fromStdString(std::to_string((task.duration_in_seconds()) / 86400)) + " days");
    else if (value == 1)
        ui->lastsLabel->setText(QString::fromStdString(std::to_string((task.duration_in_seconds()) / 3600)) + " hours");
}

void MainWindow::add_open_recent(const QString & pathName)
{
    // remove duplicates
    for(int i = ui->menuOpenRecent_2->actions().size() - 1 ; i >= 0 ; i--)
        if (pathName == ui->menuOpenRecent_2->actions()[i]->text())
            ui->menuOpenRecent_2->removeAction(ui->menuOpenRecent_2->actions()[i]);
    // set recent menu
    if (ui->menuOpenRecent_2->actions().size() == 0)
        ui->menuOpenRecent_2->addAction(pathName);
    else
    {
        // insert
        QAction * qaction = new QAction(pathName);
        qaction->setData(pathName);
        ui->menuOpenRecent_2->insertAction(ui->menuOpenRecent_2->actions()[0], qaction);
        // remove excess recents
        while (ui->menuOpenRecent_2->actions().size() > MAX_RECENT)
            ui->menuOpenRecent_2->removeAction(ui->menuOpenRecent_2->actions()[MAX_RECENT]);
    }

    // save recent
    QSettings settings("ttt", "ganttry");
    settings.beginWriteArray("recents");
    for(int i = 0 ; i < ui->menuOpenRecent_2->actions().size() ; i++)
    {
        settings.setArrayIndex(i);
        settings.setValue("recent", ui->menuOpenRecent_2->actions()[i]->text());
    }
    settings.endArray();
}
