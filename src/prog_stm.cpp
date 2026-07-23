#include "prog_stm.h"
#include "ui_prog_stm.h"
#include "neuro_programm.h"

Prog_STM::Prog_STM(QWidget *parent)
    : QWidget(parent), ui(new Ui::Prog_STM)
{
    ui->setupUi(this);
}

Prog_STM::~Prog_STM()
{
    delete ui;
}

void Prog_STM::setMainProgram(Neuro_programm *mainProgram)
{
    m_mainProgram = mainProgram;
    if (!m_mainProgram) return;

    // Безопасно связываем кнопки этой страницы с функциями главного класса
    connect(ui->btnDetect, &QPushButton::clicked, m_mainProgram, &Neuro_programm::onDetectDevice);
    connect(ui->btnBrowse, &QPushButton::clicked, m_mainProgram, &Neuro_programm::onSelectFirmwareFile);
    connect(ui->btnEraseMain,  &QPushButton::clicked, m_mainProgram, &Neuro_programm::onEraseFlash);
    connect(ui->btnFlashMain,  &QPushButton::clicked, m_mainProgram, &Neuro_programm::onWrightFlash);
    connect(ui->btnReadFlash, &QPushButton::clicked, m_mainProgram, &Neuro_programm::onReadFlash);

}

void Prog_STM::updateStatusText(const QString &text, const QString &colorHtml)
{
    //ui->lblStatus->setText(text);
    //ui->lblStatus->setStyleSheet(QString("color: %1; font-weight: bold;").arg(colorHtml));
}

void Prog_STM::setFlashButtonsEnabled(bool enabled)
{
    ui->btnEraseMain->setEnabled(enabled);  // Имя вашей кнопки стирания на форме
    ui->btnFlashMain->setEnabled(enabled);  // Имя вашей кнопки прошивки на форме
    // Если на форме есть кнопка считывания: ui->btnRead->setEnabled(enabled);
}

void Prog_STM::setDeviceHardwareInfo(const QString &chipModel, int flashKb, int sramKb, bool isConnected)
{
    if (isConnected) {
        // Выводим имя программатора и модель чипа
        ui->lblChipModel->setText(QString("ST-Link v2-1: %1").arg(chipModel));
        ui->lblChipModel->setStyleSheet("color: green; font-weight: bold;");

        // Выводим точные параметры памяти микроконтроллера
        ui->lblMemory->setText(QString("Flash: %1 КБ | SRAM: %2 КБ").arg(flashKb).arg(sramKb));
        ui->lblMemory->setStyleSheet("color: #555555;"); // Приятный темно-серый цвет
    } else {
        // Сброс в исходное состояние при ошибке или отключении
        ui->lblChipModel->setText("ST-Link v2-1: Отключен");
        ui->lblChipModel->setStyleSheet("color: red; font-weight: bold;");

        ui->lblMemory->setText("Flash: -- КБ | SRAM: -- КБ");
        ui->lblMemory->setStyleSheet("color: #888888;");
    }
}
