#include "debug_panel.h"
#include "ui_debug_panel.h"

Debug_panel::Debug_panel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Debug_panel)
{
    ui->setupUi(this);
}

Debug_panel::~Debug_panel()
{
    delete ui;
}
