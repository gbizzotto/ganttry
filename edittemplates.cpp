
#include <QMenu>
#include <QListWidgetItem>
#include <QTimer>

#include "project.hpp"
#include "edittemplates.h"
#include "ui_edittemplates.h"

EditTemplates::EditTemplates(ganttry::Workspace & w, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditTemplates),
    workspace(w)
{
    ui->setupUi(this);

    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    for (const auto & task_template : workspace.get_task_templates())
    {
        QListWidgetItem * item = new QListWidgetItem(QString::fromStdString(task_template.second.name), ui->listWidget, QListWidgetItem::ItemType::UserType);
        item->setData(Qt::ItemDataRole::UserRole, (int)task_template.second.id);
        ui->listWidget->addItem(item);

        addUnits(task_template.second.units);
        ui->unitComboBox->model()->sort(0);
    }
    ui->listWidget->sortItems();
}

EditTemplates::~EditTemplates()
{
    delete ui;
}

void EditTemplates::addUnits(const std::string & s)
{
    int i;
    for (i=0 ; i<ui->unitComboBox->count() ; i++)
        if (ui->unitComboBox->itemText(i).toStdString() == s)
            break;
    if (i == ui->unitComboBox->count())
        ui->unitComboBox->addItem(QString::fromStdString(s));
}

void EditTemplates::on_listWidget_customContextMenuRequested(const QPoint &pos)
{
    static QString default_name("New template");
    static std::string default_unit("m2");

    QMenu menu(this);
    auto action_add = menu.addAction("Add");
    auto action = menu.exec(ui->listWidget->mapToGlobal(pos));

    if (action == action_add)
    {
        //auto [it,b] = workspace.get_task_templates().insert({(int)workspace.get_next_task_template_id(), default_name.toStdString(), "", default_unit, 1.0, 0.0, 100.0, 0.0, 100.0, 0.0, true});
        ganttry::TaskTemplate & task_template = workspace.add_task_template(default_name.toStdString(), "", default_unit, 1.0, 0.0, 100.0, 0.0, 100.0, 0.0, true);
        // useless if mapid<> always inserts with new ID
        //if ( ! b)
        //{
        //    QList<QListWidgetItem*> list = ui->listWidget->findItems(default_name, Qt::MatchExactly);
        //    if ( ! list.empty())
        //        ui->listWidget->setCurrentItem(list[0]);
        //    return;
        //}
        //auto & task_template = it->second;

        QListWidgetItem * item = new QListWidgetItem(default_name, ui->listWidget, QListWidgetItem::ItemType::UserType);
        item->setData(Qt::ItemDataRole::UserRole, (int) task_template.id);
        ui->listWidget->addItem(item);
        ui->listWidget->setCurrentItem(item);

        workspace.set_changed(true);
    }
}


void EditTemplates::on_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    // display current
    if (current != nullptr)
    {
        int id = current->data(Qt::ItemDataRole::UserRole).toInt();
        if (workspace.get_task_templates().find(id) != workspace.get_task_templates().end())
        {
            displaying = true;
            auto & task_template = workspace.get_task_template(id);
            ui->nameLineEdit       ->setText       (QString::fromStdString(               task_template.name                          ));
            ui->descriptionTextEdit->setText       (QString::fromStdString(               task_template.description                   ));
            ui->unitComboBox       ->setCurrentText(QString::fromStdString(               task_template.units                         ));
            ui->UMDLineEdit        ->setText       (QString::fromStdString(std::to_string(task_template.default_UDM                   )));
            ui->materialLineEdit   ->setText       (QString::fromStdString(std::to_string(task_template.default_material_cost_per_unit)));
            ui->manpowerLineEdit   ->setText       (QString::fromStdString(std::to_string(task_template.default_manpower_cost_per_unit)));
            ui->avgCheckBox        ->setCheckState (task_template.use_avg ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
            displaying = false;
        }
    }
}

void EditTemplates::updateTemplate(QListWidgetItem * item)
{
    if (displaying)
        return;

    if (item == nullptr)
        return;

    int id = item->data(Qt::ItemDataRole::UserRole).toInt();
    if (id < 0 || id > (int)workspace.get_task_templates().rbegin()->first)
        return;

    auto & task_template = workspace.get_task_template(id);

    bool template_changed = false
        | (task_template.name                           != ui->nameLineEdit       ->       text().toStdString())
        | (task_template.description                    != ui->descriptionTextEdit->toPlainText().toStdString())
        | (task_template.units                          != ui->unitComboBox       ->currentText().toStdString())
        | (task_template.default_UDM                    != ui->UMDLineEdit        ->       text().toFloat    ())
        | (task_template.default_material_cost_per_unit != ui->materialLineEdit   ->       text().toFloat    ())
        | (task_template.default_manpower_cost_per_unit != ui->manpowerLineEdit   ->       text().toFloat    ())
        | (task_template.use_avg                        != ui->avgCheckBox        -> checkState()              )
        ;
    workspace.set_changed(workspace.get_changed() | template_changed);

    task_template.name                           = ui->nameLineEdit       ->       text().toStdString();
    task_template.description                    = ui->descriptionTextEdit->toPlainText().toStdString();
    task_template.units                          = ui->unitComboBox       ->currentText().toStdString();
    task_template.default_UDM                    = ui->UMDLineEdit        ->       text().toFloat    ();
    task_template.default_material_cost_per_unit = ui->materialLineEdit   ->       text().toFloat    ();
    task_template.default_manpower_cost_per_unit = ui->manpowerLineEdit   ->       text().toFloat    ();
    task_template.use_avg                        = ui->avgCheckBox        -> checkState();
}

void EditTemplates::on_nameLineEdit_editingFinished()
{
    if (ui->nameLineEdit->text().length() == 0)
        {
        int id = ui->listWidget->currentItem()->data(Qt::ItemDataRole::UserRole).toInt();
        if (id < 0 || id > (int)workspace.get_task_templates().rbegin()->first)
            return;
        auto & task_template = workspace.get_task_template(id);

        ui->nameLineEdit->setText(QString::fromStdString(task_template.name));

        return;
    }
    updateTemplate(ui->listWidget->currentItem());
    ui->listWidget->currentItem()->setText(ui->nameLineEdit->text());
    QTimer::singleShot(0, [&] { ui->listWidget->sortItems(); });
}

void EditTemplates::on_descriptionTextEdit_textChanged()
{
    updateTemplate(ui->listWidget->currentItem());
}


void EditTemplates::on_unitComboBox_editTextChanged(const QString &arg1)
{
    updateTemplate(ui->listWidget->currentItem());
    QString str = ui->unitComboBox->currentText();

    if (displaying)
        return;
    displaying = true;

    ui->unitComboBox->clear();
    for (const auto & task_template : workspace.get_task_templates())
        addUnits(task_template.second.units);
    ui->unitComboBox->model()->sort(0);
    ui->unitComboBox->setCurrentText(str);

    displaying = false;
}

void EditTemplates::on_manpowerLineEdit_editingFinished()
{
    updateTemplate(ui->listWidget->currentItem());
}

void EditTemplates::on_UMDLineEdit_editingFinished()
{
    updateTemplate(ui->listWidget->currentItem());
}

void EditTemplates::on_materialLineEdit_editingFinished()
{
    updateTemplate(ui->listWidget->currentItem());
}



void EditTemplates::on_avgCheckBox_stateChanged(int arg1)
{
    updateTemplate(ui->listWidget->currentItem());

    ui->     UMDLabel->setEnabled(arg1);
    ui->manpowerLabel->setEnabled(arg1);
    ui->materialLabel->setEnabled(arg1);
}

