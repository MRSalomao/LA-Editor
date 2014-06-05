#include "newproject.h"
#include "ui_newproject.h"

NewProject::NewProject(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewProject)
{
    ui->setupUi(this);

    this->setFixedSize( this->size() );

    ui->lineEdit->setFocus();
}

NewProject::~NewProject()
{
    delete ui;
}

void NewProject::on_buttonBox_accepted()
{
    close();
}

void NewProject::on_buttonBox_rejected()
{
    close();
}
