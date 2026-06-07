#include "neuro_programm.h"
#include "ui_neuro_programm.h"
#include "start_progect.h"
#include "panel_other.h"
#include "ui_panel_other.h"
#include "settings.h"

#include <QFileSystemModel>
#include <QInputDialog>
#include <QLabel>
#include <QScrollBar>
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
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCompleter>
#include <QStringListModel>
#include <QThread>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QMovie>
#include <QToolBar>
#include <QBoxLayout>
#include <QWindow>
#include <QSpacerItem>
#include <QResizeEvent>


Neuro_programm* Neuro_programm::self = nullptr;

Neuro_programm::Neuro_programm(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Neuro_programm)
{
    ui->setupUi(this);

    // 1. Отключаем нативную рамку ОС
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinMaxButtonsHint);

    recentProjectsMenu = new QMenu(" Открыть недавние", this);
    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentProjectActions[i] = new QAction(this);
        recentProjectActions[i]->setVisible(false); // Прячем, пока список пуст
        connect(recentProjectActions[i], &QAction::triggered, this, &Neuro_programm::openRecentProject);
        recentProjectsMenu->addAction(recentProjectActions[i]);
    }
    // Сразу считываем историю с диска из ~/.config/PyTorchStudio/IDE.conf
    updateRecentProjectActions();

    // =========================================================================
    // 2. СОЗДАНИЕ ДИНАМИЧЕСКИХ ДЕЙСТВИЙ (ЭКШЕНОВ) ДЛЯ ФАЙЛОВ И ПРОЕКТА
    // =========================================================================
    // Действие "Сохранить" (Ctrl + S)
    QAction *actionSave = new QAction(" Сохранить", this);
    actionSave->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(actionSave, &QAction::triggered, this, &Neuro_programm::saveCurrentActiveFile);

    // Действие "Сохранить всё" (Ctrl + Shift + S)
    QAction *actionSaveAll = new QAction(" Сохранить всё", this);
    actionSaveAll->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(actionSaveAll, &QAction::triggered, this, &Neuro_programm::saveAllProjectChanges);

    // Действие "Закрыть проект" (Ctrl + W)
    QAction *actionCloseProject = new QAction(" Закрыть проект", this);
    actionCloseProject->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(actionCloseProject, &QAction::triggered, this, &Neuro_programm::onCloseProjectClicked);


    // 2. Полностью УДАЛЯЕМ встроенный системный менюбар, который лезет наверх
    if (this->menuBar()) {
        this->menuBar()->hide();
        this->menuBar()->deleteLater();
    }

    // 3. Создаем ОДИН общий тулбар на всю ширину окна, который станет нашей новой супер-шапкой
    QToolBar *topContainerBar = new QToolBar(this);
    topContainerBar->setObjectName("topContainerBar");
    topContainerBar->setMovable(false);
    topContainerBar->setFloatable(false);
    topContainerBar->setAllowedAreas(Qt::TopToolBarArea);
    topContainerBar->setContentsMargins(0, 0, 0, 0);

    // 4. Внутренний вертикальный макет, который жестко зафиксирует порядок элементов
    QWidget *topWrapper = new QWidget(topContainerBar);
    QVBoxLayout *topLayout = new QVBoxLayout(topWrapper);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    // 5. Создаем новый ручной QMenuBar, который будет вести себя как послушный виджет
    QMenuBar *customMenuBar = new QMenuBar(topWrapper);

    // Наполняем его вашими пунктами меню и экшенами из Qt Designer
    QMenu *fileMenu = customMenuBar->addMenu("Файл");
    fileMenu->addAction(ui->New_progect);
    fileMenu->addAction(ui->new_file);
    fileMenu->addAction(ui->open_progect);
    fileMenu->addSeparator();
    fileMenu->addAction(actionSave);
    fileMenu->addAction(actionSaveAll);
    fileMenu->addSeparator();
    fileMenu->addMenu(recentProjectsMenu);
    fileMenu->addSeparator();
    //fileMenu->addAction(ui->save_files);
    fileMenu->addAction(ui->Exit);

    // 1. Создаем подменю "Открыть недавние"
    recentProjectsMenu = new QMenu("Открыть недавние", this);




    QMenu *editMenu = customMenuBar->addMenu("Правка");

    QMenu *toolsMenu = customMenuBar->addMenu("Инструменты");
    toolsMenu->addAction(ui->action_settngs);

    QMenu *helpMenu = customMenuBar->addMenu("Справка");
    helpMenu->addAction(ui->action_3);
    helpMenu->addAction(ui->action_4);

    // 6. Собираем элементы в ИДЕАЛЬНОМ вертикальном порядке (Шапка -> Menu -> Файлы)
    topLayout->addWidget(ui->customTitleBarPanel); // 1. Самый верх приложения
    topLayout->addWidget(customMenuBar);           // 2. Строго под шапкой
    if (ui->widget_3)
    {
        ui->widget_3->setStyleSheet("");
        topLayout->addWidget(ui->widget_3);       // 3. Строго под меню
    }

        // Убеждаемся, что сигналы подключены к нашей новой функции отступов
        connect(ui->leftDockWidget, &QDockWidget::visibilityChanged, this, [this](bool visible) {
            Q_UNUSED(visible);
            this->updateWidget3Padding();
        });

        // Сдвигаем виджеты сразу после старта, когда Qt отрисует геометрию дока
        QTimer::singleShot(100, this, &Neuro_programm::updateWidget3Padding);



    // 7. Регистрируем собранный блок в главном окне
    topContainerBar->addWidget(topWrapper);
    this->addToolBar(Qt::TopToolBarArea, topContainerBar);

    // 8. Корректируем углы док-виджетов
    this->setCorner(Qt::TopLeftCorner, Qt::TopDockWidgetArea);
    this->setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);

    // =================================================================
    // НАСТРОЙКА КНОПОК УПРАВЛЕНИЯ ВНУТРИ ШАПКИ
    // =================================================================

    // Очищаем customTitleBarPanel от старого мусора из Designer
    for (QWidget *child : ui->customTitleBarPanel->findChildren<QWidget*>()) {
        child->deleteLater();
    }

    if (ui->customTitleBarPanel->layout()) {
        ui->customTitleBarPanel->layout()->setParent(nullptr);
    }

    QHBoxLayout *panelLayout = new QHBoxLayout(ui->customTitleBarPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0); // Нулевые отступы для плотного прилегания кнопок к краю
    panelLayout->setSpacing(0);

    // Левая симметричная пружина
    panelLayout->addStretch();

    // Создаем текстовую метку программно
    QLabel *titleLabel = new QLabel("PyTorch Studio", ui->customTitleBarPanel);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    panelLayout->addWidget(titleLabel);

    // Правая симметричная пружина (надпись теперь строго по центру геометрии окна)
    panelLayout->addStretch();

    // 9. Создаем кнопки управления окном (ИСПРАВЛЕНЫ бинарные символы)
    QPushButton *btnMinimize = new QPushButton("—", this);
    QPushButton *btnMaximize = new QPushButton("🗖", this);
    QPushButton *btnClose    = new QPushButton("🗙", this);

    QSize btnSize(28, 28);
    btnMinimize->setFixedSize(btnSize);
    btnMaximize->setFixedSize(btnSize);
    btnClose->setFixedSize(btnSize);

    btnMinimize->setObjectName("btnMinimize");
    btnMaximize->setObjectName("btnMaximize");
    btnClose->setObjectName("btnClose");

    btnMinimize->setFlat(true);
    btnMaximize->setFlat(true);
    btnClose->setFlat(true);

    // Добавляем плоские кнопки в самый правый край шапки
    panelLayout->addWidget(btnMinimize);
    panelLayout->addWidget(btnMaximize);
    panelLayout->addWidget(btnClose);

    // Логика сигналов для кнопок управления окном
    connect(btnMinimize, &QPushButton::clicked, this, &Neuro_programm::showMinimized);
    connect(btnClose, &QPushButton::clicked, this, &Neuro_programm::close);
    connect(btnMaximize, &QPushButton::clicked, this, [this, btnMaximize]() {
        if (this->isMaximized()) {
            this->showNormal();
            btnMaximize->setText("🗖");
        } else {
            this->showMaximized();
            btnMaximize->setText("🗗");
        }
    });

    topContainerBar->installEventFilter(this);
    topWrapper->installEventFilter(this);
    ui->customTitleBarPanel->installEventFilter(this);
    customMenuBar->installEventFilter(this);
    if (ui->widget_3) {
        ui->widget_3->installEventFilter(this);
    }

    // Активируем трекинг мыши, чтобы пробить защиту QToolBar
    topContainerBar->setMouseTracking(true);
    topWrapper->setMouseTracking(true);
    ui->customTitleBarPanel->setMouseTracking(true);

    self = this;
    trainingProcess = nullptr;

    networkManager = new QNetworkAccessManager(this);

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
    btnLogs = new QPushButton("📋 Панель быстрых команд", this);
    btnTogglePip = new QPushButton("🛠 Управление пакетами", this);
    btnAIChat = new QPushButton("💬 ИИ-Ассистент", this);

    // Настраиваем окно вывода чата
    ui->chatLogWidget->setReadOnly(true);
    ui->chatLogWidget->setOpenLinks(false); // Чтобы клики обрабатывались программно

    // Привязываем кнопку отправки и клики по ссылкам-кнопкам
    connect(ui->btnSendChat, &QPushButton::clicked, this, &Neuro_programm::sendChatMessageToAI);
    connect(ui->chatLogWidget, &QTextBrowser::anchorClicked, this, &Neuro_programm::onChatAnchorClicked);


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
        "QPushButton { "
        "   border: none; "
        "   padding: 4px 12px; "
        "   background: transparent; "
        "   font-weight: bold; "
        "}";

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


    // 2. Новый переделанный коннект для управления панелью ИИ
    connect(btnLogs, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) {
            // А. Снимаем выделение со всех остальных нижних системных кнопок
            btnTerminal->setChecked(false);
            btnSearch->setChecked(false);
            btnTogglePip->setChecked(false);

            // Б. Закрываем нижнюю панель логов/терминала, чтобы освободить место
            if (panelOther) {
                panelOther->setVisible(false);
            }

            // В. Выдвигаем боковую панель быстрых команд ИИ в сплиттере
            if (ui->quickActionsList) {
                ui->quickActionsList->setVisible(true);
            }

            // Г. Заставляем горизонтальный сплиттер плавно обновить геометрию
            if (ui->mainHorizontalSplitter) {
                ui->mainHorizontalSplitter->refresh();
            }
        }
        else {
            // Если кнопку отжали — просто прячем панель быстрых команд ИИ
            if (ui->quickActionsList) {
                ui->quickActionsList->setVisible(false);
            }
        }
    });

    // Изначально скрываем список при запуске PyTorch Studio
    if (ui->quickActionsList) {
        ui->quickActionsList->hide();
    }


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
//     this->setStyleSheet(R"(
//         /* --- НАСТРОЙКА МЕНЮБЛOКОВ --- */
//         QMenuBar {
//             background-color: #f0f0f0;
//             color: #333333;
//             margin: 0px;
//             padding: 0px;
//             border-top: none;
//             border-bottom: 1px solid #b0b0b0;
//         }

//         QMenuBar::item {
//             background-color: transparent;
//             padding: 4px 8px;
//         }

//         QMenuBar::item:selected {
//             background-color: #e0e0e0;
//         }

//         QMenu {
//             background-color: #f0f0f0;
//             border: 1px solid #b0b0b0;
//             color: #333333;
//         }

//         QMenu::item:selected {
//             background-color: #e0e0e0;
//         }

//         /* --- НАСТРОЙКА ЗАГОЛОВКОВ QDOCKWIDGET --- */
//         QDockWidget::title {
//             background-color: #f0f0f0;
//             color: #333333;
//             border-left: 1px solid #b0b0b0;
//             border-right: 1px solid #b0b0b0;
//             border-bottom: 1px solid #b0b0b0;
//             border-top: none;
//             padding-left: 6px;
//             padding-top: 4px;
//             padding-bottom: 4px;
//             height: 18px;
//             margin: 0px;
//         }

//         /* Скрываем кнопки доков в обычном состоянии */
//         QDockWidget::close-button, QDockWidget::float-button {
//             qproperty-icon: none;
//             background: transparent;
//             border: none;
//             width: 0px;
//             height: 0px;
//         }

//         /* Показываем кнопки только при наведении на заголовок дока */
//         QDockWidget::title:hover QDockWidget::close-button,
//         QDockWidget::title:hover QDockWidget::float-button {
//             qproperty-icon: theme("window-close");
//             width: 16px;
//             height: 16px;
//             padding: 2px;
//         }

//         QDockWidget::close-button:hover, QDockWidget::float-button:hover {
//             background-color: #e0e0e0;
//             border-radius: 2px;
//         }

//         /* Внутреннее содержимое дока (контейнер) */
//         /* Оставляем рамку только по бокам, нижнюю убираем, чтобы она не сливалась со статусбаром */
//         #dockWidgetContents {
//             border-left: 1px solid #b0b0b0;
//             border-right: 1px solid #b0b0b0;
//             border-bottom: none;
//             border-top: none;
//         }

//         /* --- НАСТРОЙКА ЦЕНТРАЛЬНОЙ ФАЛЬШ-ПАНЕЛИ --- */
//         #widget_3 {
//             background-color: #f0f0f0;
//             border-top: none;
//             border-bottom: 1px solid #b0b0b0;
//             border-left: none;
//             border-right: none;
//             min-height: 29px;
//             max-height: 29px;
//             margin-left: -2px;
//             margin-right: -2px;
//             margin-top: 0px;
//             margin-bottom: 0px;
//             padding-left: 0px;
//             padding-right: 0px;
//         }

//         /* Системные разделители QMainWindow */
//         QMainWindow::separator {
//             background-color: #b0b0b0;
//             width: 1px;
//             height: 1px;
//             margin: 0px;
//             padding: 0px;
//         }

//         /* --- НАСТРОЙКА ДЕРЕВА (QTreeView) --- */
//         QTreeView {
//             background-color: #ffffff;
//             color: #333333;

//             /* КОРРЕКЦИЯ ЗДЕСЬ: Жестко отключаем рамку у самого дерева, */
//             /* так как внешние границы формирует контейнер дока */
//             border: none;

//             show-decoration-selected: 1;
//         }

//         QTreeView::item:hover {
//             background-color: #f2f2f2;
//         }

//         QTreeView::item:selected {
//             background-color: #e0e0e0;
//             color: #000000;
//         }

//         /* --- НАСТРОЙКА СТРОКИ СОСТОЯНИЯ (QStatusBar) --- */
//         QStatusBar {
//             background-color: #f0f0f0;
//             color: #333333;

//             /* КОРРЕКЦИЯ ЗДЕСЬ: Рисуем тонкую линию СВЕРХУ статусбара. */
//             /* На нее будут ровно опираться доки своим бесшовным нижним краем */
//             border-top: 1px solid #b0b0b0;
//             border-bottom: none;
//             border-left: none;
//             border-right: none;
//         }
//         /* --- НАСТРОЙКА COMBOBOX НА ФАЛЬШ-ПАНЕЛИ (ИДЕАЛЬНЫЙ FLAT UI) --- */
// #widget_3 QComboBox {
//         background-color: transparent !important;

//         /* Плоские линии рамок (без верхней) */
//         border-left: 1px solid #d0d0d0 !important;
//         border-right: 1px solid #d0d0d0 !important;
//         border-bottom: 1px solid #d0d0d0 !important;
//         border-top: none !important;
//         border-radius: 0px !important;

//         /* Отступы: зажимаем текст слева и справа */
//         padding-left: 6px !important;
//         padding-right: 25px !important;
//         color: #333333 !important;
//         font-size: 11px !important;
//     }

//     /* Подсветка комбобокса при наведении — МЯГКО И ЦЕЛИКОМ */
//     #widget_3 QComboBox:hover {
//         border-left: 1px solid #b0b0b0 !important;
//         border-right: 1px solid #b0b0b0 !important;
//         border-bottom: 1px solid #b0b0b0 !important;
//         border-top: none !important;
//         background-color: #e5e5e5 !important;
//     }

//     #widget_3 QComboBox:on {
//         border-left: 1px solid #b0b0b0 !important;
//         border-right: 1px solid #b0b0b0 !important;
//         border-bottom: 1px solid #b0b0b0 !important;
//         border-top: none !important;
//         background-color: #ffffff !important;
//     }

//     /* НАМЕРТВО УНИЧТОЖАЕМ СИСТЕМНУЮ КНОПКУ BREEZE, КОТОРАЯ ДАВАЛА СБОЙНЫЙ ПРЯМОУГОЛЬНИК */
//     #widget_3 QComboBox::drop-down,
//     #widget_3 QComboBox::drop-down:hover {
//         width: 0px !important;
//         border: none !important;
//         background: transparent !important;
//     }
//     #widget_3 QComboBox::down-arrow {
//         image: none !important;
//         border: none !important;
//     }

//     /* Настройка выпадающего списка */
//     #widget_3 QComboBox QAbstractItemView {
//         background-color: #ffffff !important;
//         border: 1px solid #b0b0b0 !important;
//         border-radius: 0px !important;
//         color: #333333 !important;
//         selection-background-color: #e0e0e0 !important;
//         selection-color: #000000 !important;
//         outline: none !important;
//     }


//     )");

    //panelOther->setVisible(false);

    connect(ui->New_progect, &QAction::triggered, this, &Neuro_programm::new_progect);
    connect(ui->open_progect, &QAction::triggered, this, &Neuro_programm::open_project);
    connect(ui->action_settngs, &QAction::triggered, this, &Neuro_programm::open_settings);

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

    // СИНХРОНИЗАЦИЯ СМЕНЫ ВКЛАДОК (КОРРЕКТНЫЙ ВАРИАНТ)
    connect(ui->fileComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < ui->centralStackedWidget->count())
        {
            // Перелистываем страницу стэка редактора кода
            ui->centralStackedWidget->setCurrentIndex(index);

            // Синхронизируем выделение строки в левом боковом списке открытых документов
            if (ui->openFilesListWidget) {
                ui->openFilesListWidget->setCurrentRow(index);
            }

            QString currentKey = ui->fileComboBox->itemData(index).toString();

            if (currentKey == "MAIN_SCREEN")
            {
                if (btnTerminal) btnTerminal->setChecked(true);
                if (btnAIChat) btnAIChat->setChecked(false);

                // На сервисных экранах плавно скрываем нижний список документов в 0px
                if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                    ui->openFilesContainer->setVisible(false);
                    ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
                }
            }
            else if (currentKey == "AI_CHAT_SCREEN")
            {
                if (btnAIChat) btnAIChat->setChecked(true);
                if (btnTerminal) btnTerminal->setChecked(false);

                // На экране чата тоже скрываем нижнюю панель документов в 0px
                if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                    ui->openFilesContainer->setVisible(false);
                    ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
                }
            }
            else
            {
                // ЕСЛИ ВЫБРАН ЛЮБОЙ РЕАЛЬНЫЙ С КРИПТ ИЛИ ФАЙЛ КОДА (ИНДЕКСЫ >= 2)
                if (btnTerminal) btnTerminal->setChecked(false);
                if (btnAIChat) btnAIChat->setChecked(false);

                // ГЛАВНЫЙ ИСПРАВЛЯЮЩИЙ ТРИГГЕР: Выдвигаем нижнюю панель на экран!
                if (ui->openFilesContainer && ui->leftVerticalSplitter)
                {
                    ui->openFilesContainer->setVisible(true);

                    // Задаем жесткие, видимые пропорции сплиттеру (180 пикселей под список файлов)
                    int totalHeight = ui->leftVerticalSplitter->height();
                    if (totalHeight <= 0) totalHeight = 600; // Страховка для старта

                    ui->leftVerticalSplitter->setSizes(QList<int>({totalHeight - 180, 180}));
                    ui->leftVerticalSplitter->update();
                }
            }
        }
    });

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm ---

    // 1. Создаем кастомный виджет для шапки дока
    QWidget *customTitleWidget = new QWidget(ui->leftDockWidget);

    // // Задаем горизонтальный слой с минимальными отступами
    QHBoxLayout *customTitleLayout = new QHBoxLayout(customTitleWidget);
    // customTitleLayout->addWidget(someWidget);
    // titleLayout->setContentsMargins(10, 4, 10, 4); // Небольшие аккуратные отступы
    // titleLayout->setSpacing(0);

    // 2. Создаем красивый текстовый заголовок
    QLabel *titleLabel1 = new QLabel("📁 Открытые файлы и проект", customTitleWidget);

    // ЗАЩИТА: Если метка не была найдена в Designer, создаем её сами прямо сейчас
    if (!titleLabel) {
        titleLabel = new QLabel("PyTorch Studio", ui->customTitleBarPanel);
    }

    // Настраиваем строгий полужирный шрифт в стиле Breeze
    QFont titleFont1 = titleLabel->font();
    titleFont1.setBold(true);
    titleFont1.setPointSize(10); // Аккуратный компактный размер
    titleLabel1->setFont(titleFont1);
    titleLabel1->setStyleSheet("color: #232629;"); // Контрастный темно-серый цвет Breeze

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

    // =========================================================================
    // НАСТРОЙКА ЛЕВОЙ ПАНЕЛИ НАВИГАЦИИ (СТРОГАЯ IDE СТРУКТУРА)
    // =========================================================================
    // 1. Очищаем боковой список открытых документов от тестового мусора из Designer
    ui->openFilesListWidget->clear();

    // Добавляем постоянные системные вкладки среды разработки
    QListWidgetItem *mainScreenItem = new QListWidgetItem("🎛 Панель обучения ИИ", ui->openFilesListWidget);
    mainScreenItem->setData(Qt::UserRole, QString("MAIN_SCREEN"));

    QListWidgetItem *chatScreenItem = new QListWidgetItem("💬 ИИ-Ассистент", ui->openFilesListWidget);
    chatScreenItem->setData(Qt::UserRole, QString("AI_CHAT_SCREEN"));

    // По умолчанию при старте выделяем первую строку (Панель ИИ)
    ui->openFilesListWidget->setCurrentRow(0);

    // ХИРУРГИЧЕСКИЙ ФИКС НАЛОЖЕНИЯ ТЕКСТА:
    // Намертво удаляем старые текстовые метки из Designer, чтобы они не слипались в углу
    // if (ui->label) {
    //     ui->label->setParent(nullptr);
    //     ui->label->deleteLater();
    // }
    // if (ui->label_2) {
    //     ui->label_2->setParent(nullptr);
    //     ui->label_2->deleteLater();
    // }

    // 2. ИЗНАЧАЛЬНО СКРЫВАЕМ НИЖНИЙ КОНТЕЙНЕР ПРИ ЗАПУСКЕ ПРОГРАММЫ
    if (ui->openFilesContainer) {
        ui->openFilesContainer->setVisible(false);
    }

    // 3. СБОРКА ВЕРТИКАЛЬНОГО СПЛИТТЕРА БЕЗ РАЗРЫВА РОДИТЕЛЬСКИХ СВЯЗЕЙ
    // =========================================================================
    // ТОТАЛЬНОЕ УНИЧТОЖЕНИЕ ПУСТОТЫ И НАСТРОЙКА ЛЕВОЙ ПАНЕЛИ НАВИГАЦИИ
    // =========================================================================

    // =========================================================================
    // ТОТАЛЬНОЕ УНИЧТОЖЕНИЕ ПУСТОТЫ И НАСТРОЙКА ЛЕВОЙ ПАНЕЛИ НАВИГАЦИИ (ФИНАЛ)
    // =========================================================================
    // =========================================================================
    // ТОТАЛЬНОЕ УНИЧТОЖЕНИЕ ПУСТОТЫ И НАСТРОЙКА ЛЕВОЙ ПАНЕЛИ НАВИГАЦИИ (ФИНАЛ)
    // =========================================================================
    if (ui->leftDockWidget)
    {
        // 1. Сбрасываем минимальные лимиты высоты из Designer (строка 11)
        ui->leftDockWidget->setMinimumSize(QSize(300, 0));
        ui->leftDockWidget->setMaximumSize(QSize(300, 524287));

        // 2. Убираем встроенную плашку заголовка QDockWidget
        QWidget *emptyTitleWidget = new QWidget(ui->leftDockWidget);
        ui->leftDockWidget->setTitleBarWidget(emptyTitleWidget);

        // 3. Срезаем скрытые рамки у самих списков
        ui->leftDockWidget->setStyleSheet(
            "QDockWidget { border: 1px solid #b0b0b0; padding: 0px !important; margin: 0px !important; }"
            "QDockWidget > QWidget { padding: 0px !important; margin: 0px !important; background: #ffffff; }"
            "QTreeView, QListWidget { border: none; padding: 0px; margin: 0px; background: #ffffff; }"
        );

        // Считываем подложку дока и принудительно зануляем её макет
        QWidget *dockContents = ui->leftDockWidget->widget();
        if (dockContents && dockContents->layout()) {
            dockContents->layout()->setContentsMargins(0, 0, 0, 0);
            dockContents->layout()->setSpacing(0);
        }
    }

    // =========================================================================
    // ТОТАЛЬНОЕ УНИЧТОЖЕНИЕ ПУСТОТЫ И НАСТРОЙКА ЛЕВОЙ ПАНЕЛИ НАВИГАЦИИ (ФИНАЛ)
    // =========================================================================
    if (ui->leftDockWidget)
    {
        // 1. Сбрасываем минимальные лимиты высоты из Designer
        ui->leftDockWidget->setMinimumSize(QSize(300, 0));
        ui->leftDockWidget->setMaximumSize(QSize(300, 524287));

        // 2. Убираем встроенную плашку заголовка QDockWidget
        QWidget *emptyTitleWidget = new QWidget(ui->leftDockWidget);
        emptyTitleWidget->setFixedHeight(0); // Схлопываем невидимую заглушку в ноль
        emptyTitleWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        ui->leftDockWidget->setTitleBarWidget(emptyTitleWidget);

        // 3. ХИРУРГИЧЕСКИЙ CSS-СДВИГ: Выталкиваем первые элементы списков вверх,
        // полностью уничтожая скрытые XML-пустоты под надписями!
        ui->leftDockWidget->setStyleSheet(
                    "QDockWidget {"
                       " border: 1px solid #b0b0b0;"
                       " padding: 0px !important;"
                       " margin: 0px !important;"
                       "}"
                       "QDockWidget > QWidget {"
                       " padding: 0px !important;"
                       " margin: 0px !important;"
                       " background: #ffffff;"
                       "}"
                       "QTreeView, QListWidget {"
                       " border: none;"
                       " margin: 0px !important;"
                       " padding: 0px !important;"
                       " background: #ffffff;"
                       "}"
                       "QTreeView::item, QListWidget::item {"
                       " padding-top: 2px !important;"
                       " padding-bottom: 2px !important;"
                       "}"
                   );

        // Считываем подложку дока и принудительно зануляем её макет
        QWidget *dockContents = ui->leftDockWidget->widget();
        if (dockContents && dockContents->layout()) {
            dockContents->layout()->setContentsMargins(0, 0, 0, 0);
            dockContents->layout()->setSpacing(0);
        }
    }


    if (ui->leftVerticalSplitter)
    {
        // Убираем толщину внутренней ручки-разделителя сплиттера
        ui->leftVerticalSplitter->setHandleWidth(0);

        // Принудительно сбрасываем любые сохраненные ограничения контейнеров из Designer
        ui->projectContainer->setMinimumSize(QSize(0, 0));
        ui->openFilesContainer->setMinimumSize(QSize(0, 0));

        ui->projectContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
        ui->openFilesContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);

        // БЕЗОПАСНЫЙ ФИКС БЕЗ УДАЛЕНИЯ: Настраиваем существующие макеты из XML прямо в памяти
        // Настраиваем верхнюю сетку gridLayout_2 (Страница 12 вашего XML)
        // БЕЗОПАСНЫЙ ФИКС БЕЗ УДАЛЕНИЯ: Настраиваем существующие макеты из XML прямо в памяти
        // Настраиваем верхнюю сетку gridLayout_2 (Страница 12 вашего XML)
        if (ui->gridLayout_2) {
            // 1. Полностью удаляем старую сетку из контейнера, чтобы она не мешала
            delete ui->gridLayout_2;

            // 2. Создаем чистый вертикальный компоновщик с абсолютно НУЛЕВЫМИ отступами
            QVBoxLayout *projectLayout = new QVBoxLayout(ui->projectContainer);
            projectLayout->setContentsMargins(6, 0, 6, 0); // 6px по бокам для Breeze, 0px сверху/снизу
            projectLayout->setSpacing(2);                 // Зазор ровно 2 пикселя между текстом и деревом

            // 3. Создаем компактный заголовок
            QLabel *lblProjectTitle = new QLabel(" Структура проекта", ui->projectContainer);
            lblProjectTitle->setStyleSheet(
                "font-weight: bold; "
                "color: #505050; "
                "font-size: 11px; "
                "padding: 0px; "
                "margin: 0px !important;"
            );
            lblProjectTitle->setFixedHeight(14); // Жестко фиксируем высоту текста

            // 4. Просто складываем их по порядку сверху вниз
            projectLayout->addWidget(lblProjectTitle);
            projectLayout->addWidget(ui->treeView);
        }


        // Настраиваем нижнюю сетку gridLayout_3 (Страница 12 вашего XML)
        if (ui->gridLayout_3) {
            ui->gridLayout_3->setContentsMargins(6, 4, 6, 0);
            ui->gridLayout_3->setSpacing(0);
            ui->gridLayout_3->setVerticalSpacing(0); // Намертво убираем пустой шаг между строками

            QLabel *lblFilesTitle = new QLabel("📝 Открытые документы", ui->openFilesContainer);
            lblFilesTitle->setStyleSheet("font-weight: bold; color: #505050; font-size: 11px; padding: 0px; margin: 0px; background: transparent;");

            ui->gridLayout_3->removeWidget(ui->openFilesListWidget);
            ui->gridLayout_3->addWidget(lblFilesTitle, 0, 0);
            ui->gridLayout_3->addWidget(ui->openFilesListWidget, 1, 0);

            ui->gridLayout_3->setRowMinimumHeight(0, 15);
            ui->gridLayout_3->setRowMinimumHeight(1, 0);

            ui->gridLayout_3->setRowStretch(0, 0);
            ui->gridLayout_3->setRowStretch(1, 1);
        }

        ui->treeView->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        ui->openFilesListWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);

        // Фиксируем двухкомпонентную иерархию сплиттера
        ui->leftVerticalSplitter->insertWidget(0, ui->projectContainer);
        ui->leftVerticalSplitter->insertWidget(1, ui->openFilesContainer);

        ui->leftVerticalSplitter->setStretchFactor(0, 1);
        ui->leftVerticalSplitter->setStretchFactor(1, 1);

        ui->leftVerticalSplitter->setCollapsible(0, false);
        ui->leftVerticalSplitter->setCollapsible(1, true);

        // Схлопываем нижнюю панель в 0 при старте, отдавая всё место дереву файлов
        ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
    }

    ui->treeView->setIndentation(20);

    // Полностью убираем мелкую системную рамку-заголовок DockWidget
    QWidget *emptyTitleWidget = new QWidget(ui->leftDockWidget);
    ui->leftDockWidget->setTitleBarWidget(emptyTitleWidget);

    // Коннект для обработки двойного клика по строкам списка открытых документов
    connect(ui->openFilesListWidget, &QListWidget::itemDoubleClicked, this,
            &Neuro_programm::onOpenFileListItemDoubleClicked);


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



    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm в файле neuro_programm.cpp ---

    // // 1. Создаем действие "Сохранить" (Ctrl + S)
    // QAction *actionSave = new QAction("💾 Сохранить", this);
    // actionSave->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S)); // Подсказка горячих клавиш в меню
    // connect(actionSave, &QAction::triggered, this, &Neuro_programm::saveCurrentActiveFile);

    // // 2. Создаем действие "Сохранить всё" (Ctrl + Shift + S)
    // QAction *actionSaveAll = new QAction("📚 Сохранить всё", this);
    // actionSaveAll->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    // connect(actionSaveAll, &QAction::triggered, this, &Neuro_programm::saveAllProjectChanges);

    // 3. Создаем наше старое подменю "Открыть недавние"
    recentProjectsMenu = new QMenu("📁 Открыть недавние", this);

    // 4. ИНТЕГРАЦИЯ В ВЕРХНЕЕ МЕНЮ "ФАЙЛ" ПО ЕГО НАЗВАНИЮ
    //QMenuBar *bar = this->menuBar();
    // if (bar)
    // {
    //     QList<QAction*> actions = bar->actions();
    //     bool attached = false;

    //     for (QAction *action : actions)
    //     {
    //         // Ищем вкладку меню, которая содержит слово "Файл" или "File"
    //         if (action->menu() && (action->text().contains("Файл") || action->text().contains("File")))
    //         {
    //             QMenu *fileMenu = action->menu();

    //             fileMenu->addSeparator();             // Рисуем тонкую разделительную линию Breeze
    //             fileMenu->addAction(actionSave);      // Вставляем пункт "Сохранить"
    //             fileMenu->addAction(actionSaveAll);   // Вставляем пункт "Сохранить всё"
    //             fileMenu->addSeparator();             // Рисуем вторую линию разделителя
    //             fileMenu->addMenu(recentProjectsMenu); // Вставляем наш список недавних проектов

    //             attached = true;
    //             break;
    //         }
    //     }

        // // Создаем действие "Закрыть проект"
        // QAction *actionCloseProject = new QAction("❌ Закрыть проект", this);
        // actionCloseProject->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
        // connect(actionCloseProject, &QAction::triggered, this, &Neuro_programm::onCloseProjectClicked);

        // // ИНТЕГРИРУЕМ В НАШ СУЩЕСТВУЮЩИЙ ЦИКЛ ДИНАМИЧЕСКОГО ПОИСКА ВКЛАДКИ "ФАЙЛ"
        // QMenuBar *bar = this->menuBar();
        // if (bar)
        // {
        //     QList<QAction*> actions = bar->actions();
        //     for (QAction *action : actions)
        //     {
        //         if (action->menu() && (action->text().contains("Файл") || action->text().contains("File")))
        //         {
        //             QMenu *fileMenu = action->menu();

        //             // =================================================================
        //             // ИСПРАВЛЕНИЕ: Просто добавляем кнопку в конец меню без угадывания имен!
        //             // =================================================================
        //             fileMenu->addSeparator();            // Тонкая разделительная черта Breeze
        //             fileMenu->addAction(actionCloseProject); // Вставляем пункт "Закрыть проект"
        //             break;
        //         }
        //     }
        // }


        // // Резервный вариант: если меню "Файл" не найдено, выводим элементы на верхнюю панель напрямую
        // if (!attached) {
        //     bar->addAction(actionSave);
        //     bar->addAction(actionSaveAll);
        //     bar->addMenu(recentProjectsMenu);
        // }
  //  }


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

   // detectCudaDevices();

    if (!currentOpenProjectPath.isEmpty()) {
        initLspServer();
    }


    connect(ui->new_file, &QAction::triggered, this, [this]() {
        // Защита: Если проект еще не создан или не открыт, не даем создавать файлы
        if (currentOpenProjectPath.isEmpty()) {
            sendSystemNotification("Внимание", "Сначала откройте или создайте проект (*.pystudio)");
            return;
        }

        bool ok;
        // Всплывающее окно для ввода имени файла
        QString fileName = QInputDialog::getText(this, "Создание файла",
                                                 "Введите имя нового Python-файла:",
                                                 QLineEdit::Normal, "script", &ok);

        // Если пользователь нажал OK и строка не пустая
        if (ok && !fileName.trimmed().isEmpty()) {
            // Автоматически добавляем расширение .py, если пользователь его забыл
            if (!fileName.endsWith(".py", Qt::CaseInsensitive)) {
                fileName += ".py";
            }

            // Собираем абсолютный путь (например, создаем сразу в корне проекта)
            QString fullPath = currentOpenProjectPath + "/" + fileName.trimmed();

            // Запускаем ваш пуленепробиваемый метод открытия и инициализации в Jedi!
            this->openNewFileInEditor(fullPath);
        }
    });

    // Внутри конструктора Neuro_programm в файле neuro_programm.cpp
    connect(this, &Neuro_programm::completionDataReceived,
            this, &Neuro_programm::showCompletionMenuInGuiThread);

    ui->chatLogWidget->setReadOnly(true);
    ui->chatLogWidget->setOpenLinks(false); // Отключаем открытие ссылок в браузере
    connect(ui->chatLogWidget, &QTextBrowser::anchorClicked, this, &Neuro_programm::onChatAnchorClicked);
    connect(ui->quickActionsList, &QListWidget::itemDoubleClicked, this, &Neuro_programm::onQuickActionTriggered);
    connect(ui->quickActionsList, &QListWidget::itemDoubleClicked, this, &Neuro_programm::onQuickActionTriggered);

    sendInitialWelcomeRequest();

    this->applyGlobalFonts();
}

Neuro_programm::~Neuro_programm()
{
    if (lspProcess) {
        lspProcess->disconnect(); // Намертво отключаем сигналы чтения логов
        lspProcess->kill();       // Посылаем системный сигнал завершения
        lspProcess->waitForFinished(300); // Даем 300мс на безопасную очистку ОЗУ
    }

    if (trainingProcess) {
        trainingProcess->disconnect();
        trainingProcess->kill();
        trainingProcess->waitForFinished(300);
    }

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
    initLspServer();
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
    // =========================================================================
    // 1. ВЫЗОВ ОРИГИНАЛЬНОГО ОКНА KDE PLASMA НАПРЯМУЮ ЧЕРЕЗ СИСТЕМНЫЙ КАНАЛ KDIALOG
    // =========================================================================
    QProcess kdialogProcess;
    QStringList arguments;
    arguments << "--title" << "Открыть проект PyTorch Studio"
              << "--getopenfilename" << QDir::homePath()
              << "*.pystudio | Файлы проекта PyTorch Studio (*.pystudio)";

    // Запускаем процесс kdialog (нативный компонент KDE Plasma)
    kdialogProcess.start("kdialog", arguments);

    // Ждем, пока пользователь выберет файл (таймаут 60 секунд)
    if (!kdialogProcess.waitForFinished(60000)) {
        kdialogProcess.kill();
        return;
    }

    // Считываем чистый путь к выбранному файлу, удаляя лишние символы переноса строки
    QString selectedFile = QString::fromUtf8(kdialogProcess.readAllStandardOutput()).trimmed();

    // Если пользователь закрыл окно kdialog или нажал "Отмена" — безопасно выходим
    if (selectedFile.isEmpty()) return;


    // =========================================================================
    // 2. ОТКРЫВАЕМ И ЧИТАЕМ JSON КОНФИГУРАЦИЮ ФАЙЛА ПРОЕКТА
    // =========================================================================
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


    // =========================================================================
    // 3. ИЗВЛЕКАЕМ СОХРАНЕННЫЕ ДАННЫЕ ПРОЕКТА
    // =========================================================================
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
    // 4. ИНИЦИАЛИЗИРУЕМ МОДЕЛЬ ДЕРЕВА И СТЭК-ВИДЖЕТ ПАНЕЛИ
    // =========================================================================
    initProjectTreeModel(fullProjectPath);

    // Синхронизируем фокус главного экрана на Панель обучения (индекс 0)
    ui->centralStackedWidget->setCurrentIndex(0);
    if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);
    if (ui->openFilesListWidget) ui->openFilesListWidget->setCurrentRow(0);


    // =========================================================================
    // 5. ОБЯЗАТЕЛЬНЫЙ ПЕРЕОПРОС ЖЕЛЕЗА ДЛЯ ЭТОГО КОНКРЕТНОГО ПК
    // =========================================================================
    detectCudaDevices();


    // =========================================================================
    // 6. СИНХРОНИЗАЦИЯ ИНТЕРФЕЙСА С СОХРАНЕННЫМИ НАСТРОЙКАМИ JSON
    // =========================================================================
    if (ui->spinBoxEpochs) ui->spinBoxEpochs->setValue(epochs);
    if (ui->spinBoxLR) ui->spinBoxLR->setValue(lr);

    // Выставляем сохраненный размер батча нейросети
    if (ui->comboBatchSize) {
        int batchIdx = ui->comboBatchSize->findText(QString::number(batchSize));
        if (batchIdx != -1) ui->comboBatchSize->setCurrentIndex(batchIdx);
    }

    // УМНАЯ ПРОВЕРКА ДЕВАЙСА: Проверяем, доступна ли на ТЕКУЩЕМ компьютере сохраненная видеокарта CUDA
    if (ui->comboDevice) {
        int deviceIdx = ui->comboDevice->findText(savedDevice);
        if (deviceIdx != -1) {
            ui->comboDevice->setCurrentIndex(deviceIdx);
        } else {
            ui->comboDevice->setCurrentIndex(0);
            sendSystemNotification("Конфигурация железа",
                                   "⚠️ Предупреждение: Сохраненное устройство CUDA недоступно на этом ПК. Сброшено на CPU.");
        }
    }

    // Передаем актуальный путь в нижнюю панель для доустановки пакетов pip
    if (panelOther) {
        panelOther->setCurrentProjectPath(fullProjectPath);
    }

    // Обновляем заголовок главного окна ИИ-студии и уведомляем пользователя
    this->setWindowTitle(QString("PyTorch Studio - %1 [%2]").arg(projName).arg(fullProjectPath));
    sendSystemNotification("PyTorch Studio", QString("✔ Проект '%1' успешно загружен").arg(projName));
    addProjectToRecent(selectedFile);
    initLspServer();
}

void Neuro_programm::onFileDoubleClicked(const QModelIndex &index)
{
    // 1. ИЗВЛЕКАЕМ АБСОЛЮТНЫЙ ПУТЬ К ФАЙЛУ ИЗ МОДЕЛИ ДЕРЕВА (Ваш рабочий код)
    QString filePath = projectModel->fileInfo(index).absoluteFilePath();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << " [ОШИБКА] Не удалось физически прочитать файл с диска:" << filePath;
        return;
    }

    // Считываем сырые байты Python-скрипта в текстовую переменную
    QString fileContent = QString::fromUtf8(file.readAll());
    file.close();

    // 2. ПРОВЕРЯЕМ, НЕ ОТКРЫТ ЛИ ЭТОТ ДОКУМЕНТ УЖЕ В СОСЕДНЕЙ ВКЛАДКЕ
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && page->objectName() == filePath) {
            ui->centralStackedWidget->setCurrentWidget(page);

            // Если в комбобоксе файлов есть этот документ — принудительно синхронизируем индекс
            if (ui->fileComboBox) {
                int comboIdx = ui->fileComboBox->findData(filePath);
                if (comboIdx != -1) ui->fileComboBox->setCurrentIndex(comboIdx);
            }
            return; // Файл уже на экране, просто переключили фокус вкладок
        }
    }

    // 3. СОЗДАЕМ НОВУЮ ГРАФИЧЕСКУЮ СТРАНИЦУ-КОНТЕЙНЕР ВНУТРИ STACKED-ПАНЕЛИ
    QWidget *newPage = new QWidget(ui->centralStackedWidget);
    QVBoxLayout *layout = new QVBoxLayout(newPage);
    layout->setContentsMargins(0, 0, 0, 0);

    // Инициализируем кастомный текстовый редактор автодополнения
    CodeEditor *editor = new CodeEditor(newPage);

    // Записываем абсолютный путь к файлу в объектное имя самого виджета для связки путей
    editor->setObjectName(filePath);
    newPage->setObjectName(filePath);

    // Настраиваем параметры отображения кода на экране (отключаем ломающий перенос строк)
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(editor);

    // Добавляем созданную страницу в StackedWidget и выводим её на экран
    ui->centralStackedWidget->addWidget(newPage);
    ui->centralStackedWidget->setCurrentWidget(newPage);

    // Загружаем в редактор считанный код Python-файла
    editor->setPlainText(fileContent);

    // 4. ИНИЦИАЛИЗИРУЕМ СЕРВЕР JEDI И ОТПРАВЛЯЕМ DIDOPEN ЗАПРОС
    this->initLspServer();

    if (this->lspProcess && this->lspProcess->state() == QProcess::Running)
    {
        QJsonObject openParams;
        QJsonObject textDocument;

        // Собираем идеальный каноничный URI-путь через QUrl (file:///)
        textDocument["uri"] = QUrl::fromLocalFile(filePath).toString();
        textDocument["languageId"] = "python";
        this->globalLspDocVersion = 1;
        textDocument["version"] = this->globalLspDocVersion;

        // Очищаем стартовый буфер текста от скрытых DOS-переносов строк \r
        QString cleanStartText = fileContent;
        cleanStartText.remove('\r');
        textDocument["text"] = cleanStartText;
        openParams["textDocument"] = textDocument;

        // Отправляем официальный didOpen запрос серверу Jedi
        this->sendLspRequest("textDocument/didOpen", openParams);
    }

    // =========================================================================
    // 5. НАПОЛНЕНИЕ СПИСКОВ И ЖЕСТКАЯ АКТИВАЦИЯ КОНТЕЙНЕРА НА ЭКРАНЕ
    // =========================================================================
    QFileInfo info(filePath);

    // Шаг А: Добавляем имя файла в нижний боковой список документов, если он есть
    if (ui->openFilesListWidget) {
        // Проверяем, нет ли уже такого файла в списке, чтобы не дублировать строки
        bool exists = false;
        for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
            if (ui->openFilesListWidget->item(i)->data(Qt::UserRole).toString() == filePath) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            QListWidgetItem *newDocItem = new QListWidgetItem(info.fileName(), ui->openFilesListWidget);
            newDocItem->setData(Qt::UserRole, filePath);
        }
    }

    // Шаг Б: Добавляем имя файла в верхний комбобокс открытых документов
    if (ui->fileComboBox) {
        ui->fileComboBox->addItem(info.fileName(), filePath);
        ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count() - 1);
    }

    // ШАГ В (ГЛАВНЫЙ СУПЕР-ФИКС ГЕОМЕТРИИ):
    // Принудительно разворачиваем нижний контейнер, ломая любые запреты из Qt Designer
    if (ui->openFilesContainer && ui->leftVerticalSplitter)
    {
        // 1. Делаем контейнер видимым
        ui->openFilesContainer->setVisible(true);

        // 2. ЖЕСТКО СТИРАЕМ ОГРАНИЧЕНИЯ ДИЗАЙНЕРА:
        // Переключаем политику размеров нижнего контейнера на Ignored по вертикали.
        // Это заставит сплиттер беспрекословно принять те размеры, которые мы укажем в коде!
        ui->openFilesContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
        ui->openFilesContainer->setMaximumHeight(10000); // Сбрасываем жесткий лимит высоты, если он был

        // 3. Расчет высоты
        int totalHeight = ui->leftVerticalSplitter->height();
        if (totalHeight <= 0) totalHeight = this->height() - 150; // Страховка на основе высоты главного окна

        // Выделяем под дерево проекта верхнюю часть, а нижнему списку документов отдаем 180 пикселей
        int topSize = totalHeight - 180;
        if (topSize < 100) topSize = 350; // Защита от ухода в отрицательные пиксели

        // Принудительно заталкиваем массив размеров в сплиттер
        ui->leftVerticalSplitter->setSizes(QList<int>({topSize, 180}));

        // ИСПРАВЛЕННЫЙ ФИКС ОБНОВЛЕНИЯ: Принудительно пересчитываем сетку виджета
        ui->leftVerticalSplitter->updateGeometry();
        ui->leftVerticalSplitter->refresh();
        ui->leftVerticalSplitter->update();
    }

    // Обновляем шрифты интерфейса
    this->applyGlobalFonts();
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

void Neuro_programm::initProjectTreeModel(QString path)
{
    QString safePath = path.trimmed();
    if (safePath.isEmpty() || safePath == "") {
        qWarning() << " [LSP ПРЕДУПРЕЖДЕНИЕ] Вызван initProjectTreeModel с пустым путем. Пропуск.";
        return;
    }

    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    projectModel = new QFileSystemModel(this);
    projectModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    projectModel->setNameFilters(QStringList() << "[^v]*" << "v[^e]*" << "ve[^n]*" << "ven[^v]*" << "venv?*");
    projectModel->setNameFilterDisables(false);
    projectModel->setRootPath(safePath);

    ui->treeView->setModel(projectModel);

    QModelIndex rootIndex = projectModel->index(safePath);
    ui->treeView->setRootIndex(rootIndex);
    ui->treeView->expand(rootIndex);

    if (projectModel != nullptr && ui->treeView->model() != nullptr)
    {
        for (int i = 1; i < projectModel->columnCount(); ++i) {
            ui->treeView->hideColumn(i);
        }
    }

    // =========================================================================
    // БЕЗОПАСНЫЙ И ПУЛЕНЕПРОБИВАЕМЫЙ ФИКС СКРЫТИЯ ЗАЗОРА (БЕЗ УДАЛЕНИЯ ОБЪЕКТА)
    // =========================================================================
    if (ui->treeView->header())
    {
        // 1. Скрываем текстовое поле заголовка ("Имя")
        ui->treeView->setHeaderHidden(true);

        // 2. Схлопываем высоту скрытого заголовка в 0 пикселей,
        // чтобы он физически больше не расталкивал пустое пространство!
        ui->treeView->header()->setMaximumHeight(0);
        ui->treeView->header()->setMinimumSectionSize(0);
        ui->treeView->header()->resizeSections(QHeaderView::Fixed);

        // 3. Отключаем любые отступы рамки заголовка
        ui->treeView->header()->setStyleSheet("QHeaderView { margin: 0px; padding: 0px; height: 0px; border: none; }");
    }
}


void Neuro_programm::sendChatMessageToAI()
{
    QString userQuery = ui->inputChatText->toPlainText().trimmed();
    if (userQuery.isEmpty()) return;

    ui->chatLogWidget->setReadOnly(true);
    ui->chatLogWidget->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->chatLogWidget->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    // Форматируем вывод реплики пользователя в логе чата
    if (userQuery.startsWith("Напиши профессиональные комментарии") ||
        userQuery.startsWith("Выступи в роли эксперта") ||
        userQuery.startsWith("Оптимизируй этот код"))
    {
        QString commandTitle = userQuery.split('\n').first();
        ui->chatLogWidget->append("<font color='#0056b3'><b>Вы:</b><br><i>" + commandTitle.toHtmlEscaped() + "</i></font><br>");
    } else {
        ui->chatLogWidget->append("<font color='#0056b3'><b>Вы:</b><br>" + userQuery.toHtmlEscaped().replace("\n", "<br>") + "</font><br>");
    }

    ui->inputChatText->clear();

    QMovie *chatLoader = this->property("chatLoader").value<QMovie*>();
    if (chatLoader) {
        chatLoader->start();
        ui->chatLogWidget->append("<font color='#555555'><b>ИИ:</b> думает <img src=':/images/loader.gif' height='14'></font>");
    } else {
        ui->chatLogWidget->append("<font color='#555555'><b>ИИ:</b> Читаю файлы проекта и генерирую ответ...</font>");
    }
    ui->chatLogWidget->moveCursor(QTextCursor::End);

    ui->inputChatText->setEnabled(false);
    ui->btnSendChat->setEnabled(false);

    // =========================================================================
    // 2. УМНЫЙ СБОР КОНТЕКСТА ДЛЯ ОТПРАВКИ НА СЕРВЕР
    // =========================================================================
    QString finalSystemContent = "";

    // ЕСЛИ КОД УЖЕ ЗАШИТ В КВАРТЕТЕ ЗАПРОСА (Быстрая команда) — отправляем его монолитом
    if (userQuery.contains("```python"))
    {
        QString systemInstruction = "Ты — встроенный ИИ-помощник в среде 'PyTorch Studio'. Твоя цель — помогать пользователю настраивать обучение нейросетей PyTorch на основе предоставленного кода.";
        finalSystemContent = systemInstruction + "\n\n" + userQuery;
    }
    else
    {
        // Свободный режим: считываем текст активного CodeEditor
        QString projectContext = "--- КОНТЕКСТ ИСХОДНОГО КОДА ПРОЕКТА ---\n";
        int currentFileIdx = ui->fileComboBox->currentIndex();

        if (currentFileIdx >= 2)
        {
            QWidget *filePageWidget = ui->centralStackedWidget->widget(currentFileIdx);
            if (filePageWidget) {
                CodeEditor *editor = filePageWidget->findChild<CodeEditor*>();
                if (editor) {
                    QString activeFileName = ui->fileComboBox->itemText(currentFileIdx);
                    projectContext += QString("\n[Текущий открытый файл в PyTorch Studio: %1]\n```python\n%2\n```\n")
                                          .arg(activeFileName).arg(editor->toPlainText());
                }
            }
        }
        QString systemInstruction = "Ты — встроенный ИИ-помощник в среде 'PyTorch Studio'. Твоя цель — помогать пользователю настраивать обучение нейросетей PyTorch на основе предоставленного кода.";
        finalSystemContent = systemInstruction + "\n\n" + projectContext;
    }

    // =========================================================================
    // 3. СБОРКА И ОТПРАВКА JSON В OLLAMA API (/api/chat)
    // =========================================================================
    QJsonObject requestBody;
    requestBody["model"] = "qwen2.5-coder:1.5b";
    requestBody["stream"] = false;

    QJsonArray messagesArray;

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = finalSystemContent;
    messagesArray.append(systemMessage);

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = userQuery.split('\n').first(); // Краткое действие
    messagesArray.append(userMessage);

    requestBody["messages"] = messagesArray;

    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("http://localhost:11434/api/chat"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, QJsonDocument(requestBody).toJson());

    // =========================================================================
    // 4. ПРИЕМ ОТВЕТА
    // =========================================================================
    connect(reply, &QNetworkReply::finished, this, [this, reply, networkManager, chatLoader]() {
        if (chatLoader) chatLoader->stop();

        ui->inputChatText->setEnabled(true);
        ui->btnSendChat->setEnabled(true);
        ui->inputChatText->setFocus();

        QTextCursor cursor = ui->chatLogWidget->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.removeSelectedText();
        cursor.deletePreviousChar();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject responseObj = QJsonDocument::fromJson(reply->readAll()).object();
            QString aiResponse = responseObj["message"].toObject()["content"].toString().trimmed();

            if (!aiResponse.isEmpty()) {
                QString formattedHtml = this->parseMarkdownCodeBlocks(aiResponse);
                QString responseId = QString("resp_%1").arg(++responseCounter);
                aiResponsesMap.insert(responseId, aiResponse);

                QString actionPanelHtml = QString(
                                              "<div style='margin-top: 8px; padding-top: 6px; border-top: 1px dashed #cbd5e1; font-family: sans-serif; font-size: 12px; text-align: left;'>"
                                              " <span style='color: #718096; margin-right: 15px;'>Действия:</span>"
                                              " <a href='action:copy:%1' style='color: #0056b3; text-decoration: none; margin-right: 12px;'>📋 Копировать ответ</a>"
                                              " <a href='action:export:%1' style='color: #0056b3; text-decoration: none;'>💾 В TXT</a>"
                                              "</div>"
                                              ).arg(responseId);

                ui->chatLogWidget->append(
                    "<div style='margin-bottom: 25px; padding: 12px; background-color: #f8f9fa; border-left: 4px solid #007acc; border-radius: 4px;'>"
                    " <b style='color: #007acc;'>Ollama AI:</b>"
                    " <div style='color: #232629; margin-top: 5px;'>" + formattedHtml + "</div>" + actionPanelHtml +
                    "</div>"
                    );
            }
        } else {
            ui->chatLogWidget->append("<font color='#cc0000'><b>Ошибка:</b> Оллама не отвечает.</font><br>");
        }
        ui->chatLogWidget->moveCursor(QTextCursor::End);
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

    // Считываем сквозной поток данных (включая ошибки импорта и рантайма)
    QByteArray rawData = trainingProcess->readAll();
    QString outputText = QString::fromUtf8(rawData);

    // ВЫВОД: Выводим текст во встроенный терминал на экране
    QTextEdit *richConsole = panelOther->findChild<QTextEdit*>();
    if (richConsole != nullptr) {
        richConsole->insertPlainText(outputText);
        richConsole->moveCursor(QTextCursor::End); // Авто-скролл вниз
    }

    // =========================================================================
    // АВТОМАТИЧЕСКОЕ РАСПОЗНАВАНИЕ ОШИБОК КОМПИЛЯЦИИ/РАНТАЙМА PYTHON
    // =========================================================================
    // Проверяем, содержит ли прилетевший лог признаки критической ошибки Python
    if (outputText.contains("Traceback (most recent call last):") || outputText.contains("Error:"))
    {
        QStringList lines = outputText.trimmed().split('\n', Qt::SkipEmptyParts);
        QString mainErrorLine = "Неизвестная ошибка PyTorch";

        // Пытаемся найти финальную значимую строку ошибки (обычно это последняя строка)
        if (!lines.isEmpty()) {
            mainErrorLine = lines.last().trimmed();
        }

        // Отправляем интерактивную плашку-предложение в чат-ассистент
        ui->chatLogWidget->append(
            "<div style='margin: 12px 0; padding: 12px; background-color: #fff5f5; "
            "border: 1px solid #feb2b2; border-left: 5px solid #cc0000; border-radius: 4px; font-family: sans-serif;'>"
            "  <b style='color: #cc0000;'>⚠️ Обнаружена ошибка выполнения в PyTorch!</b><br>"
            "  <code style='color: #2d3748; font-size: 13px; font-weight: bold;'>" + mainErrorLine.toHtmlEscaped() + "</code><br><br>"
                                              "  <a href='action:fix_error' style='color: #0056b3; font-weight: bold; text-decoration: none;'>[🤖 Исправить ошибку через Ollama ИИ]</a>"
                                              "</div>"
            );

        // Запоминаем этот лог ошибки внутри динамических свойств программы
        this->setProperty("lastPythonErrorTraceback", outputText);
        ui->chatLogWidget->moveCursor(QTextCursor::End);
    }

    // =========================================================================
    // ВАШ РОДНОЙ НЕИЗМЕНЯЕМЫЙ КОД ПАРСИНГА ГРАФИКОВ И HTML МЕТРИК
    // =========================================================================
    static QRegularExpression lossRegex("PROGRESS:\\s*epoch=(\\d+),\\s*loss=([0-9.]+)");
    static QRegularExpression metricsRegex("METRICS:\\s*epoch=(\\d+),\\s*accuracy=([0-9.]+)%\\s*vram=([0-9.]+)GB,\\s*speed=(\\d+)");

    QStringList lines = outputText.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        // Парсинг синей кривой Loss
        QRegularExpressionMatch lossMatch = lossRegex.match(line.trimmed());
        if (lossMatch.hasMatch()) {
            double epoch = lossMatch.captured(1).toDouble();
            double loss = lossMatch.captured(2).toDouble();
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
            QString accStr = metricsMatch.captured(2);
            QString vramStr = metricsMatch.captured(3);
            QString speedStr = metricsMatch.captured(4);
            QString htmlReport = QString(
                                     "<div style='font-family:\"Monospace\"; font-size:13px; color:#232629;'>"
                                     " <b style='color:#0056b3; font-size:14px;'> МОНИТОРИНГ МЕТРИК НЕЙРОСЕТИ:</b><br>"
                                     " <hr style='border:none; border-top:1px solid #b0b0b0; margin: 5px 0;'>"
                                     " <table width='100%' cellpadding='2' cellspacing='0'>"
                                     " <tr><td><b>Текущая эпоха:</b></td><td align='right' style='color:#27ae60; font-weight:bold;'>%1</td></tr>"
                                     " <tr><td><b>Точность (Accuracy):</b></td><td align='right' style='color:#2980b9; font-weight:bold;'>%2 %</td></tr>"
                                     " <tr><td><b>Видеопамять VRAM:</b></td><td align='right' style='color:#8e44ad; font-weight:bold;'>%3 ГБ</td></tr>"
                                     " <tr><td><b>Скорость вычислений:</b></td><td align='right' style='color:#f39c12; font-weight:bold;'>%4 img/s</td></tr>"
                                     " </table>"
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

void Neuro_programm::initLspServer()
{
    // Если процесс уже запущен, не переинициализируем его
    if (lspProcess && lspProcess->state() == QProcess::Running) return;

    if (!lspProcess) {
        lspProcess = new QProcess(this);
    }

    // =========================================================================
    // ШАГ 1: НАСТРОЙКА ОКРУЖЕНИЯ OS (БОРЬБА С БУФЕРИЗАЦИЕЙ В LINUX)
    // =========================================================================
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Отключаем буферизацию Python внутри Jedi, заставляя его отвечать мгновенно
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("PYTHONIOENCODING", "utf-8");

    lspProcess->setProcessEnvironment(env);

    // =========================================================================
    // ШАГ 2: АСИНХРОННЫЕ СИГНАЛ-СЛОТЫ ДЛЯ МОНИТОРИНГА И СЧИТЫВАНИЯ
    // =========================================================================

    // Перехватчик стандартного вывода (Ответы сервера)
    connect(lspProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        // Подглядываем в буфер для вывода красивого лога в консоль
        QByteArray peekData = lspProcess->peek(lspProcess->bytesAvailable());
        std::cerr << " [LSP СЫРОЙ JSON ВЫВОД (PEEK)]:\n" << QString::fromUtf8(peekData).toStdString() << std::endl;
        std::cerr.flush();

        // Вызываем ваш оригинальный метод парсинга ответов без аргументов
        this->readLspResponse();
    });

    // Перехватчик потока ошибок (Для отлова внутренних сбоев Jedi)
    connect(lspProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray errData = lspProcess->readAllStandardError();
        std::cerr << " [JEDI СИСТЕМНЫЙ ВЫВОД ERR]:\n" << errData.toStdString() << std::endl;
        std::cerr.flush();
    });

    // Мониторинг непредвиденного падения процесса
    connect(lspProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [](int exitCode, QProcess::ExitStatus exitStatus) {
                std::cerr << " [LSP СТАТУС] Сервер Jedi завершил работу. Код выхода: "
                          << exitCode << " Статус: " << exitStatus << std::endl;
                std::flush(std::cerr);
            });

    // =========================================================================
    // ШАГ 3: ЗАПУСК ПРЯМОГО БИНАРНИКА СЕРВЕРА БЕЗ ФЛАГОВ И АРГУМЕНТOВ
    // =========================================================================
    // Жестко задаем абсолютный путь к прямому исполняемому файлу сервера внутри venv
    QString localLspBinary = "/home/elf/projects/z1/venv/bin/jedi-language-server";
    this->venvPythonBinary = localLspBinary;

    // Фикс: Список аргументов СТРОГО пустой, так как бинарник не принимает флаг -m
    QStringList lspArgs;

    std::cerr << " [LSP СИСТЕМНЫЙ СТАРТ] Запускаю прямой бинарник Jedi Language Server..." << std::endl;
    std::cerr.flush();

    // Запускаем процесс асинхронно
    lspProcess->start(localLspBinary, lspArgs);

    if (!lspProcess->waitForStarted(1500)) {
        std::cerr << " [КРИТИЧЕСКАЯ ОШИБКА] Не удалось запустить процесс LSP сервера по пути: "
                  << localLspBinary.toStdString() << std::endl;
        std::cerr.flush();
        return;
    }

    // =========================================================================
    // ШАГ 4: ФОРМИРОВАНИЕ ПАКЕТА ИНИЦИАЛИЗАЦИИ (ОПТИМИЗАЦИЯ ПОД PYTORCH)
    // =========================================================================
    QJsonObject rootObj;
    rootObj["jsonrpc"] = "2.0";
    this->lspRequestId = 1;
    rootObj["id"] = this->lspRequestId;
    rootObj["method"] = "initialize";

    QJsonObject params;
    params["processId"] = QCoreApplication::applicationPid();

    // Задаем корневую директорию проекта для контекста импортов
    if (!currentOpenProjectPath.isEmpty()) {
        params["rootUri"] = QUrl::fromLocalFile(currentOpenProjectPath).toString();
        params["rootPath"] = currentOpenProjectPath;
    } else {
        params["rootUri"] = QJsonValue::Null;
        params["rootPath"] = QJsonValue::Null;
    }

    // Описываем базовые возможности нашего Qt-клиента
    QJsonObject capabilities;
    QJsonObject textDocumentCaps;
    QJsonObject completionCaps;
    completionCaps["contextSupport"] = true;
    textDocumentCaps["completion"] = completionCaps;
    capabilities["textDocument"] = textDocumentCaps;
    params["capabilities"] = capabilities;

    // Сборка оптимизированных настроек Jedi Settings
    QJsonObject initializationOptions;
    QJsonObject jediSettings;

    // Указываем путь к Python интерпретатору venv, чтобы сервер подхватил установленный torch
    jediSettings["pythonExecutablePath"] = "/home/elf/projects/z1/venv/bin/python";

    // Отключаем глубокое статическое сканирование ИИ-библиотеки при автоимпорте
    QJsonArray disableAutoImport;
    disableAutoImport.append("torch");
    disableAutoImport.append("pytorch");
    jediSettings["disable_auto_import_modules"] = disableAutoImport;

    // Добавляем torch и os в preload для динамической подгрузки типов в рантайме
    QJsonArray preloadModules;
    preloadModules.append("torch");
    preloadModules.append("os");
    jediSettings["preload_modules"] = preloadModules;

    // Лимитируем вложенность разбора функций для ускорения отклика IDE
    jediSettings["max_function_parses"] = 150;

    initializationOptions["jediSettings"] = jediSettings;
    params["initializationOptions"] = initializationOptions;

    rootObj["params"] = params;

    // Маркируем пакет по стандарту LSP (Content-Length) и отправляем в пайп
    QByteArray jsonData = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
    QByteArray headerData = QString("Content-Length: %1\r\n\r\n").arg(jsonData.size()).toUtf8();

    lspProcess->write(headerData + jsonData);
    lspProcess->waitForBytesWritten(500);

    std::cerr << " [LSP КЛИЕНТ] Стартовый пакет 'initialize' отправлен на сервер." << std::endl;
    std::cerr.flush();
}

void Neuro_programm::sendLspRequest(const QString &method, QJsonObject params)
{
    if (!lspProcess || lspProcess->state() != QProcess::Running) return;

    QJsonObject requestObj;
    requestObj["jsonrpc"] = "2.0";
    requestObj["method"] = method;

    // Супер-фикс для валидатора pygls (убирает KeyError)
    if (method == "initialized") {
        requestObj["params"] = QJsonValue::Null;
    } else {
        requestObj["params"] = params;
    }

    // Проверяем: если метод НЕ является системным уведомлением (Notification),
    // то генерируем инкрементальный ID, чтобы сервер прислал нам ответ в readLspResponse
    QString lowerMethod = method.toLower();
    if (!lowerMethod.contains("didchange") && !lowerMethod.contains("didopen") &&
        !lowerMethod.contains("didsave") && lowerMethod != "initialized")
    {
        // Это Request (например, textDocument/completion) — ему необходим ID!
        this->lspRequestId++;
        requestObj["id"] = this->lspRequestId;
    }

    // Упаковываем в монолитный JSON-RPC 2.0 пакет по спецификации LSP
    QByteArray jsonData = QJsonDocument(requestObj).toJson(QJsonDocument::Compact);
    QByteArray headerData = QString("Content-Length: %1\r\n\r\n").arg(jsonData.size()).toUtf8();
    QByteArray monolithicPacket = headerData + jsonData;

    // Пишем напрямую в поток запущенного процесса venv
    lspProcess->write(monolithicPacket);
    lspProcess->waitForBytesWritten(50);
}

void Neuro_programm::readLspResponse()
{
    if (!lspProcess) return;

    // 1. Забираем только свежие сырые байты из пайпа
    QByteArray rawData = lspProcess->readAllStandardOutput();
    if (rawData.isEmpty()) return;

    // Накапливаем байты в статическом буфере класса
    static QByteArray lspBuffer;
    lspBuffer.append(rawData);

    // =========================================================================
    // СТАНДАРТНЫЙ ИНДУСТРИАЛЬНЫЙ СТРИМ-ПАРСЕР ДЛЯ LSP ПРОТОКОЛА
    // =========================================================================
    while (!lspBuffer.isEmpty())
    {
        // Находим, где в буфере начинается сам JSON-объект (ищем первую фигурную скобку)
        int jsonStartIndex = lspBuffer.indexOf('{');

        if (jsonStartIndex == -1) {
            // Если фигурной скобки вообще нет, значит в буфере лежит только текстовый заголовок.
            // Мы просто выходим и ждем, когда из пайпа догрузится сам JSON.
            return;
        }

        // Проверяем, если перед фигурной скобкой застрял заголовок Content-Length,
        // мы временно заглядываем в него, чтобы узнать точную длину пакета.
        int headerIndex = lspBuffer.indexOf("Content-Length:");
        int expectedLength = 0;
        if (headerIndex != -1 && headerIndex < jsonStartIndex) {
            int headerEndIndex = lspBuffer.indexOf("\r\n\r\n", headerIndex);
            if (headerEndIndex != -1) {
                int valStart = headerIndex + 15;
                expectedLength = lspBuffer.mid(valStart, headerEndIndex - valStart).trimmed().toInt();
            }
        }

        // Если мы смогли узнать ожидаемую длину, проверяем, накопилось ли столько байт в буфере.
        // Если буфер меньше, значит пакет еще долетает по сети. Выходим и ждем readyRead!
        if (expectedLength > 0 && lspBuffer.size() < (jsonStartIndex + expectedLength)) {
            return;
        }

        // Вырезаем кусок буфера, начиная строго от фигурной скобки '{' и до конца буфера
        QByteArray jsonCandidate = lspBuffer.mid(jsonStartIndex);

        // Позволяем встроенному парсеру Qt САМОМУ распарсить JSON.
        // Qt безупречно определяет реальные границы объекта по балансу фигурных скобок {},
        // полностью игнорируя любые проблемы со смещениями строк в заголовках!
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate, &parseError);

        // СЛУЧАЙ 1: Пакет оборван на полуслове (парсер ругается на неожиданный конец файла)
        if (parseError.error == QJsonParseError::UnterminatedObject ||
            parseError.error == QJsonParseError::UnterminatedArray)
        {
            return; // Спокойно выходим и ждем, когда QProcess догрузит оставшиеся байты
        }

        // СЛУЧАЙ 2: Пакет успешно распарсился!
        if (parseError.error == QJsonParseError::NoError)
        {
            // Вычисляем, сколько байт реально занял этот JSON-объект на диске
            int actualJsonSize = doc.toJson(QJsonDocument::Compact).size();

            // Намертво стираем из буфера обработанный заголовок и сам JSON,
            // продвигая очередь строго к следующему LSP-пакету
            lspBuffer.remove(0, jsonStartIndex + actualJsonSize);

            // Обрабатываем валидный JSON-объект
            QJsonObject rootObj = doc.object();

            // Пропускаем пакеты асинхронной диагностики синтаксиса
            if (!rootObj.contains("id")) {
                continue; // Переходим к следующему пакету в цикле while
            }

            int responseId = rootObj["id"].toInt();

            // Ответ на initialize (id: 1) — завершаем рукопожатие
            if (responseId == 1) {
                std::clog << " [LSP УСПЕХ] Рукопожатие с Jedi пройдено!" << std::endl;
                std::clog.flush();
                QJsonObject initializedParams;
                this->sendLspRequest("initialized", initializedParams);
                continue;
            }

            // Обрабатываем долгожданный пакет автодополнения (textDocument/completion)
            if (rootObj.contains("result"))
            {
                QJsonValue resultVal = rootObj["result"];
                QJsonArray itemsArray;

                if (resultVal.isArray()) {
                    itemsArray = resultVal.toArray();
                } else if (resultVal.isObject() && resultVal.toObject().contains("items")) {
                    itemsArray = resultVal.toObject()["items"].toArray();
                }

                QStringList completionList;
                for (int i = 0; i < itemsArray.size(); ++i) {
                    QJsonObject item = itemsArray[i].toObject();
                    QString label = item["label"].toString();
                    if (!label.isEmpty()) completionList.append(label);
                }

                std::clog << " [ПОТОК I/O] ПОДСКАЗКИ УСПЕШНО РАЗОБРАНЫ! Найдено элементов: "
                          << completionList.size() << ". Отправляю в GUI..." << std::endl;
                std::clog.flush();

                if (!completionList.isEmpty()) {
                    // Испускаем сигнал межпотокового взаимодействия для вывода QMenu
                    emit this->completionDataReceived(completionList);
                }
            }
            continue;
        }

        // СЛУЧАЙ 3: Если в буфере застрял совсем непонятный мусор, из-за которого Qt выдает ошибку,
        // мы просто сдвигаем буфер на 1 байт вперед, чтобы не зайти в бесконечный цикл зависания.
        lspBuffer.remove(0, jsonStartIndex + 1);
    }
}


QString Neuro_programm::getCurrentOpenFilePath() const
{
    if (!ui->fileComboBox) return "";
    int idx = ui->fileComboBox->currentIndex();
    if (idx < 2) return "";
    return ui->fileComboBox->itemData(idx).toString();
}

void Neuro_programm::openNewFileInEditor(const QString &absoluteFilePath)
{
    if (absoluteFilePath.isEmpty()) return;

    // =========================================================================
    // ШАГ 1: ФИЗИЧЕСКОЕ СОЗДАНИЕ ФАЙЛА НА ДИСКЕ (ЕСЛИ ОН ЕЩЕ НЕ СУЩЕСТВУЕТ)
    // =========================================================================
    QFile file(absoluteFilePath);
    if (!file.exists()) {
        // Автоматически проверяем и создаем подпапки (например, /models/), если их нет
        QFileInfo fileInfo(absoluteFilePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            dir.mkpath(dir.absolutePath());
        }

        // Открываем файл для записи в кодировке UTF-8 с базовым шаблоном
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            out.setEncoding(QStringConverter::Utf8);
#else
            out.setCodec("UTF-8");
#endif
            out << "# -*- coding: utf-8 -*-\n";
            out << "import torch\n";
            out << "import torch.nn as nn\n\n";
            file.close();
        } else {
            qCritical() << " [ОШИБКА OS] Не удалось создать файл на диске:" << absoluteFilePath;
            return;
        }
    }

    // =========================================================================
    // ШАГ 2: ПРОВЕРКА — НЕ ОТКРЫТ ЛИ ЭТОТ ФАЙЛ УЖЕ В ДРУГОЙ ВКЛАДКЕ
    // =========================================================================
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && page->objectName() == absoluteFilePath) {
            // Файл уже открыт, просто переключаем фокус на него
            ui->centralStackedWidget->setCurrentWidget(page);
            if (ui->fileComboBox) {
                int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
                if (comboIdx != -1) ui->fileComboBox->setCurrentIndex(comboIdx);
            }
            return;
        }
    }

    // =========================================================================
    // ШАГ 3: ПРОГРАММНОЕ СОЗДАНИЕ И НАСТРОЙКА ВИДЖЕТА РЕДАКТОРА КОДА
    // =========================================================================
    // Читаем содержимое созданного файла
    QString fileContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fileContent = QString::fromUtf8(file.readAll());
        file.close();
    }

    // Создаем новую страницу-контейнер для стэка виджетов
    QWidget *newPage = new QWidget(ui->centralStackedWidget);
    QVBoxLayout *layout = new QVBoxLayout(newPage);
    layout->setContentsMargins(0, 0, 0, 0);

    // Инициализируем ваш кастомный класс редактора из PDF
    CodeEditor *editor = new CodeEditor(newPage);

    // ЖЕСТКАЯ АРХИТЕКТУРНАЯ СВЯЗКА: Присваиваем абсолютный путь в имя объекта,
    // чтобы keyPressEvent внутри CodeEditor знал свой URI.
    editor->setObjectName(absoluteFilePath);
    newPage->setObjectName(absoluteFilePath);

    // Настраиваем базовые параметры отображения Breeze
    editor->setLineWrapMode(QPlainTextEdit::NoWrap); // Отключаем ломающий синтаксис Python перенос строк
    editor->setPlainText(fileContent);

    // Добавляем созданный редактор в менеджер разметки страницы
    layout->addWidget(editor);

    // Встраиваем страницу в главный StackedWidget и выводим на экран
    ui->centralStackedWidget->addWidget(newPage);
    ui->centralStackedWidget->setCurrentWidget(newPage);

    // Снабжаем редактор встроенным в систему QCompleter-ом подсказок, если он активен
    if (this->codeCompleter) {
        editor->setCompleter(this->codeCompleter);
    }

    // =========================================================================
    // ШАГ 4: СИНХРОНИЗАЦИЯ С ВЕРХНИМ И БOКОВЫМ ИНТЕРФЕЙСОМ НАВИГАЦИИ
    // =========================================================================
    if (ui->fileComboBox) {
        QFileInfo info(absoluteFilePath);
        // Добавляем чистое имя в выпадающий список, а скрытый путь — в userData
        ui->fileComboBox->addItem(info.fileName(), absoluteFilePath);
        ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count() - 1);
    }

    // Подключаем слот отслеживания изменений, чтобы рисовать звёздочку изменения "*"
    connect(editor, &CodeEditor::textChanged, this, &Neuro_programm::onCurrentFileTextChanged);

    // =========================================================================
    // ШАГ 5: ОФИЦИАЛЬНАЯ ИНДЕКСИРOВАНИЯ ФАЙЛА В ПАМЯТИ СЕРВЕРА JEDI (didOpen)
    // =========================================================================
    if (this->lspProcess && this->lspProcess->state() == QProcess::Running)
    {
        QJsonObject openParams;
        QJsonObject textDocument;

        textDocument["uri"] = QUrl::fromLocalFile(absoluteFilePath).toString();
        textDocument["languageId"] = "python";

        this->globalLspDocVersion = 1; // Устанавливаем стартовую версию по спецификации LSP
        textDocument["version"] = this->globalLspDocVersion;

        // Очищаем буфер от скрытых DOS-символов \r, ломающих сетку координат LSP
        QString cleanStartText = fileContent;
        cleanStartText.remove('\r');
        textDocument["text"] = cleanStartText;

        openParams["textDocument"] = textDocument;

        // Отправляем уведомлениеdidOpen в пайп сервера
        this->sendLspRequest("textDocument/didOpen", openParams);

        std::cerr << " [LSP] Новый файл зарегистрирован в AST-дереве Jedi: "
                  << absoluteFilePath.toStdString() << std::endl;
        std::cerr.flush();
    }
}

void Neuro_programm::showCompletionMenuInGuiThread(const QStringList &completions)
{
    if (completions.isEmpty()) return;

    QWidget *currentWidget = ui->centralStackedWidget->currentWidget();
    if (!currentWidget) return;

    CodeEditor *activeEditor = currentWidget->findChild<CodeEditor*>();
    if (!activeEditor) return;

    // 1. Создаем всплывающий контейнер (Breeze Light)
    QWidget *popupWindow = new QWidget(activeEditor, Qt::Popup | Qt::FramelessWindowHint);
    popupWindow->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(popupWindow);
    layout->setContentsMargins(0, 0, 0, 0);

    QListWidget *listWidget = new QListWidget(popupWindow);
    listWidget->addItems(completions);
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Применяем стили KDE Breeze Light
    listWidget->setStyleSheet(
        "QListWidget { background-color: #fcfcfc; color: #232629; border: 1px solid #c7c7c7; font-family: monospace; font-size: 11pt; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:hover { background-color: #eff0f1; color: #232629; }"
        "QListWidget::item:selected { background-color: #3daee9; color: #ffffff; }"
        "QScrollBar:vertical { background-color: #eff0f1; width: 10px; margin: 0px; }"
        "QScrollBar::handle:vertical { background-color: #b0b3b6; min-height: 20px; border-radius: 2px; margin: 1px; }"
        "QScrollBar::handle:vertical:hover { background-color: #3daee9; }"
        "QScrollBar::handle:vertical:pressed { background-color: #2a7da6; }"
        "QScrollBar::sub-line:vertical, QScrollBar::add-line:vertical { background: none; height: 0px; }"
        "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical, QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        );

    layout->addWidget(listWidget);

    // Связываем клик мыши (если пользователь решит выбрать элемент не с клавиатуры)
    connect(listWidget, &QListWidget::itemClicked, this, [activeEditor, listWidget, popupWindow]() {
        QListWidgetItem *currentItem = listWidget->currentItem();
        if (currentItem) {
            QString itemText = currentItem->text();
            QTextCursor tc = activeEditor->textCursor();
            int prefixLength = activeEditor->textCursor().position() - activeEditor->property("startPosition").toInt();
            tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefixLength);
            tc.beginEditBlock();
            tc.insertText(itemText);
            tc.endEditBlock();
            activeEditor->setTextCursor(tc);
        }
        popupWindow->close();
        activeEditor->setFocus();
    });

    // === ЖЕСТКАЯ СВЯЗКА С КЛАССOМ РЕДАКТОРА ===
    // Передаем созданные указатели напрямую в свойства активного CodeEditor,
    // чтобы его встроенный keyPressEvent мог ими управлять!
    activeEditor->setProperty("startPosition", activeEditor->textCursor().position());

    // Используем динамическое приведение типов, чтобы записать указатели в переменные класса
    // (Поскольку свойства Qt не любят голые указатели C++, запишем их через кастомный метод или напрямую)
    // Но проще использовать встроенный механизм метаобъектов Qt:
    activeEditor->m_popupWindow = popupWindow;
    activeEditor->m_listWidget = listWidget;

    // Геометрия (Wayland-совместимая)
    QTextCursor cursor = activeEditor->textCursor();
    QRect cursorRect = activeEditor->cursorRect(cursor);
    QPoint globalPos = activeEditor->mapToGlobal(cursorRect.bottomLeft());
    globalPos.setY(globalPos.y() + 5);

    int width = listWidget->sizeHintForColumn(0) + 40;
    if (width < 250) width = 250;
    if (width > 450) width = 450;

    popupWindow->setGeometry(globalPos.x(), globalPos.y(), width, 200);
    listWidget->setCurrentRow(0);

    // Показываем окно. Фокус ОСТАЕТСЯ в текстовом поле, чтобы работал ввод букв!
    popupWindow->show();
}

bool Neuro_programm::eventFilter(QObject *obj, QEvent *event)
{
    // =================================================================
    // БЛОК 1: СИСТЕМНОЕ ПЕРЕТАСКИВАНИЕ + ДВОЙНОЙ ЩЕЛЧОК ДЛЯ РАЗВОРAЧИВАНИЯ
    // =================================================================
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        if (mouseEvent->button() == Qt::LeftButton)
        {
            // Защита: если кликнули по кнопкам управления или меню, отдаем управление им
            QString className = obj->metaObject()->className();
            if (className == "QPushButton" || className == "QMenuBar" ||
                className == "QComboBox" || (obj->parent() && obj->parent()->metaObject()->className() == "QComboBox"))
            {
                return false;
            }

            // Переводим позицию курсора на экране в координаты главного окна
            QPoint windowLocalPos = this->mapFromGlobal(QCursor::pos());

            // Если клик пришелся на верхнюю область шапки (первые 75 пикселей по высоте)
            if (windowLocalPos.y() >= 0 && windowLocalPos.y() <= 75)
            {
                // 1. ОБРАБОТКА ДВОЙНОГО ЩЕЛЧКА (Разворачивание / Сворачивание в нормальный размер)
                if (event->type() == QEvent::MouseButtonDblClick)
                {
                    if (this->isMaximized()) {
                        this->showNormal();
                        // Если у вас на кнопке btnMaximize меняется текст, обновите его:
                        // ui->btnMaximize->setText("🗖");
                    } else {
                        this->showMaximized();
                        // ui->btnMaximize->setText("🗗");
                    }
                    return true; // Перехватываем событие, дальше код не идет
                }

                // 2. ОБРАБОТКА ОДИНОЧНОГО КЛИКА (Перетаскивание силами ОС)
                if (event->type() == QEvent::MouseButtonPress && this->windowHandle())
                {
                    this->windowHandle()->startSystemMove();
                    return true;
                }
            }
        }
    }

    // =================================================================
    // БЛОК 2: КОД ДЛЯ РАБОТЫ С JEDI (ПОЛНОСТЬЮ ИЗОЛИРОВАН ОТ МЫШИ)
    // =================================================================
    if (this->activeCompletionPopup && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QListWidget *listWidget = this->activeCompletionPopup->findChild<QListWidget*>("completionListWidget");
        CodeEditor *editor = qobject_cast<CodeEditor*>(obj->parent());

        if (listWidget && editor)
        {
            // --- 1. УПРАВЛЕНИЕ СТРЕЛКАМИ ВВЕРХ / ВНИЗ ---
            if (keyEvent->key() == Qt::Key_Up) {
                int currentRow = listWidget->currentRow();
                for (int i = currentRow - 1; i >= 0; --i) {
                    if (!listWidget->item(i)->isHidden()) {
                        listWidget->setCurrentRow(i);
                        break;
                    }
                }
                return true;
            }

            if (keyEvent->key() == Qt::Key_Down) {
                int currentRow = listWidget->currentRow();
                for (int i = currentRow + 1; i < listWidget->count(); ++i) {
                    if (!listWidget->item(i)->isHidden()) {
                        listWidget->setCurrentRow(i);
                        break;
                    }
                }
                return true;
            }

            // --- 2. ПОДТВЕРЖДЕНИЕ ВЫБОРА (ENTER / RETURN / TAB) ---
            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Tab) {
                QListWidgetItem *currentItem = listWidget->currentItem();
                if (currentItem && !currentItem->isHidden()) {
                    QString itemText = currentItem->text();
                    QTextCursor tc = editor->textCursor();
                    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, editor->property("prefixLength").toInt());
                    tc.beginEditBlock();
                    tc.insertText(itemText);
                    tc.endEditBlock();
                    editor->setTextCursor(tc);
                }
                this->activeCompletionPopup->close();
                editor->setFocus();
                return true;
            }

            // --- 3. ЗАКРЫТИЕ ПО ESC ---
            if (keyEvent->key() == Qt::Key_Escape) {
                this->activeCompletionPopup->close();
                editor->setFocus();
                return true;
            }

            // --- 4. ДИНАМИЧЕСКАЯ ФИЛЬТРАЦИЯ НА ЛЕТУ ---
            if (!keyEvent->text().isEmpty() && keyEvent->key() != Qt::Key_Backspace &&
                keyEvent->key() != Qt::Key_Left && keyEvent->key() != Qt::Key_Right)
            {
                QCoreApplication::sendEvent(obj, event);

                int startPos = editor->property("startPosition").toInt();
                int currentPos = editor->textCursor().position();
                int prefixLength = currentPos - startPos;
                editor->setProperty("prefixLength", prefixLength);

                QTextCursor tc = editor->textCursor();
                tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefixLength);
                QString currentPrefix = tc.selectedText().toLower();

                int firstVisibleRow = -1;
                for (int i = 0; i < listWidget->count(); ++i) {
                    QListWidgetItem *item = listWidget->item(i);
                    bool matches = item->text().toLower().contains(currentPrefix);
                    item->setHidden(!matches);

                    if (matches && firstVisibleRow == -1) {
                        firstVisibleRow = i;
                    }
                }

                if (firstVisibleRow != -1) {
                    listWidget->setCurrentRow(firstVisibleRow);
                } else {
                    this->activeCompletionPopup->close();
                }

                return true;
            }
        }
    }

    // Во всех остальных случаях отдаем события базовому классу QMainWindow
    return QMainWindow::eventFilter(obj, event);
}


void Neuro_programm::sendInitialWelcomeRequest()
{
    // Настраиваем QTextEdit вывода чата на случай, если это еще не сделано
    ui->chatLogWidget->setReadOnly(true);
    ui->chatLogWidget->setLineWrapMode(QTextEdit::WidgetWidth);

    // Добавляем временный маркер ожидания
    ui->chatLogWidget->append("<font color='#555555'><i>ИИ-Ассистент подключается...</i></font>");

    // Блокируем интерфейс, пока ИИ не поприветствует пользователя
    ui->inputChatText->setEnabled(false);
    ui->btnSendChat->setEnabled(false);

    // 1. Формируем жесткую системную инструкцию
    QString systemInstruction =
        "Ты — встроенный ИИ-помощник в среде 'PyTorch Studio'. Твоя цель — помогать пользователю "
        "проектировать, исправлять ошибки и настраивать обучение нейросетей PyTorch. "
        "Сейчас чат только что открылся. Напиши короткое, дружелюбное стартовое приветствие для пользователя "
        "длиной не более 2-3 предложений. Предложи свою помощь по коду PyTorch или настройкам сети.";

    // 2. Сборка JSON пакета
    QJsonObject requestBody;
    requestBody["model"] = "qwen2.5-coder:1.5b";
    requestBody["stream"] = false;

    QJsonArray messagesArray;
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = systemInstruction;
    messagesArray.append(systemMessage);

    requestBody["messages"] = messagesArray;

    QJsonDocument jsonDoc(requestBody);
    QByteArray jsonData = jsonDoc.toJson();

    // 3. Отправка скрытого запроса
    QNetworkAccessManager *networkManager = new QNetworkAccessManager(this);
    QUrl apiUrl("http://localhost:11434/api/chat");
    QNetworkRequest request(apiUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, jsonData);

    // 4. Обработка ответа
    connect(reply, &QNetworkReply::finished, this, [this, reply, networkManager]() {
        // Разблокируем интерфейс для работы
        ui->inputChatText->setEnabled(true);
        ui->btnSendChat->setEnabled(true);
        ui->inputChatText->setFocus();

        // Удаляем временный маркер ("ИИ-Ассистент подключается...")
        QTextCursor cursor = ui->chatLogWidget->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.removeSelectedText();
        cursor.deletePreviousChar();

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray rawResponse = reply->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(rawResponse);
            QJsonObject responseObj = responseDoc.object();
            QJsonObject messageObj = responseObj["message"].toObject();
            QString aiWelcome = messageObj["content"].toString().trimmed();

            if (!aiWelcome.isEmpty())
            {
                // Выводим красивое приветствие от ИИ
                ui->chatLogWidget->append("<font color='#232629'><b>ИИ-Ассистент:</b><br>" + aiWelcome.toHtmlEscaped().replace("\n", "<br>") + "</font><br>");
            }
        }
        else
        {
            // Если Ollama не запущена, сразу сообщаем пользователю на старте
            ui->chatLogWidget->append("<font color='#cc0000'><b>ИИ: Ошибка инициализации.</b> Локальная служба Ollama не отвечает. "
                                      "Запустите сервер через терминал: <i>'sudo systemctl start ollama'</i></font><br>");
        }

        ui->chatLogWidget->moveCursor(QTextCursor::End);
        reply->deleteLater();
        networkManager->deleteLater();
    });
}

QString Neuro_programm::parseMarkdownCodeBlocks(const QString &rawText) {
    QString htmlResult = rawText.toHtmlEscaped();
    htmlResult.replace("&lt;br&gt;", "\n");

    QRegularExpression rx("```(?:python|py)?\\n([\\s\\S]*?)\\n```");
    QRegularExpressionMatchIterator it = rx.globalMatch(htmlResult);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString rawCode = match.captured(1);

        QString blockId = QString("code_block_%1").arg(++codeBlockCounter);

        QString cleanCodeForClipboard = rawCode;
        cleanCodeForClipboard.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&");
        codeBlocksMap.insert(blockId, cleanCodeForClipboard);

        QString codeContainerHtml = QString(
                                        "<div style='background-color: #eaedf1; border: 2px solid #cbd5e1; "
                                        "border-left: 5px solid #0056b3; border-radius: 6px; margin: 12px 0; overflow: hidden;'>"
                                        "  <pre style='padding: 12px; margin: 0; font-family: Consolas, Monaco, monospace; "
                                        "  font-size: 13px; color: #1a1a1a; white-space: pre-wrap; overflow-x: auto;'>%2</pre>"
                                        "  <div style='background-color: #cbd5e1; padding: 6px 12px; font-size: 12px; "
                                        "  text-align: right; font-family: sans-serif; border-top: 1px solid #b8c9de;'>"
                                        "    <a href='copy:%1' style='color: #004494; text-decoration: none; font-weight: bold;'>[📋 Копировать код]</a>"
                                        "  </div>"
                                        "</div>"
                                        ).arg(blockId, rawCode.replace("\n", "<br>"));

        htmlResult.replace(match.capturedStart(), match.capturedLength(), codeContainerHtml);
    }

    htmlResult.replace("\n", "<br>");
    return htmlResult;
}


void Neuro_programm::onChatAnchorClicked(const QUrl &link) {
    QString urlStr = link.toString();

    // --- ПЕРЕХВАТ ИСПРАВЛЕНИЯ ОШИБОК КОМПИЛЯЦИИ ---
    if (urlStr == "action:fix_error") {
        // Извлекаем сохраненный Traceback падения скрипта
        QString traceback = this->property("lastPythonErrorTraceback").toString();
        if (traceback.isEmpty()) return;

        // Показываем пользователю, что запрос пошел
        ui->chatLogWidget->append("<font color='#0056b3'><b>Вы:</b><br><i>[Автоматический запрос] Исправь ошибку обучения сети.</i></font><br>");

        // Формируем скрытый промпт для Олламы
        QString errorPrompt = QString(
                                  "Мой скрипт PyTorch упал во время обучения со следующей ошибкой:\n"
                                  "```\n%1\n```\n"
                                  "Пожалуйста, детально разбери этот лог Traceback, объясни причину падения (например, "
                                  "несовпадение размерностей слоев, неверный индекс или тип данных) и напиши исправленный вариант кода."
                                  ).arg(traceback);

        // Помещаем промпт в скрытый буфер ввода и триггерим вашу отправку
        ui->inputChatText->setPlainText(errorPrompt);

        // Запускаем ваш основной метод общения с Ollama
        this->sendChatMessageToAI();
        return;
    }

    // --- ПОДДЕРЖКА СТАРОЙ КНОПКИ (Копирование отдельного блока кода) ---
    if (urlStr.startsWith("copy:")) {
        QString blockId = urlStr.mid(5);
        if (codeBlocksMap.contains(blockId)) {
            QApplication::clipboard()->setText(codeBlocksMap.value(blockId));
            sendSystemNotification("PyTorch Studio", "Код скопирован!");
        }
        return;
    }

    // --- ОБРАБОТКА СТАНДАРТНОЙ ПАНЕЛИ ДЕЙСТВИЙ ОТВЕТА ---
    if (urlStr.startsWith("action:")) {
        // Парсим строку вида "action:команда:resp_ID"
        QStringList parts = urlStr.split(':');
        if (parts.size() < 3) return;

        QString command = parts[1]; // copy, export или share
        QString responseId = parts[2]; // resp_X

        // Достаем из памяти именно тот чистый текст ответа, под которым была нажата кнопка
        if (!aiResponsesMap.contains(responseId)) return;
        QString textToProcess = aiResponsesMap.value(responseId);

        // 1. Кнопка "Копировать весь ответ"
        if (command == "copy") {
            QApplication::clipboard()->setText(textToProcess);
            sendSystemNotification("PyTorch Studio", "Ответ скопирован в буфер обмена.");
        }

        // 2. Кнопка "Экспорт в документы"
        else if (command == "export") {
            QString savePath = QFileDialog::getSaveFileName(this, "Экспорт ответа ИИ", "", "Текстовые файлы (*.txt);;Документы Markdown (*.md)");
            if (!savePath.isEmpty()) {
                QFile file(savePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << textToProcess;
                    file.close();
                    sendSystemNotification("PyTorch Studio", "Файл успешно сохранен.");
                }
            }
        }

        // 3. Кнопка "Передача в соцсеть / Мессенджер"
        else if (command == "share") {
            // Самый безопасный способ передать длинный технический текст в соцсети (например, в Telegram) —
            // открыть веб-ссылку шеринга. Из-за ограничений длины URL, текст обрезается до 400 символов.
            QString shortText = textToProcess.left(400) + "...";

            // Формируем ссылку для отправки в Telegram (можно заменить на VK, WhatsApp и т.д.)
            QString encodedText = QUrl::toPercentEncoding(shortText);
            QUrl shareUrl("https://t.me" + encodedText);

            // Qt аппаратно открывает браузер по умолчанию на компьютере пользователя
            QDesktopServices::openUrl(shareUrl);
        }
    }
}

void Neuro_programm::onQuickActionTriggered(QListWidgetItem *item)
{
    if (!item) return;

    // =========================================================================
    // 0. АППАРАТНАЯ ЗАЩИТА ОТ ДУБЛИРОВАНИЯ И СЕРВИСНЫХ СТРАНИЦ
    // =========================================================================
    static bool isProcessing = false;
    if (isProcessing) {
        qDebug() << "[ИИ ЗАЩИТА] Заблокирован каскадный вызов функции!";
        return;
    }

    int currentFileIdx = ui->fileComboBox->currentIndex();

    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Если индекс < 2 (чат или панель), это 100% повторный ложный вызов.
    // Мы просто молча выходим из функции, не ломая интерфейс и не выводя ошибок.
    if (currentFileIdx < 2) {
        qDebug() << "[ИИ ЗАЩИТА] Игнорируем вызов, так как интерфейс уже переключен на чат.";
        return;
    }

    isProcessing = true; // Включаем блокировку

    QString actionText = item->text();

    // =========================================================================
    // 1. СБОР КОДА ИЗ ВАШЕГО КЛАССА CodeEditor
    // =========================================================================
    QString targetCode = "";
    QString debugReport = "<b>🔍 Отчет системы сбора кода:</b><br>";
    debugReport += QString("• Индекс активного файла в комбобоксе: %1<br>").arg(currentFileIdx);

    QWidget *filePageWidget = ui->centralStackedWidget->widget(currentFileIdx);
    if (filePageWidget)
    {
        debugReport += QString("• Найдена страница в StackedWidget с именем: '%1'<br>").arg(filePageWidget->objectName());

        QList<QWidget*> allChildren = filePageWidget->findChildren<QWidget*>();
        debugReport += QString("• Всего дочерних виджетов на странице: %1<br>").arg(allChildren.size());

        CodeEditor *editor = filePageWidget->findChild<CodeEditor*>();
        if (editor)
        {
            debugReport += QString("<font color='green'>• УСПЕХ: Объект класса CodeEditor обнаружен!</font><br>");
            targetCode = editor->textCursor().selectedText().trimmed();
            if (targetCode.isEmpty()) {
                targetCode = editor->toPlainText().trimmed();
                debugReport += QString("• Выделения нет. Считан весь файл. Длина текста: %1 симв.<br>").arg(targetCode.length());
            } else {
                debugReport += QString("• Считан ВЫДЕЛЕННЫЙ фрагмент кода. Длина текста: %1 симв.<br>").arg(targetCode.length());
            }
        } else {
            debugReport += "<font color='red'>• ОШИБКА: Виджет класса CodeEditor НЕ НАЙДЕН!</font><br>";
        }
    }

    // Опционально: можно закомментировать строку ниже, чтобы серый блок не мозолил глаза в чате
    ui->chatLogWidget->append("<div style='background-color: #edf2f7; padding: 10px; border-radius: 4px; margin-bottom: 10px;'>" + debugReport + "</div>");
    ui->chatLogWidget->moveCursor(QTextCursor::End);

    if (targetCode.isEmpty()) {
        ui->chatLogWidget->append("<font color='#cc0000'><b>Система:</b> Сбор кода прерван из-за ошибок выше.</font><br>");
        if (ui->quickActionsList) ui->quickActionsList->hide();
        if (this->btnLogs) this->btnLogs->setChecked(false);

        isProcessing = false;
        return;
    }

    // =========================================================================
    // 2. СИНХРОНИЗАЦИЯ ИНТЕРФЕЙСА
    // =========================================================================
    if (ui->quickActionsList) ui->quickActionsList->hide();
    if (this->btnLogs) this->btnLogs->setChecked(false);

    // =========================================================================
    // 3. ПЕРЕКЛЮЧЕНИЕ НА СТРАНИЦУ ЧАТА ИИ
    // =========================================================================
    int realChatStackIndex = ui->centralStackedWidget->indexOf(ui->page_chat);
    if (realChatStackIndex != -1) {
        ui->fileComboBox->setCurrentIndex(realChatStackIndex);
        ui->inputChatText->setFocus();
    }

    // =========================================================================
    // 4. ДИСПЕТЧЕР ИИ-КОМАНД И ФОРМИРОВАНИЕ ПРОМПТА ПОД OLLAMA
    // =========================================================================
    QString systemInstruction = "";
    QString userHeading = "";

    if (actionText.contains("Документировать")) {
        userHeading = "📝 Документировать код";
        systemInstruction = "Напиши профессиональные комментарии docstring для этого кода PyTorch. "
                            "Опиши структуру входных/выходных тензоров, назначение слоев и аргументов:\n";
    }
    else if (actionText.contains("Найти баги")) {
        userHeading = "🔍 Проверить на баги";
        systemInstruction = "Выступи в роли эксперта по глубокому обучению. Проверь этот код PyTorch на логические баги, "
                            "несовпадения размерностей слоев, ошибки инициализации или утечки памяти CUDA. Выдай исправленный вариант:\n";
    }
    else if (actionText.contains("Оптимизировать")) {
        userHeading = "🚀 Оптимизировать код";
        systemInstruction = "Оптимизируй этот код PyTorch для ускорения обучения. Сделай упор на векторизацию, "
                            "эффективное использование VRAM/CUDA, ускорение DataLoader или замену медленных циклов. Напиши оптимизированный вариант:\n";
    }

    if (systemInstruction.isEmpty()) {
        isProcessing = false;
        return;
    }

    QString finalPrompt = systemInstruction + "```python\n" + targetCode + "\n```";

    // Блокируем сигналы поля ввода
    ui->inputChatText->blockSignals(true);
    ui->inputChatText->setPlainText(finalPrompt);
    ui->inputChatText->blockSignals(false);

    // Запускаем отправку к Ollama
    this->sendChatMessageToAI();

    // МАКРОС АСИНХРОННОСТИ: Снимаем блокировку не сразу, а через 100 миллисекунд.
    // Это гарантирует, что все "эхо-сигналы" от переключения комбобокса успеют разбиться о наш предохранитель!
    QTimer::singleShot(100, [=]() {
        isProcessing = false;
    });
}

void Neuro_programm::open_settings()
{
    rsc2 = new Settings(this);
    //rsc2->wf = this;
    rsc2->setWindowTitle("Настройки программы");
    rsc2->exec();
}

void Neuro_programm::applyGlobalFonts()
{
    QSettings settings("PyTorchStudio", "EditorSettings");

    QString editorFamily = settings.value("Editor/FontFamily", "Monospace").toString();
    int editorSize = settings.value("Editor/FontSize", 12).toInt();
    QString currentTheme = settings.value("Theme/Name", "Светлая тема").toString();

    QFont editorFont(editorFamily, editorSize);
    QPalette palette;

    qApp->setStyle(QStyleFactory::create("Fusion"));

    QString chatBgColor, chatTextColor, chatBorderColor;
    QString windowBgHex, containerBgHex, borderAccentHex, textAccentHex;

    bool isDark = currentTheme.contains("Тёмная")  ||
                  currentTheme.contains("Темная")  ||
                  currentTheme.contains("Dark")    ||
                  currentTheme.contains("Darcula") ||
                  currentTheme.contains("Monokai");

    if (isDark)
    {
        windowBgHex     = "#232629"; // Родной тёмный фон окон Breeze Dark
        containerBgHex  = "#2a2e32";
        borderAccentHex = "#4d5053";
        textAccentHex   = "#3daee9"; // Синий акцент Breeze

        QColor darkBg(windowBgHex);
        QColor darkWidget(containerBgHex);
        QColor darkText("#eff0f1");

        palette.setColor(QPalette::Window, darkBg);
        palette.setColor(QPalette::WindowText, darkText);
        palette.setColor(QPalette::Base, QColor("#1b1e20"));
        palette.setColor(QPalette::AlternateBase, darkBg);
        palette.setColor(QPalette::ToolTipBase, darkWidget);
        palette.setColor(QPalette::ToolTipText, darkText);
        palette.setColor(QPalette::Text, darkText);
        palette.setColor(QPalette::Button, darkWidget);
        palette.setColor(QPalette::ButtonText, darkText);
        palette.setColor(QPalette::Highlight, QColor(textAccentHex));
        palette.setColor(QPalette::HighlightedText, Qt::white);

        chatBgColor = "#1b1e20";
        chatTextColor = "#eff0f1";
        chatBorderColor = "#31363b";

        // Сигнал оконному менеджеру Linux KWin перекрасить Title Bar ОС в тёмный цвет!
        qApp->setProperty("activeColorScheme", "BreezeDark");
    }
    else
    {
        windowBgHex     = "#eff0f1"; // Серый фон окон Breeze Light
        containerBgHex  = "#ffffff";
        borderAccentHex = "#bcbebf";
        textAccentHex   = "#3daee9";

        QColor lightBg(windowBgHex);
        QColor lightWidget(containerBgHex);
        QColor lightText("#232629");

        palette.setColor(QPalette::Window, lightBg);
        palette.setColor(QPalette::WindowText, lightText);
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, lightBg);
        palette.setColor(QPalette::ToolTipBase, lightWidget);
        palette.setColor(QPalette::ToolTipText, lightText);
        palette.setColor(QPalette::Text, lightText);
        palette.setColor(QPalette::Button, lightBg);
        palette.setColor(QPalette::ButtonText, lightText);
        palette.setColor(QPalette::Highlight, QColor(textAccentHex));
        palette.setColor(QPalette::HighlightedText, Qt::white);

        chatBgColor = "#ffffff";
        chatTextColor = "#232629";
        chatBorderColor = "#cfc9c2";

        qApp->setProperty("activeColorScheme", "BreezeLight");
    }

    qApp->setPalette(palette);

    // =========================================================================
    // 2. CSS-МАНИФЕСТ С УСТРАНЕНИЕМ НАПЛЫВА БУКВ И ЧЁТКИМИ СЛОЯМИ
    // =========================================================================
    QString globalStyle =
        "/* Базовый шрифт */"
        "QWidget { font-family: 'Segoe UI', Arial, sans-serif; }"

        "/* Монолитное оформление вашей желтой панели ui->widget_3 */"
        "QWidget#widget_3 { "
        "   background-color: " + windowBgHex + " !important; "
                        "   border-bottom: 1px solid " + borderAccentHex + " !important; "
                            "   min-height: 34px; "
                            "   max-height: 34px; "
                            "}"

                            "/* Карточки параметров обучения */"
                            "QGroupBox, QTabWidget::panel { "
                            "   background-color: " + containerBgHex + " !important; "
                           "   border: 1px solid " + borderAccentHex + "; "
                            "   border-radius: 6px; "
                            "   margin-top: 10px; "
                            "   padding: 15px; "
                            "}"

                            "/* КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Находим шапку левого дока и жестко отодвигаем её под кнопки! */"
                            "QDockWidget, QTreeView, QTreeWidget, QListWidget, #leftDockWidget { "
                            "   background-color: " + containerBgHex + " !important; "
                           //"   margin-top: 36px !important; " // Отодвигает надпись "Проект" ровно под линию кнопок
                           "}"
                           "QGroupBox QTreeView, QTabWidget QTreeView { margin-top: 0px !important; }" // Сброс для внутренних окон

                           "QGroupBox::title { "
                           "   subcontrol-origin: margin; "
                           "   subcontrol-position: top left; "
                           "   padding: 0 5px; "
                           "   color: " + textAccentHex + "; "
                          "   font-weight: bold; "
                          "}"

                          "/* Поля ввода и выпадающие списки */"
                          "QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit, QHeaderView::section { "
                          "   background-color: " + containerBgHex + " !important; "
                           "   color: " + palette.color(QPalette::Text).name() + " !important; "
                                                 "   border: 1px solid " + borderAccentHex + " !important; "
                            "   border-radius: 4px; "
                            "   padding: 4px 8px; "
                            "   min-height: 22px; "
                            "}"

                            "/* Текстовые редакторы и ваш CodeEditor */"
                            "QPlainTextEdit, QTextEdit, CodeEditor, [class*='CodeEditor'] { "
                            "   font-family: '" + editorFamily + "' !important; "
                         "   font-size: " + QString::number(editorSize) + "px !important; "
                                        "   background-color: " + containerBgHex + " !important; "
                           "   color: " + palette.color(QPalette::Text).name() + " !important; "
                                                 "   border: none; "
                                                 "}"

                                                 "/* Верхний файловое меню */"
                                                 "QMenuBar { "
                                                 "   background-color: " + windowBgHex + " !important; "
                        "   border-bottom: 1px solid " + borderAccentHex + "; "
                            "   padding: 6px 2px; "
                            "}"
                            "QMenuBar::item { "
                            "   background: transparent; "
                            "   color: " + palette.color(QPalette::WindowText).name() + " !important; "
                                                       "   padding: 6px 12px; "
                                                       "   font-weight: bold; "
                                                       "}"
                                                       "QMenuBar::item:selected { background-color: " + borderAccentHex + " !important; }"

                            "/* Нижний статусбар */"
                            "QStatusBar { "
                            "   border-top: 1px solid " + borderAccentHex + "; "
                            "   background-color: " + windowBgHex + " !important; "
                        "}"
                        "QStatusBar QPushButton, QStatusBar QLabel { "
                        "   background-color: transparent !important; "
                        "   color: " + palette.color(QPalette::WindowText).name() + " !important; "
                                                       "   border: none; "
                                                       "   font-weight: bold; "
                                                       "   padding: 4px 12px; "
                                                       "}"
                                                       "QStatusBar QPushButton:hover { background-color: " + borderAccentHex + " !important; }"

                            "/* Окно переписки чата ИИ */"
                            "QTextBrowser#chatLogWidget { "
                            "   background-color: " + chatBgColor + " !important; "
                        "   color: " + chatTextColor + " !important; "
                          "   font-family: 'Segoe UI', Arial, sans-serif !important; "
                          "   font-size: " + QString::number(editorSize) + "px !important; "
                                        "   border: 1px solid " + chatBorderColor + "; "
                            "   border-radius: 4px; "
                            "}";

    qApp->setStyleSheet(globalStyle);

    // =========================================================================
    // 3. АППАРАТНЫЙ ОБХОД И СБРОС СТИЛЕЙ БЛОКИРОВЩИКОВ
    // =========================================================================
    for (QWidget *widget : QApplication::allWidgets())
    {
        if (!widget) continue;

        if (widget->window()->metaObject()->className() == QString("Settings")) {
            continue;
        }

        QString className = widget->metaObject()->className();
        QString objName = widget->objectName();

        if (className.contains("CodeEditor") || className == "QPlainTextEdit" || className == "QTextEdit")
        {
            if (objName != "chatLogWidget")
            {
                widget->setStyleSheet("");
                widget->setFont(editorFont);

                QPlainTextEdit *pe = qobject_cast<QPlainTextEdit*>(widget);
                if (pe && pe->document()) {
                    pe->document()->setDefaultFont(editorFont);
                    PythonHighlighter *highlighter = pe->findChild<PythonHighlighter*>();
                    if (highlighter) {
                        highlighter->loadThemeSettings();
                    }
                }
            }
        }
        else
        {
            if (objName != "btnTerminal" && objName != "btnSearch" &&
                objName != "btnLogs"     && objName != "btnTogglePip" &&
                objName != "btnAIChat"   && className != "QStatusBar" &&
                className != "QMenuBar"  && objName != "widget_3")
            {
                widget->setStyleSheet("");
            }

            if (className == "QWidget") {
                widget->setAttribute(Qt::WA_StyledBackground, true);
            } else {
                widget->setAttribute(Qt::WA_StyledBackground, false);
            }
        }
        QEvent event(QEvent::StyleChange);
        QApplication::sendEvent(widget, &event);
        widget->update();
    }
    qApp->processEvents();
}


void Neuro_programm::on_actionOpenSettings_triggered()
{
    // 1. Создаем объект диалогового окна
    Settings dialog(this);

    // 2. СВЯЗЫВАЕМ СИГНАЛ ДИАЛОГА С МЕТОДОМ ГЛАВНОГО ОКНА
    // Как только в диалоге нажмут кнопку "Сохранить/ОК", сработает этот коннект
    connect(&dialog, &Settings::settingsChanged, this, &Neuro_programm::applyGlobalFonts);

    // 3. Запускаем диалоговое окно в модальном режиме
    dialog.exec();
}

void Neuro_programm::applyThemeColors(bool /*isDarkTheme*/) {
    QPalette sysPalette = qApp->palette();
    QString colorHighlight = sysPalette.color(QPalette::Highlight).name();

    QString colorBreezeLight   = "#eff0f1";
    QString colorBreezeText    = "#232629";
    QString colorWidget3Bg     = "#f5f6f7";
    QString colorBorder        = "#bcbebf"; // Оставляем только для нижней панели файлов widget_3

    QString style = QString(
                        // Полностью убираем любые рамки и линии у главного окна и тулбара
                        "QMainWindow, QToolBar {"
                        "   background: %1 !important;"
                        "   background-color: %1 !important;"
                        "   border: none !important;"
                        "   padding: 0px !important;"
                        "   spacing: 0px !important;"
                        "}"

                        // ЖЕСТКО УБИРАЕМ ЛИНИЮ У ЗАГОЛОВКА: выставляем border: none для всей панели
                        "QWidget#customTitleBarPanel {"
                        "   background: %1 !important;"
                        "   background-color: %1 !important;"
                        "   border: none !important;" // Никаких рамок и линий ни с одной из сторон!
                        "   padding: 0px !important;"
                        "   margin: 0px !important;"
                        "}"

                        "QLabel { color: %2 !important; }"

                        // КНОПКИ УПРАВЛЕНИЯ ОКНОМ
                        "QPushButton#btnMinimize, QPushButton#btnMaximize, QPushButton#btnClose {"
                        "   background-color: transparent !important;"
                        "   border: 0px solid transparent !important;"
                        "   border-radius: 0px; outline: none;"
                        "   color: %2 !important;"
                        "   font-size: 11px;"
                        "}"
                        "QPushButton#btnMinimize:hover, QPushButton#btnMaximize:hover {"
                        "   background-color: #e1e2e3 !important;"
                        "}"
                        "QPushButton#btnClose:hover {"
                        "   background-color: #e81123 !important;"
                        "   color: white !important;"
                        "}"

                        // МЕНЮБАР (Сливается с заголовком без разделителей)
                        "QMenuBar {"
                        "   background-color: %1 !important;"
                        "   color: %2 !important;"
                        "   border: none !important;"
                        "}"
                        "QMenuBar::item:selected {"
                        "   background-color: %3 !important;"
                        "   color: white !important;"
                        "}"

                        // Линию оставляем ТОЛЬКО под widget_3, чтобы рабочая область визуально отделялась от шапки
                        "QWidget#widget_3 {"
                        "   background-color: %5 !important;"
                        "   color: %2 !important;"
                        "   border-top: none !important;"
                        "   border-left: none !important;"
                        "   border-right: none !important;"
                        "   border-bottom: 1px solid %4 !important;"
                        "}"
                        )
                        .arg(colorBreezeLight) // %1
                        .arg(colorBreezeText)  // %2
                        .arg(colorHighlight)   // %3
                        .arg(colorBorder)      // %4
                        .arg(colorWidget3Bg);  // %5

    this->setStyleSheet(style);
}

void Neuro_programm::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    // Просто вызываем нашу новую светлую палитру
    applyThemeColors(false);

    // Заставляем Qt гарантированно обновить интерфейс на экране
    this->update();
}

void Neuro_programm::updateWidget3Padding()
{
    if (!ui->widget_3 || !ui->leftDockWidget) return;

    int currentDockWidth = 0;

    // 1. Сохраняем идеальное выравнивание по левому краю QTextBrowser
    if (ui->leftDockWidget->isVisible() && !ui->leftDockWidget->isFloating()) {
        currentDockWidth = ui->leftDockWidget->frameGeometry().width();
        currentDockWidth += 28;
    }

    // 2. Извлекаем или принудительно создаем горизонтальный макет
    QHBoxLayout *layout = qobject_cast<QHBoxLayout*>(ui->widget_3->layout());
    if (!layout) {
        layout = new QHBoxLayout(ui->widget_3);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // Находим комбобоксы и кнопку внутри widget_3
        QComboBox *fileCombo = ui->widget_3->findChild<QComboBox*>("fileComboBox");
        QComboBox *deviceCombo = ui->widget_3->findChild<QComboBox*>("comboDevice");
        QPushButton *anyButton = ui->widget_3->findChild<QPushButton*>();

        // СТИЛЬ ПОЛНОСТЬЮ ПЛОСКИХ КОМБОБОКСОВ С ПРИНУДИТЕЛЬНОЙ СТРЕЛОЧКОЙ
        QString flatComboStyle =
            "QComboBox {"
            "    border: 1px solid transparent;"
            "    background-color: rgba(0, 0, 0, 0);" // Прозрачный фон
            "    border-radius: 4px;"
            "    padding: 4px 26px 4px 8px;"
            "    min-width: 150px;"
            "    color: #232629;"
            "}"
            "QComboBox:hover {"
            "    background-color: rgba(61, 174, 233, 0.1);" // Синяя подсветка Breeze
            "    border: 1px solid #3daee9;"
            "}"
            // Принудительно выделяем область под стрелочку, делая её видимой
            "QComboBox::drop-down {"
            "    subcontrol-origin: padding;"
            "    subcontrol-position: top right;"
            "    width: 20px;"
            "    border: none;"
            "    background: transparent;"
            "}"
            // ФОРСИРОВАННАЯ ОТРИСОВКА СТРЕЛОЧКИ-ТРЕУГОЛЬНИКА
            "QComboBox::down-arrow {"
            "    border-left: 4px solid transparent !important;"
            "    border-right: 4px solid transparent !important;"
            "    border-top: 5px solid #232629 !important;" // Четкий темный треугольник
            "    width: 0px !important;"
            "    height: 0px !important;"
            "    visibility: visible !important;" // Игнорируем попытки Qt скрыть стрелку
            "}"
            "QComboBox QAbstractItemView {"
            "    border: 1px solid #babdbf;"
            "    background-color: #ffffff;"
            "    selection-background-color: #3daee9;"
            "    selection-color: #ffffff;"
            "}";

        // СТИЛЬ ДЛЯ СОВЕРШЕННО ПЛОСКОЙ КНОПКИ
        QString flatButtonStyle =
            "QPushButton {"
            "    border: 1px solid transparent;"
            "    background: transparent;"
            "    border-radius: 4px;"
            "    padding: 4px 12px;"
            "    color: #232629;"
            "    font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "    background-color: rgba(61, 174, 233, 0.1);"
            "    border: 1px solid #3daee9;"
            "}"
            "QPushButton:pressed {"
            "    background-color: rgba(61, 174, 233, 0.2);"
            "}";

        if (fileCombo) {
            fileCombo->setStyleSheet(flatComboStyle);
            layout->addWidget(fileCombo);
        }

        if (deviceCombo) {
            deviceCombo->setStyleSheet(flatComboStyle);
            layout->addWidget(deviceCombo);
        }

        if (anyButton) {
            anyButton->setStyleSheet(flatButtonStyle);
            layout->addWidget(anyButton);
        }

        layout->addStretch();
    }

    // 3. Применяем проверенный динамический отступ
    layout->setContentsMargins(currentDockWidth, 0, 0, 0);
    layout->invalidate();
    layout->activate();
}

void Neuro_programm::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // Корректируем левый отступ widget_3 вслед за изменением окна
    updateWidget3Padding();
}