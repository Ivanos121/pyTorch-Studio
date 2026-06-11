#include "about_program.h"
#include "ui_about_program.h"

About_program::About_program(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::About_program)
{
    ui->setupUi(this);
    connect(ui->pushButton, &QPushButton::clicked, this, &About_program::close_window);
}

About_program::~About_program()
{
    delete ui;
}

void About_program::close_window()
{
    close();
}