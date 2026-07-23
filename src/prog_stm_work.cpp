#include "prog_stm_work.h"
#include "ui_prog_stm_work.h"

Prog_STM_work::Prog_STM_work(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Prog_STM_work)
{
    ui->setupUi(this);
}

Prog_STM_work::~Prog_STM_work()
{
    delete ui;
}
