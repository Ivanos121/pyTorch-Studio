#include "start_progect.h"
#include "ui_start_progect.h"
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>

Start_progect::Start_progect(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Start_progect)
{
    ui->setupUi(this);

    // --- 1. СТАРТОВАЯ ИНИЦИАЛИЗАЦИЯ ИНТЕРФЕЙСА ---
    ui->stackedWidget->setCurrentIndex(0);
    ui->btnBack->setVisible(false);
    ui->btnNext->setEnabled(false);
    ui->btn_dyr_folder->setEnabled(true);

    // Наполнение списка шагов в левой панели
    ui->listWidget->clear();
    QStringList steps;
    steps << "> Размещение"
          << "  Конфигурация"
          << "  Структура данных";
    ui->listWidget->addItems(steps);

    // Делаем первый элемент списка жирным
    QListWidgetItem *firstItem = ui->listWidget->item(0);
    if (firstItem)
    {
        QFont font = firstItem->font();
        font.setBold(true);
        firstItem->setFont(font);
    }

    // Задаем высоту строк в списке
    for(int i = 0; i < ui->listWidget->count(); ++i)
    {
        QListWidgetItem *item = ui->listWidget->item(i);
        if (item) item->setSizeHint(QSize(0, 35));
    }
    ui->listWidget->clearSelection();

    // --- 2. НАСТРОЙКА РАДИОКНОПОК НА СТРАНИЦЕ 2 (КОНФИГУРАЦИЯ VENV) ---
    // Сброс верхней группы (Полная установка venv)
    ui->rbCreateVenv->setAutoExclusive(false);
    ui->rbExistingVenv->setAutoExclusive(false);
    ui->rbCreateVenv->setChecked(false);
    ui->rbExistingVenv->setChecked(false);
    ui->rbCreateVenv->setAutoExclusive(true);
    ui->rbExistingVenv->setAutoExclusive(true);

    ui->dir_folder_venve->setEnabled(false);
    ui->btn_dyr->setEnabled(false);

    // Сброс нижней группы (Выборочная установка PyTorch)
    ui->rbCpuVersion->setAutoExclusive(false);
    ui->rbCudaVersion->setAutoExclusive(false);
    ui->rbCpuVersion->setChecked(false);
    ui->rbCudaVersion->setChecked(false);
    ui->rbCpuVersion->setAutoExclusive(true);
    ui->rbCudaVersion->setAutoExclusive(true);

    // --- 3. НАСТРОЙКА РАДИОКНОПОК НА СТРАНИЦЕ 3 (СТРУКТУРА ДАННЫХ) ---
    ui->rbDatasetPath->setAutoExclusive(false);
    ui->rbDatasetOption1->setAutoExclusive(false);
    ui->rbDatasetOption2->setAutoExclusive(false);

    ui->rbDatasetPath->setChecked(false);
    ui->rbDatasetOption1->setChecked(false);
    ui->rbDatasetOption2->setChecked(false);

    ui->rbDatasetPath->setAutoExclusive(true);
    ui->rbDatasetOption1->setAutoExclusive(true);
    ui->rbDatasetOption2->setAutoExclusive(true);

    ui->dyr_dataset->setEnabled(false);

    // --- 4. СИСТЕМНЫЕ КОННЕКТЫ ---
    connect(ui->btnBack, &QPushButton::clicked, this, &Start_progect::onBackClicked);
    connect(ui->btnexit, &QPushButton::clicked, this, &Start_progect::onexitlicked);
    connect(ui->btn_dyr, &QPushButton::clicked, this, &Start_progect::open_dyr);
    connect(ui->btn_dyr_folder, &QPushButton::clicked, this, &Start_progect::open_dyr);
    connect(ui->btnNext, &QPushButton::clicked, this, &Start_progect::create_progect);

    // Сигналы валидации при изменении текста
    connect(ui->name_progect, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->dir_root_folder, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->dir_folder_venve, &QLineEdit::textChanged, this, &Start_progect::validateFields);
    connect(ui->dyr_dataset, &QLineEdit::textChanged, this, &Start_progect::validateFields);

    // Валидация по физическому клику на любую из четырех радиокнопок
    connect(ui->rbCreateVenv,   &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->rbExistingVenv, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->rbCpuVersion,   &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->rbCudaVersion,  &QRadioButton::clicked, this, &Start_progect::validateFields);

    // ВЗАИМНЫЙ СБРОС МЕЖДУ ДВУМЯ НЕЗАВИСИМЫМИ РАДИОГРУППАМИ (МАГИЯ СИНХРОНИЗАЦИИ)
    // Если кликаем на верхнюю группу — принудительно гасим нижнюю группу кнопок
    connect(ui->rbCreateVenv, &QRadioButton::clicked, this, [this]() {
        ui->rbCpuVersion->setAutoExclusive(false);
        ui->rbCudaVersion->setAutoExclusive(false);
        ui->rbCpuVersion->setChecked(false);
        ui->rbCudaVersion->setChecked(false);
        ui->rbCpuVersion->setAutoExclusive(true);
        ui->rbCudaVersion->setAutoExclusive(true);
    });
    connect(ui->rbExistingVenv, &QRadioButton::clicked, this, [this]() {
        ui->rbCpuVersion->setAutoExclusive(false);
        ui->rbCudaVersion->setAutoExclusive(false);
        ui->rbCpuVersion->setChecked(false);
        ui->rbCudaVersion->setChecked(false);
        ui->rbCpuVersion->setAutoExclusive(true);
        ui->rbCudaVersion->setAutoExclusive(true);
    });

    // Если кликаем на нижнюю группу — принудительно гасим верхнюю группу кнопок
    connect(ui->rbCpuVersion, &QRadioButton::clicked, this, [this]() {
        ui->rbCreateVenv->setAutoExclusive(false);
        ui->rbExistingVenv->setAutoExclusive(false);
        ui->rbCreateVenv->setChecked(false);
        ui->rbExistingVenv->setChecked(false);
        ui->rbCreateVenv->setAutoExclusive(true);
        ui->rbExistingVenv->setAutoExclusive(true);
    });
    connect(ui->rbCudaVersion, &QRadioButton::clicked, this, [this]() {
        ui->rbCreateVenv->setAutoExclusive(false);
        ui->rbExistingVenv->setAutoExclusive(false);
        ui->rbCreateVenv->setChecked(false);
        ui->rbExistingVenv->setChecked(false);
        ui->rbCreateVenv->setAutoExclusive(true);
        ui->rbExistingVenv->setAutoExclusive(true);
    });

    // Управление доступностью поля lineEdit при переключении верхних кнопок
    connect(ui->rbExistingVenv, &QRadioButton::toggled, this, [this](bool checked) {
        ui->dir_folder_venve->setEnabled(checked);
        ui->btn_dyr->setEnabled(checked);
    });

    // Пересчет кнопки при смене страниц мастера
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &Start_progect::validateFields);

    // Автоматическое управление доступностью полей ввода Шага 3 через лямбды
    // Привязываем клики по радиокнопкам датасета к общему пересчету кнопки btnNext
    connect(ui->rbDatasetPath,    &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->rbDatasetOption1, &QRadioButton::clicked, this, &Start_progect::validateFields);
    connect(ui->rbDatasetOption2, &QRadioButton::clicked, this, &Start_progect::validateFields);

    // Принудительный пересчет, если пользователь вручную стер или вставил путь к датасету
    connect(ui->dyr_dataset, &QLineEdit::textChanged, this, &Start_progect::validateFields);

    // Запускаем стартовую валидацию
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
        if (currentIndex == 1)      currentItem->setText("  Конфигурация");
        else if (currentIndex == 2) currentItem->setText("  Структура данных");

        QFont normalFont = currentItem->font();
        normalFont.setBold(false);
        currentItem->setFont(normalFont);
    }

    QListWidgetItem *prevItem = ui->listWidget->item(prevIndex);
    if (prevItem) {
        if (prevIndex == 0)      prevItem->setText("> Размещение");
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
    QString selectedPath = QFileDialog::getExistingDirectory(
        this,
        "Выберите директорию",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (selectedPath.isEmpty()) return;

    if (currentIndex == 0)      ui->dir_root_folder->setText(selectedPath);
    else if (currentIndex == 1) ui->dir_folder_venve->setText(selectedPath);
    else if (currentIndex == 2) ui->dyr_dataset->setText(selectedPath);

    // ВАЖНО: После записи пути принудительно запускаем пересчет кнопки!
    validateFields();
}

void Start_progect::create_progect()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    int pageCount = ui->stackedWidget->count();

    // =========================================================================
    // СЦЕНАРИЙ А: Мы на ПОСЛЕДНЕЙ странице (индекс 2) — ФИНАЛЬНЫЙ КЛИК "Создать проект"
    // =========================================================================
    if (currentIndex == pageCount - 1)
    {
        QString projRoot = ui->dir_root_folder->text().trimmed();
        QString projName = ui->name_progect->text().trimmed();
        if (projName.isEmpty()) projName = "New_PyTorch_Project";

        QString fullProjectPath = projRoot + "/" + projName;
        QDir dir;

        // 1. Физически разворачиваем структуру ИИ-папок на жестком диске Arch Linux
        if (dir.mkpath(fullProjectPath + "/datasets/train") &&
            dir.mkpath(fullProjectPath + "/datasets/val") &&
            dir.mkpath(fullProjectPath + "/models") &&
            dir.mkpath(fullProjectPath + "/weights"))
        {
            // Создаем пустой __init__.py для корректного импорта моделей в Python
            QFile initFile(fullProjectPath + "/models/__init__.py");
            if (initFile.open(QIODevice::WriteOnly)) initFile.close();

            // Генерируем базовый каркас главного скрипта обучения train.py
            QFile trainFile(fullProjectPath + "/train.py");
            if (trainFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&trainFile);
                out << "import sys\nimport json\nimport time\n\n";
                out << "def main():\n    print(json.dumps({'status': 'ready'}))\n    sys.stdout.flush()\n\n";
                out << "if __name__ == '__main__':\n    main()\n";
                trainFile.close();
            }

            // =========================================================================
            // 2. ДИНАМИЧЕСКАЯ СБОРКА REQUIREMENTS.TXT НА ОСНОВЕ ВАШИХ РАДИОБАТТОНОВ
            // =========================================================================
            QFile reqFile(fullProjectPath + "/requirements.txt");
            if (reqFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QTextStream out(&reqFile);

                // ВАРИАНТ 1: Выбран радиобаттон облегченной CPU-версии
                if (ui->rbCpuVersion->isChecked())
                {
                    // ИСПРАВЛЕНИЕ: Используем официальноеwhl-зеркало CPU-сборок PyTorch
                    out << "--index-url https://pytorch.org\n";

                    out << "torch\n";
                    out << "torchvision\n";
                    out << "torchaudio\n";
                }
                // ВАРИАНТ 2: Выбран радиобаттон полной CUDA-версии (или остальные варианты)
                else
                {
                    // Для CUDA/GPU-сборок в Arch Linux пишем стандартный набор.
                    out << "torch\n";
                    out << "torchvision\n";
                    out << "torchaudio\n";
                }

                reqFile.close();
            }

            // =========================================================================
            // 3. НОВОЕ: ГЕНЕРАЦИЯ КОНТРОЛЬНОГО ФАЙЛА ПРОЕКТА (*.pystudio) В JSON
            // =========================================================================
            QFile configFile(fullProjectPath + "/" + projName + ".pystudio");
            if (configFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QJsonObject configObject;

                // Записываем базовые метаданные создаваемого ИИ-проекта
                configObject["project_name"] = projName;
                configObject["architecture"] = ui->rbCpuVersion->isChecked() ? "CPU" : "CUDA";
                configObject["dataset_path"] = ui->dyr_dataset->text().trimmed();

                // Бронируем дефолтные гиперпараметры под будущую центральную панель обучения
                configObject["epochs"] = 10;
                configObject["batch_size"] = 32;
                configObject["learning_rate"] = 0.001;
                configObject["device"] = ui->rbCpuVersion->isChecked() ? "cpu" : "cuda:0";

                // Упаковываем JSON-документ со стандартными отступами для красивого сохранения
                QJsonDocument jsonDoc(configObject);
                configFile.write(jsonDoc.toJson(QJsonDocument::Indented));
                configFile.close();
            }

            // Закрываем модальное окно и передаем управление в главное окно Neuro_programm
            this->accept();
        }
        return;
    }

    // =========================================================================
    // СЦЕНАРИЙ Б: Промежуточные шаги (0 или 1) — КНОПКА РАБОТАЕТ КАК "Далее"
    // =========================================================================
    ui->btnBack->setVisible(true);

    QListWidgetItem *currentItem = ui->listWidget->item(currentIndex);
    if (currentItem) {
        if (currentIndex == 0)      currentItem->setText("  Размещение");
        else if (currentIndex == 1) currentItem->setText("  Конфигурация");

        QFont normalFont = currentItem->font();
        normalFont.setBold(false);
        currentItem->setFont(normalFont);
    }

    int nextIndex = currentIndex + 1;

    QListWidgetItem *nextItem = ui->listWidget->item(nextIndex);
    if (nextItem) {
        if (nextIndex == 1)      nextItem->setText("> Конфигурация");
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


QString Start_progect::getProjectName() const
{
    return ui->name_progect->text().trimmed();
}

QString Start_progect::getProjectLocation() const
{
    return ui->dir_root_folder->text().trimmed();
}

QString Start_progect::getDatasetLocation() const
{
    if (ui->rbDatasetPath->isChecked())
        return ui->dyr_dataset->text().trimmed();
    return QString("");
}

void Start_progect::validateFields()
{
    int currentIndex = ui->stackedWidget->currentIndex();
    bool isValid = false;

    // Конструкция switch должна открываться и закрываться строго ОДИН раз
    switch (currentIndex)
    {
    case 0: // ШАГ 1: Размещение
    {
        isValid = !ui->name_progect->text().trimmed().isEmpty() &&
                  !ui->dir_root_folder->text().trimmed().isEmpty();
        break;
    } // Убедитесь, что здесь скобка закрывает ТОЛЬКО блок case 0

    case 1: // ШАГ 2: Конфигурация venv и архитектуры PyTorch
    {
        // Вариант 1: Полная автоустановка venv
        if (ui->rbCreateVenv->isChecked()) {
            isValid = true;
        }
        // Вариант 2: Существующий venv (требует заполненный путь)
        else if (ui->rbExistingVenv->isChecked()) {
            isValid = !ui->dir_folder_venve->text().trimmed().isEmpty();
        }
        // Вариант 3: Выборочная CPU-установка
        else if (ui->rbCpuVersion->isChecked()) {
            isValid = true;
        }
        // Вариант 4: Выборочная CUDA-установка
        else if (ui->rbCudaVersion->isChecked()) {
            isValid = true;
        }
        break;
    }

    case 2: // ШАГ 3: Структура данных
    {
        if (ui->rbDatasetOption1->isChecked() || ui->rbDatasetOption2->isChecked()) {
            isValid = true;
        }
        else if (ui->rbDatasetPath->isChecked()) {
            isValid = !ui->dyr_dataset->text().trimmed().isEmpty();
        }
        break;
    }

    default:
        break;
    } // Эта скобка закрывает САМ switch

    // Применяем итоговый статус доступности к кнопке
    ui->btnNext->setEnabled(isValid);
}

bool Start_progect::shouldInstallVenv() const
{
    // Установка нужна во всех случаях, КРОМЕ использования уже существующего venv
    return !ui->rbExistingVenv->isChecked();
}

QString Start_progect::getPyTorchArchitecture() const
{
    if (ui->rbCpuVersion->isChecked())
    {
        return "CPU (Облегченная сборка)";
    }
    else if (ui->rbCudaVersion->isChecked())
    {
        return "GPU / CUDA (Полная сборка)";
    }
    // Дефолтный вариант на случай, если выбрана полная автоустановка venv
    return "GPU / CUDA (По умолчанию)";
}