#ifndef EDITTEMPLATES_H
#define EDITTEMPLATES_H

#include <QDialog>
#include <QListWidgetItem>

#include "workspace.hpp"

namespace Ui {
class EditTemplates;
}

class EditTemplates : public QDialog
{
    Q_OBJECT
    bool displaying = false;
    Ui::EditTemplates *ui;
    ganttry::Workspace & workspace;

public:
    explicit EditTemplates(ganttry::Workspace & w, QWidget *parent = nullptr);
    ~EditTemplates();

    void addUnits(const std::string & s);
    void updateTemplate(QListWidgetItem * item);

private slots:
    void on_listWidget_customContextMenuRequested(const QPoint &pos);

    void on_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    void on_nameLineEdit_editingFinished();

    void on_descriptionTextEdit_textChanged();

    void on_unitComboBox_editTextChanged(const QString &arg1);

    void on_UMDLineEdit_editingFinished();

    void on_manpowerLineEdit_editingFinished();

    void on_materialLineEdit_editingFinished();
    void on_avgCheckBox_stateChanged(int arg1);
};

#endif // EDITTEMPLATES_H
