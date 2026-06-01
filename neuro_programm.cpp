#include "neuro_programm.h"
#include "ui_neuro_programm.h"
#include "start_progect.h"
#include "panel_other.h"
#include "ui_panel_other.h"

#include <QFileSystemModel>
#include <QLabel>
#include <QScreen>
#include <QStyleFactory>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QButtonGroup>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QVariantList>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QTimer>
#include <QtCharts/QValueAxis>
#include <QPainter>
#include <QSettings>
#include <QShortcut>
#include <QKeySequence>
#include "codeeditor.h"
#include <thread>
#include <memory>
#include <cstdio>
#include <iostream>

Neuro_programm* Neuro_programm::self = nullptr;

Neuro_programm::Neuro_programm(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Neuro_programm)
{
    ui->setupUi(this);

    self = this;
    trainingProcess = nullptr;

    // 1. Создаем вертикальный сплиттер
    mainVerticalSplitter = new QSplitter(Qt::Vertical, this);

    // 2. Добавляем старый центр
    QWidget *oldCentral = ui->centralwidget;
    mainVerticalSplitter->addWidget(oldCentral);

    // 3. ВАЖНО: ВЫДЕЛЯЕМ ПАМЯТЬ ПОД PANEL_OTHER ТУТ!
    // Если этой строки нет, или она написана ниже — программа упадет!
    panelOther = new Panel_other(this);
    mainVerticalSplitter->addWidget(panelOther);

    // Назначаем новый центр
    setCentralWidget(mainVerticalSplitter);

    // Изначально скрываем панель
    panelOther->setVisible(false);

    // 1. Инициализируем глобальную переменную класса (без QPushButton* в начале!)
    btnTerminal = new QPushButton("💻 Терминал", this);
    btnSearch = new QPushButton("🔍 Поиск по коду", this);
    btnLogs = new QPushButton("📋 Логи PyTorch", this);
    btnTogglePip = new QPushButton("🛠 Управление пакетами", this);
    btnAIChat = new QPushButton("💬 ИИ-Ассистент", this);


    QWidget *leftSpacer = new QWidget(this);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    btnTerminal->setCheckable(true);
    btnSearch->setCheckable(true);
    btnLogs->setCheckable(true);
    btnTogglePip->setCheckable(true);
    btnAIChat->setCheckable(true);

    // Изначально при старте приложения нижняя панель закрыта, кнопки отжаты
    btnTerminal->setChecked(false);
    btnSearch->setChecked(false);
    btnLogs->setChecked(false);
    btnTogglePip->setChecked(false);

    // Настраиваем плоский аккуратный стиль в стиле Breeze
    QString statusBarBtnStyle =
        "QPushButton { border: none; padding: 4px 10px; background: transparent; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
        "QPushButton:checked { background-color: #d0d0d0; font-weight: bold; }";

    btnTerminal->setStyleSheet(statusBarBtnStyle);
    btnSearch->setStyleSheet(statusBarBtnStyle);
    btnLogs->setStyleSheet(statusBarBtnStyle);
    btnTogglePip->setStyleSheet(statusBarBtnStyle);
    btnAIChat->setStyleSheet(statusBarBtnStyle);

    // 2. Добавляем кнопку в левую часть статусбара
    ui->statusbar->addWidget(leftSpacer);
    ui->statusbar->addWidget(btnTerminal);
    ui->statusbar->addWidget(btnSearch);
    ui->statusbar->addWidget(btnLogs);
    ui->statusbar->addWidget(btnAIChat);
    ui->statusbar->addPermanentWidget(btnTogglePip);
    ui->statusbar->addPermanentWidget(btnTogglePip);

    // 2. СВЯЗЫВАЕМ СИГНАЛЫ КНОПОК С УМНЫМ ТРИГГЕРОМ ПОКАЗА/СКРЫТИЯ

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm в файле neuro_programm.cpp ---

    connect(btnAIChat, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            // Отжимаем остальные кнопки навигации в статусбаре
            btnTerminal->setChecked(false);
            btnSearch->setChecked(false);
            btnLogs->setChecked(false);
            btnTogglePip->setChecked(false);

            // ДИНАМИЧЕСКИЙ ПОИСК: Узнаем, под каким индексом страница чата реально лежит в стэке
            int realChatStackIndex = ui->centralStackedWidget->indexOf(ui->page_chat);

            if (realChatStackIndex != -1)
            {
                // Принудительно выставляем правильный индекс в комбобоксе
                // Наш синхронный коннект сам перелистнет stackedWidget на нужный экран!
                ui->fileComboBox->setCurrentIndex(realChatStackIndex);

                // Переводим фокус на строку ввода, чтобы пользователь мог сразу писать
                ui->inputChatText->setFocus();
            }
            else
            {
                // Защитный лог на случай, если страница в Designer была случайно удалена
                qWarning() << "Критическая ошибка: Страница page_chat не найдена внутри центрального QStackedWidget!";
            }
        }
        else
        {
            // Повторный клик — возвращаемся на пульт управления обучением (Индекс 0)
            ui->fileComboBox->setCurrentIndex(0);
            btnTerminal->setChecked(true);
        }
    });


    // connect(btnTerminal, &QPushButton::clicked, this, [this]() { btnAIChat->setChecked(false); });
    // connect(btnSearch,   &QPushButton::clicked, this, [this]() { btnAIChat->setChecked(false); });
    // connect(btnLogs,     &QPushButton::clicked, this, [this]() { btnAIChat->setChecked(false); });

    connect(btnTerminal, &QPushButton::clicked, this, [this]() {
        // Если панель была открыта именно на Терминале, то повторный клик её СКРЫВАЕТ
        if (panelOther->isVisible() && ui->statusbar->findChild<QPushButton*>("", Qt::FindDirectChildrenOnly) == nullptr /* проверка текущего индекса ниже */)
        {
            // Но проще и надежнее проверить напрямую состояние:
            // Если кнопка была нажата, а сейчас мы кликнули и индекс совпадает:
            static int lastIndex = -1;

            // Упростим логику до идеала:
            if (btnTerminal->isChecked()) {
                // Отжимаем все остальные кнопки навигации в статусбаре
                btnSearch->setChecked(false);
                btnLogs->setChecked(false);
                btnTogglePip->setChecked(false);

                panelOther->setVisible(true);
                panelOther->setTerminalPageActive();
                if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
            } else {
                // Повторный клик! Скрываем панель
                panelOther->setVisible(false);
            }
        }
    });

    // Чтобы код не раздувать, сделаем красивее и компактнее (Идеальный вариант для C++):
    // Перепишем обработчики кнопок на чистое переключение:

    connect(btnTerminal, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            btnSearch->setChecked(false);
            btnLogs->setChecked(false);
            btnTogglePip->setChecked(false);
            panelOther->setVisible(true);
            panelOther->setTerminalPageActive();
            if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
        } else {
            panelOther->setVisible(false);
            panelOther->togglePipPanel(false);
        }
    });

    connect(btnSearch, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            btnTerminal->setChecked(false);
            btnLogs->setChecked(false);
            btnTogglePip->setChecked(false);
            panelOther->setVisible(true);
            panelOther->setSearchPageActive();
            if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
        } else {
            panelOther->setVisible(false);
        }
    });

    connect(btnLogs, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            btnTerminal->setChecked(false);
            btnSearch->setChecked(false);
            btnTogglePip->setChecked(false);
            panelOther->setVisible(true);
            panelOther->setLogsPageActive();
            if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
        } else {
            panelOther->setVisible(false);
        }
    });

    connect(btnTogglePip, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            // При включении PIP автоматически нажимается кнопка Терминала
            btnTerminal->setChecked(true);
            btnSearch->setChecked(false);
            btnLogs->setChecked(false);

            panelOther->setVisible(true);
            panelOther->togglePipPanel(true);
            if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
        } else {
            // При повторном клике по кнопке управления пакетами - скрываем только строку ввода,
            // но сам терминал логов оставляем открытым
            panelOther->togglePipPanel(false);
        }
    });

    // // Если пользователь закрыл PIP-панель крестиком внутри консоли:
    // connect(panelOther, &Panel_other::pipPanelClosed, this, [this]() {
    //     if (btnTogglePip) btnTogglePip->setChecked(false);
    // });


    connect(btnSearch, &QPushButton::clicked, this, [this]() {
        if (btnTogglePip) btnTogglePip->setChecked(false);
    });

    connect(btnLogs, &QPushButton::clicked, this, [this]() {
        if (btnTogglePip) btnTogglePip->setChecked(false);
    });

    projectModel = nullptr;


    // projectModel = new QFileSystemModel(this);

    // // 1. Указываем модели отображать АБСОЛЮТНО ВСЕ файлы и папки (включая скрытые, если нужно)
    // projectModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);

    // // Указываем модели, на какой путь смотреть по умолчанию при старте программы (например, домашняя папка)
    // //projectModel->setRootPath(QDir::homePath());

    // projectModel->setNameFilters(QStringList() << "[^v]*" << "v[^e]*" << "ve[^n]*" << "ven[^v]*" << "venv?*");
    // projectModel->setNameFilterDisables(false);

    // // Привязываем к QTreeView из Designer
    // ui->treeView->setModel(projectModel);

    // // Скрываем служебные колонки ОС (Размер, Тип, Дата изменения)
    // ui->treeView->setHeaderHidden(true);
    // for (int i = 1; i < projectModel->columnCount(); ++i) {
    //     ui->treeView->hideColumn(i);
    // }

    // // Настраиваем режим плавного раскрытия папок по одинарному клику (опционально)
    // ui->treeView->setAnimated(true);
    ui->treeView->setIndentation(20);

    // ИСПРАВЛЕННЫЙ STYLE SHEET (Все стыки с деревом и статусбаром строго 1px)
    this->setStyleSheet(R"(
        /* --- НАСТРОЙКА МЕНЮБЛOКОВ --- */
        QMenuBar {
            background-color: #f0f0f0;
            color: #333333;
            margin: 0px;
            padding: 0px;
            border-bottom: 1px solid #b0b0b0;
        }

        QMenuBar::item {
            background-color: transparent;
            padding: 4px 8px;
        }

        QMenuBar::item:selected {
            background-color: #e0e0e0;
        }

        QMenu {
            background-color: #f0f0f0;
            border: 1px solid #b0b0b0;
            color: #333333;
        }

        QMenu::item:selected {
            background-color: #e0e0e0;
        }

        /* --- НАСТРОЙКА ЗАГОЛОВКОВ QDOCKWIDGET --- */
        QDockWidget::title {
            background-color: #f0f0f0;
            color: #333333;
            border-left: 1px solid #b0b0b0;
            border-right: 1px solid #b0b0b0;
            border-bottom: 1px solid #b0b0b0;
            border-top: none;
            padding-left: 6px;
            padding-top: 4px;
            padding-bottom: 4px;
            height: 18px;
            margin: 0px;
        }

        /* Скрываем кнопки доков в обычном состоянии */
        QDockWidget::close-button, QDockWidget::float-button {
            qproperty-icon: none;
            background: transparent;
            border: none;
            width: 0px;
            height: 0px;
        }

        /* Показываем кнопки только при наведении на заголовок дока */
        QDockWidget::title:hover QDockWidget::close-button,
        QDockWidget::title:hover QDockWidget::float-button {
            qproperty-icon: theme("window-close");
            width: 16px;
            height: 16px;
            padding: 2px;
        }

        QDockWidget::close-button:hover, QDockWidget::float-button:hover {
            background-color: #e0e0e0;
            border-radius: 2px;
        }

        /* Внутреннее содержимое дока (контейнер) */
        /* Оставляем рамку только по бокам, нижнюю убираем, чтобы она не сливалась со статусбаром */
        #dockWidgetContents {
            border-left: 1px solid #b0b0b0;
            border-right: 1px solid #b0b0b0;
            border-bottom: none;
            border-top: none;
        }

        /* --- НАСТРОЙКА ЦЕНТРАЛЬНОЙ ФАЛЬШ-ПАНЕЛИ --- */
        #widget_3 {
            background-color: #f0f0f0;
            border-top: none;
            border-bottom: 1px solid #b0b0b0;
            border-left: none;
            border-right: none;
            min-height: 29px;
            max-height: 29px;
            margin-left: -2px;
            margin-right: -2px;
            margin-top: 0px;
            margin-bottom: 0px;
            padding-left: 2px;
            padding-right: 2px;
        }

        /* Системные разделители QMainWindow */
        QMainWindow::separator {
            background-color: #b0b0b0;
            width: 1px;
            height: 1px;
            margin: 0px;
            padding: 0px;
        }

        /* --- НАСТРОЙКА ДЕРЕВА (QTreeView) --- */
        QTreeView {
            background-color: #ffffff;
            color: #333333;

            /* КОРРЕКЦИЯ ЗДЕСЬ: Жестко отключаем рамку у самого дерева, */
            /* так как внешние границы формирует контейнер дока */
            border: none;

            show-decoration-selected: 1;
        }

        QTreeView::item:hover {
            background-color: #f2f2f2;
        }

        QTreeView::item:selected {
            background-color: #e0e0e0;
            color: #000000;
        }

        /* --- НАСТРОЙКА СТРОКИ СОСТОЯНИЯ (QStatusBar) --- */
        QStatusBar {
            background-color: #f0f0f0;
            color: #333333;

            /* КОРРЕКЦИЯ ЗДЕСЬ: Рисуем тонкую линию СВЕРХУ статусбара. */
            /* На нее будут ровно опираться доки своим бесшовным нижним краем */
            border-top: 1px solid #b0b0b0;
            border-bottom: none;
            border-left: none;
            border-right: none;
        }
        /* --- НАСТРОЙКА COMBOBOX НА ФАЛЬШ-ПАНЕЛИ (ИДЕАЛЬНЫЙ FLAT UI) --- */
#widget_3 QComboBox {
        background-color: transparent !important;

        /* Плоские линии рамок (без верхней) */
        border-left: 1px solid #d0d0d0 !important;
        border-right: 1px solid #d0d0d0 !important;
        border-bottom: 1px solid #d0d0d0 !important;
        border-top: none !important;
        border-radius: 0px !important;

        /* Отступы: зажимаем текст слева и справа */
        padding-left: 6px !important;
        padding-right: 25px !important;
        color: #333333 !important;
        font-size: 11px !important;
    }

    /* Подсветка комбобокса при наведении — МЯГКО И ЦЕЛИКОМ */
    #widget_3 QComboBox:hover {
        border-left: 1px solid #b0b0b0 !important;
        border-right: 1px solid #b0b0b0 !important;
        border-bottom: 1px solid #b0b0b0 !important;
        border-top: none !important;
        background-color: #e5e5e5 !important;
    }

    #widget_3 QComboBox:on {
        border-left: 1px solid #b0b0b0 !important;
        border-right: 1px solid #b0b0b0 !important;
        border-bottom: 1px solid #b0b0b0 !important;
        border-top: none !important;
        background-color: #ffffff !important;
    }

    /* НАМЕРТВО УНИЧТОЖАЕМ СИСТЕМНУЮ КНОПКУ BREEZE, КОТОРАЯ ДАВАЛА СБОЙНЫЙ ПРЯМОУГОЛЬНИК */
    #widget_3 QComboBox::drop-down,
    #widget_3 QComboBox::drop-down:hover {
        width: 0px !important;
        border: none !important;
        background: transparent !important;
    }
    #widget_3 QComboBox::down-arrow {
        image: none !important;
        border: none !important;
    }

    /* Настройка выпадающего списка */
    #widget_3 QComboBox QAbstractItemView {
        background-color: #ffffff !important;
        border: 1px solid #b0b0b0 !important;
        border-radius: 0px !important;
        color: #333333 !important;
        selection-background-color: #e0e0e0 !important;
        selection-color: #000000 !important;
        outline: none !important;
    }


    )");

    //panelOther->setVisible(false);

    connect(ui->New_progect, &QAction::triggered, this, &Neuro_programm::new_progect);
    connect(ui->open_progect, &QAction::triggered, this, &Neuro_programm::open_project);

    // Очищаем комбобокс и стэк-виджет от тестовых данных из Designer
    ui->fileComboBox->clear();

    ui->fileComboBox->setCurrentIndex(0);

    // 2. Дублируем синхронизацию для левой боковой панели документов
    if (ui->openFilesListWidget) {
        ui->openFilesListWidget->setCurrentRow(0);
    }

    ui->fileComboBox->addItem("🎛 Панель обучения ИИ", QVariant("MAIN_SCREEN"));
    ui->fileComboBox->addItem("💬 ИИ-Ассистент", QVariant("AI_CHAT_SCREEN"));

    while (ui->centralStackedWidget->count() > 2)
    {
        QWidget *w = ui->centralStackedWidget->widget(2);
        ui->centralStackedWidget->removeWidget(w);
        delete w;
    }

    // 1. Коннект двойного щелчка по дереву файлов
    connect(ui->treeView, &QTreeView::doubleClicked, this, &Neuro_programm::onFileDoubleClicked);

    // 2. Коннект смены элемента в комбобоксе (переключение файлов пользователем)
    // Используем современную сигнатуру для переключения индексов stackedWidget
    connect(ui->fileComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < ui->centralStackedWidget->count())
        {
            // Перелистываем страницу стэка
            ui->centralStackedWidget->setCurrentIndex(index);

            // Синхронизируем выделение строки в левом боковом списке открытых документов
            if (ui->openFilesListWidget) {
                ui->openFilesListWidget->setCurrentRow(index);
            }

            // Умное управление док-виджетами и кнопками на основе ключа userData
            QString currentKey = ui->fileComboBox->itemData(index).toString();

            if (currentKey == "MAIN_SCREEN")
            {
                //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(true); // Показываем док
                if (btnTerminal) btnTerminal->setChecked(true);  // Подсвечиваем Терминал
                if (btnAIChat)   btnAIChat->setChecked(false);   // Тушим Чат в статусбаре
            }
            else if (currentKey == "AI_CHAT_SCREEN")
            {
                //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(false); // Прячем док
                if (btnAIChat)   btnAIChat->setChecked(true);    // Зажигаем Чат в статусбаре
                if (btnTerminal) btnTerminal->setChecked(false); // Тушим Терминал
            }
            else
            {
                // Если выбран любой динамический файл кода (индексы >= 2)
                //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(false);
                if (btnTerminal) btnTerminal->setChecked(false);
                if (btnAIChat)   btnAIChat->setChecked(false);
            }
        }
    });

    // 3. Коннект кнопки закрытия текущего файла
    connect(ui->btnCloseFile, &QPushButton::clicked, this, &Neuro_programm::onCloseCurrentFileClicked);

    // Найдите этот блок внутри конструктора Neuro_programm::Neuro_programm:

    connect(ui->fileComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < ui->centralStackedWidget->count())
        {
            // Перелистываем страницу стэка
            ui->centralStackedWidget->setCurrentIndex(index);

            // Сразу синхронизируем выделение строки в левом боковом списке
            if (ui->openFilesListWidget) {
                ui->openFilesListWidget->setCurrentRow(index);
            }

            // Умное управление док-виджетом на основе ключа userData
            QString currentKey = ui->fileComboBox->itemData(index).toString();

            if (currentKey == "MAIN_SCREEN") {
                //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(true); // Возвращаем док параметров
                if (btnTerminal) btnTerminal->setChecked(true); // Подсвечиваем кнопку Терминала в статусбаре
                if (btnAIChat)   btnAIChat->setChecked(false);
            }
            else if (currentKey == "AI_CHAT_SCREEN") {
                //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(false); // Прячем док
                if (btnAIChat)   btnAIChat->setChecked(true);  // Зажигаем кнопку ИИ в статусбаре
                if (btnTerminal) btnTerminal->setChecked(false);
            }
            else {
                // Если выбран любой динамический файл кода (индексы >= 2)
                //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(false);
                if (btnTerminal) btnTerminal->setChecked(false);
                if (btnAIChat)   btnAIChat->setChecked(false);
            }
        }
    });


    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm ---

    // 1. Создаем кастомный виджет для шапки дока
    QWidget *customTitleWidget = new QWidget(ui->leftDockWidget);

    // Задаем горизонтальный слой с минимальными отступами
    QHBoxLayout *titleLayout = new QHBoxLayout(customTitleWidget);
    titleLayout->setContentsMargins(10, 4, 10, 4); // Небольшие аккуратные отступы
    titleLayout->setSpacing(0);

    // 2. Создаем красивый текстовый заголовок
    QLabel *titleLabel = new QLabel("📁 Открытые файлы и проект", customTitleWidget);

    // Настраиваем строгий полужирный шрифт в стиле Breeze
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(10); // Аккуратный компактный размер
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #232629;"); // Контрастный темно-серый цвет Breeze

    // Добавляем текст в слой шапки
    titleLayout->addWidget(titleLabel);

    // Добавляем пружину, чтобы прижать текст к левому краю
    titleLayout->addStretch();

    // 3. НАМЕРТВО УСТАНАВЛИВАЕМ КАСТОМНУЮ ШАПКУ В ВАШ ЛЕВЫЙ ДОК
    // (Замените leftDockWidget на реальное objectName вашего дока из Designer)
    ui->leftDockWidget->setTitleBarWidget(customTitleWidget);

    // Накатываем общий стиль для рамки шапки дока через CSS главного окна
    ui->leftDockWidget->setStyleSheet(
        "QDockWidget {"
        "   border: 1px solid #b0b0b0;"
        "}"
        "QDockWidget::title {"
        "   background-color: #eff0f1;" /* Светло-серая благородная подложка Breeze */
        "   border-bottom: 1px solid #b0b0b0;"
        "}"
        );

    // 1. Очищаем боковой список
    ui->openFilesListWidget->clear();

    // Добавляем постоянный заголовок панели обучения в боковой список открытых документов
    QListWidgetItem *mainScreenItem = new QListWidgetItem("🎛 Панель обучения ИИ", ui->openFilesListWidget);
    mainScreenItem->setData(Qt::UserRole, QString("MAIN_SCREEN"));

    // Добавляем постоянный заголовок ИИ-чата в боковой список
    QListWidgetItem *chatScreenItem = new QListWidgetItem("💬 ИИ-Ассистент", ui->openFilesListWidget);
    chatScreenItem->setData(Qt::UserRole, QString("AI_CHAT_SCREEN"));

    // По умолчанию при старте выделена первая строка (Панель ИИ)
    ui->openFilesListWidget->setCurrentRow(0);

    // 2. ЖЕСТКО СКРЫВАЕМ НИЖНИЙ КОНТЕЙНЕР (Скроется и список, и его заголовок!)
    if (ui->openFilesContainer) {
        ui->openFilesContainer->setVisible(false);
    }

    // НАСТРОЙКА КОРРЕКТНОЙ ГЕОМЕТРИИ СПЛИТТЕРА ПРИ СТАРТЕ
    if (ui->leftVerticalSplitter)
    {
        // 1. Разрешаем сплиттеру полностью "схлопывать" нижнюю панель в ноль пикселей!
        ui->leftVerticalSplitter->setCollapsible(0, false); // Верхнее дерево схлопывать нельзя
        ui->leftVerticalSplitter->setCollapsible(1, true);  // Нижний список — МОЖНО схлопнуть

        // 2. КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Жестко отдаем ВСЁ место верхнему элементу (дереву),
        // а нижнему контейнеру прописываем ровно 0 пикселей.
        ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
    }

    connect(ui->openFilesListWidget, &QListWidget::itemDoubleClicked, this, &Neuro_programm::onOpenFileListItemDoubleClicked);


    // 1. Привязываем кнопку отправки (наш коннект)
    connect(ui->btnSendChat, &QPushButton::clicked, this, &Neuro_programm::sendChatMessageToAI);

    // 2. Устанавливаем фильтр клавиатуры на многострочное текстовое поле
    // (Это заставит работать комбинацию Ctrl + Enter, которую мы писали в eventFilter)
    ui->inputChatText->installEventFilter(this);

    ui->comboBatchSize->clear();
    ui->comboBatchSize->addItems(QStringList() << "4" << "8" << "16" << "32" << "64" << "128" << "256");

    // --- ЕЖЕСЕКУНДНЫЙ ТАЙМЕР МОНИТОРИНГА НАГРУЗКИ ЖЕЛЕЗА ---
    QTimer *monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this]() {

        // =========================================================================
        // 1. РАСЧЕТ ЗАГРУЗКИ CPU ИЗ СИСТЕМНОГО ЯДРА LINUX (/proc/stat)
        // =========================================================================
        static long double oldUser = 0, oldNice = 0, oldSystem = 0, oldIdle = 0;

        QFile file("/proc/stat");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            QString line = in.readLine(); // Читаем самую первую строчку "cpu ..."
            file.close();

            QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (tokens.size() > 4)
            {
                long double user   = tokens[1].toDouble();
                long double nice   = tokens[2].toDouble();
                long double system = tokens[3].toDouble();
                long double idle   = tokens[4].toDouble();

                long double oldTotal = oldUser + oldNice + oldSystem + oldIdle;
                long double newTotal = user + nice + system + idle;

                long double totalDiff = newTotal - oldTotal;
                long double idleDiff  = idle - oldIdle;

                if (totalDiff > 0) {
                    int cpuPercentage = static_cast<int>(100.0 * (totalDiff - idleDiff) / totalDiff);
                    // Задаем живое значение на толстый CPU прогресс-бар!
                    if (ui->progressCPU) ui->progressCPU->setValue(cpuPercentage);
                }

                oldUser = user; oldNice = nice; oldSystem = system; oldIdle = idle;
            }
        }

        // =========================================================================
        // 2. РАСЧЕТ ЗАГРУЗКИ GPU (ЧЕРЕЗ КИШКИ ДРАЙВЕРА NVIDIA)
        // =========================================================================
        if (ui->progressGPU)
        {
            // Проверяем: если сейчас выбран режим "cpu" в комбобоксе, то GPU простаивает (0%)
            if (ui->comboDevice && ui->comboDevice->currentText() == "cpu") {
                ui->progressGPU->setValue(0);
            }
            else
            {
                // Если выбран режим CUDA — быстро и асинхронно спрашиваем загрузку ядер у видеодрайвера
                QProcess query;
                // Команда возвращает чистое число процентов нагрузки (например, "45")
                query.start("nvidia-smi", QStringList() << "--query-gpu=utilization.gpu" << "--format=csv,noheader,nounits");

                if (query.waitForFinished(100) && query.exitCode() == 0) {
                    int gpuPercentage = QString::fromUtf8(query.readAllStandardOutput()).trimmed().toInt();
                    ui->progressGPU->setValue(gpuPercentage);
                } else {
                    // Если nvidia-smi не установлена на ПК, просто ставим 0
                    ui->progressGPU->setValue(0);
                }
            }
        }
    });

    // Запускаем непрерывный ежесекундный опрос загрузки
    monitorTimer->start(1000);

    // Принудительно выставляем элемент "32" активным по умолчанию при старте программы
    ui->comboBatchSize->setCurrentText("32");

    // ui->btnStartTraining->disconnect();

    // // НАМЕРТВО ПРИВЯЗЫВАЕМ СИГНАЛ НАПРЯМУЮ ЧЕРЕЗ ЛЯМБДУ
    // connect(ui->btnStartTraining, &QPushButton::clicked, this, [this]() {
    //     // Выводим тестовую зеленую строчку во ВНЕШНИЙ терминал Arch Linux,
    //     // чтобы прямо сейчас увидеть в консоли, что клик физически сработал!
    //     qInfo() << "📱 [GUI ТРИГГЕР] Физический клик по кнопке Старт зафиксирован ядрами C++!";

    //     // Вызываем сам метод запуска обучения
    //     this->onStartTrainingClicked();
    // });

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm в neuro_programm.cpp ---

    // Динамически сканируем всю графическую форму и ищем виджет с именем btnStartTraining
    QPushButton *realStartButton = this->findChild<QPushButton*>("btnStartTraining");

    if (realStartButton != nullptr)
    {
        qInfo() << "🟢 [СИСТЕМНЫЙ ЛОГ] Кнопка btnStartTraining успешно найдена в памяти GUI!";

        // Принудительно отключаем любые старые Designer-связи
        realStartButton->disconnect();

        // Жестко привязываем сигнал клика напрямую
        connect(realStartButton, &QPushButton::clicked, this, [this, realStartButton]() {
            qInfo() << "📱 [GUI КЛИК] Сигнал успешно пробит! Вызываем запуск обучения.";

            // Вручную проверяем, не заблокирована ли кнопка какими-то слоями
            if (!realStartButton->isVisible()) {
                qWarning() << "⚠️ Внимание: Кнопка скрыта с экрана (isVisible == false)!";
            }

            // Запускаем основной метод
            this->onStartTrainingClicked();
        });
    }
    else
    {
        // Если qmake подсовывает старый ui_*.h, кнопка не найдена. Мы выведем это жирным красным цветом!
        qCritical() << "🔴 [КРИТИЧЕСКАЯ ОШИБКА IDE] Кнопка с objectName 'btnStartTraining' ФИЗИЧЕСКИ ОТСУТСТВУЕТ на форме!";
        qCritical() << "Проверьте имя objectName большой зеленой кнопки в Qt Designer.";
    }


    connect(ui->btnStopTraining,  &QPushButton::clicked, this, &Neuro_programm::onStopTrainingClicked);

    if (ui->widgetRightCharts) {
        ui->widgetRightCharts->setVisible(false);
    }

    // 2. Настраиваем пропорции главного горизонтального сплиттера
    if (ui->mainHorizontalSplitter) {
        // Первое число (1000) — ширина левой области (код/пульт параметров)
        // Второе число (0) — ширина правой области (наши графики)
        ui->mainHorizontalSplitter->setSizes(QList<int>({1000, 0}));

        // Запрещаем пользователю случайно "захлопнуть" левую область кода мышкой в ноль
        ui->mainHorizontalSplitter->setCollapsible(0, false);
        ui->mainHorizontalSplitter->setCollapsible(1, true);  // А правые графики — можно схлопывать
    }

    // Инициализируем наш график (код из предыдущего шага)
    initLossChart();

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm в файле neuro_programm.cpp ---

    // 1. Создаем подменю "Открыть недавние"
    recentProjectsMenu = new QMenu("Открыть недавние", this);

    QMenuBar *bar = this->menuBar();
    if (bar) {
        // Сканируем все существующие вкладки (Файл, Справка и т.д.)
        QList<QAction*> actions = bar->actions();
        bool attached = false;

        for (QAction *action : actions) {
            // Если нашли меню, текст которого содержит слово "Файл" (или "File")
            if (action->menu() && (action->text().contains("Файл") || action->text().contains("File"))) {
                action->menu()->addMenu(recentProjectsMenu); // Встраиваем подменю "Открыть недавние" внутрь него
                attached = true;
                break;
            }
        }

        // Резервный вариант: если меню "Файл" вдруг не нашлось по тексту,
        // мы просто добавим "Открыть недавние" прямой кнопкой на верхнюю панель
        if (!attached) {
            bar->addMenu(recentProjectsMenu);
        }
    }

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm в файле neuro_programm.cpp ---

    // 1. Создаем действие "Сохранить" (Ctrl + S)
    QAction *actionSave = new QAction("💾 Сохранить", this);
    actionSave->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S)); // Подсказка горячих клавиш в меню
    connect(actionSave, &QAction::triggered, this, &Neuro_programm::saveCurrentActiveFile);

    // 2. Создаем действие "Сохранить всё" (Ctrl + Shift + S)
    QAction *actionSaveAll = new QAction("📚 Сохранить всё", this);
    actionSaveAll->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(actionSaveAll, &QAction::triggered, this, &Neuro_programm::saveAllProjectChanges);

    // 3. Создаем наше старое подменю "Открыть недавние"
    recentProjectsMenu = new QMenu("📁 Открыть недавние", this);

    // 4. ИНТЕГРАЦИЯ В ВЕРХНЕЕ МЕНЮ "ФАЙЛ" ПО ЕГО НАЗВАНИЮ
    //QMenuBar *bar = this->menuBar();
    if (bar)
    {
        QList<QAction*> actions = bar->actions();
        bool attached = false;

        for (QAction *action : actions)
        {
            // Ищем вкладку меню, которая содержит слово "Файл" или "File"
            if (action->menu() && (action->text().contains("Файл") || action->text().contains("File")))
            {
                QMenu *fileMenu = action->menu();

                fileMenu->addSeparator();             // Рисуем тонкую разделительную линию Breeze
                fileMenu->addAction(actionSave);      // Вставляем пункт "Сохранить"
                fileMenu->addAction(actionSaveAll);   // Вставляем пункт "Сохранить всё"
                fileMenu->addSeparator();             // Рисуем вторую линию разделителя
                fileMenu->addMenu(recentProjectsMenu); // Вставляем наш список недавних проектов

                attached = true;
                break;
            }
        }

        // Создаем действие "Закрыть проект"
        QAction *actionCloseProject = new QAction("❌ Закрыть проект", this);
        actionCloseProject->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
        connect(actionCloseProject, &QAction::triggered, this, &Neuro_programm::onCloseProjectClicked);

        // ИНТЕГРИРУЕМ В НАШ СУЩЕСТВУЮЩИЙ ЦИКЛ ДИНАМИЧЕСКОГО ПОИСКА ВКЛАДКИ "ФАЙЛ"
        QMenuBar *bar = this->menuBar();
        if (bar)
        {
            QList<QAction*> actions = bar->actions();
            for (QAction *action : actions)
            {
                if (action->menu() && (action->text().contains("Файл") || action->text().contains("File")))
                {
                    QMenu *fileMenu = action->menu();

                    // =================================================================
                    // ИСПРАВЛЕНИЕ: Просто добавляем кнопку в конец меню без угадывания имен!
                    // =================================================================
                    fileMenu->addSeparator();            // Тонкая разделительная черта Breeze
                    fileMenu->addAction(actionCloseProject); // Вставляем пункт "Закрыть проект"
                    break;
                }
            }
        }


        // Резервный вариант: если меню "Файл" не найдено, выводим элементы на верхнюю панель напрямую
        if (!attached) {
            bar->addAction(actionSave);
            bar->addAction(actionSaveAll);
            bar->addMenu(recentProjectsMenu);
        }
    }


    // 2. Инициализируем массив скрытых действий (экшенов) для недавних проектов
    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentProjectActions[i] = new QAction(this);
        recentProjectActions[i]->setVisible(false); // Изначально они невидимы, пока список пуст

        // Связываем клик по недавнему проекту с нашей функцией открытия
        connect(recentProjectActions[i], &QAction::triggered, this, &Neuro_programm::openRecentProject);

        // Добавляем экшен в подменю
        recentProjectsMenu->addAction(recentProjectActions[i]);
    }

    // 3. ЗАГРУЖАЕМ ИЗ ПАМЯТИ И ВЫВОДИМ СПИСОК ПРИ СТАРТЕ ПРОГРАММЫ
    updateRecentProjectActions();

    detectCudaDevices();
}

Neuro_programm::~Neuro_programm()
{
    delete ui;
}

void Neuro_programm::new_progect()
{
    rsc = new Start_progect(this);
    rsc->wf = this;
    rsc->setWindowTitle("Выбор режима работы");

    if (rsc->exec() == QDialog::Accepted)
    {
        QString projName = rsc->getProjectName();
        QString projRoot = rsc->getProjectLocation();
        if (projName.isEmpty()) projName = "New_AI_Project";

        QString fullProjectPath = projRoot + "/" + projName;

        if (bootstrapProjectStructure(fullProjectPath))
        {
            // =================================================================
            // АБСОЛЮТНАЯ ЗАЩИТА: Проверяем, инициализирована ли модель в памяти
            // =================================================================

            initProjectTreeModel(fullProjectPath);

            // Запуск терминала с установкой venv (если требуется)
            if (rsc->shouldInstallVenv() && panelOther)
            {
                panelOther->setVisible(true);
                if (btnTerminal) btnTerminal->setChecked(true);
                if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));

                // ИСПРАВЛЕНИЕ: Объявляем строковую переменную и забираем её из мастера
                QString archType = rsc->getPyTorchArchitecture();

                // Теперь переменная существует в текущей области видимости и передается в терминал без ошибок
                panelOther->startVenvInstallation(fullProjectPath, archType);
            }
        }
    }
    delete rsc;
    rsc = nullptr;

    if (projectModel)
    {
        // Считываем абсолютный путь к открытой папке из корня дерева файлов
        QString activePath = projectModel->filePath(ui->treeView->rootIndex());

        // Класс QDir мгновенно вытащит чистое имя папки (например, из "/home/elf/ааа/z1" достанет "z1")
        QDir projectDir(activePath);
        QString autoProjectName = projectDir.dirName();

        // Собираем точный путь к контрольному файлу проекта .pystudio
        QString createdPystudioFile = activePath + "/" + autoProjectName + ".pystudio";

        // Добавляем свежесозданный проект в историю верхнего меню "Файл"
        addProjectToRecent(createdPystudioFile);
    }
}

void Neuro_programm::sync()
{
    // 1. Создаем модель файловой системы
    QFileSystemModel *model = new QFileSystemModel(this);

    // 2. Указываем путь к папке вашего Python-проекта (например, текущая папка приложения)
    QString projectPath = QDir::currentPath(); // Либо жесткий путь к вашей рабочей папке ИИ
    model->setRootPath(projectPath);

    // 3. Прячем лишние папки (например, venv и __pycache__) через фильтр имен
    QStringList nameFilters;
    nameFilters << "*" ; // Показываем всё, но...
    model->setNameFilters(nameFilters);
    model->setNameFilterDisables(false); // Скрывать файлы, не прошедшие фильтр (а не делать серыми)


    // 4. Подключаем модель к нашему TreeView
    ui->treeView->setModel(model);
    ui->treeView->setRootIndex(model->index(projectPath));

    // 5. Оставляем только одну колонку "Имя", скрывая Размер, Тип и Дату изменения (стиль IDE)
    ui->treeView->setHeaderHidden(true); // Прячем верхнюю шапку дерева
    for (int i = 1; i < model->columnCount(); ++i) {
        ui->treeView->hideColumn(i);
    }
}

bool Neuro_programm::bootstrapProjectStructure(const QString &rootPath)
{
    QDir dir;

    // 1. СПИСОК ПАПОК ДЛЯ СОЗДАНИЯ
    // Собираем все пути, которые нужно развернуть внутри корня проекта
    QStringList foldersToCreate = {
        rootPath + "/datasets/train",
        rootPath + "/datasets/val",
        rootPath + "/models",
        rootPath + "/weights"
    };

    // Циклом создаем каждую папку на диске Arch Linux
    for (const QString &folder : foldersToCreate) {
        if (!dir.mkpath(folder)) {
            qWarning() << "Не удалось создать директорию:" << folder;
            return false; // Если операционная система запретила доступ, прерываемся
        }
    }

    // 2. СОЗДАНИЕ СЛУЖЕБНОГО ФАЙЛА __init__.py В ПАПКЕ MODELS
    // Он нужен Python, чтобы папка models импортировалась как модуль
    QFile initFile(rootPath + "/models/__init__.py");
    if (initFile.open(QIODevice::WriteOnly)) {
        initFile.close(); // Оставляем его просто пустым
    }

    // 3. СОЗДАНИЕ КАРКАСА ГЛАВНОГО СКРИПТА ОБУЧЕНИЯ (train.py)
    QFile trainFile(rootPath + "/train.py");
    if (trainFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&trainFile);
        out.setEncoding(QStringConverter::Utf8); // Гарантируем кодировку UTF-8 в Qt6

        // Записываем базовый шаблон логгера, который будет слать JSON-метрики в ваше Qt GUI
        out << "import sys\n";
        out << "import json\n";
        out << "import time\n\n";
        out << "def main():\n";
        out << "    print(json.dumps({'status': 'initialized'}))\n";
        out << "    sys.stdout.flush()\n\n";
        out << "if __name__ == '__main__':\n";
        out << "    main()\n";

        trainFile.close();
    }

    // 4. СОЗДАНИЕ КАРКАСА СКРИПТА ТЕСТИРОВАНИЯ (test.py)
    QFile testFile(rootPath + "/test.py");
    if (testFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&testFile);
        out << "# Script for model evaluation and inference\n";
        out << "import torch\n";
        testFile.close();
    }

    // 5. СОЗДАНИЕ ДЕФОЛТНОГО ФАЙЛА requirements.txt
    QFile reqFile(rootPath + "/requirements.txt");
    if (reqFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&reqFile);
        // Сразу прописываем базовые зависимости для venv
        out << "torch\ntorchvision\ntorchaudio\n";
        reqFile.close();
    }

    return true; // Все папки и шаблоны успешно созданы!
}

void Neuro_programm::open_project()
{
    // 1. ОТКРЫВАЕМ СИСТЕМНЫЙ ДИАЛОГ ВЫБОРА ФАЙЛА ПРОЕКТА
    QString selectedFile = QFileDialog::getOpenFileName(
        this,
        "Открыть проект PyTorch Studio",
        QDir::homePath(),
        "Файлы проекта PyTorch Studio (*.pystudio)"
        );

    if (selectedFile.isEmpty()) return;

    // 2. ОТКРЫВАЕМ И ЧИТАЕМ JSON КОНФИГУРАЦИЮ
    QFile configFile(selectedFile);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sendSystemNotification("Ошибка открытия", "❌ Не удалось прочитать файл проекта");
        return;
    }

    QByteArray rawData = configFile.readAll();
    configFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(rawData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        sendSystemNotification("Ошибка конфигурации", "❌ Файл проекта поврежден или имеет неверный формат JSON");
        return;
    }

    QJsonObject configObject = jsonDoc.object();

    // 3. ИЗВЛЕКАЕМ СОХРАНЕННЫЕ ДАННЫЕ ПРОЕКТА (ОЧИЩЕНО ОТ ДУБЛИКАТОВ)
    QString projName     = configObject["project_name"].toString("New_Project");
    QString datasetPath  = configObject["dataset_path"].toString("");
    QString architecture = configObject["architecture"].toString("CUDA");
    QString savedDevice  = configObject["device"].toString("cpu");

    int epochs    = configObject["epochs"].toInt(10);
    int batchSize = configObject["batch_size"].toInt(32);
    double lr     = configObject["learning_rate"].toDouble(0.001);

    // Вычисляем путь к корневой папке проекта на основе расположения .pystudio файла
    QFileInfo fileInfo(selectedFile);
    QString fullProjectPath = fileInfo.absoluteDir().absolutePath();
    currentOpenProjectPath = fullProjectPath;


    // =========================================================================
    // 4. КРИТИЧЕСКИЙ ШАГ: СНАЧАЛА ИНИЦИАЛИЗИРУЕМ МОДЕЛЬ ДЕРЕВА И СТЭК-ВИДЖЕТ!
    // =========================================================================
    initProjectTreeModel(fullProjectPath);

    // Синхронизируем фокус на главную страницу (индекс 0)
    ui->centralStackedWidget->setCurrentIndex(0);
    if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);
    if (ui->openFilesListWidget) ui->openFilesListWidget->setCurrentRow(0);

    // =========================================================================
    // 5. ВЫПОЛНЕНИЕ ВАШЕГО ГЛАВНОГО УСЛОВИЯ: ОБЯЗАТЕЛЬНЫЙ ПЕРЕОПРОС ЖЕЛЕЗА ПРИ ОТКРЫТИИ
    // =========================================================================
    // Этот метод полностью очистит comboDevice и заполнит его РЕАЛЬНЫМИ картами ПК прямо сейчас!
    detectCudaDevices();

    // 6. СИНХРОНИЗАЦИЯ С СХРАНЕННЫМИ НАСТРОЙКАМИ
    // Выставляем числовые значения в счетчики
    // if (ui->spinBoxEpochs) ui->spinBoxEpochs->setValue(epochs);
    // if (ui->spinBoxLR) ui->spinBoxLR->setValue(lr);

    // Выставляем размер батча
    // if (ui->comboBatchSize) {
    //     int batchIdx = ui->comboBatchSize->findText(QString::number(batchSize));
    //     if (batchIdx != -1) ui->comboBatchSize->setCurrentIndex(batchIdx);
    // }

    // УМНАЯ ПРОВЕРКА ДЕВАЙСА: Проверяем, доступно ли на ЭТОМ компьютере сохраненное устройство
    if (ui->comboDevice) {
        int deviceIdx = ui->comboDevice->findText(savedDevice);
        if (deviceIdx != -1) {
            // Если сохраненная карта (например, cuda:0) найдена в системе — включаем её
            ui->comboDevice->setCurrentIndex(deviceIdx);
        } else {
            // Если в проекте была записана CUDA, но на ЭТОМ ПК видеокарты нет —
            // принудительно сбрасываем на безопасный cpu и выдаем D-Bus предупреждение!
            ui->comboDevice->setCurrentIndex(0);
            sendSystemNotification("Конфигурация железа",
                                   "⚠️ Предупреждение: Сохраненное устройство CUDA недоступно на этом ПК. Сброшено на CPU.");
        }
    }

    // Передаем актуальный путь в нижнюю панель для доустановки пакетов pip
    if (panelOther) {
        panelOther->setCurrentProjectPath(fullProjectPath);
    }

    // Обновляем заголовок главного окна ИИ-студии
    this->setWindowTitle(QString("PyTorch Studio - %1 [%2]").arg(projName).arg(fullProjectPath));
    sendSystemNotification("PyTorch Studio", QString("✔ Проект '%1' успешно загружен").arg(projName));
    addProjectToRecent(selectedFile);
}

void Neuro_programm::onFileDoubleClicked(const QModelIndex &index)
{
    // --- 1. ЗАЩИТА: Проверяем, инициализирована ли модель файловой системы ---
    if (!projectModel) return;

    // Считываем абсолютный путь к объекту на жестком диске Arch Linux
    QString filePath = projectModel->filePath(index);
    QFileInfo fileInfo(filePath);

    // ВАЖНОЕ УСЛОВИЕ: Если пользователь кликнул по ПАПКЕ — просто выходим
    if (fileInfo.isDir()) return;

    // --- 2. ЗАЩИТА ОТ ДУБЛИКАТОВ ВКЛАДОК ---
    // Сканируем комбобокс. Если файл уже открыт — переключаем фокус на него и выходим
    for (int i = 0; i < ui->fileComboBox->count(); ++i)
    {
        if (ui->fileComboBox->itemData(i).toString() == filePath)
        {
            ui->fileComboBox->setCurrentIndex(i);

            // Синхронизируем подсветку строки в боковом списке документов
            if (ui->openFilesListWidget) {
                ui->openFilesListWidget->setCurrentRow(i);
            }
            return;
        }
    }

    // --- 3. ФИЗИЧЕСКОЕ ЧТЕНИЕ ТЕКСТА С ДИСКА (Кодировка UTF-8) ---
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCritical() << "❌ ОШИБКА ФАЙЛА: Не удалось прочитать исходный код по пути:" << filePath;
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8); // Стандарт для Linux и Python-скриптов
    QString fileContent = in.readAll();
    file.close();

    // =========================================================================
    // 4. ДИНАМИЧЕСКОЕ СОЗДАНИЕ СТРАНИЦЫ И КАСTOMНОГО CodeEditor С НОМЕРАМИ СТРОК
    // =========================================================================

    // Создаем базовый QWidget новой изолированной страницы стэка
    QWidget *newPage = new QWidget(this);

    // Менеджер вертикальной компоновки элементов на этой странице
    QVBoxLayout *layout = new QVBoxLayout(newPage);

    // Намертво убираем внутренние рамки и отступы менеджера слоев страницы
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ИСПРАВЛЕНИЕ: Вместо QPlainTextEdit создаем наш кастомный CodeEditor!
    CodeEditor *editor = new CodeEditor(newPage);

    // Загружаем в него считанный код Python-файла
    editor->setPlainText(fileContent);

    // ПОДКЛЮЧАЕМ СИГНАЛ: Любое нажатие клавиши в коде вешает звездочку изменения файла
    connect(editor, &CodeEditor::textChanged, this, &Neuro_programm::onCurrentFileTextChanged);

    // Накатываем строгий плоский стиль Breeze для текстового редактора
    editor->setStyleSheet(
        "CodeEditor {"
        "   background-color: #ffffff;"
        "   color: #232629;"
        "   font-family: 'Monospace', 'Courier New', 'Liberation Mono';"
        "   font-size: 13px;"
        "   border: none;"
        "}"
        );

    // Устанавливаем размер табуляции Tab ровно в 4 пробела (критично для Python синтаксиса)
    editor->setTabStopDistance(4 * editor->fontMetrics().horizontalAdvance(' '));

    // Интегрируем кастомный текстовый редактор внутрь слоя созданной страницы
    layout->addWidget(editor);

    // =========================================================================
    // 5. ИНТЕГРАЦИЯ СТРАНИЦЫ В ЦЕНТРАЛЬНЫЙ СТЭК, КОМБОБОКС И ЛЕВЫЙ СПИСОК
    // =========================================================================

    // Добавляем готовую страницу со сплиттером-линейкой в центральный QStackedWidget
    int newStackIndex = ui->centralStackedWidget->addWidget(newPage);

    // Извлекаем чистое имя файла с расширением (например, "train.py")
    QString fileName = fileInfo.fileName();

    // Добавляем имя файла в верхний QComboBox, пряча полный путь в userData (второй параметр)
    ui->fileComboBox->addItem(fileName, QVariant(filePath));

    // Добавляем имя файла в левый боковой список QListWidget открытых документов
    QListWidgetItem *listItem = new QListWidgetItem(fileName, ui->openFilesListWidget);
    listItem->setData(Qt::UserRole, filePath); // Прячем путь в скрытые метаданные строки

    // =========================================================================
    // 6. УПРАВЛЕНИЕ ВИДИМОСТЬЮ ЛЕВОГО КОНТЕЙНЕРА И ПРАВОГО ДОКА
    // =========================================================================

    // Если это первый открытый текстовый файл в сессии — плавно выводим левую панель документов
    if (ui->openFilesContainer && !ui->openFilesContainer->isVisible())
    {
        ui->openFilesContainer->setVisible(true);

        // Перераспределяем высоту левого вертикального сплиттера окон навигации
        if (ui->leftVerticalSplitter) {
            ui->leftVerticalSplitter->setCollapsible(1, false); // Защита от случайного скрытия
            ui->leftVerticalSplitter->setSizes(QList<int>({600, 200}));
        }
    }

    // Синхронизируем фокус: выставляем в комбобоксе последний добавленный индекс
    ui->fileComboBox->setCurrentIndex(newStackIndex);

    // Подсвечиваем созданную строку в левом боковом списке открытых документов
    ui->openFilesListWidget->setCurrentRow(ui->openFilesListWidget->count() - 1);

    // Скрываем правую панель параметров при переходе к редактору кодов, давая максимум ширины экрана
    if (ui->widgetRightCharts) {
        ui->widgetRightCharts->setVisible(false);
    }
    if (ui->mainHorizontalSplitter) {
        ui->mainHorizontalSplitter->setCollapsible(1, true);
        ui->mainHorizontalSplitter->setSizes(QList<int>({this->width(), 0}));
    }

    qInfo() << "✔ [IDE] Открыта новая вкладка редактора кода для файла:" << filePath;
}

void Neuro_programm::onCloseCurrentFileClicked()
{
    int currentIndex = ui->fileComboBox->currentIndex();

    // БЕЗОПАСНОСТЬ: Если пользователь пытается закрыть Панель обучения (0) или Чат (1) — блокируем операцию!
    if (currentIndex < 2) {
        ui->statusbar->showMessage("ℹ Сервисные вкладки среды разработки нельзя закрыть", 3000);
        return;
    }

    // Извлекаем указатель на динамическую страницу с кодом
    QWidget *filePageWidget = ui->centralStackedWidget->widget(currentIndex);

    if (filePageWidget)
    {
        // 1. Удаляем запись из верхнего комбобокса
        ui->fileComboBox->removeItem(currentIndex);

        // 2. Удаляем запись из левого бокового списка документов
        QListWidgetItem *listItem = ui->openFilesListWidget->item(currentIndex);
        if (listItem) {
            delete ui->openFilesListWidget->takeItem(currentIndex);
        }

        // 3. Вынимаем страницу из контейнера стэка
        ui->centralStackedWidget->removeWidget(filePageWidget);

        // 4. Намертво зачищаем память, удаляя QWidget вместе с QPlainTextEdit
        filePageWidget->deleteLater();
    }

    // После удаления элемента комбобокс сам переключится на предыдущий индекс,
    // а наш лямбда-коннект из Шага 2 автоматически настроит видимость правого дока!
}

void Neuro_programm::onOpenFileListItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;

    // Считываем сохраненный полный путь к файлу из кликнутой строки
    QString targetFilePath = item->data(Qt::UserRole).toString();

    // Проверяем: если пользователь дважды кликнул по строке "Панель обучения ИИ"
    if (targetFilePath == "MAIN_SCREEN") {
        ui->fileComboBox->setCurrentIndex(0);
        return;
    }

    // Сканируем элементы верхнего комбобокса, чтобы найти страницу с этим файлом
    for (int i = 0; i < ui->fileComboBox->count(); ++i)
    {
        if (ui->fileComboBox->itemData(i).toString() == targetFilePath)
        {
            // Нашли! Принудительно переключаем комбобокс.
            // Его собственный сигнал currentIndexChanged сам перелистнет centralStackedWidget
            // и автоматически скроет правый док-виджет настроек!
            ui->fileComboBox->setCurrentIndex(i);

            // На всякий случай синхронизируем выделение (подсвечиваем строку)
            ui->openFilesListWidget->setCurrentRow(i);
            break;
        }
    }
}

void Neuro_programm::detectCudaDevices()
{
    if (!ui->comboDevice) return;

    // Базовая очистка: CPU доступен всегда
    ui->comboDevice->clear();
    ui->comboDevice->addItem("cpu");

    int gpuCount = 0;

    // СТРАТЕГИЯ №1: Пробуем официальную утилиту nvidia-smi
    QProcess queryProcess;
    queryProcess.start("nvidia-smi", QStringList() << "--list-gpus");

    if (queryProcess.waitForFinished(500) && queryProcess.exitCode() == 0)
    {
        QString output = QString::fromUtf8(queryProcess.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            QStringList gpus = output.split('\n', Qt::SkipEmptyParts);
            gpuCount = gpus.count();

            for (int i = 0; i < gpuCount; ++i) {
                QString deviceName = QString("cuda:%1").arg(i);
                ui->comboDevice->addItem(deviceName);
                ui->comboDevice->setItemData(ui->comboDevice->count() - 1, gpus.at(i).trimmed(), Qt::ToolTipRole);
            }
        }
    }

    // СТРАТЕГИЯ №2: Если nvidia-smi нет, опрашиваем ядро Linux напрямую через /dev/
    if (gpuCount == 0)
    {
        QDir devDir("/dev");
        // Ищем в системе файлы устройств, соответствующие видеокартам NVIDIA (nvidia0, nvidia1...)
        QStringList filters;
        filters << "nvidia[0-9]*";

        QFileInfoList list = devDir.entryInfoList(filters, QDir::System);
        gpuCount = list.count();

        if (gpuCount > 0) {
            for (int i = 0; i < gpuCount; ++i) {
                QString deviceName = QString("cuda:%1").arg(i);
                ui->comboDevice->addItem(deviceName);
                // Так как имени модели без nvidia-smi мы не знаем, пишем общую инфо-строку
                ui->comboDevice->setItemData(ui->comboDevice->count() - 1,
                                             QString("NVIDIA GPU Device (Index %1)").arg(i), Qt::ToolTipRole);
            }
        }
    }

    // --- ИТОГОВЫЙ СТАТУС ВЫБОРА ---
    if (gpuCount > 0)
    {
        // Если нашли GPU хотя бы одним способом — выставляем cuda:0 по умолчанию
        ui->comboDevice->setCurrentIndex(1);

        sendSystemNotification("Аппаратное ускорение",
                               QString("⚡ CUDA ядра активны. В системе доступно %1 GPU NVIDIA.").arg(gpuCount));
    }
    else
    {
        // На ПК действительно нет графики NVIDIA
        ui->comboDevice->setCurrentIndex(0);

        sendSystemNotification("Аппаратное ускорение",
                               "Вычисления ограничены CPU. Видеокарты NVIDIA с поддержкой CUDA не обнаружены.");
    }
}

void Neuro_programm::sendSystemNotification(const QString &title, const QString &text)
{
    // 1. Подключаемся к стандартной службе уведомлений Freedesktop через D-Bus
    QDBusInterface notifyInterface(
        "org.freedesktop.Notifications",     // Имя службы на шине
        "/org/freedesktop/Notifications",    // Путь к объекту
        "org.freedesktop.Notifications",     // Имя интерфейса
        QDBusConnection::sessionBus()        // Используем пользовательскую сессию Bus
        );

    if (!notifyInterface.isValid()) {
        // Если служба D-Bus недоступна (например, в минималистичной консоли без GUI),
        // тихо выводим лог в консоль сборки, чтобы программа не падала
        qWarning() << "D-Bus Notifications interface is not valid!";
        return;
    }

    // 2. Сборка аргументов под спецификацию Freedesktop (метод Notify)
    QVariantList args;
    args << "PyTorch Studio";       // 1. Имя приложения-отправителя
    args << 0u;                    // 2. ID заменяемого уведомления (0 = создать новое)
    args << "brain";               // 3. Иконка (используем системную иконку из темы Breeze)
    args << title;                 // 4. Крупный заголовок карточки
    args << text;                  // 5. Основной текст уведомления
    args << QStringList();         // 6. Интерактивные кнопки-действия (нам пока не нужны)
    args << QVariantMap();         // 7. Дополнительные подсказки-хинты для KDE Plasma
    args << 3000;                  // 8. Время отображения карточки на экране в миллисекундах (3 секунды)

    // 3. Асинхронно отправляем сигнал в ядро KDE Plasma!
    notifyInterface.callWithArgumentList(QDBus::NoBlock, "Notify", args);
}

void Neuro_programm::initProjectTreeModel(const QString &path)
{
    // Если модель уже существовала от старого проекта — безопасно удаляем её из памяти
    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    // 1. Создаем модель строго в момент реального открытия или создания проекта!
    projectModel = new QFileSystemModel(this);
    projectModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);

    // 2. Накатываем фильтр скрытия venv и показа всех остальных файлов в корне
    projectModel->setNameFilters(QStringList() << "[^v]*" << "v[^e]*" << "ve[^n]*" << "ven[^v]*" << "venv?*");
    projectModel->setNameFilterDisables(false); // Полное скрытие из видимости

    // 3. Задаем корень сканирования файловой системы
    projectModel->setRootPath(path);

    // 4. Монтируем модель к виджету дерева на экране
    ui->treeView->setModel(projectModel);

    // 5. Очищаем геометрию колонок (Стиль профессиональной IDE)
    QModelIndex rootIndex = projectModel->index(path);
    ui->treeView->setRootIndex(rootIndex);
    ui->treeView->expand(rootIndex); // Автоматически раскрываем корневую папку

    for (int i = 1; i < projectModel->columnCount(); ++i) {
        ui->treeView->hideColumn(i); // Оставляем видимым строго имя файла
    }
}

void Neuro_programm::sendChatMessageToAI()
{
    // --- 1. ИЗВЛЕКАЕМ МНОГОСТРОЧНЫЙ ТЕКСТ ЗАПРОСА ---
    QString userQuery = ui->inputChatText->toPlainText().trimmed();
    if (userQuery.isEmpty()) return;

    // Отображаем сообщение пользователя в логе чата синим цветом Breeze
    QListWidgetItem *userItem = new QListWidgetItem("Вы:\n" + userQuery, ui->chatLogWidget);
    userItem->setForeground(QBrush(QColor("#0056b3")));
    ui->inputChatText->clear(); // Сразу очищаем поле ввода

    // Добавляем маркер ожидания ответа локального ИИ
    QListWidgetItem *aiItem = new QListWidgetItem("ИИ: Сканирую файлы проекта и генерирую ответ...", ui->chatLogWidget);
    aiItem->setForeground(QBrush(QColor("#555555"))); // Серый цвет
    ui->chatLogWidget->scrollToBottom();

    // Блокируем ввод на время генерации, чтобы защитить от спама кнопками
    ui->inputChatText->setEnabled(false);
    ui->btnSendChat->setEnabled(false);

    // =========================================================================
    // 2. АВТОМАТИЧЕСКИЙ СБОР ЖИВОГО КОНТЕКСТА ТЕКУЩЕГО ПРОЕКТА
    // =========================================================================
    QString projectContext = "--- ТЕКУЩИЙ КОНТЕКСТ ИИ-ПРОЕКТА ---\n";

    // А. Считываем гиперпараметры с пульта управления на первой странице стэка
    // projectContext += QString("Настройки обучения в GUI:\n- Количество эпох: %1\n- Размер пакета (Batch Size): %2\n- Шаг градиента (LR): %3\n- Вычислительный чип: %4\n\n")
    //                       .arg(ui->spinBoxEpochs ? QString::number(ui->spinBoxEpochs->value()) : "10")
    //                       .arg(ui->comboBatchSize ? ui->comboBatchSize->currentText() : "32")
    //                       .arg(ui->spinBoxLR ? QString::number(ui->spinBoxLR->value(), 'f', 5) : "0.001")
    //                       .arg(ui->comboDevice ? ui->comboDevice->currentText() : "cpu");

    // Б. Считываем исходный код Python из активного текстового редактора (если файл открыт)
    int currentStackIdx = ui->centralStackedWidget->currentIndex();
    if (currentStackIdx >= 2) // Страницы 0 и 1 заняты Панелью и Чатом, файлы идут дальше
    {
        QWidget *currentPage = ui->centralStackedWidget->widget(currentStackIdx);
        if (currentPage)
        {
            QPlainTextEdit *activeEditor = currentPage->findChild<QPlainTextEdit*>();
            if (activeEditor) {
                QString activeFileName = ui->fileComboBox->itemText(currentStackIdx);
                projectContext += QString("Исходный код открытого файла (%1):\n```python\n%2\n```\n\n")
                                      .arg(activeFileName).arg(activeEditor->toPlainText());
            }
        }
    }

    // В. Настраиваем жесткую системную роль (инструкцию) для локальной модели
    QString systemInstruction = "Ты — встроенный ИИ-помощник в среде 'PyTorch Studio'. Твоя цель — помогать пользователю проектировать, "
                                "исправлять ошибки и настраивать обучение нейросетей PyTorch на основе предоставленного кода и настроек GUI.";

    // =========================================================================
    // 3. СБОРКА СТАНДАРТНОГО JSON ПАКЕТА ПОД OLLAMA API
    // Структура Ollama API: {"model": "llama3", "messages": [...], "stream": false}
    // =========================================================================
    QJsonObject requestBody;

    // Рекомендуется загрузить в систему модель qwen2.5-coder:7b или deepseek-coder
    // Если у вас скачана другая модель, просто замените "qwen2.5-coder" на её имя (например, "llama3")
    requestBody["model"] = "qwen2.5-coder:1.5b";
    requestBody["stream"] = false; // Получаем ответ целиком одной порцией текста

    QJsonArray messagesArray;

    // Сообщение №1: Передаем системную инструкцию и весь контекст проекта
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = systemInstruction + "\n\n" + projectContext;
    messagesArray.append(systemMessage);

    // Сообщение №2: Передаем живой многострочный вопрос пользователя
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = userQuery;
    messagesArray.append(userMessage);

    requestBody["messages"] = messagesArray;

    QJsonDocument jsonDoc(requestBody);
    QByteArray jsonData = jsonDoc.toJson();

    // =========================================================================
    // 4. НАСТРОЙКА ЛОКАЛЬНОГО СЕТЕВОГО ПОДКЛЮЧЕНИЯ (БЕЗ API-КЛЮЧЕЙ)
    // =========================================================================
    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);

    // Адрес локального сервера Ollama в вашей системе Arch Linux (порт 11434 по умолчанию)
    QUrl apiUrl("http://localhost:11434/api/chat");
    QNetworkRequest request(apiUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Отправляем асинхронный POST запрос на собственный компьютер
    QNetworkReply *reply = networkManager->post(request, jsonData);

    // =========================================================================
    // 5. АСИНХРОННЫЙ ОБРАБОТЧИК ОТВЕТА ОТ OLLAMA
    // =========================================================================
    connect(reply, &QNetworkReply::finished, this, [this, reply, networkManager, aiItem]() {
        // Мгновенно возвращаем управление и фокус интерфейсу чата
        ui->inputChatText->setEnabled(true);
        ui->btnSendChat->setEnabled(true);
        ui->inputChatText->setFocus();

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray rawResponse = reply->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(rawResponse);
            QJsonObject responseObj = responseDoc.object();

            // ПАРСИНГ OLLAMA JSON ДЕРЕВА: Извлекаем объект message напрямую из корня
            QJsonObject messageObj = responseObj["message"].toObject();
            QString aiResponse     = messageObj["content"].toString().trimmed();

            if (!aiResponse.isEmpty())
            {
                // УСПЕХ: Выводим развернутый ответ локального ИИ в историю чата!
                aiItem->setText("ИИ-Ассистент:\n" + aiResponse);
                aiItem->setForeground(QBrush(QColor("#232629"))); // Цвет Breeze
                sendSystemNotification("ИИ-Ассистент", "🤖 Локальный ответ ИИ успешно сгенерирован.");
            }
            else
            {
                aiItem->setText("ИИ: ⚠️ Сервер Ollama вернул пустую структуру контента. Проверьте запущенную модель.");
                aiItem->setForeground(QBrush(QColor("#b57c00")));
            }
        }
        else
        {
            // Выводим сетевую ошибку, если служба Ollama выключена или упала
            aiItem->setText(QString("ИИ: ❌ Локальная служба Ollama не отвечает!\n"
                                    "Убедитесь, что служба запущена в терминале: 'sudo systemctl start ollama'\n"
                                    "И скачана модель: 'ollama run qwen2.5-coder'\n"
                                    "Системный лог ошибки: %1").arg(reply->errorString()));
            aiItem->setForeground(QBrush(QColor("#cc0000")));
        }

        ui->chatLogWidget->scrollToBottom();
        reply->deleteLater();
        networkManager->deleteLater();
    });
}

// =============================================================================
// 1. СЛOT ЗАПУСКА ПРОЦЕССА ОБУЧЕНИЯ НЕЙРОСЕТИ
// =============================================================================

void Neuro_programm::onStartTrainingClicked()
{
    // 1. ЗАЩИТНЫЕ ПРОВЕРКИ
    if (currentOpenProjectPath.isEmpty()) {
        qCritical() << "Ошибка: Сначала создайте или откройте ИИ-проект (*.pystudio).";
        return;
    }

    // 2. ВЫДВИГАЕМ ИНТЕРФЕЙСНЫЕ ПАНЕЛИ
    if (panelOther) {
        panelOther->setVisible(true);
        panelOther->setTerminalPageActive();
        if (btnTerminal) btnTerminal->setChecked(true);
        if (btnAIChat)   btnAIChat->setChecked(false);
        if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
    }

    if (ui->widgetRightCharts && !ui->widgetRightCharts->isVisible()) {
        ui->widgetRightCharts->setVisible(true);
        if (ui->mainHorizontalSplitter) {
            ui->mainHorizontalSplitter->setCollapsible(1, false);
            ui->mainHorizontalSplitter->setSizes(QList<int>({this->width() - 350, 350}));
        }
        if (ui->summaryMetrics) {
            ui->summaryMetrics->clear();
            ui->summaryMetrics->append("⏱ Статус: Инициализация ядра вычислений...");
        }
        if (lossSeries) lossSeries->clear();
    }

    // Блокируем пульт параметров
    ui->btnStartTraining->setEnabled(false);
    ui->btnStartTraining->setText("⏳ Обучение...");
    ui->btnStopTraining->setEnabled(true);

    // =========================================================================
    // 3. БЕЗОПАСНЫЙ АСИНХРОННЫЙ ЗАПУСК КЕРНЕЛ-ПРОЦЕССА В СТИЛЕ РУЧНОГО ТЕРМИНАЛА
    // =========================================================================


    // --- ВНУТРИ МЕТОДА Neuro_programm::onStartTrainingClicked() в neuro_programm.cpp ---

    // --- ВНУТРИ МЕТОДА Neuro_programm::onStartTrainingClicked() в neuro_programm.cpp ---

    if (trainingProcess == nullptr) {
        trainingProcess = new QProcess(this);
    } else {
        trainingProcess->disconnect();
    }

    // Намертво сливаем стандартный вывод и ошибки (чтобы поймать вылеты Python)
    trainingProcess->setProcessChannelMode(QProcess::MergedChannels);
    trainingProcess->setWorkingDirectory(currentOpenProjectPath);

    // Настраиваем изолированное небуферизированное окружение Linux
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("PYTHONIOENCODING", "UTF-8");
    env.insert("PYTHONPATH", currentOpenProjectPath);
    trainingProcess->setProcessEnvironment(env);

    // СВЯЗЫВАЕМ НАПРЯМУЮ С ВАШИМ ОФИЦИАЛЬНЫМ СЛОТОМ ИЗ ЗАГОЛОВКА
    connect(trainingProcess, &QProcess::readyReadStandardOutput, this, &Neuro_programm::readTrainingOutput);
    connect(trainingProcess, &QProcess::finished, this, &Neuro_programm::trainingFinished);

    // Формируем чистые аргументы запуска
    QString absoluteTrainScriptPath = currentOpenProjectPath + "/train.py";
    QStringList arguments;
    arguments << "-u" << absoluteTrainScriptPath;

    QString venvPythonPath = currentOpenProjectPath + "/venv/bin/python";
    qInfo() << "🚀 [QProcess] Боевой асинхронный запуск PyTorch:" << venvPythonPath << arguments;

    // Запускаем асинхронно!
    trainingProcess->start(venvPythonPath, arguments);
}


void Neuro_programm::onStopTrainingClicked()
{
    if (trainingProcess && trainingProcess->state() != QProcess::NotRunning)
    {
        ui->btnStopTraining->setEnabled(false); // Сразу выключаем кнопку, защищая от повторных кликов
        ui->btnStopTraining->setText("🛑 Остановка...");

        // Намертво убиваем процесс Python в операционной системе Linux.
        // Метод terminate() шлет мягкий сигнал SIGTERM, если скрипт завис —
        // метод kill() гарантированно прибьет его на уровне ядра через SIGKILL.
        trainingProcess->kill();

        sendSystemNotification("Обучение ИИ", "🛑 Процесс обучения принудительно прерван пользователем.");
    }
}

void Neuro_programm::readTrainingOutput()
{
    if (!trainingProcess || !panelOther) return;

    // Считываем сквозной поток данных (включая ошибки импорта)
    QByteArray rawData = trainingProcess->readAll();
    QString outputText = QString::fromUtf8(rawData);

    // =========================================================================
    // ПУЛЕНЕПРОБИВАЕМЫЙ ВЫВОД: Ищем текстовый виджет панели по его типу QTextEdit!
    // Это заставит текст мгновенно отобразиться в вашем ui->textEdit на экране.
    // =========================================================================
    QTextEdit *richConsole = panelOther->findChild<QTextEdit*>();
    if (richConsole != nullptr) {
        richConsole->insertPlainText(outputText);
        richConsole->moveCursor(QTextCursor::End); // Авто-скролл вниз
    }

    // =========================================================================
    // ВАШ РОДНОЙ НЕИЗМЕНЯЕМЫЙ КОД ПАРСИНГА ГРАФИКОВ И HTML МЕТРИК
    // =========================================================================
    static QRegularExpression lossRegex("PROGRESS:\\s*epoch=(\\d+),\\s*loss=([0-9.]+)");
    static QRegularExpression metricsRegex("METRICS:\\s*epoch=(\\d+),\\s*accuracy=([0-9.]+)%,\\s*vram=([0-9.]+)GB,\\s*speed=(\\d+)");

    QStringList lines = outputText.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        // Парсинг синей кривой Loss
        QRegularExpressionMatch lossMatch = lossRegex.match(line.trimmed());
        if (lossMatch.hasMatch()) {
            double epoch = lossMatch.captured(1).toDouble();
            double loss  = lossMatch.captured(2).toDouble();
            if (lossSeries) lossSeries->append(epoch, loss);
            if (lossChart) {
                lossChart->axes(Qt::Horizontal).first()->setRange(1, qMax(10.0, epoch));
                static double maxLoss = 0.1;
                if (loss > maxLoss) maxLoss = loss;
                lossChart->axes(Qt::Vertical).first()->setRange(0, maxLoss * 1.1);
            }
        }

        // Обновление цветного HTML дашборда
        QRegularExpressionMatch metricsMatch = metricsRegex.match(line.trimmed());
        if (metricsMatch.hasMatch() && ui->summaryMetrics) {
            QString epochStr = metricsMatch.captured(1);
            QString accStr   = metricsMatch.captured(2);
            QString vramStr  = metricsMatch.captured(3);
            QString speedStr = metricsMatch.captured(4);

            QString htmlReport = QString(
                                     "<div style='font-family:\"Monospace\"; font-size:13px; color:#232629;'>"
                                     "  <b style='color:#0056b3; font-size:14px;'> МОНИТОРИНГ МЕТРИК НЕЙРОСЕТИ:</b><br>"
                                     "  <hr style='border:none; border-top:1px solid #b0b0b0; margin: 5px 0;'>"
                                     "  <table width='100%' cellpadding='2' cellspacing='0'>"
                                     "    <tr><td><b>Текущая эпоха:</b></td><td align='right' style='color:#27ae60; font-weight:bold;'>%1</td></tr>"
                                     "    <tr><td><b>Точность (Accuracy):</b></td><td align='right' style='color:#2980b9; font-weight:bold;'>%2 %</td></tr>"
                                     "    <tr><td><b>Видеопамять VRAM:</b></td><td align='right' style='color:#8e44ad; font-weight:bold;'>%3 ГБ</td></tr>"
                                     "    <tr><td><b>Скорость вычислений:</b></td><td align='right' style='color:#f39c12; font-weight:bold;'>%4 img/s</td></tr>"
                                     "  </table>"
                                     "</div>"
                                     ).arg(epochStr).arg(accStr).arg(vramStr).arg(speedStr);
            ui->summaryMetrics->setHtml(htmlReport);
        }
    }
}


void Neuro_programm::trainingFinished(int exitCode)
{
    // Разблокируем пульт управления (наш старый код)
    ui->btnStartTraining->setEnabled(true);
    ui->btnStartTraining->setText(" 🚀 Начать обучение ");
    ui->btnStopTraining->setEnabled(false);
    ui->spinBoxEpochs->setEnabled(true);
    ui->comboBatchSize->setEnabled(true);
    ui->spinBoxLR->setEnabled(true);
    ui->comboDevice->setEnabled(true);

    // =========================================================================
    // ТОТАЛЬНЫЙ ПЕРЕХВАТ КОДА ВЫЛЕТА ПРОЦЕССА ИЗ ЯДРА LINUX
    // =========================================================================
    // Проверяем: если процесс завершился аварийно (краш памяти или нехватка прав)
    if (trainingProcess && trainingProcess->exitStatus() == QProcess::CrashExit)
    {
        QString crashLog = QString(
            "\n<span style='color:#cc0000; font-weight:bold;'>❌ КРИТИЧЕСКИЙ ВЫЛЕТ ЯДРА PYTHON!</span><br>"
            "<span style='color:#cc0000;'>Процесс обучения аварийно рухнул на уровне операционной системы (Segmentation Fault / Core Dumped).</span><br>"
            "<span style='color:#555555;'>Возможная причина: Конфликт несовместимости PyTorch с версией Python 3.14 в Arch Linux.</span>"
            );

        if (panelOther) {
            QTextEdit *richConsole = panelOther->findChild<QTextEdit*>();
            if (richConsole) richConsole->append(crashLog);
        }

        qCritical() << "🔴 Критический краш подпроцесса Python внутри ОС Linux!";
        sendSystemNotification("Краш PyTorch", "❌ Процесс обучения аварийно рухнул.");
        return;
    }

    // Если процесс завершился нормально, но выдал ошибку логики (например, FileNotFound)
    if (exitCode != 0)
    {
        QString errorLog = QString(
                               "\n<span style='color:#e67e22; font-weight:bold;'>⚠️ ПРОЦЕСС ЗАВЕРШИЛСЯ С ОШИБКОЙ LOGIC. Код вылета Linux: %1</span><br>"
                               "<span style='color:#555555;'>Скрипт train.py выполнился, но прервал работу. Проверьте пути к датасетам или импорты.</span>"
                               ).arg(exitCode);

        if (panelOther) {
            QTextEdit *richConsole = panelOther->findChild<QTextEdit*>();
            if (richConsole) richConsole->append(errorLog);
        }
        return;
    }

    // Успешный финал (код 0)
    qInfo() << "✔ Обучение полностью завершено.";
    sendSystemNotification("Обучение завершено", "🎉 Обучение успешно завершено.");
}




// =============================================================================
// РЕАЛИЗАЦИЯ АВТОСОХРАНЕНИЯ ПАРАМЕТРОВ ОБУЧЕНИЯ В КОНТРОЛЬНЫЙ ФАКТ .pystudio
// =============================================================================
void Neuro_programm::save_project_config()
{
    // 1. ЗАЩИТА: Если путь к проекту еще не определен в памяти — выходим
    if (currentOpenProjectPath.isEmpty()) return;

    QDir dir(currentOpenProjectPath);
    QString projName = dir.dirName(); // Извлекаем имя папки проекта
    QString configFilePath = currentOpenProjectPath + "/" + projName + ".pystudio";

    QFile configFile(configFilePath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "❌ ОШИБКА ЗАПИСИ: Не удалось открыть файл конфигурации проекта для перезаписи.";
        return;
    }

    // 2. СБОРКА АКТУАЛЬНЫХ ДАННЫХ С ПАНЕЛИ GUI
    QJsonObject configObject;
    configObject["project_name"]  = projName;

    // Безопасно считываем данные со счетчиков и комбобоксов
    configObject["epochs"]        = ui->spinBoxEpochs ? ui->spinBoxEpochs->value() : 10;
    configObject["batch_size"]    = ui->comboBatchSize ? ui->comboBatchSize->currentText().toInt() : 32;
    configObject["learning_rate"] = ui->spinBoxLR ? ui->spinBoxLR->value() : 0.001; // Используем ваше имя doubleSpinBox/spinBoxLR
    configObject["device"]        = ui->comboDevice ? ui->comboDevice->currentText() : "cpu";
    configObject["architecture"]  = (configObject["device"].toString() == "cpu") ? "CPU" : "CUDA";

    // Резервируем путь к датасету
    configObject["dataset_path"]  = "";

    // 3. ПЕРЕЗАПИСЫВАЕМ ФАЙЛ JSON С ОТСТУПАМИ ДЛЯ ЧИТАЕМОСТИ
    QJsonDocument jsonDoc(configObject);
    configFile.write(jsonDoc.toJson(QJsonDocument::Indented));
    configFile.close();

    qInfo() << "💾 Конфигурация проекта .pystudio успешно обновлена.";
    sendSystemNotification("Конфигурация ИИ", "💾 Изменения параметров успешно записаны в .pystudio");
}

void Neuro_programm::forceOpenConsoleWithError(const QString &errorMessage)
{
    // Проверяем, что нижняя панель инициализирована в памяти
    if (!panelOther) return;

    // 1. АВТО-РАСКРЫТИЕ ИНТЕРФЕЙСА НИЖНЕЙ ПАНЕЛИ
    panelOther->setVisible(true);
    panelOther->setTerminalPageActive(); // Переключаем stackedWidget панели на Терминал

    // Синхронизируем кнопки в статусбаре
    if (btnTerminal) btnTerminal->setChecked(true);
    if (btnAIChat)   btnAIChat->setChecked(false);

    // Раздвигаем центральный сплиттер, отдавая консоли фиксированные 250 пикселей
    if (mainVerticalSplitter) {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
    }

    // 2. ВЫВОД ТЕКСТА ОШИБКИ ВО ВСТРОЕННЫЙ ТЕРМИНАЛ consoleOutput
    QTextEdit *console = panelOther->findChild<QTextEdit*>("consoleOutput");
    if (console) {
        // Окрашиваем критическую ошибку в ярко-красный цвет
        console->append(QString("\n>>> [CRITICAL IDE ERROR] %1").arg(errorMessage));
        console->moveCursor(QTextCursor::End); // Скроллим вниз
    }
}

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ И СТИЛИЗАЦИЯ СЕТКИ ГРАФИКА LOSS (QT CHARTS)
// =============================================================================
void Neuro_programm::initLossChart()
{
    // Защита: если виджет на форме не найден — выходим
    if (!ui->chartViewLoss) return;

    // Выделяем память под глобальные переменные класса (без типов данных в начале!)
    lossSeries = new QLineSeries();
    lossChart = new QChart();

    // Связываем линию данных с графиком
    lossChart->addSeries(lossSeries);
    lossChart->setTitle("📊 График падения ошибки (Training Loss)");
    lossChart->setAnimationOptions(QChart::SeriesAnimations); // Плавная анимация точек

    // Создаем и настраиваем горизонтальную ось X (Эпохи)
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Эпохи обучения");
    axisX->setLabelFormat("%d");
    axisX->setRange(1, 10); // Стартовый диапазон по умолчанию
    lossChart->addAxis(axisX, Qt::AlignBottom);
    lossSeries->attachAxis(axisX);

    // Создаем и настраиваем вертикальную ось Y (Значение ошибки Loss)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Значение Loss");
    axisY->setLabelFormat("%.4f");
    axisY->setRange(0, 1.0); // Стартовый диапазон по умолчанию
    lossChart->addAxis(axisY, Qt::AlignLeft);
    lossSeries->attachAxis(axisY);

    // Монтируем настроенный график внутрь продвинутого виджета на экране
    ui->chartViewLoss->setChart(lossChart);
    ui->chartViewLoss->setRenderHint(QPainter::Antialiasing); // Включаем сглаживание линий (anti-aliasing)

    currentEpochCounter = 0;
}

#include <QSettings>

// =============================================================================
// 1. ДОБАВЛЕНИЕ НОВОГО ПУТИ В КОНФИГУРАЦИОННЫЙ СПИСОК RECENT
// =============================================================================
void Neuro_programm::addProjectToRecent(const QString &projectPath)
{
    if (projectPath.isEmpty()) return;

    // Инициализируем хранилище настроек (создаст файл ~/.config/PyTorchStudio/IDE.conf)
    QSettings settings("PyTorchStudio", "IDE");

    // Считываем текущий уже сохраненный список строк
    QStringList files = settings.value("recentProjectList").toStringList();

    // Извлекаем дубликаты: если этот проект уже был в списке — удаляем его со старой позиции
    files.removeAll(projectPath);

    // Вставляем свежий проект на самое первое, верхнее место (Индекс 0)
    files.prepend(projectPath);

    // Обрезаем список, если он превысил лимит (оставляем строго 5 последних проектов)
    while (files.size() > MaxRecentFiles) {
        files.removeLast();
    }

    // Записываем обновленный массив обратно на жесткий диск
    settings.setValue("recentProjectList", files);

    // Принудительно перерисовываем верхнее меню, чтобы изменения появились на экране
    updateRecentProjectActions();
}

// =============================================================================
// 2. ДИНАМИЧЕСКАЯ ПЕРЕРИСОВКА ПОДМЕНЮ "ОТКРЫТЬ НЕДАВНИЕ"
// =============================================================================
void Neuro_programm::updateRecentProjectActions()
{
    QSettings settings("PyTorchStudio", "IDE");
    QStringList files = settings.value("recentProjectList").toStringList();

    // Определяем, сколько реальных проектов сейчас сохранено в памяти
    int numRecentFiles = qMin(files.size(), (int)MaxRecentFiles);

    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        if (i < numRecentFiles)
        {
            QString fullPath = files[i];
            QFileInfo fileInfo(fullPath);

            // Формируем красивый плоский текст для строки меню:
            // "1. z1 [/home/elf/ааа/z1]"
            QString actionText = QString("&%1. %2 [%3]")
                                     .arg(i + 1)
                                     .arg(fileInfo.dir().dirName()) // Имя папки проекта
                                     .arg(fullPath);                // Полный путь

            recentProjectActions[i]->setText(actionText);

            // Прячем полный путь внутрь свойства DATA экшена, чтобы считать его при клике!
            recentProjectActions[i]->setData(fullPath);
            recentProjectActions[i]->setVisible(true); // Делаем строку видимой
        }
        else
        {
            // Если сохраненных проектов меньше 5 — остальные кнопки просто тушим
            recentProjectActions[i]->setVisible(false);
        }
    }

    // Если в истории вообще нет ни одного проекта — блокируем (делаем серым) само подменю
    recentProjectsMenu->setEnabled(numRecentFiles > 0);
}

// =============================================================================
// 3. СЛОТ ОБРАБОТКИ КЛИКА ПО СТРОКЕ ИЗ СПИСКА НЕДАВНИХ
// =============================================================================
void Neuro_programm::openRecentProject()
{
    // Получаем указатель на экшен, по которому физически кликнул пользователь
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action) return;

    // Извлекаем сохраненный полный путь к файлу проекта .pystudio
    QString selectedFile = action->data().toString();

    QFile configFile(selectedFile);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "❌ Сбой открытия: Файл недавнего проекта больше не существует на диске!";

        // Авто-очистка: если файла на диске нет (удален), стираем его из истории настроек
        QSettings settings("PyTorchStudio", "IDE");
        QStringList files = settings.value("recentProjectList").toStringList();
        files.removeAll(selectedFile);
        settings.setValue("recentProjectList", files);
        updateRecentProjectActions();
        return;
    }

    // ... ЗДЕСЬ ИДЕТ ВАШ КЛАССИЧЕСКИЙ НЕИЗМЕНЯЕМЫЙ КОД ПАРСИНГА JSON ИЗ МЕТОДА open_project() ...
    // (Код чтения QJsonDocument, извлечения epochs, batchSize, вызова initProjectTreeModel и т.д.)
    QByteArray rawData = configFile.readAll();
    configFile.close();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(rawData);
    QJsonObject configObject = jsonDoc.object();

    QString projName     = configObject["project_name"].toString("New_Project");
    QString architecture = configObject["architecture"].toString("CUDA");
    QString device       = configObject["device"].toString("cpu");
    int epochs           = configObject["epochs"].toInt(10);
    int batchSize        = configObject["batch_size"].toInt(32);
    double lr            = configObject["learning_rate"].toDouble(0.001);

    QFileInfo fileInfo(selectedFile);
    QString fullProjectPath = fileInfo.absoluteDir().absolutePath();

    // Активируем среду разработки под выбранный недавний проект
    currentOpenProjectPath = fullProjectPath;
    initProjectTreeModel(fullProjectPath);

    ui->centralStackedWidget->setCurrentIndex(0);
    if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);

    detectCudaDevices();

    if (ui->spinBoxEpochs) ui->spinBoxEpochs->setValue(epochs);
    if (ui->spinBoxLR) ui->spinBoxLR->setValue(lr);
    if (ui->comboBatchSize) {
        int batchIdx = ui->comboBatchSize->findText(QString::number(batchSize));
        if (batchIdx != -1) ui->comboBatchSize->setCurrentIndex(batchIdx);
    }

    if (panelOther) panelOther->setCurrentProjectPath(fullProjectPath);
    this->setWindowTitle(QString("PyTorch Studio - %1 [%2]").arg(projName).arg(fullProjectPath));
    sendSystemNotification("PyTorch Studio", QString("✔ Проект '%1' успешно загружен").arg(projName));
}

#include <QFile>
#include <QTextStream>
#include <QPlainTextEdit>

// =============================================================================
// 1. АЛГОРИТМ СОХРАНЕНИЯ ОДНОГО АКТИВНОГО ФАЙЛА (Ctrl + S)
// =============================================================================
void Neuro_programm::saveCurrentActiveFile()
{
    // Узнаем, какая страница центрального стэка сейчас открыта перед пользователем
    int currentIndex = ui->fileComboBox->currentIndex();
    if (currentIndex < 0) return;

    // ЗАЩИТА: Индексы 0 и 1 — это постоянные панели обучения и чата ИИ. Их сохранять как файлы нельзя!
    if (currentIndex < 2) return;

    // Извлекаем сохраненный абсолютный путь к файлу из метаданных (userData) комбобокса
    QString filePath = ui->fileComboBox->itemData(currentIndex).toString();
    if (filePath.isEmpty() || filePath == "AI_CHAT_SCREEN") return;

    // Получаем указатель на страницу QWidget внутри QStackedWidget
    QWidget *currentPage = ui->centralStackedWidget->widget(currentIndex);
    if (!currentPage) return;

    // Динамически находим встроенный в эту страницу текстовый редактор QPlainTextEdit
    QPlainTextEdit *editor = currentPage->findChild<QPlainTextEdit*>();
    if (!editor) return;

    // ФИЗИЧЕСКАЯ ЗАПИСЬ НА ДИСК ARCH LINUX (В КОДИРОВКЕ UTF-8)
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "❌ ОШИБКА ЗАПИСИ: Не удалось открыть файл для сохранения:" << filePath;
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8); // Родной стандарт для скриптов Python
    out << editor->toPlainText();            // Записываем весь текст из редактора в файл
    file.close();

    // ... ваш код физической записи out << editor->toPlainText(); file.close(); ...

    // =========================================================================
    // ОЧИСТКА ЗВЁЗДОЧКИ ПОСЛЕ УСПЕШНОГО СОХРАНЕНИЯ ФАЙЛА
    // =========================================================================
    QString currentName = ui->fileComboBox->itemText(currentIndex);
    if (currentName.endsWith("*"))
    {
        // Отрезаем последний символ (звёздочку)
        QString cleanName = currentName.left(currentName.length() - 1);

        // Возвращаем чистое имя в комбобокс и боковой список документов
        ui->fileComboBox->setItemText(currentIndex, cleanName);
        if (ui->openFilesListWidget->item(currentIndex)) {
            ui->openFilesListWidget->item(currentIndex)->setText(cleanName);
        }
    }

    // Системные уведомления (остаются как были)
    QFileInfo fileInfo(filePath);
    sendSystemNotification("Редактор кода", QString("💾 Файл '%1' успешно сохранен.").arg(fileInfo.fileName()));


    // Отправляем красивое D-Bus уведомление на рабочий стол KDE Plasma
    //QFileInfo fileInfo(filePath);
    sendSystemNotification("Редактор кода", QString("💾 Файл '%1' успешно сохранен.").arg(fileInfo.fileName()));
    qInfo() << "✔ Файл успешно сохранен на диск:" << filePath;
}

// =============================================================================
// 2. АЛГОРИТМ СОХРАНЕНИЯ ВСЕГО ПРОЕКТА ЦЕЛИКОМ (Ctrl + Shift + S)
// =============================================================================
void Neuro_programm::saveAllProjectChanges()
{
    // ШАГ А. Принудительно перезаписываем живые параметры пульта (эпохи, батчи) в .pystudio
    // (Используем метод автосохранения, который мы писали ранее)
    save_project_config();

    // ШАГ Б. Циклом обходим абсолютно ВСЕ открытые текстовые вкладки в нашей IDE
    int totalPages = ui->fileComboBox->count();
    int savedFilesCount = 0;

    for (int i = 2; i < totalPages; ++i) // Стартуем с индекса 2, пропуская Панель ИИ и Чат
    {
        QString filePath = ui->fileComboBox->itemData(i).toString();
        if (filePath.isEmpty() || filePath == "AI_CHAT_SCREEN") continue;

        QWidget *pageWidget = ui->centralStackedWidget->widget(i);
        if (!pageWidget) continue;

        QPlainTextEdit *editor = pageWidget->findChild<QPlainTextEdit*>();
        if (!editor) continue;

        // Перезаписываем очередной текстовый файл на жесткий диск
        QFile file(filePath);
        for (int i = 2; i < totalPages; ++i)
        {
            // ... ваш код извлечения путей и записи out << editor->toPlainText(); file.close(); ...

            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out.setEncoding(QStringConverter::Utf8);
                out << editor->toPlainText();
                file.close();
                savedFilesCount++;

                // =================================================================
                // ОЧИСТКА ЗВЁЗДОЧЕК ДЛЯ ВСЕХ СОХРАНЕННЫХ ФАЙЛОВ ПОДРЯД
                // =================================================================
                QString currentName = ui->fileComboBox->itemText(i);
                if (currentName.endsWith("*")) {
                    QString cleanName = currentName.left(currentName.length() - 1);
                    ui->fileComboBox->setItemText(i, cleanName);
                    if (ui->openFilesListWidget->item(i)) {
                        ui->openFilesListWidget->item(i)->setText(cleanName);
                    }
                }
            }
        }

    }

    // ШАГ В. Выводим комплексный системный отчет об успешном сохранении всей рабочей среды
    QString summaryMessage = QString("💾 Сохранено файлов: %1. Конфигурация .pystudio обновлена.").arg(savedFilesCount);
    sendSystemNotification("Сохранить всё", summaryMessage);
    qInfo() << "✔ [IDE] Выполнено полное сохранение проекта целиком. Количество записанных файлов:" << savedFilesCount;
}

void Neuro_programm::onCurrentFileTextChanged()
{
    // Узнаем, какой файл сейчас открыт на экране
    int currentIndex = ui->fileComboBox->currentIndex();
    if (currentIndex < 2) return; // Пропускаем сервисные Панель ИИ и Чат

    // Получаем текущее название вкладки из комбобокса (например, "train.py")
    QString currentName = ui->fileComboBox->itemText(currentIndex);

    // Если звёздочка уже добавлена — ничего не делаем, чтобы не спамить в цикле
    if (currentName.endsWith("*")) return;

    // Формируем новое имя с индикатором изменений
    QString modifiedName = currentName + "*";

    // 1. Обновляем текст в верхнем комбобоксе
    ui->fileComboBox->setItemText(currentIndex, modifiedName);

    // 2. Обновляем текст в левом боковом списке открытых документов
    QListWidgetItem *listItem = ui->openFilesListWidget->item(currentIndex);
    if (listItem) {
        listItem->setText(modifiedName);
    }
}

void Neuro_programm::onCloseProjectClicked()
{
    // 1. ЗАЩИТА: Если проект и так не открыт — ничего не делаем
    if (currentOpenProjectPath.isEmpty()) return;

    qInfo() << "🧹 [IDE] Запущен процесс полной очистки среды разработки и закрытия проекта:" << currentOpenProjectPath;

    // 2. ОЧИСТКА БОКОВОГО ДЕРЕВА ФАЙЛОВ И МОДЕЛИ
    if (ui->treeView) {
        // Намертво отвязываем модель QFileSystemModel от графического дерева
        ui->treeView->setModel(nullptr);
    }
    if (projectModel) {
        // Безопасно удаляем сам объект модели файловой системы из ОЗУ Linux
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    // 3. ЗАКРЫТИЕ ВСЕХ ДИНАМИЧЕСКИХ ВКЛАДОК РЕДАКТОРА КОДА
    // Индексы 0 и 1 — это постоянные "Панель обучения" и "ИИ-Чат". Мы их строго оставляем!
    // Удаляем все вкладки, начиная с конца (с самого верхнего индекса) до индекса 2.
    if (ui->centralStackedWidget && ui->fileComboBox)
    {
        int totalTabs = ui->fileComboBox->count();
        for (int i = totalTabs - 1; i >= 2; --i)
        {
            // Извлекаем виджет страницы кода из стэка
            QWidget *pageWidget = ui->centralStackedWidget->widget(i);
            if (pageWidget) {
                ui->centralStackedWidget->removeWidget(pageWidget);
                pageWidget->deleteLater(); // Очищаем память от редактора CodeEditor
            }
            // Удаляем строку файла из верхнего комбобокса
            ui->fileComboBox->removeItem(i);
        }
    }

    // Очищаем левый боковой список открытых документов QListWidget
    if (ui->openFilesListWidget) {
        int totalListItems = ui->openFilesListWidget->count();
        for (int i = totalListItems - 1; i >= 2; --i) {
            delete ui->openFilesListWidget->takeItem(i);
        }
    }

    // 4. СБРОС ИНТЕРФЕЙСНЫХ КОНТЕЙНЕРОВ В СТАРТОВОЕ СОСТОЯНИЕ
    // Скрываем левый нижний контейнер открытых файлов
    if (ui->openFilesContainer) {
        ui->openFilesContainer->setVisible(false);
    }
    // Схлопываем левый вертикальный сплиттер навигации в дефолт (100% дерева)
    if (ui->leftVerticalSplitter) {
        ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
    }
    // Схлопываем правую выезжающую панель графиков, если она была открыта
    if (ui->widgetRightCharts) {
        ui->widgetRightCharts->setVisible(false);
    }
    if (ui->mainHorizontalSplitter) {
        ui->mainHorizontalSplitter->setSizes(QList<int>({this->width(), 0}));
    }

    // Сбрасываем фокус стэка на самую первую страницу — Пульт параметров
    if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);
    if (ui->openFilesListWidget) ui->openFilesListWidget->setCurrentRow(0);

    // 5. СБРОС ПЕРЕМЕННЫХ И ЗАГОЛОВКА ОКНА
    currentOpenProjectPath.clear(); // Намертво обнуляем путь проекта в памяти!

    // Возвращаем нативное чистое имя программы в заголовок окна
    this->setWindowTitle("PyTorch Studio - Среда разработки ИИ");

    if (panelOther) {
        panelOther->setCurrentProjectPath(""); // Сбрасываем путь и в нижней панели
    }

    qInfo() << "✔ [IDE] Проект успешно закрыт. Интерфейс переведен в стерильное стартовое состояние.";
    sendSystemNotification("PyTorch Studio", "📁 Проект успешно закрыт.");
}
