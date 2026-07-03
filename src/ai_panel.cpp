#include "ai_panel.h"
#include "ui_ai_panel.h"

#include "neuro_programm.h"    // 3. И только теперь раскрываем кишки главного окна
#include "ui_neuro_programm.h"

AI_panel::AI_panel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AI_panel)
{
    ui->setupUi(this);
    this->comboBatchSize = ui->comboBatchSize;
    this->progressCPU = ui->progressCPU;
    this->progressGPU = ui->progressGPU;
    this->cosmboDevice_2 = ui->comboDevice_2;
    this->spinBoxEpochs = ui->spinBoxEpochs;
    this->spinBoxLR = ui->spinBoxLR;


    //this->btnStartTraining = ui->btnStartTraining;
}

AI_panel::~AI_panel()
{
    delete ui;
}
