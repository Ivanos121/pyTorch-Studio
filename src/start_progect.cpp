#include "start_progect.h"
#include "ui_start_progect.h"
#include "neuro_programm.h"
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>

Start_progect::Start_progect(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Start_progect)
{
    ui->setupUi(this);

    // Инициализируем наш классовый указатель напрямую через parent
    mainWin = qobject_cast<Neuro_programm*>(parent);
    if (!mainWin && wf) {
        mainWin = qobject_cast<Neuro_programm*>(wf);
    }

    // --- 1. СТАРТОВАЯ ИНИЦИАЛИЗАЦИЯ ИНТЕРФЕЙСА СТЕКА (ШАГ 1) ---
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnBack->setVisible(false);
    ui->btnNext->setEnabled(false);
    ui->btn_dyr_folder->setEnabled(true);

    // Наполнение списка шагов в левой навигационной панели
    ui->listWidget->clear();
    QStringList steps;
    steps << "> Размещение" << " Конфигурация" << " Структура данных";
    ui->listWidget->addItems(steps);

    // Делаем первый элемент списка ("Размещение") полужирным
    QListWidgetItem *firstItem = ui->listWidget->item(0);
    if (firstItem) {
        QFont font = firstItem->font();
        font.setBold(true);
        firstItem->setFont(font);
    }

    // Задаем комфортную высоту строк в списке шагов IDE
    for (int i = 0; i < ui->listWidget->count(); ++i) {
        QListWidgetItem *item = ui->listWidget->item(i);
        if (item) item->setSizeHint(QSize(0, 35));
    }
    ui->listWidget->clearSelection();

    // --- 2. НАСТРОЙКА РАДИОКНОПОК НА СТРАНИЦЕ 2 (КОНФИГУРАЦИЯ VENV) ---
    ui->radioCreateNewVenv->setAutoExclusive(false);
    ui->radioUseExistingVenv->setAutoExclusive(false);

    ui->radioCreateNewVenv->setChecked(true);
    ui->radioCreateNewVenv->setEnabled(true);
    ui->radioUseExistingVenv->setChecked(false);

    ui->radioCreateNewVenv->setAutoExclusive(true);
    ui->radioUseExistingVenv->setAutoExclusive(true);

    ui->lineEditCreateNewPath->setEnabled(false);
    ui->btn_dyr->setEnabled(false);
    ui->btn_dyr2->setEnabled(true);

    // Настройка нижней группы (Архитектура PyTorch)
    ui->radioBuildCpu->setAutoExclusive(false);
    ui->radioBuildGpu->setAutoExclusive(false);
    ui->radioBuildCpu->setChecked(false);
    ui->radioBuildGpu->setChecked(true);
    ui->radioBuildCpu->setAutoExclusive(true);
    ui->radioBuildGpu->setAutoExclusive(true);

    // --- 3. СИСТЕМНЫЕ КОННЕКТЫ И СВЯЗИ КНОПОК НАВИГАЦИИ ---
    connect(ui->btnBack, &QPushButton::clicked, this, &Start_progect::onBackClicked);
    connect(ui->btnexit, &QPushButton::clicked, this, &Start_progect::onexitlicked);
    connect(ui->btn_dyr, &QPushButton::clicked, this, &Start_progect::open_dyr);
    connect(ui->btn_dyr2, &QPushButton::clicked, this, &Start_progect::open_dyr);
    connect(ui->btn_dyr_folder, &QPushButton::clicked, this, &Start_progect::open_dyr);
    connect(ui->btnNext, &QPushButton::clicked, this, &Start_progect::create_progect);

    // Кнопка выбора датасета Шага 3 и кнопка кастомного файла Шага 2
    connect(ui->dataSetButon, &QPushButton::clicked, this, &Start_progect::open_dyr);

    connect(ui->btnBrowseRequirements, &QPushButton::clicked, this, [this]() {
        QString selectedFile = QFileDialog::getOpenFileName(
            this, "Выберите файл зависимостей", QDir::homePath(), "Текстовые файлы (*.txt);;Все файлы (*)"
            );
        if (!selectedFile.isEmpty()) {
            ui->lineEditRequirementsPath->setText(selectedFile);
        }
    });

    // Сигналы мгновенной валидации полей при изменении текста
    connect(ui->lineEditName, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->lineEditLocation, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->lineEditCreateNewPath, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->lineEditExistingPath, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->lineEditExistsDatasetPath, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->lineEditRequirementsPath, &QLineEdit::textChanged, this, &Start_progect::validateFields);

    // Валидация по физическому клику на переключатели
    connect(ui->radioCreateNewVenv, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->radioUseExistingVenv, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->radioBuildCpu, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->radioBuildGpu, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->radioModeCopy, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->radioModeSymlink, &QRadioButton::clicked, this, &Start_progect::validateFields);

    // Управление доступностью элементов Шага 2
    connect(ui->radioUseExistingVenv, &QRadioButton::toggled, this, [this](bool checked) {
        ui->lineEditExistingPath->setEnabled(checked);
        ui->btn_dyr->setEnabled(checked);
    });

    connect(ui->radioCreateNewVenv, &QRadioButton::toggled, this, [this](bool checked) {
        ui->lineEditCreateNewPath->setEnabled(checked);
        ui->btn_dyr2->setEnabled(checked);
    });

    // Включение/выключение поля requirements.txt от чекбокса
    connect(ui->checkBoxCustomRequirements, &QCheckBox::toggled, this, [this](bool checked) {
        ui->lineEditRequirementsPath->setEnabled(checked);
        ui->btnBrowseRequirements->setEnabled(checked);
        if (!checked) {
            ui->lineEditRequirementsPath->clear();
        }
        validateFields();
    });

    // =========================================================================
    // ЖЕСТКАЯ UX-ЛОГИКА ДЛЯ ЧЕКБОКСА ДАТАСЕТА (ШАГ 3) — СИНХРОННАЯ БЛОКИРОВКА
    // =========================================================================
    connect(ui->checkBoxEnableDataset, &QCheckBox::toggled, this, [this](bool checked) {
        ui->lineEditExistsDatasetPath->setEnabled(checked);
        ui->dataSetButon->setEnabled(checked);
        ui->radioModeCopy->setEnabled(checked);
        ui->radioModeSymlink->setEnabled(checked);

        if (!checked) {
            ui->lineEditExistsDatasetPath->clear();
            ui->radioModeCopy->setAutoExclusive(false);
            ui->radioModeSymlink->setAutoExclusive(false);
            ui->radioModeCopy->setChecked(false);
            ui->radioModeSymlink->setChecked(false);
            ui->radioModeCopy->setAutoExclusive(true);
            ui->radioModeSymlink->setAutoExclusive(true);
        } else {
            ui->radioModeSymlink->setChecked(true);
            bool hasPath = !ui->lineEditExistsDatasetPath->text().trimmed().isEmpty();
            ui->radioModeCopy->setEnabled(hasPath);
            ui->radioModeSymlink->setEnabled(hasPath);
        }
        validateFields();
    });

    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &Start_progect::validateFields);

    // =========================================================================
    // 3. ЖЕСТКАЯ СТАРТОВАЯ ИНИЦИАЛИЗАЦИЯ И СБРОС СОСТОЯНИЙ ПРИ ОТКРЫТИИ ОКНА
    // =========================================================================
    ui->checkBoxCustomRequirements->setChecked(false);
    ui->lineEditRequirementsPath->setEnabled(false);
    ui->btnBrowseRequirements->setEnabled(false);

    ui->checkBoxEnableDataset->setChecked(false);
    ui->lineEditExistsDatasetPath->setEnabled(false);
    ui->dataSetButon->setEnabled(false);
    ui->radioModeCopy->setEnabled(false);
    ui->radioModeSymlink->setEnabled(false);

    validateFields();
}

Start_progect::~Start_progect()
{
    delete ui;
}

void Start_progect::onBackClicked()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    if (currentIndex <= 0) return;
    int prevIndex = currentIndex - 1;

    QListWidgetItem *currentItem = ui->listWidget->item(currentIndex);
    if (currentItem) {
        if (currentIndex == 1) currentItem->setText(" Конфигурация");
        else if (currentIndex == 2) currentItem->setText(" Структура данных");
        QFont normalFont = currentItem->font();
        normalFont.setBold(false);
        currentItem->setFont(normalFont);
    }

    QListWidgetItem *prevItem = ui->listWidget->item(prevIndex);
    if (prevItem) {
        if (prevIndex == 0) prevItem->setText("> Размещение");
        else if (prevIndex == 1) prevItem->setText("> Конфигурация");
        QFont boldFont = prevItem->font();
        boldFont.setBold(true);
        prevItem->setFont(boldFont);
    }

    ui->stackedWidget->setCurrentIndex(prevIndex);
    ui->listWidget->clearSelection();
    ui->btnNext->setText("Далее");
    if (prevIndex == 0) {
        ui->btnBack->setVisible(false);
    }
}

void Start_progect::onexitlicked()
{
    close();
}

void Start_progect::open_dyr()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    QObject *senderButton = sender(); // Получаем указатель на нажатую кнопку "Обзор"

    // =========================================================================
    // ИСПРАВЛЕНО: Используем нативный диалог ОС, который ГАРАНТИРОВАННО
    // разрешает пользователю нажимать кнопку "Создать папку" (New Folder)
    // =========================================================================
    QString selectedPath = QFileDialog::getExistingDirectory(
        this,
        "Выберите или создайте директорию размещения",
        QDir::homePath(),
        QFileDialog::DontResolveSymlinks // <── Убрали ShowDirsOnly, ломавший создание папок!
        );

    // Если пользователь передумал и закрыл окно — выходим
    if (selectedPath.isEmpty()) return;

    // --- ШАГ 1: Размещение проекта ---
    if (currentIndex == 0) {
        ui->lineEditLocation->setText(selectedPath);
    }
    // --- ШАГ 2: Конфигурация виртуального окружения (venv) ---
    else if (currentIndex == 1) {
        if (senderButton == ui->btn_dyr2) {
            ui->lineEditCreateNewPath->setText(selectedPath);
        } else if (senderButton == ui->btn_dyr) {
            ui->lineEditExistingPath->setText(selectedPath);
        }
    }
    // --- ШАГ 3: Структура данных (Датасет) ---
    else if (currentIndex == 2) {
        if (senderButton == ui->dataSetButon) {
            ui->lineEditExistsDatasetPath->setText(selectedPath);
        }
    }

    validateFields(); // Пересчитываем доступность кнопки "Далее"
}

void Start_progect::create_progect()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    int pageCount = ui->stackedWidget->count();

    // Мы на последней странице (Шаг 3) -> Финиш
    if (currentIndex == pageCount - 1)
    {
        // Закрываем окно, отправляя QDialog::Accepted в главное окно Neuro_programm
        this->accept();
        return;
    }

    // Промежуточные шаги -> Работает как кнопка "Далее"
    ui->btnBack->setVisible(true);
    QListWidgetItem *currentItem = ui->listWidget->item(currentIndex);
    if (currentItem) {
        if (currentIndex == 0) currentItem->setText(" Размещение");
        else if (currentIndex == 1) currentItem->setText(" Конфигурация");
        QFont normalFont = currentItem->font();
        normalFont.setBold(false);
        currentItem->setFont(normalFont);
    }

    int nextIndex = currentIndex + 1;
    QListWidgetItem *nextItem = ui->listWidget->item(nextIndex);
    if (nextItem) {
        if (nextIndex == 1) nextItem->setText("> Конфигурация");
        else if (nextIndex == 2) nextItem->setText("> Структура данных");
        QFont boldFont = nextItem->font();
        boldFont.setBold(true);
        nextItem->setFont(boldFont);
    }

    ui->stackedWidget->setCurrentIndex(nextIndex);
    ui->listWidget->clearSelection();

    if (nextIndex == pageCount - 1) {
        ui->btnNext->setText("Создать проект");
    }
}

void Start_progect::validateFields()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    bool isValid = false;

    switch (currentIndex)
    {
    case 0: // ШАГ 1: Размещение
    {
        isValid = !ui->lineEditName->text().trimmed().isEmpty() &&
                  !ui->lineEditLocation->text().trimmed().isEmpty();
        break;
    }
    case 1: // ШАГ 2: Конфигурация venv
    {
        bool isFolderSectionValid = false;
        if (ui->radioCreateNewVenv->isChecked()) {
            isFolderSectionValid = true; // Создание нового venv валидно по дефолту
        }
        else if (ui->radioUseExistingVenv->isChecked()) {
            isFolderSectionValid = !ui->lineEditExistingPath->text().trimmed().isEmpty();
        }
        bool isArchitectureSectionValid = ui->radioBuildCpu->isChecked() || ui->radioBuildGpu->isChecked();
        isValid = isFolderSectionValid && isArchitectureSectionValid;
        break;
    }
    case 2: // ШАГ 3: Структура данных (Датасет)
    {
        // ПЕРЕПИСАНО ПОД ЧЕКБОКС:
        if (!ui->checkBoxEnableDataset->isChecked()) {
            // Если чекбокс НЕ активен — создавать пустой проект можно сразу!
            isValid = true;
        } else {
            // Если чекбокс АКТИВЕН — требуем ввести путь к данным И выбрать радиобаттон
            bool hasPath = !ui->lineEditExistsDatasetPath->text().trimmed().isEmpty();
            bool hasMode = ui->radioModeCopy->isChecked() || ui->radioModeSymlink->isChecked();
            isValid = hasPath && hasMode;
        }
        break;
    }
    default:
        break;
    }

    ui->btnNext->setEnabled(isValid);
}

// Геттеры для передачи параметров в главное кодовое окно Neuro_programm
QString Start_progect::getProjectName() const { return ui->lineEditName->text().trimmed(); }
QString Start_progect::getProjectLocation() const { return ui->lineEditLocation->text().trimmed(); }
bool Start_progect::isCreateNewVenv() const { return ui->radioCreateNewVenv->isChecked(); }
bool Start_progect::isGpuArchitecture() const { return ui->radioBuildGpu->isChecked(); }
bool Start_progect::isDatasetEnabled() const { return ui->checkBoxEnableDataset->isChecked(); }
QString Start_progect::getDatasetPath() const { return ui->lineEditExistsDatasetPath->text().trimmed(); }
bool Start_progect::isSymlinkMode() const { return ui->radioModeSymlink->isChecked(); }

bool Start_progect::isCustomRequirementsEnabled() const
{
    return ui->checkBoxCustomRequirements->isChecked();
}

QString Start_progect::getCustomRequirementsPath() const
{
    return ui->lineEditRequirementsPath->text().trimmed();
}

QString Start_progect::getExistingVenvPath() const
{
    return ui->lineEditExistingPath->text().trimmed();
}

QString Start_progect::getCreateNewVenvPath() const
{
    return ui->lineEditCreateNewPath->text().trimmed();
}

bool Start_progect::isUseExistingVenv() const
{
    return ui->radioUseExistingVenv->isChecked();
}