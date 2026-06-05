#include "settings.h"
#include "ui_settings.h"

#include <QMessageBox>

Settings::Settings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Settings)
{
    ui->setupUi(this);
    connect(ui->pushButton, &QPushButton::clicked, this, &Settings::close_window);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &Settings::apply_settings);
}

Settings::~Settings()
{
    delete ui;
}

void Settings::close_window()
{
    close();
}

void Settings::apply_settings()
{
    close();
    QMessageBox::information(this, "d", "d");
}