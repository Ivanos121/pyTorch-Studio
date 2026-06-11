#include "help_window.h"
#include "ui_help_window.h"

Help_window::Help_window(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Help_window)
{
    ui->setupUi(this);
}

Help_window::~Help_window()
{
    delete ui;
}
