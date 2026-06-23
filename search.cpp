#include "search.h"
#include "ui_search.h"

Search::Search(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Search)
{
    ui->setupUi(this);

    ui->btnMatchCase->setCheckable(true);
    ui->btnWholeWords->setCheckable(true);
    ui->btnRegex->setCheckable(true);

    // Направляем все внутренние изменения на генерацию ОДНОГО внешнего сигнала searchParametersChanged
    connect(ui->btnMatchCase, &QPushButton::toggled, this, &Search::searchParametersChanged);
    connect(ui->btnWholeWords, &QPushButton::toggled, this, &Search::searchParametersChanged);
    connect(ui->btnRegex, &QPushButton::toggled, this, &Search::searchParametersChanged);
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, &Search::searchParametersChanged);

    // Связываем клики по кнопкам с генерацией внешних сигналов
    connect(ui->btnFindNext, &QPushButton::clicked, this, &Search::findNextRequested); // Продолжить поиск
    connect(ui->btnFindPrev, &QPushButton::clicked, this, &Search::findPrevRequested); // Найти предыдущее
    connect(ui->btnSelectAll, &QPushButton::clicked, this, &Search::selectAllRequested); // Выделить все

    connect(ui->btnReplace, &QPushButton::clicked, this, &Search::replaceCurrentRequested);
    connect(ui->btnReplaceNext, &QPushButton::clicked, this, &Search::replaceAndFindNextRequested);
    connect(ui->replaceAllRequested, &QPushButton::clicked, this, &Search::replaceAllRequested);

    ui->lineEditSearch->setClearButtonEnabled(true);
    ui->lineEditReplace->setClearButtonEnabled(true);

}

// Также добавим геттер для получения текста из поля ввода
QString Search::getSearchText() const {
    return ui->lineEditSearch->text();
}

QString Search::getReplaceText() const {
    return ui->lineEditReplace->text(); // имя поля "Заменить на" из вашего дизайнера
}

Search::~Search()
{
    delete ui;
}

bool Search::isMatchCase() const
{
    return ui->btnMatchCase->isChecked();
}

bool Search::isWholeWords() const
{
    return ui->btnWholeWords->isChecked();
}

bool Search::isRegex() const
{
    return ui->btnRegex->isChecked();
}


