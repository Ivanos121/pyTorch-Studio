#include "neuro_programm.h"
#include "qmenubar.h"
#include "ui_neuro_programm.h"
#include "start_progect.h"
#include "panel_other.h"
#include "ui_panel_other.h"
#include "settings.h"
#include "breezeflatstyle.h"
#include "codeeditor.h"
#include "replwidget.h"
#include "advancedclosedialog.h"
#include "projectrootproxymodel.h"
#include "editorplaceholder.h"
#include "elidedlabel.h"
#include "ui_ai_panel.h"
#include "ai_panel.h"
#include "ui_ai_panel.h"

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
#include <QStandardPaths>
#include <QMessageBox>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QGraphicsDropShadowEffect>
#include <QWindow>
#include <QBitmap>
#include <QPainter>
#include <QMenu>
#include <QPalette>
#include <QColor>
#include <QActionGroup>
#include <QToolBar>
#include <QToolButton>
#include <QStackedWidget>

Neuro_programm* Neuro_programm::self = nullptr;
QList<Neuro_programm::LspErrorData> Neuro_programm::globalLspErrors;

Neuro_programm::Neuro_programm(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Neuro_programm)
{
    ui->setupUi(this);

    if (ui->widget_2)
    {
        // Приводим базовый QWidget к вашему кастомному классу AI_panel [INDEX]
        AI_panel *panelInstance = qobject_cast<AI_panel*>(ui->widget_2);

        if (panelInstance)
        {
            panelInstance->wf = this;       // Передаем обратную связь во встроенную панель [INDEX]
            this->aiPanel = panelInstance; // Заполняем короткий указатель главного окна
            qDebug() << ">>> [СВЯЗКА УСПЕШНА] Указатель wf для AI_panel инициализирован.";
        }
    }

    debuggedScriptProcess = new QProcess(this);

    projectModel = nullptr;
    projectProxyModel = nullptr;

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

    // Принудительно заставляем Qt регистрировать каждое микроперемещение мыши на шапке
    if (ui->customTitleBarPanel) {
        ui->customTitleBarPanel->setMouseTracking(true);
        ui->customTitleBarPanel->installEventFilter(this); // Вешаем фильтр на саму панель
    }

    // Принудительно отключаем верхнюю зону для док-виджетов
    if (ui->leftDockWidget) {
        ui->leftDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    }

    if (ui->leftDockWidget)
    {
        // Запрещаем доку стыковаться в верхнюю и нижнюю зоны главного окна.
        // Теперь он сможет жить ТОЛЬКО слева или справа, и никогда не вылезет наверх!
        ui->leftDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    }

    // Внутри конструктора Neuro_programm:
    this->setAttribute(Qt::WA_TranslucentBackground, false);

    connect(ui->centralStackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index);

        int placeholderIndex = this->property("placeholderIndex").toInt();
        if (index == placeholderIndex || index == 0) {
            return;
        }

        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (!currentPage) return;

        CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
        if (!currentEditor) return;

        // Отключаем старые коннекты
        //disconnect(currentEditor, &CodeEditor::textChanged, this, nullptr);

        // Подключаем чистый сигнал ввода текста
        // Убедитесь, что на странице 2 код внутри connect(currentEditor, &CodeEditor::textChanged...) выглядит так:
        // connect(currentEditor, &CodeEditor::textChanged, this, [this, currentEditor]() {
        //     QString absoluteFilePath = currentEditor->objectName();
        //     if (absoluteFilePath.isEmpty() || absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") {
        //         return;
        //     }

        //     if (ui->fileComboBox) {
        //         int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
        //         if (comboIdx != -1) {
        //             QString currentText = ui->fileComboBox->itemText(comboIdx);

        //             // Если текст НЕ заканчивается на пробел и звёздочку — зажигаем маркеры легитимно!
        //             if (!currentText.endsWith(" *")) {
        //                 this->setWindowModified(true);
        //                 currentEditor->document()->setModified(true);

        //                 QFileInfo info(absoluteFilePath);
        //                 ui->fileComboBox->setItemText(comboIdx, info.fileName() + " *");

        //                 if (ui->openFilesListWidget) {
        //                     for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
        //                         QListWidgetItem *item = ui->openFilesListWidget->item(i);
        //                         if (item && item->data(Qt::UserRole).toString() == absoluteFilePath) {
        //                             item->setText(info.fileName() + " *");
        //                             break;
        //                         }
        //                     }
        //                 }
        //                 updateTabName();
        //             }
        //         }
        //     }
        // });

        // ВСТАВИТЬ СЮДА (Страница 2, низ):
        connect(currentEditor, &CodeEditor::textChanged, this, [this, currentEditor]() {
            // Получаем путь строго из objectName виджета, убирая рассинхронизацию вкладок
            QString absoluteFilePath = currentEditor->objectName();
            if (absoluteFilePath.isEmpty() || absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") {
                return;
            }
            if (ui->fileComboBox) {
                int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
                if (comboIdx != -1) {
                    QString currentText = ui->fileComboBox->itemText(comboIdx);
                    if (!currentText.endsWith(" *")) {
                        this->setWindowModified(true);
                        currentEditor->document()->setModified(true);
                        QFileInfo info(absoluteFilePath);
                        ui->fileComboBox->setItemText(comboIdx, info.fileName() + " *");
                        if (ui->openFilesListWidget) {
                            for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
                                QListWidgetItem *item = ui->openFilesListWidget->item(i);
                                if (item && item->data(Qt::UserRole).toString() == absoluteFilePath) {
                                    item->setText(info.fileName() + " *");
                                    break;
                                }
                            }
                        }
                        updateTabName();
                    }
                }
            }
            // ФИКС-ТРИГГЕР: Принудительно запускаем асинхронный таймер didChange пакета в Jedi
            currentEditor->sendLspDidChange();
        });


    });

    ui->centralStackedWidget->setCurrentIndex(0);

    recentProjectsMenu = new QMenu(" Открыть недавние", this);
    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        recentProjectActions[i] = new QAction(this);
        recentProjectActions[i]->setVisible(false); // Прячем, пока список пуст
        connect(recentProjectActions[i], &QAction::triggered, this, &Neuro_programm::openRecentProject);
        recentProjectsMenu->addAction(recentProjectActions[i]);
    }
    // Сразу считываем историю с диска из ~/.config/PyTorchStudio/IDE.conf
    updateRecentProjectActions();

    // =========================================================================
    // НАПОЛНЕНИЕ МЕНЮ "ФАЙЛ"
    // =========================================================================

    // Действие "Новый проект" (Ctrl + Shift + N)
    QAction *New_progect = new QAction("Новый проект", this);
    New_progect->setShortcut(QKeySequence("Ctrl+Shift+N"));
    New_progect->setIcon(QIcon(":/Data/system_icons/document-new.svg"));
    connect(New_progect, &QAction::triggered, this, &Neuro_programm::new_progect);

    // Действие "Новый файл" (Ctrl + N)
    QAction *New_file = new QAction(" Новый файл", this);
    New_file->setShortcut(QKeySequence("Ctrl + N"));
    New_file->setIcon(QIcon(":/Data/system_icons/document-new.svg"));
    // В файле neuro_programm.cpp внутри коннекта действия New_file (Страница 3-4 вашего PDF):
    connect(New_file, &QAction::triggered, this, [this]()
            {
        if (currentOpenProjectPath.isEmpty()) {
            sendSystemNotification("Внимание", "Сначала откройте или создайте проект (*.pystudio)");
            return;
        }

        bool ok;
        QString fileName = QInputDialog::getText(this, "Создание файла",
                                                 "Введите имя нового Python-файла:",
                                                 QLineEdit::Normal, "script", &ok);

        if (ok && !fileName.trimmed().isEmpty()) {
            if (!fileName.endsWith(".py", Qt::CaseInsensitive)) {
                fileName += ".py";
            }

            QString fullPath = currentOpenProjectPath + "/" + fileName.trimmed();

            // 1. Создаем физически пустой файл на диске, чтобы openNewFileInEditor прочел его без багов
            QFile file(fullPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.close(); // Создали пустым
            }

            // 2. Вызываем ваш метод вывода редактора на экран (он прогрузит вкладку)
            this->openNewFileInEditor(fullPath);

            // =========================================================================
            // КРИТИЧЕСКИЙ ФИКС KEYERROR ДЛЯ НОВЫХ ФАЙЛОВ:
            // Регистрируем свежесозданный пустой документ на сервере LSP!
            // Теперь Jedi внесет script.py в ОЗУ, и KeyError больше никогда не возникнет.
            // =========================================================================
            this->sendLspDidOpenForFile(fullPath, "");
        }
    });

    // Действие "Открыть проект" (Ctrl + Shift + N)
    QAction *Open_progect = new QAction("Открыть проект", this);
    Open_progect->setShortcut(QKeySequence("Ctrl+O"));
    Open_progect->setIcon(QIcon(":/Data/system_icons/document-open.svg"));
    connect(Open_progect, &QAction::triggered, this, &Neuro_programm::onOpenProjectMenuTriggered);

    // Действие "Сохранить" (Ctrl + S)
    QAction *actionSave = new QAction("Сохранить", this);
    actionSave->setShortcut(QKeySequence("Ctrl + S"));
    actionSave->setIcon(QIcon(":/Data/system_icons/document-save.svg"));
    connect(actionSave, &QAction::triggered, this, &Neuro_programm::saveCurrentActiveFile);

    // Действие "Сохранить как" (Ctrl + A + S)
    QAction *actionSave_as = new QAction("Сохранить как", this);
    //actionSave_as->setShortcut(QKeySequence("Ctrl + A + S"));
    actionSave_as->setIcon(QIcon(":/Data/system_icons/document-save-as.svg"));
    connect(actionSave_as, &QAction::triggered, this, &Neuro_programm::saveProjectAs);

    // Действие "Сохранить всё" (Ctrl + Shift + S)
    QAction *actionSaveAll = new QAction("Сохранить всё", this);
    actionSaveAll->setShortcut(QKeySequence("Ctrl + Shift + S"));
    actionSaveAll->setIcon(QIcon(":/Data/system_icons/document-save-as.svg"));
    connect(actionSaveAll, &QAction::triggered, this, &Neuro_programm::saveAllProjectChanges);

    // Действие "Сохранить проект" (Shift + S)
    QAction *save_progect_all = new QAction("Сохранить проект", this);
    save_progect_all->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_S));
    //connect(save_progect_all, &QAction::triggered, this, &Neuro_programm::saveAllProjectChanges);

    // Действие "Закрыть проект" (Ctrl + W)
    QAction *actionCloseProject = new QAction("Закрыть проект", this);
    actionCloseProject->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(actionCloseProject, &QAction::triggered, this, &Neuro_programm::onCloseProjectClicked);

    // Действие "Выход" (Ctrl + Q)
    QAction *actionClose = new QAction("Выход", this);
    actionClose->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    actionClose->setIcon(QIcon(":/Data/system_icons/application-exit.svg"));
    connect(actionClose, &QAction::triggered,this, &Neuro_programm::close_program);


    // 2. Полностью УДАЛЯЕМ встроенный системный менюбар, который лезет наверх
    if (this->menuBar()) {
        this->menuBar()->clear();
        this->menuBar()->setVisible(false);
        delete this->menuBar();
        this->setMenuBar(nullptr);
    }
    this->setMenuBar(nullptr);
    this->setContentsMargins(0, 0, 0, 0);
    this->setStyleSheet("QMainWindow { background-color: blue; }");

    // =========================================================================
    // 1. СОЗДАНИЕ ВЕРТИКАЛЬНОЙ ПАНЕЛИ НА БАЗЕ QWIDGET (IDE СТИЛЬ)
    // =========================================================================
    // =========================================================================
    // 1. СОЗДАНИЕ ВЕРТИКАЛЬНОЙ ПАНЕЛИ НА БАЗЕ QWIDGET (IDE СТИЛЬ)
    // =========================================================================
    QWidget *leftSideBarContainer = new QWidget(this);
    leftSideBarContainer->setObjectName("leftSideBarContainer");

    leftSideBarContainer->setMinimumWidth(68);
    leftSideBarContainer->setMaximumWidth(68);
    leftSideBarContainer->setFixedWidth(68);

    // --------------------------------=========================================
    // ЖЕСТКИЙ АППАРАТНЫЙ ФИКС ЦВЕТА ЧЕРЕЗ ПАЛИТРУ (ИГНОРИРУЕТ ЛЮБОЙ СТОРОННИЙ CSS)
    // --------------------------------=========================================
    leftSideBarContainer->setAutoFillBackground(true);
    QPalette sidebarPalette = leftSideBarContainer->palette();
    // Насильно заливаем фон виджета в эталонный светлый цвет Breeze (#eff0f1)
    sidebarPalette.setColor(QPalette::Window, QColor(239, 240, 241));
    leftSideBarContainer->setPalette(sidebarPalette);

    QVBoxLayout *sidebarLayout = new QVBoxLayout(leftSideBarContainer);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(6);

    // В CSS оставляем только правила для кнопок-ховеров, убрав background-color панели
    QString buttonStyle =
        "QToolButton {"
        " background-color: transparent !important;"
        " border: none !important;"
        " margin: 0px 0px 0px 0px !important;"
        " padding: 0px 0px !important;"
        " border-radius: 0px !important;"
        "}"
        /* Мягкий серый цвет выделения при наведении мыши */
        "QToolButton:hover {"
        " background-color: #e4e5e6 !important;"
        " border: none !important;"
        "}"
        "QToolButton QLabel {"
        " color: #232629 !important;"
        "}"
        "QToolButton:hover QLabel {"
        " color: #000000 !important;"
        "}";


    // =========================================================================
    // 2. СТРУКТУРА ДАННЫХ ДЛЯ АППАРАТНОЙ СБОРКИ КНОПОК
    // =========================================================================
    struct SidebarButtonConfig {
        QAction** targetActionPtr;
        QString text;
        QIcon icon;
    };

    QList<SidebarButtonConfig> buttonConfigs = {
        {&actProject, "Проект", QIcon(":/Data/system_icons/document-open.svg")},
        {&actControlPanel, "Настройки ИИ", QIcon::fromTheme(":/Data/system_icons/configure.svg")},
        {&actTensor, "Графики", QIcon(":/Data/system_icons/document-save-as.svg")},
        {&actPip, "Пакеты PIP", QIcon(":/Data/system_icons/document-open.svg")},
        {&actSearch, "Поиск", QIcon(":/Data/system_icons/edit-find.svg")}
    };

    sidebarLayout->addSpacing(10);

    // =========================================================================
    // 3. ЦИКЛ СБОРКИ: 100% ШИРИНА И ПОЛНЫЙ КОНТРОЛЬ ЗАЗОРОВ
    // =========================================================================
    for (const auto& config : buttonConfigs)
    {
        *(config.targetActionPtr) = new QAction(config.text, this);
        (*(config.targetActionPtr))->setCheckable(false);

        QToolButton *btn = new QToolButton(leftSideBarContainer);
        btn->setObjectName(config.text); // Жестко связываем имя объекта

        btn->setDefaultAction(*(config.targetActionPtr));

        // Прямая гарантированная установка иконки без системных тем Linux:
        btn->setIcon(config.icon);
        btn->setIconSize(QSize(22, 22));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setStyleSheet(buttonStyle);

        btn->setMinimumWidth(68);
        btn->setMaximumWidth(68);
        btn->setFixedWidth(68);
        btn->setFixedHeight(54);

        QLabel *lbl = new QLabel(config.text, btn);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #b9bbbe; background: transparent; border: none;");

        QFont lblFont = lbl->font();
        lblFont.setPixelSize(9);
        lblFont.setBold(true);
        lbl->setFont(lblFont);

        QVBoxLayout *btnLayout = new QVBoxLayout(btn);
        btnLayout->setContentsMargins(0, 6, 0, 4);
        btnLayout->setSpacing(0);

        btnLayout->addStretch(1);
        btnLayout->addWidget(lbl, 0, Qt::AlignHCenter | Qt::AlignBottom);

        sidebarLayout->addWidget(btn);
    }

    sidebarLayout->addStretch(1);

    // =========================================================================
    // 4. КОРРЕКТНЫЙ МОНТАЖ В ОКНО ПОВЕРХ ВСЕХ СЛОЕВ
    // =========================================================================

    this->leftSideBarContainer = leftSideBarContainer;

    // 8. Логика переключения режимов (Слот-обработчик)
    QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");

    if (actProject && ui->leftDockWidget) {
        // Очищаем старые привязки, чтобы они не двоились в памяти
        actProject->disconnect();

        connect(actProject, &QAction::triggered, this, [this]() {
            QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");
            if (!dockStack) return;

            // Вычисляем целевую страницу (0 - дерево, 1 - стартовые кнопки)
            int targetPageIndex = this->currentOpenProjectPath.isEmpty() ? 1 : 0;

            // СЛУЧАЙ 1: Док-виджет полностью скрыт
            if (ui->leftDockWidget->isHidden()) {
                // Сначала настраиваем правильную вкладку, пока док еще в тени
                dockStack->setCurrentIndex(targetPageIndex);
                // Даем команду Qt нативно и плавно развернуть док-виджет
                ui->leftDockWidget->toggleViewAction()->trigger();
            }
            // СЛУЧАЙ 2: Док-виджет открыт на экране
            else {
                // Если в нем активна именно кнопка проекта — закрываем док нативно
                if (dockStack->currentIndex() == targetPageIndex) {
                    ui->leftDockWidget->toggleViewAction()->trigger();
                }
                // Если в доке была открыта другая панель (например, ИИ настройки) — просто переключаем вкладку
                else {
                    dockStack->setCurrentIndex(targetPageIndex);
                }
            }
        });
    }







    // НА СТРАНИЦЕ 6-7 ЗАМЕНИТЕ СЛОТ ДЛЯ actProject:
    connect(actProject, &QAction::triggered, this, [this, dockStack]()
    {
        if (!dockStack) {
            qWarning() << "[SIDEBAR] Критическая ошибка: dockContentsStack не найден в доке!";
            return;
        }

        // Проверяем, открыт ли сейчас какой-либо файл в редакторе (индексы файлов >= 2)
        bool isFileOpened = (ui->centralStackedWidget->currentIndex() >= 2);
        int targetPageIndex = currentOpenProjectPath.isEmpty() ? 1 : 0;

        if (isFileOpened) {
            // РЕЖИМ С ОТКРЫТЫМ ФАЙЛОМ: Просто открываем/закрываем док-виджет справа
            bool isVisible = ui->leftDockWidget->isVisible();
            ui->leftDockWidget->setVisible(!isVisible);
            actProject->setChecked(!isVisible); // Синхронизируем кнопку
            qInfo() << "[SIDEBAR] Переключение видимости панели при открытом файле:" << !isVisible;
        }
        else {
            // РЕЖИМ БЕЗ ОТКРЫТОГО ФАЙЛА (Стартовый экран / Панель ИИ): Ваша оригинальная логика PyCharm
            if (ui->leftDockWidget->isVisible() && dockStack->currentIndex() == targetPageIndex) {
                ui->leftDockWidget->setVisible(false);
                actProject->setChecked(false);
                qInfo() << "[SIDEBAR] Схлопываем панель проекта.";
            }
            else {
                dockStack->setCurrentIndex(targetPageIndex);
                ui->leftDockWidget->setVisible(true);
                actProject->setChecked(true);
                qInfo() << "[SIDEBAR] Разворачиваем панель проекта.";
            }
        }
    });

    // СИНХРОНИЗАЦИЯ: Если пользователь закрыл док крестиком или кодом, отжимаем кнопку на панели
    connect(ui->leftDockWidget, &QDockWidget::visibilityChanged, this, [this](bool visible)
    {
        if (!visible && actProject)
        {
            actProject->setChecked(false);
        }
    });

    // 1. Создаем НАСТОЯЩИЙ QToolBar, чтобы он аппаратно занимал 100% ширины поверх доков!
    QToolBar *topContainerBar = new QToolBar(this);
    topContainerBar->setObjectName("topContainerBar");

    // Блокируем перетаскивание тулбара, чтобы пользователь не сломал интерфейс
    topContainerBar->setMovable(false);
    topContainerBar->setFloatable(false);
    topContainerBar->setAllowedAreas(Qt::TopToolBarArea);

    // 2. ЖЕЛЕЗНЫЙ ФИКС СЕРОЙ ПОЛОСЫ И ОТСТУПОВ НА УРОВНЕ СИСТЕМЫ
    topContainerBar->setContentsMargins(0, 0, 0, 0);
    if (topContainerBar->layout()) {
        topContainerBar->layout()->setContentsMargins(0, 0, 0, 0);
        topContainerBar->layout()->setSpacing(0);
    }

    // 3. Заставляем тулбар намертво подчиняться цветам темной/светлой темы
    topContainerBar->setAttribute(Qt::WA_StyledBackground, true);
    topContainerBar->setStyleSheet(
                "QToolBar#topContainerBar {"
                "   background-color: #eff0f1;" // Светлый фон по дефолту (перекрасится в settings.cpp)
                "   border: none;"               // ПОЛНОСТЬЮ УДАЛЯЕТ СЕРУЮ ЛИНЕЙКУ СНИЗУ!
                "   padding: 0px;"
                "   margin: 0px;"
                "}"
                );

    // 4. Помещаем вашу готовую внутреннюю обертку (topWrapper) внутрь тулбара
    // Убедитесь, что внутри topWrapper->layout() (ваш topLayout) уже уложены:
    // customTitleBarPanel, customMenuBar и widget_3!

    // 5. Жестко фиксируем итоговую высоту (35 шапка + 35 меню + 30 чипы = 100px)
    topContainerBar->setFixedHeight(100);

    // 6. НАДЁЖНЫЙ МОНТАЖ В ОКНО: Тулбар встает на самый верхний слой приложения, прижимая доки вниз!
    this->addToolBar(Qt::TopToolBarArea, topContainerBar);

    // Сдвигаем виджеты сразу после старта, когда Qt отрисует геометрию дока
    QTimer::singleShot(100, this, &Neuro_programm::updateWidget3Padding);

    // 7. Правильно настраиваем углы для док-виджетов, чтобы они не лезли наверх
    this->setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    this->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    // 4. Внутренний вертикальный макет, который жестко зафиксирует порядок элементов
    QWidget *topWrapper = new QWidget(topContainerBar);
    QVBoxLayout *topLayout = new QVBoxLayout(topWrapper);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    topContainerBar->addWidget(topWrapper);

    // 5. Создаем новый ручной QMenuBar, который будет вести себя как послушный виджет
    QMenuBar *customMenuBar = new QMenuBar(topWrapper);

    customMenuBar->setAutoFillBackground(true);

    // 2. Создаем и настраиваем изолированную темную палитру
    QPalette menuPalette = customMenuBar->palette();

    // Красим подложку менюбара в глубокий темный цвет вашего интерфейса (#202225)
    menuPalette.setColor(QPalette::ColorRole::Window, QColor(32, 34, 37));

    // Задаем светлый цвет текста для пунктов "Файл", "Правка"...
    menuPalette.setColor(QPalette::ColorRole::WindowText, QColor(212, 212, 212));
    menuPalette.setColor(QPalette::ColorRole::Text, QColor(212, 212, 212));

    customMenuBar->setPalette(menuPalette);

    // Наполняем его вашими пунктами меню и экшенами из Qt Designer
    QMenu *fileMenu = customMenuBar->addMenu("Файл");
    fileMenu->addAction(New_progect);
    fileMenu->addAction(New_file);
    fileMenu->addAction(Open_progect);
    fileMenu->addSeparator();
    fileMenu->addAction(actionSave);
    fileMenu->addAction(actionSave_as);
    fileMenu->addAction(actionSaveAll);
    fileMenu->addSeparator();
    fileMenu->addMenu(recentProjectsMenu);
    fileMenu->addSeparator();
    fileMenu->addAction(save_progect_all);
    fileMenu->addAction(actionCloseProject);
    fileMenu->addAction(actionClose);


    QMenu *editMenu = customMenuBar->addMenu("Правка");

    // =========================================================================
    // НАПОЛНЕНИЕ МЕНЮ "ПРАВКА" (Undo, Redo, Cut, Copy, Paste, Delete)
    // =========================================================================

    // 1. Создаем экшены, вешаем иконки (по желанию), шорткаты и объектные имена
    QAction *actionUndo = new QAction("Отменить", this);
    actionUndo->setShortcut(QKeySequence::Undo); // Автоматически выставит Ctrl+Z
    actionUndo->setObjectName("actionUndo");
    connect(actionUndo, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionRedo = new QAction("Повторить", this);
    actionRedo->setShortcut(QKeySequence::Redo); // Автоматически выставит Ctrl+Y (или Ctrl+Shift+Z)
    actionRedo->setObjectName("actionRedo");
    connect(actionRedo, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionCut = new QAction("Вырезать", this);
    actionCut->setShortcut(QKeySequence::Cut); // Ctrl+X
    actionCut->setObjectName("actionCut");
    connect(actionCut, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionCopy = new QAction("Копировать", this);
    actionCopy->setShortcut(QKeySequence::Copy); // Ctrl+C
    actionCopy->setObjectName("actionCopy");
    connect(actionCopy, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionPaste = new QAction("Вставить", this);
    actionPaste->setShortcut(QKeySequence::Paste); // Ctrl+V
    actionPaste->setObjectName("actionPaste");
    connect(actionPaste, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionSelectAll = new QAction("Выделить всё", this);
    actionSelectAll->setShortcut(QKeySequence::SelectAll); // Автоматически назначит Ctrl+A
    actionSelectAll->setObjectName("actionSelectAll");
    connect(actionSelectAll, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionExtra = new QAction("Удалить", this);
    actionExtra->setObjectName("actionExtra");

    QAction *actionDelete = new QAction("Удалить", this);
    actionDelete->setShortcut(QKeySequence::Delete); // Delete
    actionDelete->setObjectName("actionDelete");
    connect(actionDelete, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    QAction *actionSearch = new QAction("Поиск/замена", this);
    actionSearch->setShortcut(QKeySequence::Find);
    actionSearch->setObjectName("actionSearch");
    connect(actionSearch, &QAction::triggered, this, &Neuro_programm::triggerEditAction);

    // 2. Добавляем созданные пункты в меню "Правка" с разделителями
    editMenu->addAction(actionUndo);
    editMenu->addAction(actionRedo);
    editMenu->addSeparator();
    editMenu->addAction(actionCut);
    editMenu->addAction(actionDelete);
    editMenu->addAction(actionCopy);
    editMenu->addAction(actionPaste);
    editMenu->addSeparator();
    editMenu->addAction(actionExtra);
    editMenu->addAction(actionSelectAll);
    editMenu->addSeparator();
    editMenu->addAction(actionSearch);

    QMenu *toolsMenu = customMenuBar->addMenu("Инструменты");

    // =========================================================================
    // НАПОЛНЕНИЕ МЕНЮ "ИНСТРУМЕНТЫ"
    // =========================================================================

    QAction *actionSettings = new QAction("Настройки", this);
    actionSettings->setObjectName("actionSettings");
    actionSettings->setShortcut(QKeySequence("Ctrl + Shift + /"));
    actionSettings->setIcon(QIcon(":/Data/system_icons/document-save-as.svg"));
    connect(actionSettings, &QAction::triggered, this, &Neuro_programm::open_settings);
    toolsMenu->addAction(actionSettings);


    //Создаем ПОДМЕНЮ для PIP (вместо QAction используем QMenu)
    QMenu *pipSubMenu = new QMenu("Менеджер пакетов PIP", this);
    pipSubMenu->setIcon(QIcon(":/Data/system_icons/document-save-as.svg"));

    QAction *actInstallPip = new QAction("Установить пакет", this);
    connect(actInstallPip, &QAction::triggered, this, &Neuro_programm::onInstallSinglePackageTriggered);
    pipSubMenu->addAction(actInstallPip);

    // Действие Б: Установка из файла зависимостей
    QAction *actInstallReqs = new QAction("Установить из requirements.txt...", this);
    connect(actInstallReqs, &QAction::triggered, this, &Neuro_programm::install_from_requirements);
    pipSubMenu->addAction(actInstallReqs);

    // Действие В: Быстрое обновление списка пакетов
    QAction *actRefreshPip = new QAction("Обновить список пакетов", this);
    // connect(actRefreshPip, &QAction::triggered, this, &Neuro_programm::refresh_pip_list); // Слот нужно объявить в хедере
    pipSubMenu->addAction(actRefreshPip);

    // 3. ДОБАВЛЯЕМ ПОДМЕНЮ В ГЛАВНОЕ МЕНЮ
    toolsMenu->addMenu(pipSubMenu);

    QMenu *helpMenu = customMenuBar->addMenu("Справка");
    helpMenu->addAction(ui->action_help);
    helpMenu->addAction(ui->aboutProgram);

    // 6. Собираем элементы в ИДЕАЛЬНОМ вертикальном порядке (Шапка -> Menu -> Файлы)
    topLayout->addWidget(ui->customTitleBarPanel); // 1. Самый верх приложения
    topLayout->addWidget(customMenuBar);           // 2. Строго под шапкой
    if (ui->widget_3)
    {
        topLayout->addWidget(ui->widget_3);       // 3. Строго под меню
    }

    // Убеждаемся, что сигналы подключены к нашей новой функции отступов
    connect(ui->leftDockWidget, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        Q_UNUSED(visible);
        this->updateWidget3Padding();
    });

    // =========================================================================
    // ОПТИМИЗАЦИЯ И УМЕНЬШЕНИЕ ВЫСОТЫ ВЕРХНЕГО БАРА
    // =========================================================================
    // 1. Делаем кастомную шапку с кнопками управления максимально тонкой (28px вместо 35px)
    if (ui->customTitleBarPanel) {
        ui->customTitleBarPanel->setFixedHeight(25);
    }

    // 2. Ужимаем высоту строки меню Файл, Правка (25px вместо 35px)
    if (customMenuBar) {
        customMenuBar->setFixedHeight(35);
    }

    // 3. Если widget_3 (панель чипов) пустует или временно не нужен,
    // мы жестко сжимаем его высоту до нуля, либо выставляем компактные 25px!
    if (ui->widget_3) {
        ui->widget_3->setFixedHeight(25); // Поставьте 0, чтобы убрать лишнюю серую полосу, или 25 для компактности
    }

    // 4. ЖЕСТКИЙ ФИНАЛЬНЫЙ РАСЧЕТ И ФИКСАЦИЯ ВЫСОТЫ ВСЕГО ТУЛБАРА
    // Вместо старых 100 пикселей задаем строго 53 пикселя (28 шапка + 25 меню)!
    // Если widget_3 равен 25px, то итоговая высота будет 28 + 25 + 25 = 78px.
    int finalTargetHeight = 85;
    if (ui->widget_3 && ui->widget_3->height() > 0)
    {
        finalTargetHeight += ui->widget_3->height();
    }

    topContainerBar->setFixedHeight(finalTargetHeight);


    if (ui->customTitleBarPanel) {
        ui->customTitleBarPanel->setMouseTracking(true);
        ui->customTitleBarPanel->installEventFilter(this);
    }
    if (topWrapper) {
        topWrapper->setMouseTracking(true);
        topWrapper->installEventFilter(this); // Пробрасываем мышь сквозь макет-обертку
    }
    if (topContainerBar) {
        topContainerBar->setMouseTracking(true);
        topContainerBar->installEventFilter(this);
    }

    // neuro_programm.cpp -> Внутри конструктора, где настраиваются комбобоксы
    if (ui->comboDevice) {
        // 1. НАМЕРТВО ФИКСИРУЕМ ШИРИНУ НА ЭКРАНЕ (например, 220 пикселей)
        ui->comboDevice->setFixedWidth(220);

        // 2. Ограничиваем ширину выпадающего списка (View), чтобы он не разъезжался от длинных имен функций
        if (ui->comboDevice->view()) {
            ui->comboDevice->view()->setFixedWidth(220);
        }

        // 3. Задаем режим эллайдинга (троеточия), если имя функции слишком длинное
        ui->comboDevice->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    }


    // Просто и чисто укладываем обертку в контейнер через плоский горизонтальный слой
    QVBoxLayout *containerLayout = new QVBoxLayout(topContainerBar);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    containerLayout->addWidget(topWrapper); // Теперь шапка сидит строго внутри контейнера
    topContainerBar->setLayout(containerLayout);

    // Сдвигаем виджеты сразу после старта, когда Qt отрисует геометрию дока
    QTimer::singleShot(100, this, &Neuro_programm::updateWidget3Padding);

    if (ui->centralwidget) {
        ui->centralwidget->setAttribute(Qt::WA_StyledBackground, true);
        if (ui->centralwidget->layout()) {
            ui->centralwidget->layout()->setContentsMargins(0, 0, 0, 0);
            ui->centralwidget->layout()->setSpacing(0);
        }
    }

    connect(ui->action_help, &QAction::triggered, this, [this]() {

        // Если окно еще ни разу не создавалось — инициализируем его
        if (!rsc4) {
            // Создаем виджет. Передаем 'this' (главное окно) как родителя,
            // чтобы при закрытии основной программы память автоматически очищалась.
            rsc4 = new QWidget(this);

            // ХИТРОСТЬ: Задаем флаг Qt::Window. Он принудительно отрывает виджет
            // от главного окна и превращает его в полноценное независимое окно ОС.
            rsc4->setWindowFlags(Qt::Window);

            // Настраиваем базовые свойства окна
            rsc4->setWindowTitle("Автономный Терминал");
            rsc4->resize(600, 400);
        }

        // Если окно было свернуто или скрыто — восстанавливаем его и выводим на передний план
        if (rsc4->isMinimized()) {
            rsc4->showNormal();
        }

        rsc4->showMaximized();
        rsc4->activateWindow(); // Переводит фокус ОС на это окно
    });

    // =================================================================
    // НАСТРОЙКА КНОПОК УПРАВЛЕНИЯ ВНУТРИ ШАПКИ
    // =================================================================

    // Очищаем customTitleBarPanel от старого мусора из Designer
    // for (QWidget *child : std::as_const(ui->customTitleBarPanel->findChildren<QWidget*>())) {
    //     child->deleteLater();
    // }

    if (ui->customTitleBarPanel->layout()) {
        ui->customTitleBarPanel->layout()->setParent(nullptr);
    }

    QHBoxLayout *panelLayout = new QHBoxLayout(ui->customTitleBarPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0); // Нулевые отступы для плотного прилегания кнопок к краю
    panelLayout->setSpacing(6);

    // 1. Создаем графический элемент для иконки приложения
    QLabel *iconLabel = new QLabel(ui->customTitleBarPanel);

    // Достаем SVG-иконку из встроенных ресурсов Qt (путь, который вы прописали в .qrc)
    QIcon appIcon(":/Data/Icons/pytorch-studio.svg");
    // Превращаем иконку в картинку (Pixmap) аккуратного размера (например, 16x16 или 18x18)
    QPixmap iconPixmap = appIcon.pixmap(18, 18);

    iconLabel->setPixmap(iconPixmap);
    iconLabel->setFixedSize(18, 18); // Жестко фиксируем размеры значка

    // 2. Добавляем иконку в самый левый край макета шапки
    panelLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

    // Левая симметричная пружина
    panelLayout->addStretch();

    // Создаем текстовую метку программно
    titleLabel = new QLabel("PyTorch Studio", ui->customTitleBarPanel);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    panelLayout->addWidget(titleLabel);
    titleLabel->installEventFilter(this);
    panelLayout->installEventFilter(this);

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

    // Находим макет, который управляет элементами внутри вашей шапки
    if (panelLayout) {
        // Очищаем старые привязки и заставляем макет выравнивать всё строго по вертикальному центру!
        //panelLayout->setAlignment(Qt::AlignVCenter);

        // Убираем скрытые верхние/нижние зазоры самого макета шапки
        panelLayout->setContentsMargins(10, 0, 10, 0); // 10px отступы по бокам, 0px сверху и снизу
        panelLayout->setSpacing(6); // Расстояние между кнопками управления

        // ... (код добавления надписи "PyTorch Studio" и Stretch-распорки) ...

        // ПРАВИЛЬНОЕ ДОБАВЛЕНИЕ КНОПОК С ФЛАГОМ Qt::AlignVCenter:
        // Флаг принудительно выровняет каждую кнопку ровно по середине высоты бара
        if (btnMinimize) panelLayout->addWidget(btnMinimize, 0, Qt::AlignVCenter);
        if (btnMaximize) panelLayout->addWidget(btnMaximize, 0, Qt::AlignVCenter);
        if (btnClose)    panelLayout->addWidget(btnClose,    0, Qt::AlignVCenter);
    }

    // Логика сигналов для кнопок управления окном
    if (btnMinimize)
    {
        connect(btnMinimize, &QPushButton::clicked, this, &Neuro_programm::showMinimized);
    }
    if (btnClose)
    {
        connect(btnClose, &QPushButton::clicked, this, &Neuro_programm::close);
    }
    if (btnMaximize)
    {
        connect(btnMaximize, &QPushButton::clicked, this, [this]()
                {
            if (this->isMaximized())
            {
                this->showNormal();
            } else {
                this->showMaximized();
            }
        });
    }

    // Активируем трекинг мыши, чтобы пробить защиту QToolBar
    topContainerBar->setMouseTracking(true);
    topWrapper->setMouseTracking(true);
    if (ui->customTitleBarPanel) {
        ui->customTitleBarPanel->setMouseTracking(true);
        // КРИТИЧЕСКИЙ ШАГ: Вешаем фильтр событий на шапку, чтобы заработал Drag & Drop!
        ui->customTitleBarPanel->installEventFilter(this);
    }
    self = this;
    trainingProcess = nullptr;

    networkManager = new QNetworkAccessManager(this);

    // Возвращаем стандартную сборку вертикального сплиттера:
    mainVerticalSplitter = new QSplitter(Qt::Vertical, this);

    QWidget *oldCentral = ui->centralwidget;
    mainVerticalSplitter->addWidget(oldCentral);

    panelOther = new panel_other(this);
    mainVerticalSplitter->addWidget(panelOther);
    panelOther->setVisible(false);

    // Делаем сплиттер главным центральным виджетом, как у вас и было изначально
    // =========================================================================
    // АРХИТЕКТУРНЫЙ МОНТАЖ ЛЕВОЙ ПАНЕЛИ И ГЛАВНОГО СПЛИТТЕРА (КОНЕЦ КОНСТРУКТОРА)
    // =========================================================================
    if (leftSideBarContainer && mainVerticalSplitter)
    {
        // 1. Создаем главный горизонтальный разделитель окон
        QSplitter *mainHorizontalSplitter = new QSplitter(Qt::Horizontal, this);
        mainHorizontalSplitter->setObjectName("mainHorizontalSplitter");

        // Убираем у него рамки, чтобы интерфейс выглядел монолитно в темной теме
        mainHorizontalSplitter->setStyleSheet(
            "QSplitter#mainHorizontalSplitter { border: none; background-color: #202225; }"
            "QSplitter#mainHorizontalSplitter::handle { background-color: #1a1c1e; width: 1px; }"
        );
        mainHorizontalSplitter->setHandleWidth(1);

        // 2. Внедряем левую панель кнопок (Она встает на экране ПЕРВОЙ, то есть КРАЙНЕЙ СЛЕВА)
        mainHorizontalSplitter->addWidget(leftSideBarContainer);

        // 3. Добавляем ваш готовый вертикальный сплиттер (код + терминал) в правую часть
        mainHorizontalSplitter->addWidget(mainVerticalSplitter);

        // 4. Теперь именно горизонтальный сплиттер становится хозяином центра окна!
        this->setCentralWidget(mainHorizontalSplitter);

        // 5. Жестко фиксируем пропорции: левая панель намертво держит 68px и не сжимается мышкою
        mainHorizontalSplitter->setStretchFactor(0, 0);
        mainHorizontalSplitter->setStretchFactor(1, 1);
        mainHorizontalSplitter->setCollapsible(0, false);

        // Явно задаем стартовую геометрию
        mainHorizontalSplitter->setSizes(QList<int>({68, this->width() - 68}));

        qDebug() << ">>> [АРХИТЕКТУРА] Успешная сборка: Панель(68px) + Код/Терминал собраны без QToolBar.";
    }
    else
    {
        // =========================================================================
        // ГАРАНТИРОВАННЫЙ ПЕРЕНОС: КНОПКИ В САМОМ ЛЕВОМ КРАЮ (ЛЕВЕЕ ДОКВИДЖЕТА)
        // =========================================================================

        // 1. Возвращаем ваш оригинальный вертикальный сплиттер на его законное центральное место
        if (mainVerticalSplitter)
        {
            this->setCentralWidget(mainVerticalSplitter);
        }

        // 2. Монтируем контейнер кнопок через компактную обертку тулбара QMainWindow
        if (this->leftSideBarContainer)
        {
            QToolBar *wrapperBar = new QToolBar(this);
            wrapperBar->setObjectName("leftSideBar");
            wrapperBar->setMovable(false);
            wrapperBar->setFloatable(false);
            wrapperBar->setAllowedAreas(Qt::LeftToolBarArea);
            wrapperBar->setContentsMargins(0, 0, 0, 0);

            // =========================================================================
            // ТОТАЛЬНЫЙ СБРОС ТЕМНОГО ЦВЕТА ОБЕРТКИ (КОНЕЦ КОНСТРУКТОРА)
            // =========================================================================
            wrapperBar->setAutoFillBackground(true);
            QPalette wrapperPalette = wrapperBar->palette();
            // Заливаем обертку тулбара в тот же светлый цвет Breeze
            wrapperPalette.setColor(QPalette::Window, QColor(239, 240, 241));
            wrapperBar->setPalette(wrapperPalette);

            wrapperBar->setStyleSheet(
                "QToolBar#leftSideBar {"
                " border: none !important;"
                " padding: 0px 0px 0px 0px !important;"
                " margin: 0px 0px 0px -4px !important;"
                "}"
                "QToolBar#leftSideBar::handle {"
                " background-color: #eff0f1 !important;"
                " image: none !important;"
                " width: 0px !important;"
                "}"
            );

            // Обнуляем внутренние зазоры встроенного макета панели
            if (wrapperBar->layout()) {
                wrapperBar->layout()->setContentsMargins(0, 0, 0, 0);
                wrapperBar->layout()->setSpacing(0);
            }

            wrapperBar->setMinimumWidth(68);
            wrapperBar->setMaximumWidth(68);
            wrapperBar->setFixedWidth(68);

            wrapperBar->addWidget(leftSideBarContainer);
            this->addToolBar(Qt::LeftToolBarArea, wrapperBar);
            wrapperBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        }
    }

    if (oldCentral && oldCentral->layout())
    {
        oldCentral->layout()->setContentsMargins(0, 0, 0, 0);
        oldCentral->layout()->setSpacing(0);
    }


    // Дополнительно обнуляем поля самого сплиттера
    if (mainVerticalSplitter->layout())
    {
        mainVerticalSplitter->layout()->setContentsMargins(0, 0, 0, 0);
    }

    ui->search_panel->hide(); // Альтернатива: search->setVisible(false);

    // 1. Инициализируем глобальную переменную класса (без QPushButton* в начале!)
    btnTerminal = new QPushButton("💻 Терминал", this);
    btnSearch = new QPushButton("🔍 Поиск по коду", this);
    btnLogs = new QPushButton("📋 Панель быстрых команд", this);
    btnTogglePip = new QPushButton("🛠 Управление пакетами", this);
    btnAIChat = new QPushButton("💬 ИИ-Ассистент", this);
    btnStartDebug = new QPushButton("Запуск Debug", this);

    // Настраиваем окно вывода чата
    // ui->chatLogWidget->setReadOnly(true);
    // ui->chatLogWidget->setOpenLinks(false); // Чтобы клики обрабатывались программно

    // // Привязываем кнопку отправки и клики по ссылкам-кнопкам
    // connect(ui->chatLogWidget, &QTextBrowser::anchorClicked, this, &Neuro_programm::onChatAnchorClicked);
    QTextBrowser *safeChatLogPage24 = ui->centralwidget->findChild<QTextBrowser*>("chatLogWidget");
    if (safeChatLogPage24 != nullptr) {
        safeChatLogPage24->setReadOnly(true);
        safeChatLogPage24->setOpenLinks(false);
        connect(safeChatLogPage24, &QTextBrowser::anchorClicked, this, &Neuro_programm::onChatAnchorClicked);
    }

    connect(ui->btnSendChat, &QPushButton::clicked, this, &Neuro_programm::sendChatMessageToAI);

    //=======================================================================
    //                 ПРОГРАММИРОВАНИЕ STATUSBAR
    //=======================================================================

    btnTerminal->setCheckable(true);
    btnSearch->setCheckable(true);
    btnLogs->setCheckable(true);
    btnTogglePip->setCheckable(true);
    btnAIChat->setCheckable(true);
    btnStartDebug->setCheckable(true);

    // Изначально при старте приложения нижняя панель закрыта, кнопки отжаты
    btnTerminal->setChecked(false);
    btnSearch->setChecked(false);
    btnLogs->setChecked(false);
    btnTogglePip->setChecked(false);
    btnStartDebug->setChecked(false);

    btnTerminal->setAutoExclusive(false);
    btnTerminal->setStyle(new StatusButtonStyle(btnTerminal->style()));

    btnTerminal->setStyleSheet("QPushButton { border: none; padding: 4px 12px; color: #ffffff; font-weight: bold; }");

    // 2. Инициализируем левый усекаемый лог ошибок Jedi
    statusLogLabel = new ElidedLabel(this);
    statusLogLabel->setObjectName("statusLogLabel");
    statusLogLabel->setStyleSheet("color: #ef5350; font-weight: bold;");
    statusLogLabel->setMaximumWidth(400); // Ограничиваем максимальную ширину
    statusLogLabel->setFullText("Jedi: Готов к работе");

    // 3. Создаем расширяющуюся распорку-пружину по центру
    QWidget *leftSpacer = new QWidget(this);
        leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);


    btnTerminal->setMinimumSize(QSize(60, 25));
    btnSearch->setMinimumSize(QSize(60, 25));
    btnLogs->setMinimumSize(QSize(60, 25));
    btnAIChat->setMinimumSize(QSize(60, 25));
    btnStartDebug->setMinimumSize(QSize(60, 25));
    btnTogglePip->setMinimumSize(QSize(60, 25));

    // 5. Укладываем ВСЕ элементы в макет СЛЕВА НАПРАВО (в нормальном порядке!)
    ui->statusbar->addWidget(statusLogLabel, 0); // Самый левый элемент (Лог Jedi)
    ui->statusbar->addWidget(leftSpacer, 0);

    ui->statusbar->addPermanentWidget(btnTerminal, 0);
    ui->statusbar->addPermanentWidget(btnSearch, 0);
    ui->statusbar->addPermanentWidget(btnLogs, 0);
    ui->statusbar->addPermanentWidget(btnAIChat, 0);
    ui->statusbar->addPermanentWidget(btnStartDebug, 0);
    ui->statusbar->addPermanentWidget(btnTogglePip, 0);

    statusLogLabel->show();
    btnTerminal->show();
    btnSearch->show();
    btnLogs->show();
    btnAIChat->show();
    btnStartDebug->show();
    btnTogglePip->show();

    connect(btnStartDebug, &QPushButton::clicked, this, [this]() {
        // 1. Считываем настройки и пути
        QSettings settings;
        QString venvPath = settings.value("python/venv_path", "").toString();
        QString scriptPath = currentOpenProjectPath + "/test_debug.py";

#if defined(Q_OS_WIN)
        QString pythonExec = venvPath + "/Scripts/python.exe";
#else
        QString pythonExec = venvPath + "/bin/python";
#endif

        // 2. Настраиваем логи вывода в левую консоль панели panelOther
        connect(debuggedScriptProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            if (panelOther) {
                panelOther->appendLogText(QString::fromUtf8(debuggedScriptProcess->readAllStandardOutput()));
            }
        });

        connect(debuggedScriptProcess, &QProcess::readyReadStandardError, this, [this]() {
            if (panelOther) {
                panelOther->appendLogText(QString::fromUtf8(debuggedScriptProcess->readAllStandardError()));
            }
        });

        // 3. Стартуем процесс
        QStringList args;
        args << scriptPath;
        debuggedScriptProcess->start(pythonExec, args);

        // 4. Запускаем таймер на подключение сокета отладки
        if (panelOther) {
            // 1. Отправляем ПЕРВУЮ строку в дебаг-консоль
            panelOther->appendDebugLog("⏳ Скрипт запущен. Ожидание инициализации порта...");
        }

        disconnect(debuggedScriptProcess, &QProcess::started, nullptr, nullptr);
        connect(debuggedScriptProcess, &QProcess::started, this, [this]() {
            if (panelOther) {
                panelOther->appendDebugLog("🔌 Запуск автоматического поиска порта отладки...");
                panelOther->connectToDebugger(); // Сокет сам найдет порт, как только тот откроется!
            }
        });

        QTimer::singleShot(1500, this, [this]() {
            if (panelOther) {
                panelOther->appendLogText("🔌 Подключаем Консоль отладки C++ к Python...");
                panelOther->connectToDebugger();
            }
        });
    });

    // =========================================================================
    // ИСПРАВЛЕННАЯ СИСТЕМА КОННЕКТОВ СТАТУСБАРА (ЕДИНАЯ ЛОГИКА ИСКЛЮЧЕНИЯ)
    // =========================================================================

    // Вспомогательная лямбда-функция для сброса ВСЕХ кнопок в неактивное состояние
    auto resetAllStatusButtons = [this]() {
        btnTerminal->blockSignals(true);   btnTerminal->setChecked(false);   btnTerminal->blockSignals(false);
        btnSearch->blockSignals(true);     btnSearch->setChecked(false);     btnSearch->blockSignals(false);
        btnLogs->blockSignals(true);       btnLogs->setChecked(false);       btnLogs->blockSignals(false);
        btnAIChat->blockSignals(true);     btnAIChat->setChecked(false);     btnAIChat->blockSignals(false);
        btnTogglePip->blockSignals(true);  btnTogglePip->setChecked(false);  btnTogglePip->blockSignals(false);
    };

    // 1. Управление Терминалом (Используем строго toggled, убираем clicked-конфликт!)
    connect(btnTerminal, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnTerminal->blockSignals(true); btnTerminal->setChecked(true); btnTerminal->blockSignals(false);

            if (ui->search_panel) ui->search_panel->setVisible(false);
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);

            if (panelOther) {
                panelOther->setVisible(true);
                panelOther->setActivePage(panel_other::PageTerminal); // Включаем страницу терминала
            }
        } else {
            if (panelOther) panelOther->setVisible(false);
        }
    });

    // 2. Управление Поиском по коду
    connect(btnSearch, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnSearch->blockSignals(true); btnSearch->setChecked(true); btnSearch->blockSignals(false);

            if (panelOther) panelOther->setVisible(false);
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);

            if (ui->search_panel) ui->search_panel->setVisible(true);
        } else {
            if (ui->search_panel) ui->search_panel->setVisible(false);
        }
    });

    // 3. Управление Быстрыми командами ИИ
    connect(btnLogs, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnLogs->blockSignals(true); btnLogs->setChecked(true); btnLogs->blockSignals(false);

            if (panelOther) panelOther->setVisible(false);
            if (ui->search_panel) ui->search_panel->setVisible(false);

            if (ui->quickActionsList) ui->quickActionsList->setVisible(true);
            if (ui->mainHorizontalSplitter) ui->mainHorizontalSplitter->refresh();
        } else {
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);
        }
    });

    // 4. Управление Менеджером пакетов PIP
    connect(btnTogglePip, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnTogglePip->blockSignals(true); btnTogglePip->setChecked(true); btnTogglePip->blockSignals(false);

            if (ui->search_panel) ui->search_panel->setVisible(false);
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);

            if (panelOther) {
                panelOther->setVisible(true);
                panelOther->setActivePage(panel_other::PagePipTable); // Переключаем на таблицу PIP
            }
        } else {
            if (panelOther) panelOther->setVisible(false);
        }
    });

    // 5. Управление ИИ-Ассистентом (Чат)
    connect(btnAIChat, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnAIChat->blockSignals(true); btnAIChat->setChecked(true); btnAIChat->blockSignals(false);

            if (panelOther) panelOther->setVisible(false);
            if (ui->search_panel) ui->search_panel->setVisible(false);
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);

            int realChatStackIndex = ui->centralStackedWidget->indexOf(ui->page_chat);
            if (realChatStackIndex != -1) {
                this->setIDEInStartMode(false);
                ui->centralStackedWidget->setCurrentIndex(realChatStackIndex);

                int comboIdx = ui->fileComboBox->findData("AI_CHAT_SCREEN");
                if (comboIdx != -1) {
                    ui->fileComboBox->blockSignals(true);
                    ui->fileComboBox->setCurrentIndex(comboIdx);
                    ui->fileComboBox->blockSignals(false);
                }

                QTextEdit *chatInput = ui->page_chat->findChild<QTextEdit*>("inputChatText");
                if (chatInput) {
                    chatInput->setEnabled(true);
                    chatInput->setFocus();
                }
            }
        } else {
            if (this->currentOpenProjectPath.isEmpty()) {
                this->setIDEInStartMode(true);
            } else {
                ui->centralStackedWidget->setCurrentIndex(0);
                int comboIdx = ui->fileComboBox->findData("MAIN_SCREEN");
                if (comboIdx != -1) ui->fileComboBox->setCurrentIndex(comboIdx);
                btnTerminal->setChecked(true); // Возвращаем фокус на терминал
            }
        }
    });


    // Изначально скрываем список при запуске PyTorch Studio
    if (ui->quickActionsList) {
        ui->quickActionsList->hide();
    }

    // Полностью независимый коннект для btnTerminal
    connect(btnTerminal, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            // Кнопка нажата -> Показываем панель и активируем вкладку терминала
            panelOther->setVisible(true);
            panelOther->setTerminalPageActive();

            if (mainVerticalSplitter) {
                mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
            }
        }
        else {
            // Кнопка отжата -> Просто скрываем панель
            panelOther->setVisible(false);
        }

        // Принудительное обновление графического движка Qt для перерисовки QSS стиля
        btnTerminal->style()->unpolish(btnTerminal);
        btnTerminal->style()->polish(btnTerminal);
        btnTerminal->update();
    });

    connect(btnSearch, &QPushButton::clicked, this, [this]() {
        if (btnTogglePip) btnTogglePip->setChecked(false);
    });

    connect(btnLogs, &QPushButton::clicked, this, [this]() {
        if (btnTogglePip) btnTogglePip->setChecked(false);
    });

    projectModel = nullptr;

    ui->treeView->setIndentation(20);

    //connect(ui->New_progect, &QAction::triggered, this, &Neuro_programm::new_progect);
    connect(ui->aboutProgram, &QAction::triggered, this, &Neuro_programm::open_about_program);
    //connect(ui->open_progect, &QAction::triggered, this, &Neuro_programm::onOpenProjectMenuTriggered);
    connect(save_progect_all, &QAction::triggered, this, &Neuro_programm::onSaveProjectMenuTriggered);

    // Очищаем комбобокс и стэк-виджет от тестовых данных из Designer
    // ui->fileComboBox->clear();

    // ui->fileComboBox->setCurrentIndex(0);

    // // =========================================================================
    // // МОНТАЖ JETBRAINS PLACEHOLDER НА СТАРТОВЫЙ ЭКРАН
    // // =========================================================================
    // // Создаем заставку и укладываем её самой первой в центральный стек виджетов
    // EditorPlaceholder *placeholderScreen = new EditorPlaceholder(ui->centralStackedWidget);
    // ui->centralStackedWidget->insertWidget(0, placeholderScreen);

    // // По умолчанию при старте показываем именно этот красивый сплэш-экран
    // ui->centralStackedWidget->setCurrentIndex(0);

    // // 2. Дублируем синхронизацию для левой боковой панели документов
    // if (ui->openFilesListWidget) {
    //     ui->openFilesListWidget->setCurrentRow(0);
    // }

    // ui->fileComboBox->addItem("🎛 Панель обучения ИИ", QVariant("MAIN_SCREEN"));
    // ui->fileComboBox->addItem("💬 ИИ-Ассистент", QVariant("AI_CHAT_SCREEN"));

    // while (ui->centralStackedWidget->count() > 2)
    // {
    //     QWidget *w = ui->centralStackedWidget->widget(2);
    //     ui->centralStackedWidget->removeWidget(w);
    //     delete w;
    // }

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm (Взамен старого монтажа) ---
    ui->fileComboBox->clear();

    // 1. Создаем заставку шорткатов
    EditorPlaceholder *placeholderScreen = new EditorPlaceholder(ui->centralStackedWidget);
    placeholderScreen->setObjectName("JETBRAINS_PLACEHOLDER");

    // 2. ЖЕСТКИЙ ФИКС ИНДЕКСОВ: Добавляем виджет в КОНЕЦ стека через addWidget вместо insertWidget(0)
    // Теперь "Панель обучения ИИ" железно останется под индексом 0, а чат — под своим родным индексом!
    int placeholderIndex = ui->centralStackedWidget->addWidget(placeholderScreen);

    // Сохраняем индекс заставки в динамических свойствах главного окна, чтобы легко находить её
    this->setProperty("placeholderIndex", placeholderIndex);

    // Заполняем комбобокс базовыми экранами (Панель ИИ = 0, Чат = 1)
    ui->fileComboBox->addItem("  Панель обучения ИИ", QVariant("MAIN_SCREEN"));
    ui->fileComboBox->addItem("  ИИ-Ассистент", QVariant("AI_CHAT_SCREEN"));

    // При самом старте, если проект пуст, включаем заставку (которая теперь лежит в конце)
    if (this->currentOpenProjectPath.isEmpty()) {
        ui->centralStackedWidget->setCurrentIndex(placeholderIndex);
    } else {
        ui->fileComboBox->setCurrentIndex(0);
    }

    // 1. Коннект двойного щелчка по дереву файлов
    connect(ui->treeView, &QTreeView::doubleClicked, this, &Neuro_programm::onFileDoubleClicked);

    // // 2. Коннект смены элемента в комбобоксе (переключение файлов пользователем)
    // // Используем современную сигнатуру для переключения индексов stackedWidget
    // connect(ui->fileComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
    //     if (index >= 0 && index < ui->centralStackedWidget->count())
    //     {
    //         // Перелистываем страницу стэка
    //         ui->centralStackedWidget->setCurrentIndex(index);

    //         // Синхронизируем выделение строки в левом боковом списке открытых документов
    //         if (ui->openFilesListWidget) {
    //             ui->openFilesListWidget->setCurrentRow(index);
    //         }

    //         // Умное управление док-виджетами и кнопками на основе ключа userData
    //         QString currentKey = ui->fileComboBox->itemData(index).toString();
    //         if (currentKey == "MAIN_SCREEN")
    //         {
    //             updateCustomTitle("");
    //             if (btnTerminal) {
    //                 btnTerminal->setChecked(true);
    //                 btnTerminal->setProperty("active", true); // Добавить эту строчку!
    //                 btnTerminal->style()->unpolish(btnTerminal);
    //                 btnTerminal->style()->polish(btnTerminal);
    //             }
    //             if (btnAIChat) btnAIChat->setChecked(false);
    //         }
    //         else if (currentKey == "AI_CHAT_SCREEN")
    //         {
    //             updateCustomTitle("");
    //             //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(false); // Прячем док
    //             if (btnAIChat)   btnAIChat->setChecked(true);    // Зажигаем Чат в статусбаре
    //             if (btnTerminal) btnTerminal->setChecked(false); // Тушим Терминал
    //         }
    //         else
    //         {
    //             QString fileName = ui->fileComboBox->itemText(index);
    //             fileName.remove(" *"); // Убираем маркер несохраненных изменений со звездочкой
    //             updateCustomTitle(fileName);
    //             // Если выбран любой динамический файл кода (индексы >= 2)
    //             //if (ui->rightDockWidget) ui->rightDockWidget->setVisible(false);
    //             if (btnTerminal) btnTerminal->setChecked(false);
    //             if (btnAIChat)   btnAIChat->setChecked(false);
    //         }
    //     }
    // });

    // 3. Коннект кнопки закрытия текущего файла
    connect(ui->btnCloseFile, &QPushButton::clicked, this, [this]()
            {
        qDebug() << ">>> [КЛИК] Запрос на закрытие текущей вкладки с кодом...";

        // Принудительно вызываем наш отлаженный метод закрытия!
        this->onCloseCurrentFileClicked();
    });

    connect(ui->fileComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0 || index >= ui->centralStackedWidget->count()) return;

        // Извлекаем сохраненное значение из метаданных строки
        QVariant dataVal = ui->fileComboBox->itemData(index);

        // =========================================================================
        // СИНХРОННЫЙ ФИКС: Если в Data лежит сохраненное число (индекс страницы),
        // мгновенно перелистываем centralStackedWidget на него!
        // =========================================================================
        if (dataVal.userType() == QMetaType::Int) {
            int targetStackIndex = dataVal.toInt();
            if (targetStackIndex >= 0 && targetStackIndex < ui->centralStackedWidget->count()) {
                ui->centralStackedWidget->setCurrentIndex(targetStackIndex);
            }
        } else {
            // Для сервисных экранов (MAIN_SCREEN / AI_CHAT_SCREEN) перелистываем по базовому индексу
            ui->centralStackedWidget->setCurrentIndex(index);
        }

        // Умное управление списками открытых документов на основе ключа userData
        QString currentKey = dataVal.toString();
        if (currentKey == "MAIN_SCREEN")
        {
            updateCustomTitle("");
            if (btnTerminal) {
                btnTerminal->setChecked(true);
                btnTerminal->setProperty("active", true);
                btnTerminal->style()->unpolish(btnTerminal);
                btnTerminal->style()->polish(btnTerminal);
            }
            if (btnAIChat) btnAIChat->setChecked(false);

            if (ui->openFilesListWidget && ui->leftVerticalSplitter) {
                ui->openFilesListWidget->setVisible(false);
                ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
            }
        }
        else if (currentKey == "AI_CHAT_SCREEN")
        {
            updateCustomTitle("");
            if (btnAIChat) btnAIChat->setChecked(true);
            if (btnTerminal) btnTerminal->setChecked(false);

            if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                ui->openFilesContainer->setVisible(false);
                ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
            }
        }
        // НА СТРАНИЦЕ 21-22 (Внутри connect для ui->fileComboBox -> ветка else):
        else
        {
            // --- ЕСЛИ ВЫБРАН РЕАЛЬНЫЙ ФАЙЛ КОДА (ИНДЕКСЫ >= 2) ---
            QWidget *currentPage = ui->centralStackedWidget->widget(ui->centralStackedWidget->currentIndex());
            if (currentPage) {
                QFileInfo fileInfo(currentPage->objectName());
                updateCustomTitle(fileInfo.fileName());
            }

            if (btnTerminal) btnTerminal->setChecked(false);
            if (btnAIChat) btnAIChat->setChecked(false);

            if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                ui->openFilesContainer->setVisible(true);
                int totalHeight = ui->leftVerticalSplitter->height();
                if (totalHeight <= 0) totalHeight = 600;
                ui->leftVerticalSplitter->setSizes(QList<int>({totalHeight - 180, 180}));
                ui->leftVerticalSplitter->update();
            }

            // =========================================================================
            // АСИНХРОННЫЙ ЖЕСТКИЙ ФИКС ВЫТАЛКИВАНИЯ ЧЕРЕЗ ОДНОРАЗОВЫЙ ТАЙМЕР
            // =========================================================================
            QTimer::singleShot(50, this, [this]() {
                if (mainVerticalSplitter) {
                    int totalWindowHeight = this->height();
                    int bottomHeight = 0;

                    // Измеряем реальное состояние нижних панелей
                    if (panelOther && panelOther->isVisible()) {
                        bottomHeight = 250; // Высота терминала
                    } else if (ui->search_panel && ui->search_panel->isVisible()) {
                        bottomHeight = 150; // Высота панели поиска
                    }

                    // Жестко фиксируем пропорции сплиттера в границах экрана
                    mainVerticalSplitter->setSizes(QList<int>({totalWindowHeight - bottomHeight, bottomHeight}));

                    if (this->layout()) {
                        this->layout()->activate();
                    }
                }
            });
        }
    });

    // 1. Создаем кастомный виджет для шапки дока
    QWidget *customTitleWidget = new QWidget(ui->leftDockWidget);

    if (ui->leftDockWidget) {
        ui->leftDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
    }

    // 2. Создаем красивый текстовый заголовок
    QLabel *titleLabel1 = new QLabel("📁 Открытые файлы и проект", customTitleWidget);

    // ЗАЩИТА: Если метка не была найдена в Designer, создаем её сами прямо сейчас
    if (!titleLabel) {
        titleLabel = new QLabel("PyTorch Studio", ui->customTitleBarPanel);
        QWidget *completelyEmptyTitle = new QWidget(ui->leftDockWidget);
        ui->leftDockWidget->setTitleBarWidget(completelyEmptyTitle);
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

    // 2. ИЗНАЧАЛЬНО СКРЫВАЕМ НИЖНИЙ КОНТЕЙНЕР ПРИ ЗАПУСКЕ ПРОГРАММЫ
    if (ui->openFilesContainer) {
        ui->openFilesContainer->setVisible(false);
    }

    // =========================================================================
    // ТОТАЛЬНОЕ УНИЧТОЖЕНИЕ ПУСТОТЫ И НАСТРОЙКА ЛЕВОЙ ПАНЕЛИ НАВИГАЦИИ (ФИНАЛ)
    // =========================================================================
    if (ui->leftDockWidget)
    {
        // 1. Сбрасываем минимальные лимиты высоты из Designer (строка 11)
        ui->leftDockWidget->setMinimumSize(QSize(300, 0));
        ui->leftDockWidget->setMaximumSize(QSize(300, 524287));

        // 2. Убираем встроенную плашку заголовка QDockWidget
        if (ui->leftDockWidget) {
            // Способ 1: Отключаем все нативные фичи заголовка (он схлопнется сам)
            ui->leftDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);

            // Способ 2: Создаем действительно пустой виджет БЕЗ фиксации высоты
            // Qt сам поймет, что у него нулевой размер и уберет заголовок
            QWidget *emptyTitle = new QWidget(ui->leftDockWidget);
            ui->leftDockWidget->setTitleBarWidget(emptyTitle);

            // Способ 3: Срезаем внутренние отступы у контейнера дока через QSS
            ui->leftDockWidget->setStyleSheet(
                        "QDockWidget { background-color: #202225; border: none; padding: 0px; margin: 0px; }"
                        "QDockWidget::title { background-color: transparent; height: 0px; padding: 0px; margin: 0px; }"
                        );
        }

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
        this->setStyleSheet(this->styleSheet() +
                            "QDockWidget { border: none; padding: 0px; margin: 0px; }"
                            "QDockWidget::title { background: transparent; height: 0px; max-height: 0px; padding: 0px; margin: 0px; border: none; }"
                            );

        // Считываем подложку дока и принудительно зануляем её макет
        QWidget *dockContents = ui->leftDockWidget->widget();
        if (dockContents && dockContents->layout()) {
            dockContents->layout()->setContentsMargins(0, 0, 0, 0);
            dockContents->layout()->setSpacing(0);
        }

        this->setStyleSheet(this->styleSheet() +
                            "QMainWindow::separator { background: transparent; width: 0px; height: 0px; margin: 0px; padding: 0px; }"
                            "QMainWindow::separator:hover { background: transparent; width: 0px; height: 0px; }"
                            );

        if (ui->leftDockWidget) {
            // Говорим окну, что разделители вокруг этого дока не должны иметь кастомных отступов
            ui->leftDockWidget->setContentsMargins(0, 0, 0, 0);
        }

        this->layout()->invalidate();
        this->layout()->activate();
    }


    // =========================================================================
    // ХИРУРГИЧЕСКИЙ ФИКС НАЛОЖЕНИЯ (КАШИ) НА ПЕРВОЙ СТРАНИЦЕ ДОКА
    // =========================================================================
    if (ui->leftVerticalSplitter)
    {
        // 1. Извлекаем живые указатели на ваши списки из Designer, чтобы они не потерялись
        QTreeView *currentTreeView = ui->treeView;
        QListWidget *currentListWidget = ui->openFilesListWidget;

        // 2. Полностью зануляем старые слои и сетки, которые были настроены в XML
        if (ui->projectContainer) {
            if (ui->projectContainer->layout()) delete ui->projectContainer->layout();

            // Создаем чистый вертикальный контейнер для верхнего блока
            QVBoxLayout *projectLayout = new QVBoxLayout(ui->projectContainer);
            projectLayout->setContentsMargins(6, 4, 6, 4);
            projectLayout->setSpacing(4);

            QLabel *lblProjectTitle = new QLabel(" Структура проекта", ui->projectContainer);
            lblProjectTitle->setStyleSheet("font-weight: bold; color: #505050; font-size: 11px;");
            lblProjectTitle->setFixedHeight(16);

            // Укладываем строго сверху вниз: сначала заголовок, потом дерево
            projectLayout->addWidget(lblProjectTitle);
            if (currentTreeView) {
                projectLayout->addWidget(currentTreeView);
            }
        }

        if (ui->openFilesContainer) {
            if (ui->openFilesContainer->layout()) delete ui->openFilesContainer->layout();

            // Создаем чистый вертикальный контейнер для нижнего блока
            QVBoxLayout *openFilesLayout = new QVBoxLayout(ui->openFilesContainer);
            openFilesLayout->setContentsMargins(6, 4, 6, 4);
            openFilesLayout->setSpacing(4);

            QLabel *lblFilesTitle = new QLabel(" Открытые документы", ui->openFilesContainer);
            lblFilesTitle->setStyleSheet("font-weight: bold; color: #505050; font-size: 11px;");
            lblFilesTitle->setFixedHeight(16);

            // Укладываем строго сверху вниз: заголовок, затем список файлов
            openFilesLayout->addWidget(lblFilesTitle);
            if (currentListWidget) {
                openFilesLayout->addWidget(currentListWidget);
            }
        }

        // 3. Пересобираем иерархию самого сплиттера, чтобы зафиксировать элементы
        ui->leftVerticalSplitter->setHandleWidth(1); // Тонкий красивый разделитель

        if (ui->projectContainer) ui->leftVerticalSplitter->addWidget(ui->projectContainer);
        if (ui->openFilesContainer) ui->leftVerticalSplitter->addWidget(ui->openFilesContainer);

        // Задаем пропорции (верхнее дерево занимает все место, нижний список скрыт при старте)
        ui->leftVerticalSplitter->setStretchFactor(0, 1);
        ui->leftVerticalSplitter->setStretchFactor(1, 1);
        ui->leftVerticalSplitter->setCollapsible(0, false);
        ui->leftVerticalSplitter->setCollapsible(1, true);

        ui->openFilesContainer->setVisible(false);
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
    QTextEdit *chatInputWidget = ui->centralwidget->findChild<QTextEdit*>("inputChatText");

    if (chatInputWidget != nullptr) {
        // Если виджет успешно найден в памяти — безопасно вешаем фильтр Ctrl+Enter
        chatInputWidget->installEventFilter(this);
        qDebug() << ">>> [ЧАТ] Фильтр клавиатуры для inputChatText успешно активирован.";
    }
    else if (ui->inputChatText != nullptr) {
        // Резервный случай, если Designer успел его прогрузить стандартно
        // ui->inputChatText->installEventFilter(this);
    }
    else {
        // Защитный лог на случай, если вы случайно переименовали objectName в UI-формах
        qWarning() << "⚠️ Внимание: Текстовое поле ввода чата 'inputChatText' еще не прогружено Qt. Краш заблокирован!";
    }

    if (aiPanel && aiPanel->comboBatchSize)
    {
        aiPanel->comboBatchSize->clear();
        aiPanel->comboBatchSize->addItems(QStringList() << "4" << "8" << "16" << "32" << "64" << "128" << "256");

    }
   // wf->ui->comboBatchSize->clear(); // Коротко, безопасно и профессионально!

   // ui->comboBatchSize->addItems(QStringList() << "4" << "8" << "16" << "32" << "64" << "128" << "256");

    // --- ЕЖЕСЕКУНДНЫЙ ТАЙМЕР МОНИТОРИНГА НАГРУЗКИ ЖЕЛЕЗА ---
    monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this]() {

        if (this->monitorTimer && !this->monitorTimer->isActive()) {
            return;
        }
        // БЕЗОПАСНЫЙ ПЕРЕХВАТ УКАЗАТЕЛЕЙ НА УРОВНЕ ЯДРА QT
        // Создаем умные указатели, которые сами превратятся в nullptr, если страницы удалят
        QPointer<QProgressBar> safeCpuBar = aiPanel ? aiPanel->progressCPU : nullptr;
        QPointer<QProgressBar> safeGpuBar = aiPanel ? aiPanel->progressGPU : nullptr;
        QPointer<QComboBox> safeCombo = (aiPanel && aiPanel->ui) ? aiPanel->ui->comboDevice_2 : nullptr;

        // Если страницы в процессе удаления, QPointer мгновенно станет nullptr
        if (!safeGpuBar || !safeCpuBar) {
            return; // Файлы сейчас распаковываются, виджеты уничтожены — немедленно выходим!
        }

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

            static const QRegularExpression spacesRegex("\\s+");
            QStringList tokens = line.split(spacesRegex, Qt::SkipEmptyParts);
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

                if (totalDiff > 0)
                {
                    int cpuPercentage = static_cast<int>(100.0 * (totalDiff - idleDiff) / totalDiff);


                    if (aiPanel && aiPanel->ui && aiPanel->ui->progressGPU)
                    {
                        QMetaObject::invokeMethod(aiPanel->ui->progressGPU, "setValue",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(int, cpuPercentage));
                    }
                }


                oldUser = user; oldNice = nice; oldSystem = system; oldIdle = idle;
            }
        }

        // =========================================================================
        // 2. РАСЧЕТ ЗАГРУЗКИ GPU (ЧЕРЕЗ КИШКИ ДРАЙВЕРА NVIDIA) — ПОТОКОБЕЗОПАСНЫЙ
        // =========================================================================
        if (ui)
        {
            // Безопасно проверяем режим вычислительного устройства
            bool isCpuMode = true;
            if (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2)
            {
                isCpuMode = (aiPanel->ui->comboDevice_2->currentText().toLower().contains("cpu"));
            }

            if (isCpuMode)
            {
                if (aiPanel && aiPanel->progressGPU)
                {
                    QMetaObject::invokeMethod(aiPanel->progressGPU, "setValue",
                                              Qt::QueuedConnection,
                                              Q_ARG(int, 0));
                }
            }
            // if (isCpuMode) {
            //     // Проверяем существование виджета перед отправкой события
            //     if (ui->progressGPU) {
            //         QMetaObject::invokeMethod(ui->progressGPU, "setValue",
            //                                   Qt::QueuedConnection,
            //                                   Q_ARG(int, 0));
            //     }
            // }
            else
            {
                // Если выбран режим CUDA — быстро и асинхронно спрашиваем загрузку ядер у видеодрайвера
                QProcess query;
                query.start("nvidia-smi", QStringList() << "--query-gpu=utilization.gpu" << "--format=csv,noheader,nounits");

                int targetGpuPercentage = 0;

                // Даем утилите nvidia-smi 100мс на ответ
                if (query.waitForFinished(100) && query.exitCode() == 0) {
                    targetGpuPercentage = QString::fromUtf8(query.readAllStandardOutput()).trimmed().toInt();
                } else {
                    // Если nvidia-smi выдала ошибку или не установлена, оставляем 0
                    targetGpuPercentage = 0;
                }

                // ПОТОКОБЕЗОПАСНАЯ ОТПРАВКА ЗНАЧЕНИЯ В ИНТЕРФЕЙС
                // Если во время работы команды пользователь закроет/переключит проект,
                // это событие просто безопасно сгорит в очереди главного потока, не роняя ОС.
                if (aiPanel && aiPanel->progressGPU)
                {
                    QMetaObject::invokeMethod(aiPanel->progressGPU, "setValue",
                                              Qt::QueuedConnection,
                                              Q_ARG(int, targetGpuPercentage));
                }
            }
        }
    });

    // Принудительно выставляем элемент "32" активным по умолчанию при старте программы
    if (aiPanel && aiPanel->comboBatchSize)
    {
        aiPanel->comboBatchSize->setCurrentText("32");
    }

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
    //initLossChart();

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm::Neuro_programm в файле neuro_programm.cpp ---
    recentProjectsMenu = new QMenu("📁 Открыть недавние", this);

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

    // Внутри конструктора Neuro_programm в файле neuro_programm.cpp
    connect(this, &Neuro_programm::completionDataReceived,
            this, &Neuro_programm::showCompletionMenuInGuiThread);

    QTextBrowser *safeChatLog = ui->centralwidget->findChild<QTextBrowser*>("chatLogWidget");

    if (safeChatLog != nullptr) {
        // Если виджет успешно найден в памяти — безопасно настраиваем его
        safeChatLog->setReadOnly(true);
        safeChatLog->setOpenLinks(false);
        qDebug() << ">>> [ЧАТ] Окно логов chatLogWidget успешно инициализировано.";
    }
    else if (ui->chatLogWidget != nullptr) {
        // Резервный случай, если Designer прогрузил его стандартно
        //ui->chatLogWidget->setReadOnly(true);
        //ui->chatLogWidget->setOpenLinks(false);
    }
    else {
        // Полностью блокируем падение, если страница чата еще спит в памяти
        qWarning() << "⚠️ Внимание: Виджет 'chatLogWidget' еще не создан Qt. Краш успешно заблокирован!";
    }

    //ui->chatLogWidget->setOpenLinks(false); // Отключаем открытие ссылок в браузере
    //connect(ui->chatLogWidget, &QTextBrowser::anchorClicked, this, &Neuro_programm::onChatAnchorClicked);
    connect(ui->quickActionsList, &QListWidget::itemDoubleClicked, this, &Neuro_programm::onQuickActionTriggered);
    connect(ui->quickActionsList, &QListWidget::itemDoubleClicked, this, &Neuro_programm::onQuickActionTriggered);

    const auto comboList = this->findChildren<QComboBox*>();
    for (QComboBox *combo : std::as_const(comboList)) {
        if (combo) combo->setStyle(new BreezeFlatStyle(combo->style()));
    }


    const auto spinBoxes = this->findChildren<QSpinBox*>();
    for (QSpinBox *spin : std::as_const(spinBoxes)) {
        if (spin) spin->setStyle(new BreezeFlatStyle(spin->style()));
    }

    const auto dSpinBoxes = this->findChildren<QDoubleSpinBox*>();
    for (QDoubleSpinBox *dSpin : std::as_const(dSpinBoxes)) {
        if (dSpin) dSpin->setStyle(new BreezeFlatStyle(dSpin->style()));
    }

    sendInitialWelcomeRequest();

    this->applyGlobalFonts();


    // Находим дизайнерский стек виджетов
    //QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");

    if (dockStack) {
        if (currentOpenProjectPath.isEmpty()) {
            // 1. Аппаратно переключаем стек на темную страницу PyCharm
            dockStack->setCurrentIndex(1);

            // 2. ЖЕСТКИЙ ФИКС: Принудительно заставляем док обновить геометрию под эту страницу
            ui->leftDockWidget->setVisible(true);

            // 3. Синхронизируем состояние кнопки на левой панели управления
            if (actProject) {
                actProject->setChecked(true);
            }
            qInfo() << "[INIT] Проект пуст. Аппаратно разворачиваем стартовый экран PyCharm.";
        }
        else {
            // Если проект уже был автоматически подгружен из кэша IDE
            dockStack->setCurrentIndex(0); // Включаем дерево файлов
            ui->leftDockWidget->setVisible(true);
            if (actProject) actProject->setChecked(true);
            qInfo() << "[INIT] Обнаружен активный проект. Отображаем дерево файлов.";
        }

        // Вызываем принудительный перерасчет слоев Qt, чтобы убрать белую пустоту
        dockStack->update();
        ui->leftDockWidget->update();
    }


    // =========================================================================
    // САМЫЙ КОНЕЦ КОНСТРУКТОРА NEURO_PROGRAMM (Вне любых лямбд и коннектов!)
    // =========================================================================

    // 1. Создаем временный «родной» менюбар, чтобы инициализировать геометрию окна
    QMenuBar *fakeMenuBar = new QMenuBar(this);
    this->setMenuBar(fakeMenuBar);

    // 2. Намертво скрываем его и обнуляем все его отступы
    fakeMenuBar->hide();
    fakeMenuBar->setFixedHeight(0);
    fakeMenuBar->setContentsMargins(0, 0, 0, 0);

    // 3. ЖЕСТКО ВЫРЕЗАЕМ ЕГО ИЗ МАКЕТА ОКНА
    // Метод setMenuWidget(nullptr) заставит QMainWindow полностью схлопнуть
    // ту самую техническую щель, которая осталась в .ui файле
    this->setMenuWidget(nullptr);
    this->setMenuBar(nullptr);

    // 1. Принудительно отключаем встроенные системные отступы стилей ОС
    this->setStyle(QStyleFactory::create("Fusion")); // Переключаем на чистый кроссплатформенный стиль Fusion

    // 2. Жестко сбрасываем геометрию макета окна, затирая дефолтные margins
    if (this->layout()) {
        this->layout()->setContentsMargins(1, 1, 1, 1);
        this->layout()->setSpacing(0);
    }
    // Дублируем отступы для самого окна
    this->setContentsMargins(1, 1, 1, 1);

    // 3. Выставляем нулевые отступы для всех ключевых компонентов интерфейса
    if (ui && ui->centralwidget) {
        ui->centralwidget->setContentsMargins(0, 0, 0, 0);
        if (ui->centralwidget->layout()) {
            ui->centralwidget->layout()->setContentsMargins(0, 0, 0, 0);
            ui->centralwidget->layout()->setSpacing(0);
        }
    }

    // 1. Срезаем внутренние отступы у самой панели, в которой лежат кнопки
    if (ui && ui->customTitleBarPanel) {
        ui->customTitleBarPanel->setContentsMargins(0, 0, 0, 0);

        // Сбрасываем отступы макета внутри этой панели
        if (ui->customTitleBarPanel->layout()) {
            ui->customTitleBarPanel->layout()->setContentsMargins(0, 0, 0, 0);
            ui->customTitleBarPanel->layout()->setSpacing(2); // небольшой зазор между самими кнопками
        }
    }

    if (ui->customTitleBarPanel) ui->customTitleBarPanel->installEventFilter(this);
    if (topWrapper)              topWrapper->installEventFilter(this);
    if (topContainerBar)         topContainerBar->installEventFilter(this);

    // Ищем созданную на Странице 8 надпись и тоже вешаем на неё фильтр
    QLabel *createdTitleLabel = ui->customTitleBarPanel->findChild<QLabel*>();
    if (createdTitleLabel)
    {
        createdTitleLabel->setMouseTracking(true);
        createdTitleLabel->installEventFilter(this);
    }

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView, &QTreeView::customContextMenuRequested,
            this, &Neuro_programm::showTreeViewContextMenu);
    onTorchCacheProcessFinished();
    connect(ui->search_panel, &Search::searchParametersChanged, this, &Neuro_programm::updateCodeSearch);

    // Внутри конструктора neuro_program::neuro_program
    connect(ui->search_panel, &Search::findNextRequested, this, &Neuro_programm::onFindNext);
    connect(ui->search_panel, &Search::findPrevRequested, this, &Neuro_programm::onFindPrev);
    connect(ui->search_panel, &Search::selectAllRequested, this, &Neuro_programm::onSelectAll);

    // Добавить в конструктор Neuro_programm (neuro_programm.cpp)
    connect(ui->search_panel, &Search::replaceCurrentRequested, this, &Neuro_programm::onReplaceCurrent);
    connect(ui->search_panel, &Search::replaceAndFindNextRequested, this, &Neuro_programm::onReplaceAndFindNext);
    connect(ui->search_panel, &Search::replaceAllRequested, this, &Neuro_programm::onReplaceAll);
    connect(ui->btnNewProgect, &QPushButton::clicked, this, &Neuro_programm::new_progect);
    connect(ui->btnOpenProgect, &QPushButton::clicked, this, &Neuro_programm::onOpenProjectMenuTriggered);

    // Внутри конструктора Neuro_programm::Neuro_programm:
    connect(ui->projectListWidget, &QListWidget::itemDoubleClicked,
            this, &Neuro_programm::loadProjectFromSettingsList);

    // Первично заполняем listWidget из конфигурационного файла при старте IDE
    this->updateProjectsListFromSettings();

    // =========================================================================
    // ОБЪЕДИНЕННЫЙ СТИЛЬ ДЛЯ projectListWidget (СИСТЕМНЫЙ СВЕТЛЫЙ + UX-ОТСТУПЫ)
    // =========================================================================
    ui->projectListWidget->setStyleSheet(
                "QListWidget {"
                "   background-color: #eff0f1;" // Системный светло-серый цвет окна Breeze
                "   color: #232629;"            // Читаемый темно-серый текст
                "   border: none;"              // Полностью убираем внешнюю рамку виджета
                "}"
                "QListWidget::item {"
                "   padding-top: 6px;"          // Отступы сверху и снизу для удобного клика мышью
                "   padding-bottom: 6px;"
                "   padding-left: 8px;"         // Комфортный отступ иконки проекта от левого края
                "   border-radius: 4px;"        // Красивое скругление углов при наведении и клике
                "}"
                "QListWidget::item:hover {"
                "   background-color: #e4e5e6;" // Мягкая подсветка строки при простом наведении мыши
                "}"
                "QListWidget::item:selected {"
                "   background-color: #93cee9;" // Ваш эталонный цвет выделенного проекта
                "   color: #232629;"            // Сохраняем контрастный темный текст в выделении
                "}"
                );

    QTimer::singleShot(100, this, &Neuro_programm::updateProjectsListFromSettings);

    if (this->currentOpenProjectPath.isEmpty())
    {
        // Принудительно гасим центральный виджет и оставляем только доквиджет проектов
        QTimer::singleShot(150, this, [this]()
                           {
            this->setIDEInStartMode(true);

            // =========================================================================
            // ЖЕЛЕЗНЫЙ UX ФИКС: Скрываем лейбл внутри таймера после полной отрисовки GUI
            // =========================================================================
            if (ui && ui->cursorPosLabel) {
                ui->cursorPosLabel->hide(); // Намертво прячем TextLabel при старте
            }
            // =========================================================================
        });
    }

    if (ui->widget_3)
    {
        ui->widget_3->setFixedHeight(25);
    }

    // --- В КОНЦЕ КОНСТРУКТОРА В neuro_programm.cpp ---
    // --- ВНУТРИ КОНСТРУКТОРА В neuro_programm.cpp ---
    connect(ui->comboDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index <= 0) return; // Пропускаем заголовок "Навигация по функциям..."

        // 1. Извлекаем номер строки, сохраненный в UserRole
        QVariant lineData = ui->comboDevice->itemData(index);
        if (!lineData.isValid()) return;
        int targetLine = lineData.toInt();

        // 2. ИСПРАВЛЕННЫЙ ПОИСК РЕДАКТОРА: Ищем строго на текущей активной странице стека
        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (!currentPage) return;

        // Сначала пробуем взять виджет напрямую, если страница и есть сам редактор
        CodeEditor *currentEditor = qobject_cast<CodeEditor*>(currentPage);

        // Если страница — это контейнер (как у вас в onFileDoubleClicked), ищем внутри неё
        if (!currentEditor) {
            currentEditor = currentPage->findChild<CodeEditor*>();
        }

        // 3. ЕСЛИ РЕДАКТОР НАЙДЕН — НАПРАВЛЯЕМ КАРЕТКУ И СКРОЛЛ НА СТРОКУ
        if (currentEditor) {
            qDebug() << ">>> [НАВИГАТОР СРАБОТАЛ] Перехожу на строку:" << targetLine;

            QTextCursor cursor = currentEditor->textCursor();
            QTextBlock block = currentEditor->document()->findBlockByLineNumber(targetLine);

            if (block.isValid()) {
                cursor.setPosition(block.position()); // Перемещаем позицию каретки под деф/класс
                currentEditor->setTextCursor(cursor); // Обновляем курсор на холсте

                // Смарт-фокус: плавно центрируем строку на экране и возвращаем фокус клавиатуры
                currentEditor->centerCursor();
                currentEditor->setFocus();
            }
        } else {
            qWarning() << ">>> [НАВИГАТОР ОШИБКА] Активный CodeEditor на текущей вкладке не найден!";
        }
    });

    if (this->currentOpenProjectPath.isEmpty())
    {
        QTimer::singleShot(150, this, [this]()
        {
            this->setIDEInStartMode(true);

            // ЖЕЛЕЗНЫЙ UX ФИКС: Пока ни один файл не открыт — кнопка закрытия заблокирована!
            if (ui->btnCloseFile)
            {
                ui->btnCloseFile->setEnabled(false);
            }
        });
    }

    if (this->currentOpenProjectPath.isEmpty()) {
        QTimer::singleShot(150, this, [this]() {
            this->setIDEInStartMode(true);

            // ЖЕЛЕЗНЫЙ UX ФИКС: Пока ни один файл не открыт — кнопка закрытия заблокирована!
            if (ui->btnCloseFile) {
                ui->btnCloseFile->setEnabled(false);
            }
        });
    }

    // =========================================================================
    // АБСОЛЮТНЫЙ ПЕРЕНОС: КНОПКИ В САМОМ ЛЕВОМ КРАЮ ОКНА (КОНЕЦ КОНСТРУКТОРА)
    // =========================================================================
    // Возвращаем ваш оригинальный вертикальный сплиттер в центральное гнездо
    if (mainVerticalSplitter)
    {
        this->setCentralWidget(mainVerticalSplitter);
    }

    // Монтируем панель кнопок через компактную обертку тулбара QMainWindow
    if (this->leftSideBarContainer)
    {
        QToolBar *wrapperBar = new QToolBar(this);
        wrapperBar->setObjectName("leftSideBar");
        wrapperBar->setMovable(false);
        wrapperBar->setFloatable(false);
        wrapperBar->setAllowedAreas(Qt::LeftToolBarArea);
        wrapperBar->setContentsMargins(0, 0, 0, 0);

        // ЖЕСТКИЙ ХАК CSS: Отрицательный margin полностью съедает скрытый системный
        // зазор стиля Fusion слева, выравнивая синий ховер идеально по краю экрана!
        wrapperBar->setStyleSheet(
            "QToolBar#leftSideBar {"
            " background-color: #202225 !important;"
            " border: none !important;"
            " padding: 0px 0px 0px 0px !important;"
            " margin: 0px 0px 0px -4px !important;" /* Сдвиг влево на 4px убирает щель стиля Fusion */
            "}"
        );

        // Обнуляем внутренние зазоры встроенного макета панели
        if (wrapperBar->layout()) {
            wrapperBar->layout()->setContentsMargins(0, 0, 0, 0);
            wrapperBar->layout()->setSpacing(0);
        }

        // Фиксируем габариты обертки строго под ширину кнопок
        wrapperBar->setMinimumWidth(68);
        wrapperBar->setMaximumWidth(68);
        wrapperBar->setFixedWidth(68);

        // Внедряем наш плоский контейнер с кнопками внутрь обертки
        wrapperBar->addWidget(this->leftSideBarContainer);

        // Магия архитектуры QMainWindow: addToolBar принудительно ставит панель ЛЕВЕЕ док-виджета!
        this->addToolBar(Qt::LeftToolBarArea, wrapperBar);
        wrapperBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

    // 3. Переопределяем поведение centralwidget, чтобы он не раздувал окно изнутри
    if (ui && ui->centralwidget) {
        ui->centralwidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    }
    // =========================================================================
    // 8. ИСПРАВЛЕННАЯ ЛОГИКА ПЕРЕКЛЮЧЕНИЯ РЕЖИМОВ (С УЧЕТОМ СТРАНИЦЫ 0)
    // =========================================================================
    if (leftSideBarContainer && ui->leftDockWidget)
    {
        QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");

        if (dockStack) {
            // -----------------------------------------------------------------
            // А. ОБРАБОТЧИК КНОПКИ "ПРОЕКТ"
            // -----------------------------------------------------------------
            QToolButton *btnProj = leftSideBarContainer->findChild<QToolButton*>("Проект");
            if (btnProj) {
                btnProj->disconnect();
                connect(btnProj, &QToolButton::clicked, this, [this, dockStack]() {
                    qDebug() << ">>> [UX КЛИК] Кнопка: Проект";

                    // Если проект не открыт — показываем стартовые кнопки (1), если открыт — дерево (0)
                    int targetPageIndex = this->currentOpenProjectPath.isEmpty() ? 1 : 0;

                    if (ui->leftDockWidget->isHidden()) {
                        dockStack->setCurrentIndex(targetPageIndex);
                        ui->leftDockWidget->toggleViewAction()->trigger();
                    } else {
                        // Закрываем, только если повторно кликнули на уже открытую страницу проекта
                        if (dockStack->currentIndex() == targetPageIndex) {
                            ui->leftDockWidget->toggleViewAction()->trigger();
                        } else {
                            dockStack->setCurrentIndex(targetPageIndex);
                        }
                    }
                    if (this->layout()) this->layout()->activate();
                });
            }

            // -----------------------------------------------------------------
            // Б. ОБРАБОТЧИК КНОПКИ "НАСТРОЙКИ ИИ" (ФИКС КОНФЛИКТА С КОМБОБОКСОМ)
            // -----------------------------------------------------------------
            QToolButton *btnAI = leftSideBarContainer->findChild<QToolButton*>("Настройки ИИ");
            if (btnAI) {
                btnAI->disconnect();
                connect(btnAI, &QToolButton::clicked, this, [this, dockStack]() {
                    qDebug() << ">>> [UX КЛИК] Кнопка: Настройки ИИ";

                    int targetAiPageIndex = 1;

                    // 1. БЛОКИРУЕМ СИГНАЛЫ, чтобы комбобокс не перебивал команду скрытия дока
                    if (ui->fileComboBox) {
                        ui->fileComboBox->blockSignals(true);
                    }

                    // Проверяем, открыта ли сейчас Панель управления ИИ в центре экрана
                    bool isAiPanelActive = (ui->centralStackedWidget && ui->centralStackedWidget->currentIndex() == 0);

                    // СЛУЧАЙ 1: Мы были в редакторе кода и хотим ПЕРЕЙТИ к Панели управления ИИ
                    if (!isAiPanelActive)
                    {
                        // Включаем Панель управления ИИ в центре
                        if (ui->centralStackedWidget) ui->centralStackedWidget->setCurrentIndex(0);
                        if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);

                        // Всегда открываем док-виджет со шорткатами рядом
                        dockStack->setCurrentIndex(targetAiPageIndex);
                        ui->leftDockWidget->show();
                        ui->leftDockWidget->setVisible(true);

                        qDebug() << ">>> [ТУМБЛЕР AI] Переключились на Панель управления ИИ. Док открыт.";
                    }
                    // СЛУЧАЙ 2: Мы УЖЕ находимся на Панели управления ИИ (Повторный клик)
                    else
                    {
                        // Кнопка работает как чистый тумблер для бокового док-виджета шорткатов
                        if (ui->leftDockWidget->isHidden()) {
                            dockStack->setCurrentIndex(targetAiPageIndex);
                            ui->leftDockWidget->show();
                            ui->leftDockWidget->setVisible(true);
                            qDebug() << ">>> [ТУМБЛЕР AI] Док-виджет шорткатов открыт.";
                        } else {
                            // Если док был открыт на шорткатах ИИ — повторный клик ПРЯЧЕТ его
                            if (dockStack->currentIndex() == targetAiPageIndex) {
                                ui->leftDockWidget->hide();
                                ui->leftDockWidget->setVisible(false);
                                qDebug() << ">>> [ТУМБЛЕР AI] Повторный клик. Док-виджет шорткатов скрыт.";
                            } else {
                                // Если док был открыт на дереве проекта — просто переключаем вкладку дока
                                dockStack->setCurrentIndex(targetAiPageIndex);
                                qDebug() << ">>> [ТУМБЛЕР AI] Переключили вкладку дока на шорткаты.";
                            }
                        }
                    }

                    // РАЗБЛОКИРУЕМ СИГНАЛЫ КОМБОБОКСА ОБРАТНО
                    if (ui->fileComboBox) {
                        ui->fileComboBox->blockSignals(false);
                    }

                    if (this->layout()) this->layout()->activate();
                });
            }
        }
    }

    // =========================================================================
    // 8. В. УМНАЯ ЛОГИКА ДЛЯ КНОПКИ: ГРАФИКИ (actTensor) - БЕЗ ОШИБОК КОМПИЛЯЦИИ
    // =========================================================================
    if (leftSideBarContainer && ui->mainHorizontalSplitter && ui->widgetRightCharts)
    {
        QToolButton *btnTensor = leftSideBarContainer->findChild<QToolButton*>("Графики");

        if (btnTensor) {
            btnTensor->disconnect();

            connect(btnTensor, &QToolButton::clicked, this, [this]() {
                qDebug() << ">>> [UX КЛИК] Кнопка: Графики";

                int totalWindowWidth = this->width();
                QList<int> currentSizes = ui->mainHorizontalSplitter->sizes();

                // ИСПРАВЛЕНО: точечное обращение к индексу [1] списка размеров
                bool areChartsHidden = !ui->widgetRightCharts->isVisible() || (currentSizes.size() > 1 && currentSizes[1] <= 10);

                if (areChartsHidden)
                {
                    // -------------------------------------------------------------
                    // РЕЖИМ 1: РАЗВЕРТЫВАНИЕ ГРАФИКОВ НА ВЕСЬ ЭКРАН
                    // -------------------------------------------------------------
                    ui->widgetRightCharts->setMaximumSize(QSize(16777215, 16777215));
                    ui->widgetRightCharts->setMinimumWidth(0);
                    ui->widgetRightCharts->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

                    QList<QWidget*> children = ui->widgetRightCharts->findChildren<QWidget*>();
                    if (!children.isEmpty()) {
                        QWidget *webView = children.first();

                        if (!ui->widgetRightCharts->layout()) {
                            QVBoxLayout *boxLayout = new QVBoxLayout(ui->widgetRightCharts);
                            boxLayout->setContentsMargins(0, 0, 0, 0);
                            boxLayout->setSpacing(0);
                            boxLayout->addWidget(webView);
                        }

                        webView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                        webView->setMinimumSize(QSize(0, 0));
                        webView->setMaximumSize(QSize(16777215, 16777215));
                    }

                    ui->widgetRightCharts->setVisible(true);

                    if (ui->leftDockWidget) {
                        ui->leftDockWidget->setVisible(false);
                        ui->leftDockWidget->hide();
                    }

                    ui->mainHorizontalSplitter->setCollapsible(0, true);
                    ui->mainHorizontalSplitter->setSizes(QList<int>({0, totalWindowWidth}));

                    qDebug() << ">>> [TENSORBOARD] WebView успешно растянут на 100% ширины.";
                }
                else
                {
                    // -------------------------------------------------------------
                    // РЕЖИМ 2: СВОРУЧИВАНИЕ ГРАФИКОВ (ВОЗВРАТ К КОДУ)
                    // -------------------------------------------------------------
                    ui->mainHorizontalSplitter->setSizes(QList<int>({1000, 0}));
                    ui->widgetRightCharts->setVisible(false);

                    ui->mainHorizontalSplitter->setCollapsible(0, false);

                    if (ui->leftDockWidget) {
                        QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");
                        if (dockStack) {
                            int targetIdx = this->currentOpenProjectPath.isEmpty() ? 1 : 0;
                            dockStack->setCurrentIndex(targetIdx);
                        }
                        ui->leftDockWidget->setVisible(true);
                        ui->leftDockWidget->show();
                    }

                    qDebug() << ">>> [TENSORBOARD] Интерфейс возвращен к коду.";
                }

                if (this->layout()) {
                    this->layout()->activate();
                }
                ui->mainHorizontalSplitter->refresh();
                this->update();
            });
        }
    }

    // =========================================================================
    // 8. Г. УМНАЯ ЛОГИКА ДЛЯ КНОПКИ: ПОИСК (actSearch) - ТУМБЛЕР ПАНЕЛИ ПОИСКА
    // =========================================================================
    if (leftSideBarContainer && ui->search_panel)
    {
        // Находим живой графический виджет кнопки по её уникальному имени объекта
        QToolButton *btnSearchSide = leftSideBarContainer->findChild<QToolButton*>("Поиск");

        if (btnSearchSide) {
            btnSearchSide->setObjectName("btnSearchSideBar"); // Защита от совпадений имен
            btnSearchSide->disconnect(); // Очищаем старые связи

            connect(btnSearchSide, &QToolButton::clicked, this, [this]() {
                qDebug() << ">>> [UX КЛИК] Кнопка: Поиск по коду";

                // Проверяем фактическую видимость нижней панели поиска по коду
                bool isSearchVisible = ui->search_panel->isVisible();

                if (!isSearchVisible)
                {
                    // -------------------------------------------------------------
                    // РЕЖИМ 1: ОТКРЫТИЕ ПАНЕЛИ ПОИСКА
                    // -------------------------------------------------------------
                    // 1. Вежливо тушим соседние панели внизу (Терминал/PIP в panelOther)
                    if (panelOther && panelOther->isVisible()) {
                        panelOther->setVisible(false);
                        panelOther->hide();
                    }

                    // 2. Показываем виджет поиска на экране
                    ui->search_panel->setVisible(true);
                    ui->search_panel->show();

                    // 3. Вызываем жесткий системный фикс макета, который мы настроили для сплиттера
                    if (mainVerticalSplitter) {
                        int totalHeight = this->height();
                        int bottomHeight = 150; // Наша эталонная высота под панель поиска
                        mainVerticalSplitter->setSizes(QList<int>({totalHeight - bottomHeight, bottomHeight}));
                    }

                    // 4. Автоматически переводим фокус клавиатуры в текстовое поле ввода поиска
                    // (Ищем QLineEdit внутри вашей search_panel, чтобы можно было сразу писать текст)
                    QLineEdit *searchInput = ui->search_panel->findChild<QLineEdit*>();
                    if (searchInput) {
                        searchInput->setFocus();
                        searchInput->selectAll(); // Сразу выделяем старый текст для UX
                    }

                    qDebug() << ">>> [СХЕМА ОКНА] Панель поиска по коду развернута.";
                }
                else
                {
                    // -------------------------------------------------------------
                    // РЕЖИМ 2: ЗАКРЫТИЕ ПАНЕЛИ ПОИСКА (Повторный клик)
                    // -------------------------------------------------------------
                    ui->search_panel->setVisible(false);
                    ui->search_panel->hide();

                    // Схлопываем нижнюю область сплиттера, отдавая 100% высоты редактору кода
                    if (mainVerticalSplitter) {
                        mainVerticalSplitter->setSizes(QList<int>({this->height(), 0}));
                    }

                    qDebug() << ">>> [СХЕМА ОКНА] Панель поиска скрыта. Редактор на весь экран.";
                }

                // Пересчитываем макет QMainWindow без вылетов за границы дисплея
                if (this->layout()) {
                    this->layout()->activate();
                }
            });
        }
    }
}

Neuro_programm::~Neuro_programm()
{
    // =========================================================================
    // ИЗ ДЕСТРУКТОРА ПОЛНОСТЬЮ УДАЛЯЕМ БЛОК ЗАПИСИ pipProcess, ТАК КАК ОН ЗАТИРАЛ ФАЙЛ!
    // =========================================================================

    // Оставляем строго ваш оригинальный, великолепный цикл вежливого закрытия LSP:
    if (lspProcess && lspProcess->state() == QProcess::Running)
    {
        lspProcess->disconnect(this);

        QJsonObject jsonObject;
        jsonObject["jsonrpc"] = "2.0";
        jsonObject["id"] = 9999;
        jsonObject["method"] = "shutdown";
        jsonObject["params"] = QJsonObject();

        QJsonDocument jsonDocument(jsonObject);
        lspProcess->write(jsonDocument.toJson(QJsonDocument::Compact));
        lspProcess->waitForBytesWritten(300);

        if (lspProcess->waitForReadyRead(300)) {
            lspProcess->readAllStandardOutput();
        }

        jsonObject["method"] = "exit";
        jsonObject.remove("id");
        jsonDocument.setObject(jsonObject);

        lspProcess->write(jsonDocument.toJson(QJsonDocument::Compact));
        lspProcess->waitForBytesWritten(300);

        lspProcess->setParent(nullptr);

        if (lspProcess->state() == QProcess::Running) {
            lspProcess->terminate();
            if (!lspProcess->waitForFinished(500)) {
                lspProcess->kill();
            }
        }
        lspProcess->deleteLater();
    }

    if (tensorboardProcess && tensorboardProcess->state() != QProcess::NotRunning) {
        tensorboardProcess->kill(); // Принудительно гасим сервер при закрытии IDE
        tensorboardProcess->waitForFinished(1000);
    }

    delete ui;
}


void Neuro_programm::new_progect()
{
    this->setIDEInStartMode(false);

    rsc = new Start_progect(this);
    rsc->wf = this;
    rsc->setWindowTitle("Выбор режима работы");

    // ФИКС: Объявляем переменную на самом верхнем уровне метода, чтобы она была видна везде
    QString fullProjectPath = "";

    if (rsc->exec() == QDialog::Accepted)
    {
        QString projName = rsc->getProjectName();
        QString projRoot = rsc->getProjectLocation();
        if (projName.isEmpty()) projName = "New_AI_Project";

        fullProjectPath = projRoot + "/" + projName;

        if (bootstrapProjectStructure(fullProjectPath))
        {
            // =================================================================
            // АБСОЛЮТНАЯ ЗАЩИТА: Проверяем, инициализирована ли модель в памяти
            // =================================================================
            initProjectTreeModel(fullProjectPath);
            if (panelOther) {
                panelOther->updateProjectVenv(fullProjectPath);
            }

            // Запуск терминала с установкой venv (если требуется)
            if (rsc->shouldInstallVenv() && panelOther)
            {
                panelOther->setVisible(true);
                if (btnTerminal) btnTerminal->setChecked(true);
                if (mainVerticalSplitter) mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));

                // Объявляем строковую переменную и забираем её из мастера
                QString archType = rsc->getPyTorchArchitecture();

                // Теперь переменная существует в текущей области видимости и передается в терминал без ошибок
                panelOther->startVenvInstallation(fullProjectPath, archType);
            }

            // =================================================================
            // ЛОГИКА ПЕРЕКЛЮЧЕНИЯ ЭКРАНА ДОКА СРАЗУ ПОСЛЕ УСПЕШНОГО СОЗДАНИЯ
            // =================================================================
            if (ui->leftDockWidget)
            {
                if (QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack"))
                {
                    dockStack->setCurrentIndex(0); // <--- Оживляем дерево: ПЕРЕКЛЮЧЕНИЕ НА ДЕРЕВO (Индекс 0)
                }
                ui->leftDockWidget->setVisible(true);
            }
        }
    }

    delete rsc;
    rsc = nullptr;

    // ЗАЩИТА: Если пользователь отменил диалог создания, выходим без записи мусора в историю
    if (fullProjectPath.isEmpty()) {
        initLspServer();
        return;
    }

    // --- БЕЗОПАСНЫЙ БЛОК ЗАПИСИ ПРОЕКТА В ИСТОРИЮ НЕДАВНИХ ---
    QString activePath = fullProjectPath;

    if (projectModel && ui->treeView->model() != nullptr)
    {
        QString diskPath = "";
        // Умное извлечение пути с учетом вашей прокси-модели
        if (projectProxyModel) {
            QModelIndex sourceIdx = projectProxyModel->mapToSource(ui->treeView->rootIndex());
            diskPath = projectModel->filePath(sourceIdx);
        } else {
            diskPath = projectModel->filePath(ui->treeView->rootIndex());
        }

        if (!diskPath.isEmpty()) {
            activePath = diskPath;
        }

        QDir projectDir(activePath);
        QString autoProjectName = projectDir.dirName();

        // Собираем точный путь к контрольному файлу проекта .pystudio
        QString createdPystudioFile = activePath + "/" + autoProjectName + ".pystudio";

        // Добавляем свежесозданный проект в историю верхнего меню "Файл"
        addProjectToRecent(createdPystudioFile);
    }
    else
    {
        // ФОЛБЭК: Если графическая модель дерева еще не успела пробиться в памяти
        qWarning() << "[SAFETY_MGR] projectModel еще не готов. Используем прямой путь:" << fullProjectPath;

        QDir projectDir(fullProjectPath);
        QString autoProjectName = projectDir.dirName();
        QString createdPystudioFile = fullProjectPath + "/" + autoProjectName + ".pystudio";

        addProjectToRecent(createdPystudioFile);
    }

    initLspServer();

    QList<QPushButton*> statusBarButtons = { btnTerminal, btnSearch, btnLogs, btnAIChat, btnStartDebug, btnTogglePip };
    for (QPushButton* btn : statusBarButtons) {
        if (btn) {
            btn->setVisible(true);
            btn->show();
        }
    }
    if (statusSpacer) statusSpacer->show();
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

void Neuro_programm::open_project(const QString &path)
{
    QString selectedFile = path.trimmed();

    // 1. Если путь не передан, запрашиваем его у пользователя через нативный kdialog
    if (selectedFile.isEmpty()) {
        QProcess kdialogProcess;
        QStringList arguments;
        arguments << "--title" << "Открыть проект PyTorch Studio"
                  << "--getopenfilename" << QDir::homePath()
                  << "*.pystudio | Файлы проекта PyTorch Studio (*.pystudio)";
        kdialogProcess.start("kdialog", arguments);

        if (!kdialogProcess.waitForFinished(60000)) {
            kdialogProcess.kill();
            return;
        }
        selectedFile = QString::fromUtf8(kdialogProcess.readAllStandardOutput()).trimmed();
    }

    // Защита от пустых строк (если пользователь нажал "Отмена" в kdialog)
    if (selectedFile.isEmpty()) return;

    // 2. Читаем JSON конфигурацию файла проекта
    QFile configFile(selectedFile);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sendSystemNotification("Ошибка открытия", "Не удалось прочитать файл проекта");
        return;
    }

    QByteArray rawData = configFile.readAll();
    configFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(rawData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        sendSystemNotification("Ошибка конфигурации", "Файл проекта поврежден или имеет неверный формат JSON");
        return;
    }

    // ЖЕЛЕЗНАЯ ЗАЩИТА: Как только JSON успешно прочитан, МГНОВЕННО пишем проект в историю
    addProjectToRecent(selectedFile);
    QJsonObject configObject = jsonDoc.object();

    // 3. Извлекаем сохраненные параметры проекта
    QString projName = configObject["project_name"].toString("New_Project");
    QString datasetPath = configObject["dataset_path"].toString("");
    QString architecture = configObject["architecture"].toString("CUDA");
    QString savedDevice = configObject["device"].toString("cpu");
    int epochs = configObject["epochs"].toInt(10);
    int batchSize = configObject["batch_size"].toInt(32);
    double lr = configObject["learning_rate"].toDouble(0.001);

    // Вычисляем корень проекта на основе расположения .pystudio файла
    QFileInfo fileInfo(selectedFile);
    QString fullProjectPath = fileInfo.absoluteDir().absolutePath();

    currentOpenProjectPath = fullProjectPath;
    if (panelOther) {
        panelOther->setCurrentProjectPath(fullProjectPath);
        panelOther->updateProjectVenv(fullProjectPath);
    }

    // 4. Инициализируем GUI элементы, дерево файлов и стэк
    initProjectTreeModel(fullProjectPath);
    // ui->centralStackedWidget->setCurrentIndex(0);
    // if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);
    // if (ui->openFilesListWidget) ui->openFilesListWidget->setCurrentRow(0);

    // 5. Переопрашиваем доступное на текущем ПК железо
    detectCudaDevices();

    // 6. Синхронизируем интерфейс с загруженными настройками
    // Синхронизируем интерфейс дочерней панели ИИ с загруженными настройками [INDEX]
    if (aiPanel && aiPanel->ui)
    {
        if (aiPanel->ui->spinBoxEpochs)
        {
            aiPanel->ui->spinBoxEpochs->setValue(epochs);
        }
        if (aiPanel->ui->spinBoxLR) {
            aiPanel->ui->spinBoxLR->setValue(lr);
        }
    }

    if (aiPanel && aiPanel->ui && aiPanel->ui->comboBatchSize) {
        int batchIdx = aiPanel->ui->comboBatchSize->findText(QString::number(batchSize));
        if (batchIdx != -1) {
            aiPanel->ui->comboBatchSize->setCurrentIndex(batchIdx);
        }
    }
    // if (ui->comboBatchSize) {
    //     int batchIdx = ui->comboBatchSize->findText(QString::number(batchSize));
    //     if (batchIdx != -1) ui->comboBatchSize->setCurrentIndex(batchIdx);
    // }

    if (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2)
    {
            int deviceIdx = aiPanel->ui->comboDevice_2->findText(savedDevice);
            if (deviceIdx != -1)
            {
                aiPanel->ui->comboDevice_2->setCurrentIndex(deviceIdx);
            } else {
                aiPanel->ui->comboDevice_2->setCurrentIndex(0); // Сброс на CPU
                sendSystemNotification("Конфигурация железа",
                                       "Предупреждение: Сохраненное устройство CUDA недоступно на этом ПК. Сброшено на CPU.");
            }
        }

    // 7. Передаем путь в нижнюю панель и запускаем асинхронный менеджер окружения
    if (panelOther) {
        panelOther->setCurrentProjectPath(fullProjectPath);
    }

    // Асинхронно разворачиваем venv и ставим пакеты
    this->checkAndCreateVenvAsync(fullProjectPath);

    // Обновляем заголовок главного окна ИИ-студии и уведомляем пользователя
    this->setWindowTitle(QString("PyTorch Studio - %1 [%2]").arg(projName, fullProjectPath));
    sendSystemNotification("PyTorch Studio", QString("✔ Проект '%1' успешно загружен").arg(projName));

    if (ui->leftDockWidget) {
        if (QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack")) {
            dockStack->setCurrentIndex(0);
        }
        ui->leftDockWidget->setVisible(true);
    }

    // =========================================================================
    // СУПЕР-UX ДОБАВЛЕНИЕ: УМНОЕ ВОССТАНОВЛЕНИЕ ПОСЛЕДНЕГО АКТИВНОГО ФАЙЛА
    // =========================================================================
    QSettings settings(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);
    QString projectKey = "General/lastActiveFile_" + fileInfo.baseName();
    QString lastWorkedFile = settings.value(projectKey).toString();

    if (lastWorkedFile.isEmpty() || !QFile::exists(lastWorkedFile))
    {
        lastWorkedFile = fullProjectPath + "/train.py";
    }

    if (QFile::exists(lastWorkedFile))
    {
        qDebug() << ">>> [СЕССИЯ] Визуально открываю файл:" << lastWorkedFile;
        this->openNewFileInEditor(lastWorkedFile);
        this->m_pendingAutoloadFile = lastWorkedFile;

        QFileInfo lastFileInfo(lastWorkedFile);
        this->updateCustomTitle(lastFileInfo.fileName());

        // ПЛЭЙСХОЛДЕР СМЕНЯЕТСЯ НА РЕДАКТОР КОДА:
        // Метод openNewFileInEditor сам внутри себя добавит вкладку в конец стека
        // и вызовет setCurrentIndex для новой страницы с редактором кода!
    }
    else
    {
        // ФОЛБЭК: Если ни одного файла в проекте физически не существует,
        // только тогда (после закрытия всех диалогов окружения) включаем Панель ИИ (Индекс 0)
        ui->centralStackedWidget->setCurrentIndex(0);
        if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);
        if (ui->openFilesListWidget) ui->openFilesListWidget->setCurrentRow(0);
    }
    // =========================================================================

    // Фикс возврата кнопок статусбара (Оставляем без изменений)
    QList<QPushButton*> statusBarButtons = { btnTerminal, btnSearch, btnLogs, btnAIChat, btnStartDebug, btnTogglePip };
    for (QPushButton* btn : statusBarButtons) {
        if (btn) { btn->setVisible(true); btn->show(); }
    }
}

void Neuro_programm::sendLspDidOpenForFile(const QString &filePath, const QString &fileContent)
{
    if (!lspProcess || lspProcess->state() != QProcess::Running || filePath.isEmpty()) return;

    QJsonObject params;
    QJsonObject textDocument;

    // Передаем точный URI файла по спецификации LSP
    textDocument["uri"] = QUrl::fromLocalFile(filePath).toString();
    textDocument["languageId"] = "python";
    textDocument["version"] = 1; // Начальная версия сессии всегда 1
    textDocument["text"] = fileContent; // Передаем стартовый текст для инициализации кэша Jedi

    params["textDocument"] = textDocument;

    // Отправляем системное уведомлениеdidOpen через ваш рабочий транспорт запросов
    this->sendLspRequest("textDocument/didOpen", params);

    std::clog << " [LSP] Сессия файла успешно открыта на сервере через didOpen. URI: "
              << textDocument["uri"].toString().toStdString() << std::endl;
    std::clog.flush();
}


void Neuro_programm::onFileDoubleClicked(const QModelIndex &index)
{
    // 1. ИЗВЛЕКАЕМ АБСОЛЮТНЫЙ ПУТЬ К ФАЙЛУ ИЗ МОДЕЛИ ДЕРЕВА [INDEX]
    if (!index.isValid()) return;
    QModelIndex sourceIndex;
    if (this->projectProxyModel != nullptr) {
        sourceIndex = this->projectProxyModel->mapToSource(index);
    } else {
        sourceIndex = index;
    }

    QString filePath;
    if (sourceIndex.isValid() && projectModel != nullptr) {
        filePath = projectModel->fileInfo(sourceIndex).absoluteFilePath();
    } else {
        filePath = currentOpenProjectPath;
    }

    QFileInfo checkInfo(filePath);
    if (!checkInfo.exists() || checkInfo.isDir()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << " [ОШИБКА] Не удалось физически прочитать файл с диска:" << filePath;
        return;
    }
    QString fileContent = QString::fromUtf8(file.readAll());
    file.close();

    // 2. ПРОВЕРЯЕМ, НЕ ОТКРЫТ ЛИ ЭТОТ ДОКУМЕНТ УЖЕ В СОСЕДНЕЙ ВКЛАДКЕ [INDEX]
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && page->objectName() == filePath) {
            ui->centralStackedWidget->setCurrentWidget(page);
            if (ui->fileComboBox) {
                int comboIdx = ui->fileComboBox->findData(filePath);
                if (comboIdx != -1) ui->fileComboBox->setCurrentIndex(comboIdx);
            }

            if (ui && ui->cursorPosLabel) {
                ui->cursorPosLabel->show();
            }
            this->updateCursorPositionIndicator();

            // UX-ФИКС: При переключении на уже открытый файл, обновляем comboDevice под него!
            CodeEditor *existingEditor = page->findChild<CodeEditor*>();
            if (existingEditor) {
                this->updateFunctionNavigator(existingEditor);
            }
            return;
        }
    }

    // БЛОКИРУЕМ ФОНОВЫЕ СБРОСЫ QT ПРИ СОЗДАНИИ ВКЛАДКИ [INDEX]
    if (ui->centralStackedWidget) ui->centralStackedWidget->blockSignals(true);
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(true);

    this->setIDEInStartMode(false);

    // 3. СОЗДАЕМ НОВУЮ ГРАФИЧЕСКУЮ СТРАНИЦУ-КОНТЕЙНЕР [INDEX]
    QWidget *newPage = new QWidget(ui->centralStackedWidget);
    newPage->setObjectName(filePath);
    QVBoxLayout *layout = new QVBoxLayout(newPage);
    layout->setContentsMargins(0, 0, 0, 0);
    CodeEditor *editor = nullptr;
    MinimapArea *minimap = nullptr;
    // Собираем монолитную панель (Редактор + Миникарта) [INDEX]
    QWidget *editorContainer = CodeEditor::createEditorWithMinimap(newPage, editor, minimap);
    if (layout && editorContainer)
    {
        // ВАЖНО: заставляем редактор подстраиваться под контейнер, а не расширять его
        editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        layout->addWidget(editorContainer);
    }

    if (editor) {
        editor->currentFilePath = filePath;
        editor->setObjectName(filePath);
        editor->isLspFreeze = false;

        QFont codeFont;
        codeFont.setFamilies(QStringList() << "JetBrains Mono" << "Fira Code" << "Courier New" << "Monospace");
        codeFont.setStyleHint(QFont::Monospace);
        codeFont.setPixelSize(13);
        editor->setFont(codeFont);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);

        // Блокируем внутренние сигналы ввода текста при первичной загрузке [INDEX]
        editor->blockSignals(true);
        if (editor->document()) editor->document()->blockSignals(true);
        editor->setPlainText(fileContent);
        editor->blockSignals(false);
        if (editor->document()) editor->document()->blockSignals(false);

        // Связываем сигнал логирования с вашей нижней консолью отладки [INDEX]
        connect(editor, &CodeEditor::logMessage, this, [this](const QString &message) {
            QTextEdit *console = panelOther->findChild<QTextEdit*>("consoleOutput");
            if (console) console->append(message);
        });

        // Подключаем отслеживание звездочки правок [INDEX]
        connect(editor, &CodeEditor::textChanged, this, &Neuro_programm::onCurrentFileTextChanged);

        // =========================================================================
        // ЖЕЛЕЗНЫЙ UX-КОННЕКТ НАВИГАТОРА ФУНКЦИЙ (comboDevice ПОД ФАЙЛОВЫМ МЕНЮ)
        // =========================================================================
        // Строим список функций сразу при открытии .py файла
        this->updateFunctionNavigator(editor);

        // При редактировании текста навигатор динамически обновляет список def и class
        connect(editor, &CodeEditor::textChanged, this, [this, editor]() {
            this->updateFunctionNavigator(editor);
        });
        // =========================================================================

        // Подключаем индикатор изменения координат строки/символа [INDEX]
        connect(editor, &CodeEditor::cursorPositionChanged, this, [this]() {
            this->updateCursorPositionIndicator();
        });

        // Коннект кнопки-троеточия для быстрой документации [INDEX]
        connect(editor, &CodeEditor::documentationRequested, this, [this](const QString &fPath, int ln, int ch) {
            if (!lspProcess || lspProcess->state() != QProcess::Running) return;
            QJsonObject hoverParams;
            QJsonObject textDocumentObj;
            #include <QDir>
            QString cleanPath = QDir::fromNativeSeparators(fPath);
            textDocumentObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
            hoverParams["textDocument"] = textDocumentObj;
            QJsonObject positionObj;
            positionObj["line"] = ln;
            positionObj["character"] = ch;
            hoverParams["position"] = positionObj;

            this->sendLspRequest("textDocument/hover", hoverParams, 555);
            qDebug() << ">>> [LSP КЛИЕНТ] Запрос Hover (id:555) успешно отправлен к Jedi.";
        });
    }

    // 4. ДОБАВЛЯЕМ СТРАНИЦУ В ЦЕНТРАЛЬНЫЙ СТЭК (РОВНО ОДИН РАЗ!) [INDEX]
    int newPageIndex = ui->centralStackedWidget->addWidget(newPage);
    // Настраиваем боковой список открытых документов [INDEX]
    if (ui->openFilesListWidget) {
        bool exists = false;
        for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
            if (ui->openFilesListWidget->item(i)->data(Qt::UserRole).toString() == filePath) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            QListWidgetItem *newDocItem = new QListWidgetItem(checkInfo.fileName(), ui->openFilesListWidget);
            newDocItem->setData(Qt::UserRole, filePath);
        }
    }

    // Добавляем имя файла в верхний комбобокс открытых документов [INDEX]
    if (ui->fileComboBox) {
        ui->fileComboBox->addItem(checkInfo.fileName(), QVariant(newPageIndex));
        ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count() - 1);
    }

    // Выводим созданный редактор кода на передний план экрана! [INDEX]
    ui->centralStackedWidget->setCurrentIndex(newPageIndex);

    if (ui->btnCloseFile)
    {
        ui->btnCloseFile->setEnabled(true);
    }

    // СНИМАЕМ БЛОКИРОВКУ СИГНАЛОВ: Окно полностью прорисовано на холсте [INDEX]
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(false);
    if (ui->centralStackedWidget) ui->centralStackedWidget->blockSignals(false);

    if (ui && ui->cursorPosLabel) {
        ui->cursorPosLabel->show();
    }
    this->updateCursorPositionIndicator();

    // Загружаем в редактор считанный код Python-файла и шлем DIDOPEN запрос серверу [INDEX]
    this->sendLspDidOpenForFile(filePath, fileContent);
    if (editor) {
        editor->setFocus();
        editor->update();
        editor->sendLspDidOpen();
        qDebug() << ">>> [LSP] Отправлен ручной didOpen из onFileDoubleClicked для:" << filePath;
    }

    // Выдвигаем нижнюю панель со списком открытых файлов на экран [INDEX]
    if (ui->openFilesContainer && ui->leftVerticalSplitter) {
        ui->openFilesContainer->setVisible(true);
        ui->openFilesContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
        ui->openFilesContainer->setMaximumHeight(10000);
        int totalHeight = ui->leftVerticalSplitter->height();
        if (totalHeight <= 0) totalHeight = this->height() - 150;
        int topSize = totalHeight - 180;
        if (topSize < 100) topSize = 350;
        ui->leftVerticalSplitter->setSizes(QList<int>({topSize, 180}));
        ui->leftVerticalSplitter->updateGeometry();
    }

    this->applyGlobalFonts();
    updateCustomTitle(checkInfo.fileName());

    QTimer::singleShot(50, this, [this, editor]() {
        if (editor) editor->document()->setModified(false);
        this->setWindowModified(false);
    });

    // =========================================================================
    // АБСОЛЮТНЫЙ UX-ФИКС: ВЫТАЛКИВАЕМ КНОПКИ НА ЭКРАН ПРИ ОТКРЫТИИ ФАЙЛА [INDEX]
    // =========================================================================
    QList<QPushButton*> statusBarButtons = { btnTerminal, btnSearch, btnLogs, btnAIChat, btnStartDebug, btnTogglePip };
    for (QPushButton* btn : statusBarButtons) {
        if (btn) {
            btn->setVisible(true);
            btn->show();
            btn->update();
        }
    }
    if (statusSpacer)
    {
        statusSpacer->setVisible(true);
        statusSpacer->show();
    }

    if (ui->centralStackedWidget)
    {
        ui->centralStackedWidget->blockSignals(false);
        ui->fileComboBox->blockSignals(false);

        // Перелистываем на свежесозданный файл
        ui->centralStackedWidget->setCurrentWidget(newPage);
    }

    QTimer::singleShot(50, this, [this]() {
        // 1. Срочно отрезаем у центрального контейнера способность раздувать окно вниз
        if (ui && ui->centralwidget) {
            ui->centralwidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        }

        if (mainVerticalSplitter) {
            // 2. Намертво привязываем веса: верх (код) заполняет всё окно, низ (терминал) не давит на статусбар
            mainVerticalSplitter->setStretchFactor(0, 1);
            mainVerticalSplitter->setStretchFactor(1, 0);

            // 3. Вычисляем точную высоту родительского окна (без учета вылетевших элементов)
            int totalWindowHeight = this->height();
            int targetBottomHeight = 0;

            if (panelOther && panelOther->isVisible()) {
                targetBottomHeight = 250; // Стандарт для терминала
            } else if (ui->search_panel && ui->search_panel->isVisible()) {
                targetBottomHeight = 150; // Стандарт для панели поиска
            }

            // 4. Жестко вбиваем пропорции сплиттера на основе честной высоты окна
            mainVerticalSplitter->setSizes(QList<int>({totalWindowHeight - targetBottomHeight, targetBottomHeight}));
            mainVerticalSplitter->refresh();
        }

        // 5. Пинаем менеджер компоновки QMainWindow для финальной фиксации низа на экране
        if (this->layout()) {
            this->layout()->invalidate();
            this->layout()->activate();
        }
        this->update();
    });
}


void Neuro_programm::onCloseCurrentFileClicked()
{
    if (!ui->centralStackedWidget || !ui->fileComboBox) return;

    // КРИТИЧЕСКИЙ ФИКС: Берем индекс АКТИВНОЙ страницы прямо из стэка окон [INDEX]
    int currentStackIndex = ui->centralStackedWidget->currentIndex();

    // БЕЗОПАСНОСТЬ: Системные сервисные экраны (индексы < 2) закрывать нельзя [INDEX]
    if (currentStackIndex < 2) {
        if (ui->statusbar) {
            ui->statusbar->showMessage("ℹ Сервисные вкладки среды разработки нельзя закрыть", 3000);
        }
        return;
    }

    // Извлекаем указатель на закрываемую динамическую страницу кода [INDEX]
    QWidget *filePageWidget = ui->centralStackedWidget->widget(currentStackIndex);
    if (!filePageWidget) return;

    // ФИКС БАГА 2: Находим дочерний CodeEditor внутри закрываемой страницы и проверяем изменения [INDEX]
    CodeEditor *editor = filePageWidget->findChild<CodeEditor*>();
    if (editor && editor->document() && editor->document()->isModified()) {
        QFileInfo fileInfo(filePageWidget->objectName());

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Несохраненные изменения",
                                      QString("Файл '%1' был изменен.\nСохранить изменения перед закрытием?")
                                      .arg(fileInfo.fileName()),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes) {
            this->saveCurrentActiveFile(); // Сохраняем перед выходом [INDEX]
        } else if (reply == QMessageBox::Cancel) {
            return; // Пользователь отменил закрытие [INDEX]
        }
    }

    // Получаем уникальный путь к файлу (он записан в objectName страницы) [INDEX]
    QString filePath = filePageWidget->objectName();
    qDebug() << ">>> [КРЕСТИК] Начинаю процедуру закрытия файла:" << filePath;

    // 1. Находим и удаляем соответствующую строчку из верхнего комбобокса по её filePath [INDEX]
    int comboIndex = ui->fileComboBox->findData(filePath);
    if (comboIndex != -1) {
        ui->fileComboBox->blockSignals(true);
        ui->fileComboBox->removeItem(comboIndex);
        ui->fileComboBox->blockSignals(false);
    } else {
        int matchIdx = ui->fileComboBox->findData(currentStackIndex);
        if (matchIdx != -1) ui->fileComboBox->removeItem(matchIdx);
    }

    // 2. Удаляем запись из левого бокового списка документов (openFilesListWidget) [INDEX]
    if (ui->openFilesListWidget) {
        for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
            if (ui->openFilesListWidget->item(i)->data(Qt::UserRole).toString() == filePath) {
                delete ui->openFilesListWidget->takeItem(i);
                break;
            }
        }
    }

    // 3. Вынимаем страницу из контейнера стэка и полностью зачищаем оперативную память ОЗУ [INDEX]
    ui->centralStackedWidget->removeWidget(filePageWidget);
    filePageWidget->deleteLater(); // [INDEX]
    // =========================================================================
    // СМАРТ-АНАЛИЗ ОСТАВШИХСЯ ОТКРЫТЫХ ФАЙЛОВ КОДА
    // =========================================================================
    bool hasAnyOpenedFiles = false;
    int lastFileIndex = -1;

    // Сканируем стек (сервисные экраны под индексами 0, 1 и заставку шорткатов не считаем) [INDEX]
    int placeholderIndex = this->property("placeholderIndex").toInt();

    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        if (i != 0 && i != 1 && i != placeholderIndex) {
            QWidget *w = ui->centralStackedWidget->widget(i);
            if (w && !w->objectName().isEmpty()) {
                hasAnyOpenedFiles = true;
                lastFileIndex = i; // Запоминаем индекс последней живой вкладки
            }
        }
    }

    // А. ЕСЛИ РЕАЛЬНЫХ ФАЙЛОВ БОЛЬШЕ НЕ ОСТАЛОСЬ — ЖЕСТКО СБРАСЫВАЕМ ИНТЕРФЕЙС В СТАРТ
    if (!hasAnyOpenedFiles) {
        qDebug() << ">>> [НАВИГАЦИЯ] Все рабочие файлы закрыты. Откатываем интерфейс.";

        // Намертво очищаем и прячем навигатор функций comboDevice [INDEX]
        if (ui->comboDevice) {
            ui->comboDevice->blockSignals(true);
            ui->comboDevice->clear();
            ui->comboDevice->hide();
            ui->comboDevice->blockSignals(false);
        }

        ui->fileComboBox->blockSignals(true);
        ui->fileComboBox->setCurrentIndex(-1);
        ui->fileComboBox->blockSignals(false);

        if (ui && ui->cursorPosLabel) {
            ui->cursorPosLabel->hide();
        }

        // Выводим на экран заставку шорткатов JetBrains [INDEX]
        if (placeholderIndex > 0) {
            ui->centralStackedWidget->setCurrentIndex(placeholderIndex);
        }

        if (ui->openFilesContainer) ui->openFilesContainer->setVisible(false);
        if (ui->leftVerticalSplitter) ui->leftVerticalSplitter->setSizes(QList<int>({1000, 0}));
        updateCustomTitle("");
    }
    // Б. ЕСЛИ ЕСТЬ ДРУГИЕ ОТКРЫТЫЕ ФАЙЛЫ — ПЕРЕКЛЮЧАЕМСЯ НА ПОСЛЕДНИЙ АКТИВНЫЙ
    else {
        if (lastFileIndex >= 0) {
            ui->centralStackedWidget->setCurrentIndex(lastFileIndex);

            QWidget *activePage = ui->centralStackedWidget->widget(lastFileIndex);
            if (activePage) {
                int comboIdx = ui->fileComboBox->findData(activePage->objectName());
                if (comboIdx != -1) {
                    ui->fileComboBox->blockSignals(true);
                    ui->fileComboBox->setCurrentIndex(comboIdx);
                    ui->fileComboBox->blockSignals(false);
                }

                // Пересчитываем структуру функций навигатора comboDevice под открывшийся файл!
                CodeEditor *activeEditor = activePage->findChild<CodeEditor*>();
                if (activeEditor) {
                    this->updateFunctionNavigator(activeEditor);
                }

                QFileInfo fileInfo(activePage->objectName());
                updateCustomTitle(fileInfo.fileName());
            }
        }
    }
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
    if (!aiPanel || !aiPanel->ui || !aiPanel->ui->comboDevice_2) return;

    // Базовая очистка через указатель дочерней панели: CPU доступен всегда
    aiPanel->ui->comboDevice_2->clear();
    aiPanel->ui->comboDevice_2->addItem("📟 CPU");

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

            // Цикл детекции видеокарт CUDA внутри neuro_programm.cpp
            if (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2) {
                for (int i = 0; i < gpuCount; ++i) {
                    QString deviceName = QString("🚀 CUDA:%1").arg(i);

                    // Заталкиваем элемент во встроенный комбобокс панели ИИ
                    aiPanel->ui->comboDevice_2->addItem(deviceName);

                    // Вешаем красивую всплывающую подсказку (ToolTip) с точным названием видеокарты от драйвера
                    int lastIdx = aiPanel->ui->comboDevice_2->count() - 1;
                    aiPanel->ui->comboDevice_2->setItemData(lastIdx, gpus.at(i).trimmed(), Qt::ToolTipRole);
                }
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
            // Резервный цикл наполнения CUDA устройств в neuro_programm.cpp
            if (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2) {
                for (int i = 0; i < gpuCount; ++i) {
                    QString deviceName = QString("🚀 CUDA:%1").arg(i);

                    // Заталкиваем элемент во встроенный комбобокс панели ИИ
                    aiPanel->ui->comboDevice_2->addItem(deviceName);

                    // Вешаем инфо-строку во всплывающую подсказку (ToolTip)
                    int lastIdx = aiPanel->ui->comboDevice_2->count() - 1;
                    aiPanel->ui->comboDevice_2->setItemData(lastIdx,
                                                         QString("NVIDIA GPU Device (Index %1)").arg(i),
                                                         Qt::ToolTipRole);
                }
            }

        }
    }

    // --- ИТОГОВЫЙ СТАТУС ВЫБОРА ---
    if (gpuCount > 0)
    {
        // Если нашли GPU хотя бы одним способом — выставляем cuda:0 по умолчанию
        if (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2)
        {
            aiPanel->ui->comboDevice_2->setCurrentIndex(1);
        }
        sendSystemNotification("Аппаратное ускорение",
                               QString("⚡ CUDA ядра активны. В системе доступно %1 GPU NVIDIA.").arg(gpuCount));
    }
    else
    {
        // На ПК действительно нет графики NVIDIA
        if (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2) {
            aiPanel->ui->comboDevice_2->setCurrentIndex(1);
        }
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

    // 1. Задаем путь к файлу в реальной системе (/tmp/pytorch_studio_icon.png)
    QString tempIconPath = QDir::tempPath() + "/pTS.png";
    QString finalIconParam = "brain"; // Фоллбэк: стандартная системная иконка

    // 2. Если файла в /tmp еще нет, копируем его туда из ресурсов Qt
    if (!QFile::exists(tempIconPath)) {
        QFile resFile(":/Data/Icons/pTS.png"); // Ваш точный путь из qrc
        if (resFile.copy(tempIconPath)) {
            finalIconParam = tempIconPath; // Успешно скопировали, используем этот путь
        } else {
            qWarning() << "[NOTIFY_MGR] Не удалось скопировать иконку из ресурсов в /tmp";
        }
    } else {
        finalIconParam = tempIconPath; // Файл уже был создан при прошлых уведомлениях
    }

    // 2. Сборка аргументов под спецификацию Freedesktop (метод Notify)
    QVariantList args;
    args << "PyTorch Studio";       // 1. Имя приложения-отправителя
    args << 0u;                    // 2. ID заменяемого уведомления (0 = создать новое)
    args << finalIconParam;               // 3. Иконка (используем системную иконку из темы Breeze)
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
    if (safePath.isEmpty()) {
        qWarning() << " [LSP ПРЕДУПРЕЖДЕНИЕ] Вызван initProjectTreeModel с пустым путем. Пропуск.";
        return;
    }

    // 1. Безопасное уничтожение старых моделей в правильном порядке
    if (projectProxyModel) {
        projectProxyModel->deleteLater();
        projectProxyModel = nullptr;
    }
    if (projectModel) {
        projectModel->deleteLater();
        projectModel = nullptr;
    }

    // Извлекаем чистое имя папки проекта (например, "z1")
    QString projName = QDir(safePath).dirName();

    // Находим родительскую папку, где лежит папка проекта, чтобы сделать сам проект узлом дерева
    QDir projectDir(safePath);
    projectDir.cdUp();
    QString rootContainerPath = projectDir.absolutePath();

    // 2. Инициализация и настройка базовой дисковой модели файловой системы
    projectModel = new QFileSystemModel(this);
    projectModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
    projectModel->setNameFilters(QStringList() << "[^v]*" << "v[^e]*" << "ve[^n]*" << "ven[^v]*" << "venv?*");
    projectModel->setNameFilterDisables(false);

    // Переводим фокус сканирования диска на уровень выше папки проекта
    projectModel->setRootPath(rootContainerPath);

    // 3. Создаем прокси-модель подмены имени и фильтрации соседей
    projectProxyModel = new ProjectRootProxyModel(this);
    projectProxyModel->setSourceModel(projectModel);
    projectProxyModel->setProjectInfo(safePath, projName); // Передаем данные проекта для переименования папки

    // 4. Назначаем представлению (TreeView) именно ПРОКСИ-модель
    ui->treeView->setModel(projectProxyModel);

    // 5. Вычисление индексов (Перевод из исходной модели диска в прокси)
    QModelIndex sourceContainerIndex = projectModel->index(rootContainerPath);
    QModelIndex sourceProjectIndex = projectModel->index(safePath);

    QModelIndex proxyContainerIndex = projectProxyModel->mapFromSource(sourceContainerIndex);
    QModelIndex proxyProjectIndex = projectProxyModel->mapFromSource(sourceProjectIndex);

    // Устанавливаем отображаемый корень дерева на родительскую папку ОС
    ui->treeView->setRootIndex(proxyContainerIndex);

    // ФИЗИЧЕСКИ РАСКРЫВАЕМ наш переименованный узел проекта (появится стрелочка и все файлы внутри)
    ui->treeView->expand(proxyProjectIndex);

    // 6. Скрываем лишние колонки (Размер, Тип, Дата изменения), оставляя только Имя
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
        ui->treeView->setHeaderHidden(true);
        ui->treeView->header()->setMaximumHeight(0);
        ui->treeView->header()->setMinimumSectionSize(0);
        ui->treeView->header()->resizeSections(QHeaderView::Fixed);
        ui->treeView->header()->setStyleSheet("QHeaderView { margin: 0px; padding: 0px; height: 0px; border: none; }");
    }

    // 1. Принудительно включаем отрисовку линий ветвей (бранчей)
    ui->treeView->setItemsExpandable(true);

    // 2. Задаем CSS-стиль для отображения пунктирных линий веток в темной/светлой теме
    ui->treeView->setStyleSheet(
                "QTreeView::branch:has-siblings:!adjoins-item {"
                "    border-image: url(:/icons/vline.png) 0;" /* Нужна будет иконка вертикальной линии или стандарт */
                "}"
                "QTreeView {"
                "    paint-alternating-row-colors-for-empty-area: true;"
                "    show-decoration-selected: 1;"
                "}"
                );
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
                            .arg(activeFileName,editor->toPlainText());
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
    userMessage["content"] = userQuery.split('\n').constFirst();
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
            QString aiResponse = responseObj.value("message").toObject().value("content").toString().trimmed();

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

    // if (ui->widgetRightCharts && !ui->widgetRightCharts->isVisible()) {
    //     ui->widgetRightCharts->setVisible(true);
    //     if (ui->mainHorizontalSplitter) {
    //         ui->mainHorizontalSplitter->setCollapsible(1, false);
    //         ui->mainHorizontalSplitter->setSizes(QList<int>({this->width() - 350, 350}));
    //     }
    //     if (ui->summaryMetrics) {
    //         ui->summaryMetrics->clear();
    //         ui->summaryMetrics->append("⏱ Статус: Инициализация ядра вычислений...");
    //     }
    //     if (lossSeries) lossSeries->clear();
    // }

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
    for (const QString &line : std::as_const(lines))
    {
        // Парсинг синей кривой Loss
        QRegularExpressionMatch lossMatch = lossRegex.match(line.trimmed());
        if (lossMatch.hasMatch()) {
            double epoch = lossMatch.captured(1).toDouble();
            double loss = lossMatch.captured(2).toDouble();
            if (lossSeries) lossSeries->append(epoch, loss);
            if (lossChart) {
                lossChart->axes(Qt::Horizontal).constFirst()->setRange(1, qMax(10.0, static_cast<double>(epoch)));
                static double maxLoss = 0.1;
                if (loss > maxLoss) maxLoss = loss;
                lossChart->axes(Qt::Vertical).constFirst()->setRange(0, maxLoss * 1.1);
            }
        }

        // // Обновление цветного HTML дашборда
        // QRegularExpressionMatch metricsMatch = metricsRegex.match(line.trimmed());
        // if (metricsMatch.hasMatch() && ui->summaryMetrics) {
        //     QString epochStr = metricsMatch.captured(1);
        //     QString accStr = metricsMatch.captured(2);
        //     QString vramStr = metricsMatch.captured(3);
        //     QString speedStr = metricsMatch.captured(4);
        //     QString htmlReport = QString(
        //                 "<div style='font-family:\"Monospace\"; font-size:13px; color:#232629;'>"
        //                 " <b style='color:#0056b3; font-size:14px;'> МОНИТОРИНГ МЕТРИК НЕЙРОСЕТИ:</b><br>"
        //                 " <hr style='border:none; border-top:1px solid #b0b0b0; margin: 5px 0;'>"
        //                 " <table width='100%' cellpadding='2' cellspacing='0'>"
        //                 " <tr><td><b>Текущая эпоха:</b></td><td align='right' style='color:#27ae60; font-weight:bold;'>%1</td></tr>"
        //                 " <tr><td><b>Точность (Accuracy):</b></td><td align='right' style='color:#2980b9; font-weight:bold;'>%2 %</td></tr>"
        //                 " <tr><td><b>Видеопамять VRAM:</b></td><td align='right' style='color:#8e44ad; font-weight:bold;'>%3 ГБ</td></tr>"
        //                 " <tr><td><b>Скорость вычислений:</b></td><td align='right' style='color:#f39c12; font-weight:bold;'>%4 img/s</td></tr>"
        //                 " </table>"
        //                 "</div>"
        //                 ).arg(epochStr, accStr, vramStr, speedStr);
        //     ui->summaryMetrics->setHtml(htmlReport);
        // }
    }
}

void Neuro_programm::trainingFinished(int exitCode)
{
    // Разблокируем пульт управления (наш старый код)
    ui->btnStartTraining->setEnabled(true);
    ui->btnStartTraining->setText(" 🚀 Начать обучение ");
    //ui->btnStopTraining->setEnabled(false);
    if (aiPanel && aiPanel->spinBoxEpochs)
    {
        aiPanel->spinBoxEpochs->setEnabled(true);
    }
    if (aiPanel && aiPanel->comboBatchSize)
    {
        aiPanel->comboBatchSize->setEnabled(true);
    }
    if (aiPanel && aiPanel->spinBoxLR)
    {
        aiPanel->spinBoxLR->setEnabled(true);
    }
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
    configObject["epochs"] = (aiPanel && aiPanel->ui && aiPanel->ui->spinBoxEpochs)
                             ? aiPanel->ui->spinBoxEpochs->value()
                             : 10;
    configObject["batch_size"] = (aiPanel && aiPanel->comboBatchSize)
                                 ? aiPanel->comboBatchSize->currentText().toInt()
                                 : 32;
    configObject["epochs"] = (aiPanel && aiPanel->ui && aiPanel->ui->spinBoxEpochs)
                             ? aiPanel->ui->spinBoxEpochs->value()
                             : 10;

    configObject["device"] = (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2)
                             ? aiPanel->ui->comboDevice_2->currentText()
                             : "cpu";
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
// void Neuro_programm::initLossChart()
// {
//     // Защита: если виджет на форме не найден — выходим
//     if (!ui->chartViewLoss) return;

//     // Выделяем память под глобальные переменные класса (без типов данных в начале!)
//     lossSeries = new QLineSeries();
//     lossChart = new QChart();

//     // Связываем линию данных с графиком
//     lossChart->addSeries(lossSeries);
//     lossChart->setTitle("📊 График падения ошибки (Training Loss)");
//     lossChart->setAnimationOptions(QChart::SeriesAnimations); // Плавная анимация точек

//     // Создаем и настраиваем горизонтальную ось X (Эпохи)
//     QValueAxis *axisX = new QValueAxis();
//     axisX->setTitleText("Эпохи обучения");
//     axisX->setLabelFormat("%d");
//     axisX->setRange(1, 10); // Стартовый диапазон по умолчанию
//     lossChart->addAxis(axisX, Qt::AlignBottom);
//     lossSeries->attachAxis(axisX);

//     // Создаем и настраиваем вертикальную ось Y (Значение ошибки Loss)
//     QValueAxis *axisY = new QValueAxis();
//     axisY->setTitleText("Значение Loss");
//     axisY->setLabelFormat("%.4f");
//     axisY->setRange(0, 1.0); // Стартовый диапазон по умолчанию
//     lossChart->addAxis(axisY, Qt::AlignLeft);
//     lossSeries->attachAxis(axisY);

//     // Монтируем настроенный график внутрь продвинутого виджета на экране
//     ui->chartViewLoss->setChart(lossChart);
//     ui->chartViewLoss->setRenderHint(QPainter::Antialiasing); // Включаем сглаживание линий (anti-aliasing)

//     currentEpochCounter = 0;
// }

void Neuro_programm::addProjectToRecent(const QString &filePath) {
    QSettings settings(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);

    QStringList recentProjects = settings.value("General/recentProjectsList").toStringList();

    // Удаляем дубликат, если такой файл уже был в истории, чтобы вытолкнуть его наверх
    recentProjects.removeAll(filePath);
    recentProjects.prepend(filePath); // Добавляем на первое место

    // Ограничиваем историю, например, 10 последними файлами
    while (recentProjects.size() > 10) {
        recentProjects.removeLast();
    }

    // Записываем обратно в файл конфигурации
    settings.setValue("General/recentProjectsList", recentProjects);

    // МГНОВЕННО ОБНОВЛЯЕМ БОКОВУЮ ПАНЕЛЬ НА ЭКРАНЕ
    this->updateProjectsListFromSettings();
}

void Neuro_programm::updateRecentProjectActions()
{
    // Открываем строго текстовый файл
    QString configFilePath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";
    QSettings settings(configFilePath, QSettings::IniFormat);

    QStringList recentProjects = settings.value("recentProjectsList").toStringList();
    int numRecentFiles = qMin(recentProjects.size(), (int)MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i)
    {
        QString fullPath = recentProjects[i];
        QString text = QString("&%1 %2").arg(i + 1).arg(QFileInfo(fullPath).fileName());

        recentProjectActions[i]->setText(text);
        recentProjectActions[i]->setData(fullPath); // Привязываем абсолютный путь к экшену
        recentProjectActions[i]->setVisible(true);
    }

    for (int j = numRecentFiles; j < MaxRecentFiles; ++j) {
        recentProjectActions[j]->setVisible(false);
    }
}

void Neuro_programm::openRecentProject()
{
    // 1. Безопасно определяем, какой именно пункт меню нажал пользователь
    QAction *clickedAction = qobject_cast<QAction*>(sender());
    if (!clickedAction) return;

    // 2. Извлекаем полный путь к проекту из скрытых данных экшена (.data())
    QString targetProjectPath = clickedAction->data().toString();

    qDebug() << "[RECENT_MGR] Запрос быстрого открытия файла:" << targetProjectPath;

    // 3. Проверяем, существует ли файл на диске (вдруг пользователь его удалил)
    if (!targetProjectPath.isEmpty() && QFile::exists(targetProjectPath)) {
        // Вызываем переписанный метод open_project и передаем ему готовый путь.
        // Никакие kdialog и QFileDialog больше не откроются!
        this->open_project(targetProjectPath);
    }
    else {
        qWarning() << "[RECENT_MGR] Ошибка: Файл проекта не найден по пути:" << targetProjectPath;
        if (this->statusBar()) {
            this->statusBar()->showMessage("Ошибка: Файл проекта перенесен или удален", 5000);
            this->statusBar()->setStyleSheet("QStatusBar { color: #ff0000; font-weight: bold; }");
        }
    }
}

void Neuro_programm::saveCurrentActiveFile()
{
    // 1. Извлекаем указатель на текущую активную страницу в centralStackedWidget
    QWidget *currentPage = ui->centralStackedWidget->currentWidget();
    if (!currentPage) return;

    // 2. Ищем объект текстового редактора CodeEditor внутри этой страницы
    CodeEditor *editor = currentPage->findChild<CodeEditor*>();
    if (!editor) return;

    // Получаем абсолютный путь к текущему файлу, сохраненный в свойствах редактора
    QString filePath = editor->currentFilePath;
    if (filePath.isEmpty()) return;

    // 3. ФИЗИЧЕСКАЯ ЗАПИСЬ ОБНОВЛЕННОГО КОДА НА ДИСК
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        out.setEncoding(QStringConverter::Utf8);
#else
        out.setCodec("UTF-8");
#endif
        // Записываем актуальный текст из памяти редактора в файл
        out << editor->toPlainText();
        file.close();

        qDebug() << "[ИИ СТУДИЯ] Текст скрипта успешно сохранен в файл:" << filePath;

        // =========================================================================
        // ЦЕНТРАЛЬНЫЙ АРХИТЕКТУРНЫЙ ШАГ: ПЕРЕКРАШИВАЕМ МАРКЕРЫ СТРОК В ЗЕЛЕНЫЙ!
        // =========================================================================
        // Просим редактор пробежаться по измененным блокам и сделать полосы зелеными
        editor->setChangesAsSaved();

        // Дополнительно сбрасываем звездочку модификации в заголовке вкладки
        setFileModifiedState(editor, false);

        if (this->statusBar()) {
            this->statusBar()->showMessage(QString("Файл %1 успешно сохранен").arg(QFileInfo(filePath).fileName()), 3000);
        }
    }
    else {
        qCritical() << "[ОШИБКА] Не удалось открыть файл для записи при сохранении:" << filePath;
        if (this->statusBar()) {
            this->statusBar()->showMessage("Ошибка ввода-вывода OS: Не удалось сохранить файл", 5000);
        }
    }
}

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
    CodeEditor *currentEditor = qobject_cast<CodeEditor*>(sender());
    if (!currentEditor) return;

    QString absoluteFilePath = currentEditor->objectName();
    if (absoluteFilePath.isEmpty()) return;

    // Проверяем по комбобоксу — добавлена ли уже туда звёздочка, чтобы не перерисовывать GUI зря
    if (ui->fileComboBox) {
        int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
        if (comboIdx != -1) {
            QString currentText = ui->fileComboBox->itemText(comboIdx);

            // Если текст НЕ заканчивается на " *", значит файл только что отредактировали
            if (!currentText.endsWith(" *")) {

                // Включаем встроенный флаг документа и окна ОС
                currentEditor->document()->setModified(true);
                this->setWindowModified(true);

                QFileInfo info(absoluteFilePath);

                // А. Добавляем звёздочку в верхний fileComboBox
                ui->fileComboBox->setItemText(comboIdx, info.fileName() + " *");

                // Б. Добавляем звёздочку в левый openFilesListWidget
                if (ui->openFilesListWidget) {
                    for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
                        QListWidgetItem *item = ui->openFilesListWidget->item(i);
                        if (item && item->data(Qt::UserRole).toString() == absoluteFilePath) {
                            item->setText(info.fileName() + " *");
                            break;
                        }
                    }
                }

                // В. Обновляем главный заголовок операционной системы
                updateTabName();
            }
        }
    }
}


void Neuro_programm::onCloseProjectClicked()
{
    QWidget *currentPage = ui->centralStackedWidget->currentWidget();
    if (currentPage) {
        CodeEditor *activeEditor = currentPage->findChild<CodeEditor*>();
        // Если это реальный редактор кода, а не сервисный экран
        if (activeEditor && !activeEditor->currentFilePath.isEmpty()) {
            QSettings settings(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);
            // Сохраняем путь к файлу, привязав его к текущему проекту
            settings.setValue("General/lastActiveFile_" + QFileInfo(currentOpenProjectPath).baseName(), activeEditor->currentFilePath);
        }
    }

    // 1. ПРОВЕРКА НЕСОХРАНЕННЫХ ИЗМЕНЕНИЙ В РЕДАКТОРЕ КОДА И ПРОЕКТЕ
    bool hasUnsavedChanges = false;
    CodeEditor *activeEditor = nullptr;
    currentPage = ui->centralStackedWidget->currentWidget();
    if (currentPage) {
        activeEditor = currentPage->findChild<CodeEditor*>();
    }
    if (this->isWindowModified() || (activeEditor && activeEditor->document()->isModified())) {
        hasUnsavedChanges = true;
    }

    // Вывод предупреждения (Диалоговое окно в стиле IDE)
    if (hasUnsavedChanges) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(this,
                                     "Несохраненные изменения",
                                     "В проекте или коде есть несохраненные изменения.\nХотите сохранить их перед закрытием?",
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
                                     );
        if (reply == QMessageBox::Save) {
            this->saveCurrentActiveFile();
            qDebug() << "[ЗАКРЫТИЕ] Проект принудительно сохранен пользователем.";
        }
        else if (reply == QMessageBox::Cancel) {
            qDebug() << "[ЗАКРЫТИЕ] Отмена закрытия проекта.";
            return; // Прерываем закрытие, остаемся в проекте
        }
    }

    qDebug() << ">>> [ЗАКРЫТИЕ] Запуск процедуры очистки и закрытия всего проекта...";

    // =========================================================================
    // 2. ОЧИСТКА ДИНАМИЧЕСКИХ ВКЛАДОК КОДА И СБРОС КОМБОБОКСА
    // =========================================================================
    // Блокируем сигналы, чтобы комбобокс не генерировал фоновые сбросы страниц во время очистки
    ui->fileComboBox->blockSignals(true);

    // Считываем сохраненный индекс заставки из динамических свойств окна
    int placeholderIndex = this->property("placeholderIndex").toInt();

    // ХИРУРГИЧЕСКИЙ ФИКС С++: Пробегаемся по стэку виджетов СЗАДУ НАПЕРЕД (с самого конца)
    // И удаляем абсолютно все страницы, кроме сервисных (0 - Панель ИИ, 1 - Чат) и самого плейсхолдера!
    for (int i = ui->centralStackedWidget->count() - 1; i >= 0; --i)
    {
        // Намертво пропускаем системные вкладки и заставку, их удалять нельзя
        if (i == 0 || i == 1 || i == placeholderIndex) {
            continue;
        }

        QWidget *w = ui->centralStackedWidget->widget(i);
        if (w) {
            qDebug() << ">>> [УДАЛЕНИЕ] Закрываю открытую вкладку кода:" << w->objectName();
            ui->centralStackedWidget->removeWidget(w);
            w->deleteLater();
        }
    }

    // Очищаем и заново инициализируем комбобокс до базового состояния
    ui->fileComboBox->clear();
    ui->fileComboBox->addItem("  Панель обучения ИИ", QVariant("MAIN_SCREEN"));
    ui->fileComboBox->addItem("  ИИ-Ассистент", QVariant("AI_CHAT_SCREEN"));
    ui->fileComboBox->setCurrentIndex(-1); // Сбрасываем стрелку в нейтральное пустое положение
    ui->fileComboBox->blockSignals(false);

    // Дополнительно полностью очищаем левый нижний список документов, если он используется
    if (ui->openFilesListWidget) {
        ui->openFilesListWidget->clear();
        QListWidgetItem *mainScreenItem = new QListWidgetItem("  Панель обучения ИИ", ui->openFilesListWidget);
        mainScreenItem->setData(Qt::UserRole, QString("MAIN_SCREEN"));
        QListWidgetItem *chatScreenItem = new QListWidgetItem("  ИИ-Ассистент", ui->openFilesListWidget);
        chatScreenItem->setData(Qt::UserRole, QString("AI_CHAT_SCREEN"));
        ui->openFilesListWidget->setCurrentRow(0);

        if (ui->openFilesContainer) ui->openFilesContainer->setVisible(false);
    }

    // Сброс дерева проекта (закрытие структуры файлов)
    if (ui->treeView) {
        ui->treeView->setModel(nullptr);
    }

    // Сбрасываем переменные путей и заголовки в null
    this->currentOpenProjectPath = "";
    this->setWindowTitle("PyTorch Studio");
    this->setWindowModified(false);

    // =========================================================================
    // 3. ЖЕСТКИЙ ПЕРЕХОД БОКОВОЙ ПАНЕЛИ НА СТАРТОВЫЙ ЭКРАН (ИНДЕКС 1)
    // =========================================================================
    // QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");
    // if (dockStack) {
    //     dockStack->setCurrentIndex(1); // Переключаем левый док на стартовые кнопки (Open, New)
    //     ui->leftDockWidget->setVisible(true);
    //     if (actProject) actProject->setChecked(true);
    //     dockStack->update();
    // }

    // =========================================================================
    // 4. ЦЕНТРАЛЬНЫЙ СУПЕР-ФИКС: ВКЛЮЧАЕМ СВЕТЛЫЙ ПЛЕЙСХОЛДЕР ПО ЦЕНТРУ ЭКРАНА!
    // =========================================================================
    // Пересчитываем актуальный индекс заставки, так как динамические вкладки удалились
    placeholderIndex = ui->centralStackedWidget->indexOf(ui->centralStackedWidget->findChild<QWidget*>("JETBRAINS_PLACEHOLDER"));
    if (placeholderIndex == -1) {
        placeholderIndex = this->property("placeholderIndex").toInt(); // Фолбэк на кэш свойств
    }

    if (panelOther) {
        panelOther->setVisible(false); // Скрываем нижнюю панель под кодом
    }
    if (btnTerminal) {
        btnTerminal->setChecked(false); // Сбрасываем триггер кнопки статусбара
    }

    if (ui->centralStackedWidget && placeholderIndex >= 0) {

        if (panelOther)  panelOther->setVisible(false);
        if (btnTerminal) btnTerminal->setChecked(false);

        // ЖЕСТКИЙ UX ФИКС: Принудительно гасим индикатор координат при закрытии проекта
        if (ui->cursorPosLabel) {
            ui->cursorPosLabel->hide();
        }

        ui->centralStackedWidget->setCurrentIndex(placeholderIndex);
        ui->centralStackedWidget->update();
        qDebug() << ">>> [УСПЕХ] Проект закрыт. Индикатор cursorPosLabel скрыт.";
    }
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
        QByteArray lspErrors = lspProcess->readAllStandardError();
        if (!lspErrors.isEmpty()) {
            fprintf(stderr, "\n[JEDI PYTHON ВНУТРЕННИЙ ЛОГ]: %s\n", lspErrors.constData());
            fflush(stderr);
        }
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
    // QString localLspBinary = "/home/elf/projects/z1/venv/bin/python";
    // this->venvPythonBinary = localLspBinary;

    QString localLspBinary;

    if (!currentOpenProjectPath.isEmpty() && QFile::exists(currentOpenProjectPath + "/venv/bin/python"))
    {
        // Отличный сценарий: venv существует, используем его
        localLspBinary = currentOpenProjectPath + "/venv/bin/python";
        this->venvPythonBinary = localLspBinary; // Фиксируем для всего приложения
        qDebug() << "[LSP_INIT] Используем изолированный venv проекта:" << localLspBinary;
    }
    else
    {
        // Сценарий инициализации или первого запуска на новом ПК
        localLspBinary = "/usr/bin/python"; // Используем системный Python временно, чтобы не было падения

        // ВАЖНО: Не перезаписываем venvPythonBinary системным путем,
        // если у нас уже сохранен какой-то рабочий путь!
        if (this->venvPythonBinary.isEmpty() || !QFile::exists(this->venvPythonBinary)) {
            this->venvPythonBinary = localLspBinary;
        }

        qWarning() << "[LSP_INIT] Локальный venv не найден. Временно подменяем системным интерпретатором:" << localLspBinary;
    }

    // 2. Возвращаем аргументы запуска модуля pylsp (теперь это КРИТИЧЕСКИ ВАЖНО!)
    QStringList lspArgs;
    lspArgs << "-m" << "pylsp";

    std::cerr << " [LSP СИСТЕМНЫЙ СТАРТ] Запускаю python-lsp-server с плагином pyflakes..." << std::endl;
    std::cerr.flush();

    // =========================================================================
    // КРИТИЧЕСКИЙ СИНХРОНИЗАТОР: Ловим момент, когда сервер официально запустится в Linux
    // =========================================================================
    connect(lspProcess, &QProcess::started, this, [this]() {
        qDebug() << ">>> [LSP ГОТОВ] Сервер успешно запущен системой!";

        if (!this->m_pendingAutoloadFile.isEmpty()) {
            qDebug() << ">>> [LSP] Провожу отложенную регистрацию восстановленного файла:" << this->m_pendingAutoloadFile;

            // Ищем активный редактор на текущей открытой вкладке centralStackedWidget
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (currentPage) {
                CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
                if (currentEditor && currentEditor->objectName() == this->m_pendingAutoloadFile) {

                    // 1. Ваш оригинальный вызов (Сервер готов — принудительно отправляем didOpen для первого файла) [INDEX]
                    currentEditor->sendLspDidOpen();

                    // =========================================================================
                    // ЖЕЛЕЗНЫЙ UX-ФИКС СТАРТА: Сразу же строим дерево функций для первого файла!
                    // =========================================================================
                    this->updateFunctionNavigator(currentEditor);
                    qDebug() << ">>> [СТАРТ] Навигатор функций comboDevice успешно проинициализирован при автозагрузке.";
                    // =========================================================================
                }
            }

        }
    });

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

    // --- ИСПРАВЛЕНИЕ: ЯВНО ОБЪЯВЛЯЕМ ВСЕ НЕОБХОДИМЫЕ ОБЪЕКТЫ QT ---
    QJsonObject capabilities;
    QJsonObject textDocumentCaps;

    // Описываем возможности синхронизации (убедитесь, что этот блок у вас есть)
    textDocumentCaps["synchronization"] = QJsonObject{
    {"dynamicRegistration", false},
    {"willSave", false},
    {"willSaveWaitUntil", false},
    {"didSave", true}
            };

    // Описываем поддержку быстрых исправлений Code Action
    QJsonObject codeActionCaps;
    codeActionCaps["dynamicRegistration"] = false;

    // Передаем серверу типы исправлений, которые мы умеем рисовать в QMenu
    QJsonArray codeActionKinds;
    codeActionKinds.append("quickfix");
    codeActionKinds.append("refactor");
    codeActionCaps["codeActionLiteralSupport"] = QJsonObject{{"codeActionKind", QJsonObject{{"valueSet", codeActionKinds}}}};

    // Привязываем поддержку к общим возможностям текстового документа
    textDocumentCaps["codeAction"] = codeActionCaps;

    QJsonObject hoverCaps;
    hoverCaps["dynamicRegistration"] = false;

    QJsonArray contentFormats;
    contentFormats.append("markdown");  // Объявляем поддержку Markdown (PyTorch)
    contentFormats.append("plaintext"); // Резервный чистый текст
    hoverCaps["contentFormat"] = contentFormats;

    // Привязываем Hover-возможности к общим возможностям текстового документа
    textDocumentCaps["hover"] = hoverCaps;

    // Собираем иерархию по спецификации LSP: params -> capabilities -> textDocument -> codeAction
    capabilities["textDocument"] = textDocumentCaps;
    params["capabilities"] = capabilities;

    // =========================================================================
    // ВСТАВЛЯЙТЕ НАШ КОД НАСТРОЙКИ ПЛАГИНОВ СТРОГО СЮДА:
    // =========================================================================
    QJsonObject settings;
    QJsonObject pylsp;
    QJsonObject plugins;

    // 1. Активируем pyflakes (он умеет собирать множественные ошибки синтаксиса)
    QJsonObject pyflakes;
    pyflakes["enabled"] = true;
    plugins["pyflakes"] = pyflakes;

    QJsonObject rope_changes;
    rope_changes["enabled"] = true;
    plugins["rope_changes"] = rope_changes;

    QJsonObject rope_completion;
    rope_completion["enabled"] = true;
    plugins["rope_completion"] = rope_completion;

    QJsonObject rope;
    rope["enabled"] = true;
    plugins["rope"] = rope;

    // 2. Отключаем лишний спам по стилю кода (PEP8), чтобы не тормозил PyTorch
    QJsonObject pycodestyle;
    pycodestyle["enabled"] = false;
    plugins["pycodestyle"] = pycodestyle;

    // Упаковываем ветки по стандарту протокола LSP для pylsp
    pylsp["plugins"] = plugins;
    settings["pylsp"] = pylsp;
    params["settings"] = settings; // Привязываем настройки к объекту params!

    // Сборка оптимизированных настроек Jedi Settings
    QJsonObject initializationOptions;
    QJsonObject jediSettings;

    // Указываем путь к Python интерпретатору venv, чтобы сервер подхватил установленный torch
    jediSettings["pythonExecutablePath"] = "/home/elf/projects/z1/venv/bin/python";

    QJsonObject diagnosticsObj;
    diagnosticsObj["enable"] = true;            // Принудительно включаем диагностику на лету
    diagnosticsObj["didChange"] = true;         // Запускаем перерасчет при каждой паузе ввода
    jediSettings["diagnostics"] = diagnosticsObj; // Привязываем к ядру Jedi

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

    // В САМЫЙ КОНЕЦ МЕТОДА ИНИЦИАЛИЗАЦИИ:
    if (!this->m_pendingAutoloadFile.isEmpty() && QFile::exists(this->m_pendingAutoloadFile))
    {
        qDebug() << ">>> [LSP ГОТОВ] Сервер запущен. Провожу отложенную регистрацию файла:" << m_pendingAutoloadFile;

        QFile autoFile(this->m_pendingAutoloadFile);
        if (autoFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString autoContent = QString::fromUtf8(autoFile.readAll());
            autoFile.close();

            // Теперь сервер гарантированно запущен и успешно примет этот пакет!
            this->sendLspDidOpenForFile(this->m_pendingAutoloadFile, autoContent);
        }

        // Очищаем переменную, чтобы не слать пакет повторно
        this->m_pendingAutoloadFile.clear();
    }
}

void Neuro_programm::sendLspRequest(const QString &method, const QJsonObject &params, int id)
{
    if (!lspProcess || lspProcess->state() != QProcess::Running) {
        qWarning() << "[LSP TRANSPORTER] Сервер не запущен. Запрос отклонен:" << method;
        return;
    }

    // 1. Собираем чистый JSON-пакет
    QJsonObject jsonObject;
    jsonObject["jsonrpc"] = "2.0";
    jsonObject["method"] = method;
    jsonObject["params"] = params;

    if (id > 0) {
        jsonObject["id"] = id;
    }

    QJsonDocument jsonDocument(jsonObject);
    // Берем компактный сырой JSON-текст без лишних пробелов
    QByteArray jsonBytes = jsonDocument.toJson(QJsonDocument::Compact);

    // 2. СТРОГО ПО СПЕЦИФИКАЦИИ LSP: Формируем правильный заголовок!
    // Длина должна считаться именно в БАЙТАХ (jsonBytes.size()), а не в символах строки!
    QByteArray lspHeader = QString("Content-Length: %1\r\n\r\n").arg(jsonBytes.size()).toUtf8();

    if (method == "textDocument/completion") {
        qDebug() << ">>> [ОТПРАВКА LSP completion] ID:" << id << "JSON:" << jsonBytes;
    }

    // 3. Отправляем в пайп СНАЧАЛА заголовок, а ЗАТЕМ сам JSON
    lspProcess->write(lspHeader);
    lspProcess->write(jsonBytes);

    // Принудительно выталкиваем данные в Linux пайп, чтобы убрать микрофризы
    //lspProcess->waitForBytesWritten(30);
}


// void Neuro_programm::readLspResponse()
// {
//     if (!lspProcess) return;

//     // 1. Забираем только свежие сырые байты из пайпа
//     QByteArray rawData = lspProcess->readAllStandardOutput();
//     if (rawData.isEmpty()) return;

//     // Накапливаем байты в статическом буфере класса
//     static QByteArray lspBuffer;
//     lspBuffer.append(rawData);

//     // =========================================================================
//     // СТАНДАРТНЫЙ ИНДУСТРИАЛЬНЫЙ СТРИМ-ПАРСЕР ДЛЯ LSP ПРОТОКОЛА
//     // =========================================================================
//     while (!lspBuffer.isEmpty())
//     {
//         // Находим, где в буфере начинается сам JSON-объект (ищем первую фигурную скобку)
//         int jsonStartIndex = lspBuffer.indexOf('{');

//         if (jsonStartIndex == -1) {
//             // Если фигурной скобки вообще нет, значит в буфере лежит только текстовый заголовок.
//             // Мы просто выходим и ждем, когда из пайпа догрузится сам JSON.
//             return;
//         }

//         // Проверяем, если перед фигурной скобкой застрял заголовок Content-Length,
//         // мы временно заглядываем в него, чтобы узнать точную длину пакета.
//         int headerIndex = lspBuffer.indexOf("Content-Length:");
//         int expectedLength = 0;
//         if (headerIndex != -1 && headerIndex < jsonStartIndex) {
//             int headerEndIndex = lspBuffer.indexOf("\r\n\r\n", headerIndex);
//             if (headerEndIndex != -1) {
//                 int valStart = headerIndex + 15;
//                 expectedLength = lspBuffer.mid(valStart, headerEndIndex - valStart).trimmed().toInt();
//             }
//         }

//         // Если мы смогли узнать ожидаемую длину, проверяем, накопилось ли столько байт в буфере.
//         // Если буфер меньше, значит пакет еще долетает по сети. Выходим и ждем readyRead!
//         if (expectedLength > 0 && lspBuffer.size() < (jsonStartIndex + expectedLength)) {
//             return;
//         }

//         // Вырезаем кусок буфера, начиная строго от фигурной скобки '{' и до конца буфера
//         QByteArray jsonCandidate = lspBuffer.mid(jsonStartIndex);

//         // Позволяем встроенному парсеру Qt САМОМУ распарсить JSON.
//         // Qt безупречно определяет реальные границы объекта по балансу фигурных скобок {},
//         // полностью игнорируя любые проблемы со смещениями строк в заголовках!
//         QJsonParseError parseError;
//         QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate, &parseError);

//         // СЛУЧАЙ 1: Пакет оборван на полуслове (парсер ругается на неожиданный конец файла)
//         if (parseError.error == QJsonParseError::UnterminatedObject ||
//             parseError.error == QJsonParseError::UnterminatedArray)
//         {
//             return; // Спокойно выходим и ждем, когда QProcess догрузит оставшиеся байты
//         }

//         // СЛУЧАЙ 2: Пакет успешно распарсился!
//         if (parseError.error == QJsonParseError::NoError)
//         {
//             // Вычисляем, сколько байт реально занял этот JSON-объект на диске
//             int actualJsonSize = doc.toJson(QJsonDocument::Compact).size();

//             // Намертво стираем из буфера обработанный заголовок и сам JSON,
//             // продвигая очередь строго к следующему LSP-пакету



void Neuro_programm::readLspResponse()
{
    if (!lspProcess) return;
    QByteArray rawData = lspProcess->readAllStandardOutput();
    if (rawData.isEmpty()) return;
    static QByteArray lspBuffer;
    lspBuffer.append(rawData);
    while (!lspBuffer.isEmpty()) {
        int jsonStartIndex = lspBuffer.indexOf('{');
        if (jsonStartIndex == -1) return;
        int headerIndex = lspBuffer.indexOf("Content-Length:");
        int expectedLength = 0;
        if (headerIndex != -1 && headerIndex < jsonStartIndex) {
            int headerEndIndex = lspBuffer.indexOf("\r\n\r\n", headerIndex);
            if (headerEndIndex != -1) {
                int valStart = headerIndex + 15;
                expectedLength = lspBuffer.mid(valStart, headerEndIndex - valStart).trimmed().toInt();
            }
        }
        if (expectedLength > 0 && lspBuffer.size() < (jsonStartIndex + expectedLength)) return;
        QByteArray jsonCandidate = lspBuffer.mid(jsonStartIndex, expectedLength > 0 ? expectedLength : -1);
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonCandidate, &parseError);
        if (parseError.error == QJsonParseError::UnterminatedObject || parseError.error == QJsonParseError::UnterminatedArray) return;
        if (parseError.error == QJsonParseError::NoError) {
            QJsonObject rootObj = doc.object();
            int actualJsonSize = jsonCandidate.size();
            int bytesToRemove = jsonStartIndex + actualJsonSize;
            while (bytesToRemove < lspBuffer.size() && (lspBuffer[bytesToRemove] == '\r' || lspBuffer[bytesToRemove] == '\n')) bytesToRemove++;
            lspBuffer.remove(0, bytesToRemove);
            if (rootObj.value("method").toString() == "textDocument/publishDiagnostics") {
                QJsonObject params = rootObj.value("params").toObject();
                QJsonArray diagnostics = params.value("diagnostics").toArray();
                Neuro_programm::globalLspErrors.clear();
                QList<QTextEdit::ExtraSelection> newSelections;
                CodeEditor* activeEditor = nullptr;
                if (ui->centralStackedWidget && ui->centralStackedWidget->currentWidget()) {
                    activeEditor = ui->centralStackedWidget->currentWidget()->findChild<CodeEditor*>();
                }
                for (int i = 0; i < diagnostics.size(); ++i) {
                    QJsonObject diagObj = diagnostics[i].toObject();
                    int severity = diagObj.value("severity").toInt();
                    QJsonObject range = diagObj.value("range").toObject();
                    QJsonObject start = range.value("start").toObject();
                    QJsonObject end = range.value("end").toObject();
                    Neuro_programm::LspErrorData errorBlock;
                    errorBlock.line = start.value("line").toInt();
                    errorBlock.startChar = start.value("character").toInt();
                    errorBlock.endChar = end.value("character").toInt();
                    errorBlock.isError = (severity == 1);
                    errorBlock.message = diagObj.value("message").toString();
                    QJsonValue codeVal = diagObj.value("code");
                    if (codeVal.isString()) errorBlock.code = codeVal.toString();
                    else if (codeVal.isDouble()) errorBlock.code = QString::number(codeVal.toInt());
                    else errorBlock.code = diagObj.value("code").toVariant().toString();
                    Neuro_programm::globalLspErrors.append(errorBlock);
                }
                if (activeEditor && activeEditor->document()) {
                    int maxDocLength = activeEditor->document()->characterCount();
                    for (const auto& errorBlock : std::as_const(Neuro_programm::globalLspErrors)) {
                        QTextCursor cursor(activeEditor->document());
                        QTextBlock block = activeEditor->document()->findBlockByLineNumber(errorBlock.line);
                        if (block.isValid()) {
                            int startPos = block.position() + errorBlock.startChar;
                            int endPos = block.position() + (errorBlock.endChar <= errorBlock.startChar ? errorBlock.startChar + 1 : errorBlock.endChar);
                            if (startPos >= 0 && startPos < maxDocLength && endPos >= 0 && endPos <= maxDocLength) {
                                QTextEdit::ExtraSelection selection;
                                selection.format.setUnderlineColor(QColor(255, 42, 42));
                                selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                                selection.format.setFontUnderline(true);
                                selection.format.setBackground(QColor(255, 42, 42, 35));
                                cursor.setPosition(startPos);
                                cursor.setPosition(endPos, QTextCursor::KeepAnchor);
                                selection.cursor = cursor;
                                newSelections.append(selection);
                            }
                        }
                    }
                    QMetaObject::invokeMethod(activeEditor, "applySelectionsFromLsp", Qt::QueuedConnection, Q_ARG(QList<QTextEdit::ExtraSelection>, newSelections));
                    int totalChars = static_cast<int>(activeEditor->document()->characterCount());
                    QPointer<CodeEditor> safeEditor(activeEditor);
                    QMetaObject::invokeMethod(this, [safeEditor, totalChars]() {
                        if (safeEditor.isNull()) return;
                        if (safeEditor->document() && safeEditor->viewport()) {
                            safeEditor->document()->markContentsDirty(0, totalChars);
                            safeEditor->viewport()->update();
                        }
                    }, Qt::QueuedConnection);
                }
                if (ui->centralStackedWidget && this->statusBar()) {
                    this->statusBar()->clearMessage();
                    if (!newSelections.isEmpty()) {
                        this->statusBar()->showMessage(QString("Jedi: Обнаружено ошибок синтаксиса: %1").arg(newSelections.size()));
                        this->statusBar()->setStyleSheet("QStatusBar { color: #ff2a2a; font-weight: bold; }");
                    } else {
                        this->statusBar()->showMessage("Jedi: Ошибок в коде не найдено", 3000);
                        this->statusBar()->setStyleSheet("QStatusBar { color: #00ff00; font-weight: normal; }");
                    }
                    this->statusBar()->repaint();
                }
                continue;
            }
            int responseId = rootObj.value("id").toInt(-1);
            if (responseId == 888 && rootObj.contains("result")) {
                QJsonObject resultObj = rootObj["result"].toObject();
                QString docstringText = "";
                if (resultObj.contains("contents")) {
                    QJsonValue contentsVal = resultObj["contents"];
                    if (contentsVal.isString()) docstringText = contentsVal.toString();
                    else if (contentsVal.isObject()) {
                        QJsonObject contentsObj = contentsVal.toObject();
                        if (contentsObj.contains("value")) docstringText = contentsObj.value("value").toString();
                        else if (contentsObj.contains("text")) docstringText = contentsObj.value("text").toString();
                    } else if (contentsVal.isArray()) {
                        QJsonArray contentsArr = contentsVal.toArray();
                        if (!contentsArr.isEmpty()) {
                            QJsonValue firstItem = contentsArr.at(0);
                            if (firstItem.isString()) docstringText = firstItem.toString();
                            else if (firstItem.isObject()) docstringText = firstItem.toObject().value("value").toString();
                        }
                    }
                }
                docstringText = docstringText.trimmed();
                if (!docstringText.isEmpty()) {
                    if (docstringText.startsWith("```python")) docstringText.remove(0, 9);
                    if (docstringText.endsWith("```")) docstringText.chop(3);
                    docstringText = docstringText.trimmed();
                    docstringText.replace("\\n", "<br/>");
                    docstringText.replace("\n", "<br/>");
                    docstringText.replace("**", "<b>");
                    CodeEditor* activeEditor = nullptr;
                    if (ui->centralStackedWidget && ui->centralStackedWidget->currentWidget()) {
                        activeEditor = ui->centralStackedWidget->currentWidget()->findChild<CodeEditor*>();
                    }
                    if (activeEditor) {
                        QPoint globalMousePos = activeEditor->property("lastTooltipGlobalPos").toPoint();
                        QString htmlTooltip = docstringText;
                        QMetaObject::invokeMethod(this, [this, activeEditor, htmlTooltip, globalMousePos]() {
                            QVariant oldVar = activeEditor->property("currentHoverWidget");
                            if (oldVar.isValid()) {
                                QWidget *oldWindow = oldVar.value<QWidget*>();
                                if (oldWindow) { oldWindow->close(); oldWindow->deleteLater(); }
                            }
                            QWidget *hoverWindow = new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
                            hoverWindow->setObjectName("activeLspHoverWindow");
                            QVBoxLayout *layout = new QVBoxLayout(hoverWindow);
                            layout->setContentsMargins(12, 12, 12, 12);
                            QLabel *label = new QLabel(hoverWindow);
                            label->setTextFormat(Qt::RichText);
                            label->setText(QString("<div style='color: #232629; font-family: monospace; font-size: 11pt; line-height: 145%;'><b> Справка PyTorch Studio (Hover):</b><br/><br/>%1</div>").arg(htmlTooltip));
                            hoverWindow->setStyleSheet("QWidget { background-color: #fcfcfc; border: 1px solid #b0b3b6; border-radius: 4px; }");
                            label->setWordWrap(true);
                            label->setMaximumWidth(500);
                            layout->addWidget(label);
                            hoverWindow->move(globalMousePos.x() + 15, globalMousePos.y() + 25);
                            activeEditor->setProperty("currentHoverWidget", QVariant::fromValue(hoverWindow));
                            hoverWindow->show();
                        }, Qt::QueuedConnection);
                    }
                }
                continue;
            }
            if (responseId == 100 && rootObj.contains("result")) {
                QJsonValue resultVal = rootObj["result"];
                QJsonArray itemsArray;
                if (resultVal.isArray()) itemsArray = resultVal.toArray();
                else if (resultVal.isObject()) {
                    QJsonObject resObj = resultVal.toObject();
                    if (resObj.contains("items")) itemsArray = resObj["items"].toArray();
                }
                QStringList completionSuggestions;
                for (int i = 0; i < itemsArray.size(); ++i) {
                    QJsonObject item = itemsArray[i].toObject();
                    QString label = item.value("label").toString();
                    if (!label.isEmpty()) completionSuggestions.append(label);
                }
                completionSuggestions.sort(Qt::CaseInsensitive);
                CodeEditor* activeEditor = nullptr;
                if (ui->centralStackedWidget && ui->centralStackedWidget->currentWidget()) {
                    activeEditor = ui->centralStackedWidget->currentWidget()->findChild<CodeEditor*>();
                }
                if (activeEditor && !completionSuggestions.isEmpty()) {
                    QMetaObject::invokeMethod(activeEditor, "showLspCompletionsInGui", Qt::QueuedConnection, Q_ARG(QStringList, completionSuggestions));
                }
                continue;
            }
        }
    }
}



QString Neuro_programm::getCurrentOpenFilePath() const
{
    if (!ui->fileComboBox) return "";
    int idx = ui->fileComboBox->currentIndex();
    if (idx < 2) return "";
    return ui->fileComboBox->itemData(idx).toString();
}

void Neuro_programm::updateTabName()
{
    QWidget *currentPage = ui->centralStackedWidget->currentWidget();
    if (!currentPage) {
        this->setWindowTitle("PyTorch Studio");
        return;
    }

    // Извлекаем абсолютный путь (задан на странице 26 как objectName)
    QString absoluteFilePath = currentPage->objectName();

    // Если это сервисные экраны (MAIN_SCREEN или AI_CHAT_SCREEN) — пишем простое имя
    if (absoluteFilePath.isEmpty() || absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") {
        this->setWindowTitle("PyTorch Studio");
        return;
    }

    QFileInfo fileInfo(absoluteFilePath);
    QString baseName = fileInfo.fileName(); // Извлекает чистое имя файла (например, train.py)

    // КРИТИЧЕСКИЙ МАРКЕР ДЛЯ ОС: Обязательно добавляем [*]
    // Если setWindowModified(true) -> маркер станет звёздочкой, если false -> исчезнет
    QString displayName = baseName + " [*] — PyTorch Studio";

    this->setWindowTitle(displayName);
}

void Neuro_programm::showCompletionMenuInGuiThread(const QStringList &completions)
{
    if (completions.isEmpty()) return;

    QWidget *currentWidget = ui->centralStackedWidget->currentWidget();
    if (!currentWidget) return;

    CodeEditor *activeEditor = currentWidget->findChild<CodeEditor*>();
    if (!activeEditor) return;

    // ЖЕСТКИЙ ФИКС №1: Если окно уже открыто на экране, полностью игнорируем
    // асинхронные повторные пакеты от Jedi, чтобы они не сбрасывали ввод букв!
    if (activeEditor->m_popupWindow && activeEditor->m_popupWindow->isVisible()) {
        return;
    }

    // Создаем контейнер автодополнения (Breeze Light)
    QWidget *popupWindow = new QWidget(activeEditor, Qt::Popup | Qt::FramelessWindowHint);
    popupWindow->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(popupWindow);
    layout->setContentsMargins(0, 0, 0, 0);

    QListWidget *listWidget = new QListWidget(popupWindow);
    listWidget->setObjectName("completionListWidget");
    listWidget->addItems(completions);
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Стилизация KDE Breeze Light
    listWidget->setStyleSheet(
                "QListWidget { background-color: #fcfcfc; color: #232629; border: 1px solid #c7c7c7; font-family: monospace; font-size: 11pt; }"
                "QListWidget::item { padding: 4px 8px; }"
                "QListWidget::item:hover { background-color: #eff0f1; color: #232629; }"
                "QListWidget::item:selected { background-color: #3daee9; color: #ffffff; }"
                "QScrollBar:vertical { background-color: #eff0f1; width: 10px; margin: 0px; }"
                "QScrollBar::handle:vertical { background-color: #b0b3b6; min-height: 20px; border-radius: 2px; margin: 1px; }"
                );

    layout->addWidget(listWidget);
    listWidget->installEventFilter(this);

    // Функция-лямбда для централизованной вставки выбранного текста в редактор
    auto insertSelectedCompletion = [activeEditor, listWidget, popupWindow]() {
        QListWidgetItem *currentItem = listWidget->currentItem();
        if (currentItem && !currentItem->isHidden()) {
            QString itemText = currentItem->text();
            QTextCursor tc = activeEditor->textCursor();

            // Вычисляем длину набранного префикса на основе последней точки в строке
            QString lineText = tc.block().text().left(tc.columnNumber());
            int lastDot = lineText.lastIndexOf('.');
            int charsToErase = (lastDot != -1) ? (lineText.length() - (lastDot + 1)) : 0;

            tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, charsToErase);
            tc.beginEditBlock();
            tc.insertText(itemText);
            tc.endEditBlock();
            activeEditor->setTextCursor(tc);
        }
        popupWindow->close();
        activeEditor->setFocus();
    };

    // Коннект для клика мыши
    connect(listWidget, &QListWidget::itemClicked, this, insertSelectedCompletion);

    // =========================================================================
    // ЖЕСТКИЙ ФИКС №2: ИНДУСТРИАЛЬНЫЙ СУПЕР-ФИЛЬТР КЛАВИШ (Enter и буквы)
    // Мы перехватываем события клавиатуры ПРЯМО внутри всплывающего списка!
    // =========================================================================
    listWidget->installEventFilter(this);
    connect(this, &QObject::destroyed, popupWindow, &QWidget::close);

    // Переопределяем фильтр событий динамически через лямбду метаобъекта Qt
    popupWindow->setProperty("cleanErase", true);

    // Передаем указатели в класс редактора
    activeEditor->m_popupWindow = popupWindow;
    activeEditor->m_listWidget = listWidget;

    // Устанавливаем Wayland-геометрию окна под курсором
    QTextCursor cursor = activeEditor->textCursor();
    QRect cursorRect = activeEditor->cursorRect(cursor);
    QPoint globalPos = activeEditor->mapToGlobal(cursorRect.bottomLeft());
    globalPos.setY(globalPos.y() + 5);

    int width = listWidget->sizeHintForColumn(0) + 40;
    if (width < 250) width = 250;
    if (width > 450) width = 450;

    popupWindow->setGeometry(globalPos.x(), globalPos.y(), width, 200);

    // Принудительно выводим на экран
    popupWindow->show();

    // Настраиваем стартовое синее выделение первого элемента (строго после show!)
    if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
        if (QListWidgetItem *firstItem = listWidget->item(0)) {
            firstItem->setSelected(true);
            listWidget->setCurrentItem(firstItem);
        }
        // Отдаем фокус ввода списку, чтобы Linux перехватывал клавиши навигации
        listWidget->setFocus(Qt::PopupFocusReason);
    }
}

bool Neuro_programm::eventFilter(QObject *obj, QEvent *event)
{
    static QPoint dragStartPos;
    static bool isDraggingTitle = false;

    if (event->type() == QEvent::Wheel) {
        return QMainWindow::eventFilter(obj, event);
    }

    // Игнорируем клики по кнопкам управления и менюбару, чтобы не ломать их нажатия
    QString className = obj->metaObject()->className();
    if (className == "QPushButton" || className == "QMenuBar" || className == "QComboBox" ||
            obj->objectName() == "btnMinimize" || obj->objectName() == "btnMaximize" || obj->objectName() == "btnClose")
    {
        return QMainWindow::eventFilter(obj, event);
    }

    // =========================================================================
    // БЛОК 1: АСИНХРОННОЕ СИСТЕМНОЕ ПЕРЕТАСКИВАНИЕ + ДВОЙНОЙ КЛИК (DEBBUG FIXED)
    // =========================================================================

    // 1. АППАРАТНЫЙ ПЕРЕХВАТ ДВОЙНОГО ЩЕЛЧКА ЛКМ (Сворачивание в Normal <-> Maximized)
    if (event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QPoint windowLocalPos = this->mapFromGlobal(QCursor::pos());
            if (windowLocalPos.y() >= 0 && windowLocalPos.y() <= 100)
            {
                if (this->isMaximized()) {
                    this->showNormal();   // Сжимаем обратно в оконный режим
                } else {
                    this->showMaximized(); // Разворачиваем на весь экран
                }
                event->accept();
                return true; // Поглощаем событие двойного клика
            }
        }
    }

    // 2. БЕЗОПАСНЫЙ ПРОБРОС ПЕРЕТАСКИВАНИЯ В ЯДРО LINUX (ПРИ НАЖАТИИ ЛКМ)
    // 2. БЕЗОПАСНЫЙ ПРОБРОС ПЕРЕТАСКИВАНИЯ В ЯДРО LINUX (НАЖАТИЕ + ПЕРЕМЕЩЕНИЕ)
    else if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QPoint windowLocalPos = this->mapFromGlobal(QCursor::pos());
            // Проверяем нажатие в нашей 100-пиксельной верхней зоне шапки
            if (windowLocalPos.y() >= 0 && windowLocalPos.y() <= 100)
            {
                // Запоминаем локальную точку клика для ручного сдвига развернутого окна
                dragStartPos = mouseEvent->pos();
                isDraggingTitle = true;
            }
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (isDraggingTitle && (mouseEvent->buttons() & Qt::LeftButton))
        {
            // КРИТИЧЕСКИЙ ФИКС ДЛЯ РАЗВЕРНУТОГО ОКНА:
            if (this->isMaximized())
            {
                // Считаем пропорцию клика по горизонтали, чтобы окно не прыгало краем
                double relativeX = static_cast<double>(mouseEvent->globalPosition().x()) / this->width();

                // Сбрасываем окно из полноэкранного режима в нормальный оконный
                this->showNormal();

                // Вычисляем новые координаты окна, чтобы курсор остался ровно под мышкой
                int newX = mouseEvent->globalPosition().x() - (this->width() * relativeX);
                int newY = mouseEvent->globalPosition().y() - dragStartPos.y();
                this->move(newX, newY);

                // Корректируем точку захвата под геометрию уже нормального окна
                dragStartPos = QPoint(this->width() * relativeX, dragStartPos.y());
                return true;
            }

            // Если окно уже в нормальном режиме — передаем управление менеджеру окон Linux (KWin)
            if (this->windowHandle())
            {
                isDraggingTitle = false; // Системный вызов сам завершит перемещение

                // ХАК ДЛЯ ОТЛАДКИ (Снимает Deadlock в GDB)
                QMouseEvent releaseEvent(QEvent::MouseButtonRelease, mouseEvent->position(),
                                         mouseEvent->globalPosition(), Qt::LeftButton,
                                         Qt::NoButton, mouseEvent->modifiers());
                QCoreApplication::sendEvent(obj, &releaseEvent);

                this->windowHandle()->startSystemMove();
                return true;
            }
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        isDraggingTitle = false;
    }



    // =========================================================================
    // БЛОК 2: КОД ДЛЯ РАБОТЫ С JEDI (ПОЛНОСТЬЮ ИЗОЛИРОВАН ОТ МЫШИ)
    // =========================================================================
    if (this->activeCompletionPopup && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QListWidget *listWidget = this->activeCompletionPopup->findChild<QListWidget*>("completionListWidget");
        if (!listWidget) {
            listWidget = this->activeCompletionPopup->findChild<QListWidget*>();
        }

        CodeEditor *editor = nullptr;
        if (ui->centralStackedWidget && ui->centralStackedWidget->currentWidget()) {
            editor = ui->centralStackedWidget->currentWidget()->findChild<CodeEditor*>();
        }

        if (listWidget && editor)
        {
            // --- 1. УПРАВЛЕНИЕ СТРЕЛКАМИ ВВЕРХ / ВНИЗ ---
            if (keyEvent->key() == Qt::Key_Up) {
                int currentRow = listWidget->currentRow();
                for (int i = currentRow - 1; i >= 0; --i) {
                    if (!listWidget->item(i)->isHidden()) {
                        listWidget->setCurrentRow(i);
                        listWidget->scrollToItem(listWidget->item(i), QAbstractItemView::EnsureVisible);
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
                        listWidget->scrollToItem(listWidget->item(i), QAbstractItemView::EnsureVisible);
                        break;
                    }
                }
                return true;
            }

            // --- 2. ПОДТВЕРЖДЕНИЕ ВЫБОРА (ENTER / RETURN / TAB) ---
            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Tab)
            {
                QListWidgetItem *currentItem = listWidget->currentItem();
                if (currentItem && !currentItem->isHidden())
                {
                    QString itemText = currentItem->text();
                    QTextCursor tc = editor->textCursor();
                    QString lineText = tc.block().text().left(tc.columnNumber());
                    int lastDot = lineText.lastIndexOf('.');
                    int charsToErase = (lastDot != -1) ? (lineText.length() - (lastDot + 1)) : 0;

                    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, charsToErase);
                    tc.beginEditBlock();
                    tc.insertText(itemText);
                    tc.endEditBlock();
                    editor->setTextCursor(tc);
                }
                this->activeCompletionPopup->close();
                this->activeCompletionPopup = nullptr;
                editor->setFocus();
                return true;
            }

            // --- 3. ЗАКРЫТИЕ ПО ESC ---
            if (keyEvent->key() == Qt::Key_Escape) {
                this->activeCompletionPopup->close();
                this->activeCompletionPopup = nullptr;
                editor->setFocus();
                return true;
            }

            // --- 4. НАКОПИТЕЛЬНАЯ ДИНАМИЧЕСКАЯ ФИЛЬТРАЦИЯ НА ЛЕТУ ---
            if (!keyEvent->text().isEmpty() || keyEvent->key() == Qt::Key_Backspace)
            {
                editor->document()->blockSignals(true);
                if (editor->viewport()) {
                    QCoreApplication::sendEvent(editor->viewport(), keyEvent);
                } else {
                    QCoreApplication::sendEvent(editor, keyEvent);
                }
                editor->document()->blockSignals(false);

                QTextCursor tc = editor->textCursor();
                QString lineText = tc.block().text().left(tc.columnNumber());
                int lastDotIndex = lineText.lastIndexOf('.');
                if (lastDotIndex == -1) {
                    this->activeCompletionPopup->close();
                    this->activeCompletionPopup = nullptr;
                    editor->setFocus();
                    return true;
                }

                QString currentPrefix = lineText.mid(lastDotIndex + 1).toLower();
                int firstVisibleRow = -1;

                listWidget->setUpdatesEnabled(false);
                for (int i = 0; i < listWidget->count(); ++i) {
                    QListWidgetItem *item = listWidget->item(i);
                    if (item) {
                        bool matches = item->text().toLower().startsWith(currentPrefix);
                        item->setHidden(!matches);
                        if (matches && firstVisibleRow == -1) {
                            firstVisibleRow = i;
                        }
                    }
                }
                listWidget->setUpdatesEnabled(true);

                if (firstVisibleRow != -1) {
                    listWidget->setCurrentRow(firstVisibleRow);
                    if (QListWidgetItem *firstItem = listWidget->item(firstVisibleRow)) {
                        firstItem->setSelected(true);
                        listWidget->scrollToItem(firstItem, QAbstractItemView::EnsureVisible);
                    }
                } else {
                    this->activeCompletionPopup->close();
                    this->activeCompletionPopup = nullptr;
                }

                QString absoluteFilePath = editor->property("filePath").toString();
                if (absoluteFilePath.isEmpty()) {
                    absoluteFilePath = editor->objectName();
                }
                if (!absoluteFilePath.isEmpty() && !this->isWindowModified())
                {QFileInfo info(absoluteFilePath);
                    this->setWindowModified(true);
                    editor->document()->setModified(true);
                    if (ui->fileComboBox)
                    {int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
                        if (comboIdx != -1) ui->fileComboBox->setItemText(comboIdx, info.fileName() + " *");
                    }
                }
                return true;
            }
        }
    }
    // =========================================================================
    // БЛОК 3: ПРИНУДИТЕЛЬНОЕ ДОБАВЛЕНИЕ ЗВЁЗДОЧКИ ПРИ РЕДАКТИРОВАНИИ
    // =========================================================================
    if (event->type() == QEvent::KeyPress)
    {
        CodeEditor *editor = qobject_cast<CodeEditor*>(obj);
        if (editor == nullptr && obj->parent() != nullptr)
        {
            editor = qobject_cast<CodeEditor*>(obj->parent());
        }
        if (editor)
        {QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            int key = keyEvent->key();
            if (key != Qt::Key_Left && key != Qt::Key_Right &&key != Qt::Key_Up && key != Qt::Key_Down &&key != Qt::Key_Control && key != Qt::Key_Shift &&key != Qt::Key_Alt && key != Qt::Key_CapsLock &&key != Qt::Key_Escape && key != Qt::Key_PageUp &&key != Qt::Key_PageDown && key != Qt::Key_Home && key != Qt::Key_End)
            {QString absoluteFilePath = obj->property("filePath").toString();
                if (absoluteFilePath.isEmpty() && editor)
                {absoluteFilePath = editor->property("filePath").toString();
                }
                if (absoluteFilePath.isEmpty() && editor)
                {absoluteFilePath = editor->objectName();
                }
                if (!this->isWindowModified() && !absoluteFilePath.isEmpty())
                {this->setWindowModified(true);
                    editor->document()->setModified(true);
                    QFileInfo info(absoluteFilePath);
                    if (ui->fileComboBox)
                    {int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
                        if (comboIdx != -1)
                        {ui->fileComboBox->setItemText(comboIdx, info.fileName() + " *");
                        }
                    }
                    if (ui->openFilesListWidget)
                    {for (int i = 0; i < ui->openFilesListWidget->count(); ++i)
                        {QListWidgetItem *item = ui->openFilesListWidget->item(i);
                            if (item && item->data(Qt::UserRole).toString() == absoluteFilePath)
                            {item->setText(info.fileName() + " *");
                                break;
                            }
                        }
                    }
                    updateTabName();
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void Neuro_programm::sendInitialWelcomeRequest()
{
    // Динамически ищем элементы ввода в ОЗУ, защищаясь от nullptr
    QTextEdit *chatInput = ui->centralwidget->findChild<QTextEdit*>("inputChatText");
    QPushButton *chatSendBtn = ui->centralwidget->findChild<QPushButton*>("btnSendChat");

    // Блокируем интерфейс, пока ИИ не поприветствует пользователя (только если виджеты уже созданы)
    if (chatInput) chatInput->setEnabled(false);
    if (chatSendBtn) chatSendBtn->setEnabled(false);

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

        // ДИНАМИЧЕСКИЙ ФИКС ВНУТРИ ПОТОКА ОТВЕТА: Ищем виджеты заново на случай, если пользователь открыл чат, пока шел запрос
        QTextEdit *safeInput = ui->centralwidget->findChild<QTextEdit*>("inputChatText");
        QPushButton *safeSendBtn = ui->centralwidget->findChild<QPushButton*>("btnSendChat");
        QTextBrowser *safeChatLog = ui->centralwidget->findChild<QTextBrowser*>("chatLogWidget");

        // Разблокируем интерфейс для работы, если он существует в памяти
        if (safeInput) {
            safeInput->setEnabled(true);
            safeInput->setFocus();
        }
        if (safeSendBtn) {
            safeSendBtn->setEnabled(true);
        }

        // Пытаемся почистить маркер ожидания ("ИИ-Ассистент подключается..."), только если лог-виджет валиден
        if (safeChatLog && !safeChatLog->toPlainText().isEmpty())
        {
            QTextCursor cursor = safeChatLog->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.select(QTextCursor::LineUnderCursor);
            if (cursor.selectedText().contains("подключается")) {
                cursor.removeSelectedText();
                cursor.deletePreviousChar();
            }
        }

        // Разбор ответа от Ollama
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray rawResponse = reply->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(rawResponse);
            QJsonObject responseObj = responseDoc.object();
            QJsonObject messageObj = responseObj["message"].toObject();
            QString aiWelcome = messageObj["content"].toString().trimmed();

            if (!aiWelcome.isEmpty() && safeChatLog)
            {
                // Выводим красивое приветствие от ИИ
                safeChatLog->append("<font color='#232629'><b>ИИ-Ассистент:</b><br>" + aiWelcome.toHtmlEscaped().replace("\n", "<br>") + "</font><br>");
            }
        }
        else
        {
            // Если Ollama не запущена, сразу сообщаем пользователю на старте (безопасно через safeChatLog)
            if (safeChatLog) {
                safeChatLog->append("<font color='#cc0000'><b>ИИ: Ошибка инициализации.</b> Локальная служба Ollama не отвечает. "
                                    "Запустите сервер через терминал: <i>'sudo systemctl start ollama'</i></font><br>");
            } else {
                qWarning() << "[OLLAMA ERROR] Сервер Ollama не отвечает. Запустите службу через systemctl!";
            }
        }

        if (safeChatLog) {
            safeChatLog->moveCursor(QTextCursor::End);
        }

        reply->deleteLater();
        networkManager->deleteLater();
    });
}

QString Neuro_programm::parseMarkdownCodeBlocks(const QString &rawText) {
    QString htmlResult = rawText.toHtmlEscaped();
    htmlResult.replace("&lt;br&gt;", "\n");

    static const QRegularExpression rx("```(?:python|py)?\\n([\\s\\S]*?)\\n```");
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
    QTimer::singleShot(100, this, [=]() {
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

void Neuro_programm::open_about_program()
{
    rsc3 = new About_program(this);
    rsc3->setWindowTitle("О программе");
    rsc3->exec();
}

void Neuro_programm::applyGlobalFonts()
{
    QSettings settings("PyTorchStudio", "EditorSettings");

    QString editorFamily = settings.value("Editor/FontFamily", "Monospace").toString();
    int editorSize = settings.value("Editor/FontSize", 12).toInt();
    QString currentTheme = settings.value("Theme/Name", "Светлая тема").toString();

    QFont editorFont(editorFamily, editorSize);
    QPalette palette;

    //qApp->setStyle(QStyleFactory::create("Fusion"));

    if (QStyleFactory::keys().contains("Breeze")) {
        qApp->setStyle(QStyleFactory::create("Breeze"));
    } else if (QStyleFactory::keys().contains("breeze")) {
        qApp->setStyle(QStyleFactory::create("breeze"));
    } else {
        qApp->setStyle(QStyleFactory::create("Fusion"));
    }

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
        QColor darkText(QColor(239, 240, 241));

        palette.setColor(QPalette::Window, darkBg);
        palette.setColor(QPalette::WindowText, darkText);
        palette.setColor(QPalette::Base, QColor(27, 30, 32));
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
        QColor lightText(35, 38, 41);

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

                                                                                                       "/* --- КАРТОЧКИ ПАРАМЕТРОВ ОБУЧЕНИЯ (ЖЕСТКАЯ ФИКСАЦИЯ СТИЛЯ) --- */"
                                                                                                       "/* Карточки параметров обучения */"
                                                                                                       "QGroupBox { "
                                                                                                       "   background-color: " + containerBgHex + " !important; "
                                                                                                                                                  "   border: 1px solid " + borderAccentHex + " !important; "
                                                                                                                                                                                              "   border-radius: 6px !important; "
                                                                                                                                                                                              "   margin-top: 15px !important; "
                                                                                                                                                                                              "   padding-top: 25px !important; " /* Оставляем большой отступ сверху, чтобы освободить место для текста внутри */
            "   padding-left: 15px !important; "
            "   padding-right: 15px !important; "
            "   padding-bottom: 15px !important; "
            "}"




            "/* Панель вкладок (оставляем старый стиль без изменений) */"
            "QTabWidget::panel { "
            "   background-color: " + containerBgHex + " !important; "
                                                       "   border: 1px solid " + borderAccentHex + "; "
                                                                                                   "   border-radius: 6px; "
                                                                                                   "}"


                                                                                                   "/* КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Находим шапку левого дока и жестко отодвигаем её под кнопки! */"
                                                                                                   "QDockWidget, QTreeView, QTreeWidget, QListWidget, #leftDockWidget { "
                                                                                                   "   background-color: " + containerBgHex + " !important; "
            //"   margin-top: 36px !important; " // Отодвигает надпись "Проект" ровно под линию кнопок
            "}"
            "QGroupBox QTreeView, QTabWidget QTreeView { margin-top: 0px !important; }" // Сброс для внутренних окон



            "/* --- ПОЛЯ ВВОДА И СПИСКИ В СТИЛЕ KDE PLASMA BREEZE (ФИНАЛ) --- */"
            "QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit { "
            "   background-color: " + containerBgHex + " !important; "
                                                       "   color: " + palette.color(QPalette::Text).name() + " !important; "
                                                                                                             "   border: 1px solid " + borderAccentHex + " !important; " /* Тонкая плоская рамка */
            "   border-radius: 3px !important; "
            "   padding: 4px 24px 4px 8px !important; " /* Отступ справа защищает текст от наложения на стрелочку */
            "   min-height: 22px !important; "
            "}"
            "QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover, QLineEdit:hover, "
            "QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QLineEdit:focus { "
            "   border: 1px solid " + textAccentHex + " !important; " /* Подсветка Breeze */
            "}"
            /* Код защиты статусбара QStatusBar QPushButton оставляем ниже без изменений */





            "/* Нижний статусбар (Защита от черного цвета кнопок) */"
            "QStatusBar { "
            "   border-top: 1px solid " + borderAccentHex + "; "
                                                            "   background-color: " + windowBgHex + " !important; "
                                                                                                    "}"
                                                                                                    "QStatusBar QPushButton { "
                                                                                                    "   background-color: transparent; " /* Прозрачный фон по умолчанию */
            "   color: " + palette.color(QPalette::WindowText).name() + " !important; "
                                                                        "   border: 1px solid transparent; "
                                                                        "   font-weight: bold; "
                                                                        "   padding: 4px 14px; "
                                                                        "   margin: 2px 4px; "
                                                                        "   border-radius: 4px; "
                                                                        "}"
                                                                        "QStatusBar QPushButton:hover { "
                                                                        "   background-color: " + (isDark ? "#35393d" : "#e6e6e6") + "; " /* Мягкий ховер, не ломающий Fusion */
            "}"
            "QStatusBar QPushButton:checked { "
            "   background-color: " + (isDark ? "#1b1e20" : "#cbd0d3") + " !important; " /* Фиксация серого цвета как в Qt Creator */
            "   color: " + (isDark ? "#ffffff" : "#232629") + " !important; "
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
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       "QStatusBar QPushButton { "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       "   background-color: transparent; "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       "   color: " + palette.color(QPalette::WindowText).name() + " !important; "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   "   border: none; "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   "   font-weight: bold; "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   "   padding: 4px 14px; "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   "   margin: 2px 4px; "
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   "   border-radius: 6px; " /* Скругление углов как на скриншоте */
            "}"
            "/* Наведение курсора (в половину темного) */"
            "/* Наведение курсора (ЯРКОЕ ВЫДЕЛЕНИЕ) */"
            "QStatusBar QPushButton:hover { "
            "   background-color: " + (isDark ? "#3daee9" : "#bdecff") + " !important; " /* Насыщенный синий для темной / Яркий голубой для светлой темы */
            "   color: " + (isDark ? "#ffffff" : "#004b73") + " !important; "             /* Белый текст на темной / Контрастный синий на светлой теме */
            "}"
            "/* СОСТОЯНИЕ НАЖАТИЯ: точная копия вкладки со скриншота */"
            "QStatusBar QPushButton:checked { "
            "   background-color: #d6d6d6 !important; " /* Плотный серый фон */
            "   color: #232629 !important; "             /* Темный текст */
            "}"
            "QStatusBar QPushButton:checked:hover { "
            "   background-color: #cccccc !important; "
            "}";


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
    const QWidgetList allAppWidgets = QApplication::allWidgets();
    for (QWidget *widget : std::as_const(allAppWidgets))
    {
        if (!widget) continue;

        if (widget->window()->metaObject()->className() == QString("Settings"))
        {
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

void Neuro_programm::applyThemeColors(bool /*isDarkTheme*/)
{
    QPalette sysPalette = qApp->palette();
    QString colorHighlight = sysPalette.color(QPalette::Highlight).name();

    QString colorBreezeLight   = "#eff0f1";
    QString colorBreezeText    = "#232629";
    QString colorWidget3Bg     = "#f5f6f7";
    QString colorBorder        = "#bcbebf"; // Оставляем только для нижней панели файлов widget_3

    QString style = QString(
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
            // Исправлено: Один вызов multi-arg через запятую
            .arg(colorBreezeLight, colorBreezeText, colorHighlight, colorBorder, colorWidget3Bg);

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

void Neuro_programm::triggerEditAction()
{
    // Получаем указатель на экшен, который вызвал этот слот
    QAction *senderAction = qobject_cast<QAction*>(sender());
    if (!senderAction) return;

    // Находим виджет, который сейчас находится в фокусе ввода
    QWidget *focusedWidget = QApplication::focusWidget();
    if (!focusedWidget) return;

    QString actionName = senderAction->objectName();

    // Сценарий 1: Фокус на текстовом редакторе (QTextEdit или QPlainTextEdit)
    // Подходит для окон с кодом, логов, терминалов
    if (QPlainTextEdit *textEdit = qobject_cast<QPlainTextEdit*>(focusedWidget)) {
        if (actionName == "actionUndo")        textEdit->undo();
        else if (actionName == "actionRedo")   textEdit->redo();
        else if (actionName == "actionCut")    textEdit->cut();
        else if (actionName == "actionCopy")   textEdit->copy();
        else if (actionName == "actionPaste")  textEdit->paste();
        else if (actionName == "actionDelete") textEdit->textCursor().deleteChar();
        else if (actionName == "actionSelectAll") textEdit->selectAll();
    }
    else if (QTextEdit *richTextEdit = qobject_cast<QTextEdit*>(focusedWidget)) {
        if (actionName == "actionUndo")        richTextEdit->undo();
        else if (actionName == "actionRedo")   richTextEdit->redo();
        else if (actionName == "actionCut")    richTextEdit->cut();
        else if (actionName == "actionCopy")   richTextEdit->copy();
        else if (actionName == "actionPaste")  richTextEdit->paste();
        else if (actionName == "actionDelete") richTextEdit->textCursor().deleteChar();
        else if (actionName == "actionSelectAll") richTextEdit->selectAll();
    }
    // Сценарий 2: Фокус на однострочном поле ввода (QLineEdit)
    // Например, при переименовании файлов или поиске
    else if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(focusedWidget)) {
        if (actionName == "actionUndo")        lineEdit->undo();
        else if (actionName == "actionRedo")   lineEdit->redo();
        else if (actionName == "actionCut")    lineEdit->cut();
        else if (actionName == "actionCopy")   lineEdit->copy();
        else if (actionName == "actionPaste")  lineEdit->paste();
        else if (actionName == "actionDelete") lineEdit->backspace();
        else if (actionName == "actionSelectAll") lineEdit->selectAll();
    }
    // Сценарий 3: Фокус на списках (QListWidget, QTreeView)
    // Если нужно обрабатывать физическое удаление файлов/элементов из проекта
    else if (actionName == "actionDelete") {
        if (focusedWidget == ui->openFilesListWidget) {
            // Вызываем ваш собственный метод закрытия вкладки/файла
            // Например: onCloseCurrentTab();
        }
        else if (focusedWidget == ui->treeView) {
            // Вызываем метод удаления файла с диска / из дерева проекта
            // Например: onDeleteFileFromProject();
        }
    }
}

void Neuro_programm::openNewFileInEditor(const QString &absoluteFilePath)
{
    if (absoluteFilePath.isEmpty()) return;

    // ШАГ 1: Физическое создание файла на диске
    QFile file(absoluteFilePath);
    if (!file.exists()) {
        QFileInfo fileInfo(absoluteFilePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            dir.mkpath(dir.absolutePath());
        }
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            out.setEncoding(QStringConverter::Utf8);
#else
            out.setCodec("UTF-8");
#endif
            out << "# -*- coding: utf-8 -*-\nimport torch\nimport torch.nn as nn\n\n";
            file.close();
        } else {
            qCritical() << " [ОШИБКА OS] Не удалось создать файл:" << absoluteFilePath;
            return;
        }
    }

    // ШАГ 2: Проверка — не открыт ли файл уже
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && page->objectName() == absoluteFilePath) {
            ui->centralStackedWidget->setCurrentWidget(page);
            if (ui->fileComboBox) {
                int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
                if (comboIdx != -1) ui->fileComboBox->setCurrentIndex(comboIdx);
            }
            return;
        }
    }

    // ШАГ 3: Программное создание и настройка CodeEditor
    QString fileContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fileContent = QString::fromUtf8(file.readAll());
        file.close();
    }

    // =========================================================================
    // КРИТИЧЕСКИЙ ФИКС ДВОЙНОГО КЛИКА: БЛОКИРУЕМ СИГНАЛЫ НА ВРЕМЯ СБОРКИ
    // =========================================================================
    if (ui->centralStackedWidget) ui->centralStackedWidget->blockSignals(true);
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(true);

    // 1. Снимаем режим стартовой заставки шорткатов
    this->setIDEInStartMode(false);

    // 2. Создаем контейнер-страницу
    QWidget *newPage = new QWidget(ui->centralStackedWidget);
    newPage->setObjectName(absoluteFilePath); // Назначаем имя СРАЗУ

    QVBoxLayout *layout = new QVBoxLayout(newPage);
    layout->setContentsMargins(0, 0, 0, 0);

    CodeEditor *editor = nullptr;
    MinimapArea *minimap = nullptr;

    // 3. Собираем монолитную панель (Редактор кода + миникарта)
    QWidget *editorContainer = CodeEditor::createEditorWithMinimap(newPage, editor, minimap);
    if (layout && editorContainer)
    {
        editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(editorContainer);
    }

    // 4. Настраиваем созданный объект текстового редактора editor
    if (editor) {
        editor->currentFilePath = absoluteFilePath;
        editor->setObjectName(absoluteFilePath);
        QFont codeFont;
        // Просим систему включить JetBrains Mono, если его нет — Courier New или Monospace
        codeFont.setFamilies(QStringList() << "JetBrains Mono" << "Fira Code" << "Courier New" << "Monospace");
        codeFont.setStyleHint(QFont::Monospace); // Жесткое аппаратное требование моноширинности
        codeFont.setPixelSize(13); // Комфортный компактный размер букв для ИИ-студии

        editor->setFont(codeFont);

        editor->setProperty("isLoading", true);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);

        editor->blockSignals(true);
        editor->setPlainText(fileContent);
        editor->blockSignals(false);

        if (this->codeCompleter) {
            editor->setCompleter(this->codeCompleter);
        }

        // Существующий коннект отслеживания изменений текста файла со звездочкой
        connect(editor, &CodeEditor::textChanged, this, &Neuro_programm::onCurrentFileTextChanged);

        // =========================================================================
        // ЖЕЛЕЗОБЕТОННЫЙ ПЕРЕНЕСЕННЫЙ КОННЕКТ ИНДИКАТОРА СТРОК (ЗДЕСЬ ОН ВАЛИДЕН!):
        // Каждый раз, когда курсор делает шаг — вызываем перерасчет координат Ln, Col
        // =========================================================================
        connect(editor, &CodeEditor::cursorPositionChanged, this, [this]()
                {
            this->updateCursorPositionIndicator();
        });
        // =========================================================================

        QTimer::singleShot(150, this, [this, editor]() {
            setFileModifiedState(editor, false);
            editor->setProperty("isLoading", false);
        });
    }

    // 5. ДОБАВЛЯЕМ ПОЛНОСТЬЮ ГОТОВУЮ СТРАНИЦУ В СТЕК ОКОН (РОВНО ОДИН РАЗ!)
    int newPageIndex = ui->centralStackedWidget->addWidget(newPage);

    // ШАГ 4: Синхронизация с интерфейсом навигации комбобокса
    if (ui->fileComboBox) {
        QFileInfo info(absoluteFilePath);
        ui->fileComboBox->addItem(info.fileName(), absoluteFilePath);
        ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count() - 1);
    }

    // 6. ПРИНУДИТЕЛЬНО выводим созданный редактор кода на передний план экрана!
    ui->centralStackedWidget->setCurrentIndex(newPageIndex);

    // РАЗБЛОКИРУЕМ СИГНАЛЫ СИСТЕМЫ: Сборка завершена, код на экране
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(false);
    if (ui->centralStackedWidget) ui->centralStackedWidget->blockSignals(false);

    // =========================================================================
    // СУПЕР-UX ФИКС: ВКЛЮЧАЕМ И ОБНОВЛЯЕМ ИНДИКАТOР СТРОК С ПЕРВOГO КЛИКА!
    // =========================================================================
    if (ui && ui->cursorPosLabel) {
        ui->cursorPosLabel->show(); // Выводим лейбл из стартового скрытия (hide)
    }
    // Вручную толкаем обновление, чтобы сразу загорелось "Строка: 1, Столбец: 1"
    this->updateCursorPositionIndicator();
    // =========================================================================

    if (editor) {
        editor->setFocus();
        editor->update();
    }

    // ШАГ 5: Регистрация файла в сервере JEDI (LSP)
    if (this->lspProcess && this->lspProcess->state() == QProcess::Running)
    {
        QJsonObject openParams;
        QJsonObject textDocument;
        textDocument["uri"] = QUrl::fromLocalFile(absoluteFilePath).toString();
        textDocument["languageId"] = "python";
        this->globalLspDocVersion = 1;
        textDocument["version"] = this->globalLspDocVersion;
        QString cleanStartText = fileContent;
        cleanStartText.remove('\r');
        textDocument["text"] = cleanStartText;
        openParams["textDocument"] = textDocument;
        this->sendLspRequest("textDocument/didOpen", openParams);
    }
}

void Neuro_programm::setFileModifiedState(CodeEditor* editor, bool modified)
{
    if (!editor) return;

    QString absoluteFilePath = editor->objectName();
    if (absoluteFilePath.isEmpty() || absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") {
        return;
    }

    // 1. Синхронизируем внутренний флаг документа Qt
    editor->document()->setModified(modified);

    // 2. Если этот редактор сейчас активен на экране — обновляем флаг окна ОС и заголовок
    if (ui->centralStackedWidget->currentWidget() &&
            ui->centralStackedWidget->currentWidget()->findChild<CodeEditor*>() == editor)
    {
        this->setWindowModified(modified);
        updateTabName();
    }

    QFileInfo info(absoluteFilePath);
    QString suffix = modified ? " *" : "";

    // 3. Синхронизируем верхний выпадающий список (ComboBox)
    if (ui->fileComboBox) {
        int comboIdx = ui->fileComboBox->findData(absoluteFilePath);
        if (comboIdx != -1) {
            ui->fileComboBox->setItemText(comboIdx, info.fileName() + suffix);
        }
    }

    // 4. Синхронизируем левый контейнер открытых документов
    if (ui->openFilesListWidget) {
        for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
            QListWidgetItem *item = ui->openFilesListWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == absoluteFilePath) {
                item->setText(info.fileName() + suffix);
                break;
            }
        }
    }
}

bool Neuro_programm::archiveProject(const QString &sourceDir, const QString &outputSavePath)
{
    QDir dir(sourceDir);
    if (!dir.exists()) {
        dir.mkpath(sourceDir);
    }

    // Создаем процесс изолированно в куче без передачи родителя this
    QProcess *tarProcess = new QProcess(nullptr);

    QStringList arguments;
    arguments << "--exclude=venv"
              << "--exclude=__pycache__"
              << "--exclude=.pytest_cache"
              << "-c" << "-j" << "-f" << outputSavePath << "-C" << sourceDir << ".";

    tarProcess->start("tar", arguments);

    bool success = tarProcess->waitForFinished(10000);
    int exitCode = tarProcess->exitCode();
    QByteArray errorOutput = tarProcess->readAllStandardError();

    // Удаляем файл project.json сразу после tar
    QFile::remove(sourceDir + "/project.json");

    // Удаляем сам процесс из памяти изолированно
    tarProcess->deleteLater();

    if (!success) {
        qCritical() << "Превышено время ожидания архивации проекта";
        return false;
    }

    if (exitCode != 0) {
        qCritical() << "[CRITICAL IDE ERROR] Ошибка tar при архивации:" << errorOutput;
        return false;
    }

    return true;
}


bool Neuro_programm::unarchiveProject(const QString &saveFilePath, const QString &targetExtractDir)
{
    QDir dir(targetExtractDir);
    if (!dir.exists()) {
        dir.mkpath(targetExtractDir);
    }

    // !!! КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: Создаем QProcess динамически в куче !!!
    // Передаем nullptr вместо this, чтобы полностью изолировать его от потока главного окна
    QProcess *tarProcess = new QProcess(nullptr);

    QStringList arguments;
    arguments << "-x" << "-j" << "-f" << saveFilePath << "-C" << targetExtractDir;

    tarProcess->start("tar", arguments);

    // Блокируем поток интерфейса на время распаковки, но БЕЗ обработки фоновых сигналов окон
    bool success = tarProcess->waitForFinished(10000);
    int exitCode = tarProcess->exitCode();
    QByteArray errorOutput = tarProcess->readAllStandardError();

    // !!! ВАЖНО: Даем процессу tar команду безопасно удалиться самостоятельно
    // строго на следующем витке цикла событий Qt, когда его QWeakPointer закроются
    tarProcess->deleteLater();

    if (!success) {
        qCritical() << "[CRITICAL IDE ERROR] Превышено время ожидания распаковки проекта";
        return false;
    }

    if (exitCode != 0) {
        qCritical() << "[CRITICAL IDE ERROR] Ошибка tar при распаковке:" << errorOutput;
        return false;
    }

    return true;
}

void Neuro_programm::saveProjectParameters(const QString &tmpDir)
{
    QJsonObject projectData;
    projectData["epochs"] = (aiPanel && aiPanel->ui && aiPanel->ui->spinBoxEpochs)
                            ? aiPanel->ui->spinBoxEpochs->value()
                            : 10;
    projectData["learning_rate"] = (aiPanel && aiPanel->ui && aiPanel->ui->spinBoxLR)
                                   ? aiPanel->ui->spinBoxLR->value()
                                   : 0.001;
    if (aiPanel && aiPanel->comboBatchSize) {
        projectData["batch_size"] = aiPanel->comboBatchSize->currentText().toInt();
    }
    projectData["device"] = (aiPanel && aiPanel->ui && aiPanel->ui->comboDevice_2)
                            ? aiPanel->ui->comboDevice_2->currentText()
                            : "cpu";

    QFile file(tmpDir + "/project.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(projectData);
        file.write(doc.toJson());
        file.close();
    }
}


void Neuro_programm::loadProjectParameters(const QString &tmpDir)
{
    QFile file(tmpDir + "/project.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject projectData = doc.object();

    // =========================================================================
    // ПУЛЕНЕПРОБИВАЕМЫЙ ДИНАМИЧЕСКИЙ ПОИСК ВИДЖЕТОВ НА ЭКРАНЕ
    // =========================================================================
    // Вместо использования ui->spinBoxEpochs, мы ищем живой виджет по его имени
    // среди всех активных окон приложения. Если страница была удалена, findChild вернет nullptr.

    QSpinBox *spinEpochs = this->findChild<QSpinBox*>("spinBoxEpochs");
    if (spinEpochs) {
        spinEpochs->setValue(projectData["epochs"].toInt());
    }

    QDoubleSpinBox *spinLR = this->findChild<QDoubleSpinBox*>("doubleSpinBoxLR");
    if (spinLR) {
        spinLR->setValue(projectData["learning_rate"].toDouble());
    }

    QComboBox *comboBatch = this->findChild<QComboBox*>("comboBatchSize");
    if (comboBatch) {
        QString savedBatch = QString::number(projectData["batch_size"].toInt());
        int batchIdx = comboBatch->findText(savedBatch);
        if (batchIdx != -1) comboBatch->setCurrentIndex(batchIdx);
    }

    QComboBox *comboDevice_2 = this->findChild<QComboBox*>("comboBoxDevice");
    if (comboDevice_2) {
        int idx = comboDevice_2->findText(projectData["device"].toString());
        if (idx != -1) comboDevice_2->setCurrentIndex(idx);
    }
}

void Neuro_programm::onOpenProjectMenuTriggered()
{
    qDebug() << "[MENU_TRG] Нажат пункт меню 'Открыть проект'";

    // 1. ОПРЕДЕЛЯЕМ СТАБИЛЬНЫЙ ПУТЬ ДЛЯ ОБЗОРА АРХИВОВ (папка save в корне проекта)
    QString saveFolderPath = getSafeSaveFolderPath();
    QDir(saveFolderPath).mkpath(saveFolderPath);

    // 2. ВЫЗЫВАЕМ ДИАЛОГ ВЫБОРА АРХИВА .pystudio
    QString archivePath = QFileDialog::getOpenFileName(
                this,
                "Открыть проект PyTorch Studio",
                saveFolderPath,
                "PyTorch Studio Project (*.pystudio);;All Files (*)"
                );
    if (archivePath.isEmpty()) return; // Пользователь отменил выбор

    QFileInfo archiveInfo(archivePath);
    QString archiveBaseName = archiveInfo.baseName();

    // 3. ВЫЧИСЛЯЕМ РОДИТЕЛЬСКИЙ КАТАЛОГ -> СТАБИЛЬНУЮ ПАПКУ PROJECTS
    QDir saveDirObj(saveFolderPath);
    saveDirObj.cdUp();
    QString rootDir = saveDirObj.absolutePath();
    QString defaultProjectsDirPath = rootDir + "/projects";
    QString autoExtractPath = defaultProjectsDirPath + "/" + archiveBaseName;

    // 4. ДИАЛОГ ВЫБОРА НАЗНАЧЕНИЯ (Кастомный QMessageBox)
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Распаковка проекта");
    msgBox.setText(QString("Куда распаковать данные проекта '%1'?").arg(archiveBaseName));
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *btnAuto = msgBox.addButton("В папку projects (Авто)", QMessageBox::AcceptRole);
    QPushButton *btnCustom = msgBox.addButton("Выбрать другую папку...", QMessageBox::ActionRole);
    QPushButton *btnCancel = msgBox.addButton("Отмена", QMessageBox::RejectRole);
    msgBox.exec();

    QString targetExtractDir;
    if (msgBox.clickedButton() == btnAuto) {
        QDir(autoExtractPath).mkpath(autoExtractPath);
        targetExtractDir = autoExtractPath;
    }
    else if (msgBox.clickedButton() == btnCustom) {
        targetExtractDir = QFileDialog::getExistingDirectory(
                    this,
                    "Выберите или создайте пустую папку для распаковки",
                    QDir::homePath()
                    );
        if (targetExtractDir.isEmpty()) return; // Отмена внутри диалога папок
    }
    else {
        return; // Пользователь нажал общую отмену
    }

    // =========================================================================
    // КРИТИЧЕСКИЙ РУБЕЖ ЗАЩИТЫ ПОТОКОВ №1: Полная остановка мониторинга железа
    // =========================================================================
    if (this->monitorTimer) {
        this->monitorTimer->stop();
    }
    QList<QProcess*> activeProcesses = this->findChildren<QProcess*>();
    for (QProcess *proc : std::as_const(activeProcesses))
    {
        if (proc && proc->state() != QProcess::NotRunning)
        {
            // Посылаем сигнал мягкой остановки (Ctrl+C), чтобы PyTorch успел сохранить веса.
            // Если через 2 секунды процесс не закроется — убиваем жестко.
            proc->terminate();
            if (!proc->waitForFinished(2000))
            {
                proc->kill();
            }
            proc->deleteLater();
        }
    }

    // 2. ОБЯЗАТЕЛЬНО очищаем сам контейнер, чтобы внутри не осталось "мусорных" указателей
    activeProcesses.clear();
    QCoreApplication::processEvents();

    // =========================================================================
    // КРИТИЧЕСКИЙ РУБЕЖ ЗАЩИТЫ ПОТОКОВ №2: Разрыв связей LSP и Автодополнения
    // =========================================================================
    if (this->codeCompleter) {
        this->codeCompleter->setWidget(nullptr);
    }
    if (this->activeCompletionPopup) {
        this->activeCompletionPopup->close();
        this->activeCompletionPopup = nullptr;
    }
    if (this->lspProcess && this->lspProcess->state() == QProcess::Running) {
        for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
            QWidget *page = ui->centralStackedWidget->widget(i);
            if (page && !page->objectName().isEmpty() &&
                    page->objectName() != "MAIN_SCREEN" && page->objectName() != "AI_CHAT_SCREEN")
            {
                QJsonObject closeParams;
                QJsonObject textDocument;
                textDocument["uri"] = QUrl::fromLocalFile(page->objectName()).toString();
                closeParams["textDocument"] = textDocument;
                this->sendLspRequest("textDocument/didClose", closeParams);
            }
        }
        QCoreApplication::processEvents();
    }

    // 5. АППАРАТНАЯ ОЧИСТКА СТРАНИЦ КОДА (ЗАЩИТА ГЛАВНЫХ ЭКРАНОВ ИНТЕРФЕЙСА)
    if (ui->fileComboBox) ui->fileComboBox->clear();
    if (ui->openFilesListWidget) ui->openFilesListWidget->clear();
    for (int i = ui->centralStackedWidget->count() - 1; i >= 0; --i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (!page) continue;
        CodeEditor *editor = page->findChild<CodeEditor*>();
        if (editor) {
            editor->blockSignals(true);
            editor->clearFocus();
            if (editor->document()) {
                editor->document()->blockSignals(true);
            }
            ui->centralStackedWidget->removeWidget(page);
            delete page;
        }
    }
    QCoreApplication::processEvents();

    // 6. ФИЗИЧЕСКАЯ РАСПАКОВКА АРХИВА В ВЫБРАННЫЙ КАТАЛОГ
    sendSystemNotification("Проект", "Распаковка файлов проекта...");
    if (unarchiveProject(archivePath, targetExtractDir)) {
        QDir(targetExtractDir).refresh();
        QCoreApplication::processEvents();

        // ВЫЧИСЛЯЕМ ПУТЬ К ТЕКСТОВОМУ ФАЙЛУ ПРОЕКТА ВНУТРИ РАСПАКОВАННОЙ ПАПКИ
        // Предполагается, что файл *.pystudio лежит внутри архива
        QString unarchivedProjectFile = targetExtractDir + "/" + archiveBaseName + ".pystudio";

        // Если файла конфигурации внутри архива нет, создаем резервное имя
        if (!QFile::exists(unarchivedProjectFile)) {
            unarchivedProjectFile = archivePath; // или логика поиска файла в targetExtractDir
        }

        // КЛЮЧЕВОЙ ШАГ: Передаем готовый путь в open_project, чтобы он инициализировал GUI и venv!
        this->open_project(unarchivedProjectFile);
    }
}

void Neuro_programm::onSaveProjectMenuTriggered()
{
    // 1. ОПРЕДЕЛЯЕМ ТЕКУЩИЙ КАТАЛОГ ПРОЕКТА ИЗ TREEVIEW
    if (!ui->treeView || !ui->treeView->model()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось определить структуру проекта.");
        return;
    }

    QFileSystemModel *model = qobject_cast<QFileSystemModel*>(ui->treeView->model());
    if (!model) return;

    // Получаем абсолютный путь к папке, которая сейчас открыта в дереве
    QString currentProjectDir = model->filePath(ui->treeView->rootIndex());

    if (currentProjectDir.isEmpty() || !QDir(currentProjectDir).exists()) {
        QMessageBox::warning(this, "Ошибка", "Каталог проекта не найден или не выбран.");
        return;
    }

    // 2. ОПРЕДЕЛЯЕМ ПУТЬ ДЛЯ СОХРАНЕНИЯ (Папка save в корне бинарника)
    QString saveFolderPath = getSafeSaveFolderPath();

    QFileInfo projectFolderInfo(currentProjectDir);
    QString defaultSaveName = saveFolderPath + "/" + projectFolderInfo.fileName() + ".pystudio";

    QDir saveDir(saveFolderPath);
    if (!saveDir.exists()) {
        saveDir.mkpath(saveFolderPath);
    }

    // Вызываем диалог. Именем файла по умолчанию предлагаем имя папки проекта
    //QFileInfo projectFolderInfo(currentProjectDir);
    //QString defaultSaveName = saveFolderPath + "/" + projectFolderInfo.fileName() + ".pystudio";

    QString saveFilePath = QFileDialog::getSaveFileName(
                this,
                "Сохранить проект в директорию SAVE",
                defaultSaveName,
                "PyTorch Studio Project (*.pystudio)"
                );

    if (saveFilePath.isEmpty()) return; // Пользователь отменил

    if (!saveFilePath.endsWith(".pystudio")) {
        saveFilePath += ".pystudio";
    }

    // 3. СОХРАНЯЕМ ВСЕ ОТКРЫТЫЕ РЕДАКТОРЫ НА ДИСК
    // Пробегаемся по вкладкам/стеку и пишем текст в файлы по их реальным путям в проекте
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && !page->objectName().isEmpty() &&
                page->objectName() != "MAIN_SCREEN" && page->objectName() != "AI_CHAT_SCREEN")
        {
            CodeEditor *editor = page->findChild<CodeEditor*>();
            if (editor) {
                QFile file(page->objectName()); // objectName хранит полный путь к файлу на диске
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << editor->toPlainText();
                    file.close();
                    editor->document()->setModified(false);
                }
            }
        }
    }

    // 4. ПИШЕМ ПАРАМЕТРЫ GUI (Эпохи, Батчи) в project.json ПРЯМО В КАТАЛОГ ПРОЕКТА
    saveProjectParameters(currentProjectDir);

    // 5. СЖИМАЕМ ИМЕННО КАТАЛОГ ПРОЕКТА
    sendSystemNotification("Проект", "Упаковка текущего каталога проекта...");
    if (archiveProject(currentProjectDir, saveFilePath)) {
        sendSystemNotification("Проект", "Проект успешно заархивирован в папку 'save'.");

        // Гасим звездочки модификации в интерфейсе
        this->setWindowModified(false);
        if (ui->centralStackedWidget->currentWidget()) {
            setFileModifiedState(ui->centralStackedWidget->currentWidget()->findChild<CodeEditor*>(), false);
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Критическая ошибка архивации каталога утилитой tar.");
    }
}

QString Neuro_programm::getSafeSaveFolderPath()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);

    // 1. Проверяем, содержит ли весь путь папку build
    if (dir.absolutePath().contains("/build/", Qt::CaseInsensitive) ||
            dir.absolutePath().endsWith("/build", Qt::CaseInsensitive))
    {
        // Поднимаемся вверх, пока не выйдем ИЗ папки build
        // Сравниваем имя папки с "build" без учета регистра (compare == 0 означает, что строки равны)
        while (dir.dirName().compare("build", Qt::CaseInsensitive) != 0) {
            if (!dir.cdUp()) break;
        }
        dir.cdUp(); // Делаем еще один шаг вверх, чтобы выйти ИЗ самой "build" в корень проекта
    }

    // 2. Формируем стабильный путь к папке сохранения
    QString savePath = dir.absoluteFilePath("save");

    // 3. Принудительно создаем её, если папки нет
    QDir().mkpath(savePath);

    return savePath;
}

#include "advancedclosedialog.h" // ОБЯЗАТЕЛЬНО подключите этот заголовок в самом верху neuro_programm.cpp
#include <QMessageBox>
#include <QPushButton>
#include <QCloseEvent>
#include <QFile>
#include <QProcess>
#include <iostream>

#ifndef Q_OS_WIN
#include <sys/types.h>
#include <signal.h>
#endif

void Neuro_programm::closeEvent(QCloseEvent *event)
{
    std::cout << "\n[ВХОД] Начало цепочки проверок закрытия PyTorch Studio..." << std::endl;
    std::cout.flush();

    // 1. Собираем текущее состояние среды разработки
    bool hasModifiedFiles = this->isWindowModified();
    bool isTraining = (trainingProcess && trainingProcess->state() != QProcess::NotRunning);

    // Если всё чисто, процессы молчат и нет кастомных фоновых задач — выходим мгновенно без диалогов
    if (!hasModifiedFiles && !isTraining && !this->property("isInstallingPackages").toBool()) {
        std::cout << "[БЫСТРЫЙ ВЫХОД] Нет активных процессов и изменений. Мгновенное закрытие." << std::endl;

        if (lspProcess) {
            lspProcess->kill();
            lspProcess->waitForFinished(500);
        }
        event->accept();
        return;
    }

    // 2. Вызываем наше кастомное структурное окно из 5 пунктов
    AdvancedCloseDialog dialog(hasModifiedFiles, isTraining, this);
    int result = dialog.exec();

    // 3. Разбираем действия пользователя по нажатым кнопкам
    switch (result) {
    case AdvancedCloseDialog::ResultCancel: {
        std::cout << "[ОТМЕНА] Закрытие отменено пользователем." << std::endl;
        event->ignore(); // Отменяем событие закрытия, IDE продолжает работать
        return;
    }

    case AdvancedCloseDialog::ResultToTray: {
        std::cout << "[ФОН] Окно скрыто. Процесс обучения переведен в фоновый режим." << std::endl;
        this->hide();    // Просто прячем главное окно, процессы продолжают жить
        event->ignore(); // Запрещаем операционной системе уничтожать приложение
        return;
    }

    case AdvancedCloseDialog::ResultSaveAndExit: {
        if (hasModifiedFiles) {
            std::cout << "[СОХРАНЕНИЕ] Запись изменений во все открытые файлы..." << std::endl;
            this->saveAllProjectChanges(); // Вызываем ваш метод сохранения
        }
        break; // Переходим ниже к финализации процессов
    }

    case AdvancedCloseDialog::ResultDiscardAndExit: {
        std::cout << "[СБРОС] Выход без сохранения изменений в коде." << std::endl;
        break; // Переходим ниже к финализации процессов
    }

    default:
        event->ignore();
        return;
    }

    // =========================================================================
    // ФИНАЛИЗАЦИЯ И ОСТАНОВКА ПРОЦЕССОВ ПРИ ПОДТВЕРЖДЕННОМ ВЫХОДЕ
    // =========================================================================

    // Обработка чекбокса: Сохранение весов модели перед выходом
    if (isTraining) {
        if (dialog.shouldSaveWeights()) {
            std::cout << "[КУЛЬТУРНЫЙ ОСТАНОВ] Отправляем SIGINT для перехвата в train.py и сохранения весов..." << std::endl;
            // На Linux/Unix отправляем сигнал прерывания вместо жесткого убийства процесса
#ifndef Q_OS_WIN
            pid_t pid = trainingProcess->processId();
            kill(pid, SIGINT);
#else
            trainingProcess->kill(); // На Windows штатного SIGINT для подпроцессов Qt нет
#endif
            trainingProcess->waitForFinished(3000); // Даем 3 секунды на работу torch.save() внутри вашего Python-скрипта
        } else {
            std::cout << "[ПРИНУДИТЕЛЬНО] Жесткое прерывание обучения (kill)..." << std::endl;
            trainingProcess->kill();
            trainingProcess->waitForFinished(1000);
        }
    }

    // Обработка чекбокса: Экспорт requirements.txt перед выходом (Ваш оригинальный код)
    if (dialog.shouldExportRequirements()) {
        QString safeVenvPython = "";
        if (!currentOpenProjectPath.isEmpty()) {
            QString projectVenv = currentOpenProjectPath + "/venv/bin/python";
            if (QFile::exists(projectVenv)) safeVenvPython = projectVenv;
        }
        if (safeVenvPython.isEmpty()) {
            QString z1Venv = "/home/elf/projects/z1/venv/bin/python";
            if (QFile::exists(z1Venv)) safeVenvPython = z1Venv;
        }
        if (safeVenvPython.isEmpty()) {
            safeVenvPython = "/home/elf/pyTorch-Studio/venv/bin/python";
        }

        if (QFile::exists(safeVenvPython)) {
            std::cout << "[ПАКЕТЫ] Запуск финального pip freeze..." << std::endl;
            QString programRootPath = "/home/elf/pyTorch-Studio";
            QString targetFolder = programRootPath + "/projects/z1";
            QString requirementsPath = targetFolder + "/requirements.txt";

            QProcess pipProcess;
            pipProcess.setWorkingDirectory(targetFolder);
            QStringList args;
            args << "-m" << "pip" << "freeze";
            pipProcess.setStandardOutputFile(requirementsPath);

            pipProcess.start(safeVenvPython, args);
            if (pipProcess.waitForFinished(3000)) {
                std::cout << "[УСПЕХ] Файл requirements.txt успешно зафиксирован. Размер: " << QFile(requirementsPath).size() << " байт." << std::endl;
            }
        }
    }

    // Очистка системных демонов (Ваш оригинальный код вежливого закрытия LSP-сервера Jedi)
    if (lspProcess) {
        std::cout << "[ОЧИСТКА] Остановка LSP сервера подсказок Jedi..." << std::endl;
        lspProcess->kill();
        lspProcess->waitForFinished(500);
    }

    std::cout << "[УСПЕХ] PyTorch Studio успешно завершила работу." << std::endl;
    std::cout.flush();

    event->accept(); // Окончательно закрываем приложение и освобождаем память ОС
}

void Neuro_programm::close_program()
{
    close();
}

void Neuro_programm::checkAndCreateVenvAsync(const QString &projectPath, bool isFreshExtract)
{
    if (projectPath.isEmpty()) return;

    // =========================================================================
    // ЖЕЛЕЗНЫЙ ВИЗУАЛЬНЫЙ ФИКС: МГНОВЕННО РАСКРЫВАЕМ ВСТРОЕННУЮ КОНСОЛЬ НА ЭКРАНЕ
    // =========================================================================
    if (panelOther) {
        panelOther->setVisible(true);
        panelOther->setTerminalPageActive();
    }
    if (mainVerticalSplitter) {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
    }

    // Выводим стартовые маркеры через обновленный бинарный printToConsole
    this->printToConsole("\n======================================================\n");
    this->printToConsole(">>> [ИИ АВТОМАТИКА] Инициализация структуры проекта...\n");
    this->printToConsole("======================================================\n");

    // 1. Динамически вычисляем, где в каталоге IDE лежит эталонный requirements.txt
    QDir searchDir(QCoreApplication::applicationDirPath());
    QString templateReqPath = "";
    for (int i = 0; i < 7; ++i) {
        if (searchDir.exists("pyTorch-Studio.pro") || searchDir.dirName() == "pyTorch-Studio") {
            templateReqPath = searchDir.absolutePath() + "/projects/z1/requirements.txt";
            break;
        }
        searchDir.cdUp();
    }
    if (templateReqPath.isEmpty() || !QFile::exists(templateReqPath)) {
        templateReqPath = "/home/elf/pyTorch-Studio/projects/z1/requirements.txt";
    }

    QDir cleanDir(projectPath);
    QString cleanProjectPath = cleanDir.absolutePath();

    this->sendSystemNotification("Проект распакован", "Настройте окружение Python/PyTorch для работы.");

    // Создаем диалоговое окно QMessageBox
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Настройка окружения Python - PyTorch Studio");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setText("Вы открыли сохранённый проект.\n\n"
                   "Пожалуйста, выберите способ настройки окружения Python/PyTorch на этом компьютере:");

    QPushButton *connectExistingButton = msgBox.addButton(" Указать путь к существующей venv", QMessageBox::AcceptRole);
    QPushButton *createNewButton = msgBox.addButton(" Создать новую venv с нуля", QMessageBox::AcceptRole);
    QPushButton *cancelButton = msgBox.addButton(" Использовать системный Python", QMessageBox::RejectRole);

    msgBox.setStyleSheet(
                "QMessageBox { background-color: #fcfcfc; color: #232629; font-size: 13px; }"
                "QPushButton { padding: 6px 14px; border: 1px solid #c7c7c7; border-radius: 3px; background-color: #eff0f1; }"
                "QPushButton:hover { background-color: #3daee9; color: white; }"
                );
    msgBox.exec();

    // ---------------------------------------------------------------------
    // ВЕТКА 1: УКАЗАТЬ ПУТЬ К ГОТОВОЙ ВНЕШНЕЙ ВЕНВЕ
    // ---------------------------------------------------------------------
    if (msgBox.clickedButton() == connectExistingButton)
    {
        QString existingVenvDir = QFileDialog::getExistingDirectory(
                    this, tr("Выберите ПАПКУ существующего venv (где лежит bin/python)"), "/home/elf",
                    QFileDialog::ShowDirsOnly);

        if (!existingVenvDir.isEmpty())
        {
            QString chosenPython = existingVenvDir + "/bin/python";
            saveSettings();
            if (QFile::exists(chosenPython))
            {
                this->venvPythonBinary = chosenPython;
                this->printToConsole(">>> [ИИ УСПЕХ] Подключено внешнее окружение: " + chosenPython.toUtf8() + "\n");
                this->printToConsole(">>> Запускаю фоновую валидацию зависимостей по requirements.txt...\n");
                this->installPackagesFromRequirements(cleanProjectPath, chosenPython, templateReqPath);
            }
            else {
                QMessageBox::critical(this, "Ошибка", "В выбранной папке отсутствует исполняемый файл bin/python!\nПовторите попытку.");
                this->checkAndCreateVenvAsync(cleanProjectPath, isFreshExtract);
            }
        }
        return;
    }
    // ---------------------------------------------------------------------
    // ВЕТКА 2: СГЕНЕРИРОВАТЬ СВЕЖУЮ ВЕНВУ С НУЛЯ
    // ---------------------------------------------------------------------
    else if (msgBox.clickedButton() == createNewButton)
    {
        QString targetVenvFolder = cleanProjectPath + "/venv";
        QString venvPythonPath = targetVenvFolder + "/bin/python";
        if (QFile::exists(targetVenvFolder)) {
            QDir oldDir(targetVenvFolder);
            oldDir.removeRecursively();
        }

        this->printToConsole(">>> [ИИ АВТОМАТИКА] Запущена чистая генерация venv в: " + targetVenvFolder.toUtf8() + "\n");
        this->printToConsole(">>> Вызываю системный модуль развертывания Arch Linux...\n");

        if (this->statusBar()) {
            this->statusBar()->showMessage("PyTorch Studio: Фоновое развёртывание новой структуры venv...", 0);
            this->statusBar()->setStyleSheet("QStatusBar { color: #3daee9; font-weight: bold; }");
        }

        QProcess *createProc = new QProcess(this);
        createProc->setWorkingDirectory(cleanProjectPath);

        // =========================================================================
        // КРИТИЧЕСКИЙ ШАГ КОРРЕКЦИИ: ПЕРЕДАЕМ СЫРЫЕ БАЙТЫ НАПРЯМУЮ БЕЗ ОШИБОК ДЕКОДИРОВАНИЯ
        // =========================================================================
        connect(createProc, &QProcess::readyReadStandardOutput, this, [this, createProc]() {
            this->printToConsole(createProc->readAllStandardOutput());
        });
        connect(createProc, &QProcess::readyReadStandardError, this, [this, createProc]() {
            this->printToConsole(createProc->readAllStandardError());
        });

        connect(createProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, projectPath, venvPythonPath, createProc](int exitCode, QProcess::ExitStatus status) {
            createProc->deleteLater();
            if (exitCode != 0 || status == QProcess::CrashExit) {
                this->printToConsole("❌ [КРИТИЧЕСКИЙ СБОЙ] Не удалось сгенерировать venv.\n");
                return;
            }
            this->printToConsole("✔ Базовая структура папок venv успешно создана.\n");

            // --- ШАГ Б: УЛЬТРА-НАДЁЖНЫЙ ПОИСК requirements.txt ---
            QDir searchDir(QCoreApplication::applicationDirPath());
            QString templateReqPath = "";
            for (int i = 0; i < 7; ++i) {
                if (searchDir.exists("pyTorch-Studio.pro") || searchDir.dirName() == "pyTorch-Studio") {
                    templateReqPath = searchDir.absolutePath() + "/projects/z1/requirements.txt";
                    break;
                }
                if (!searchDir.cdUp()) break;
            }
            if (templateReqPath.isEmpty() || !QFile::exists(templateReqPath)) {
                templateReqPath = "/home/elf/pyTorch-Studio/projects/z1/requirements.txt";
            }

            if (!QFile::exists(templateReqPath)) {
                this->printToConsole("❌ [ОШИБКА АВТОМАТИКИ] Эталонный файл требований отсутствует по пути:\n");
                this->printToConsole("   " + templateReqPath.toUtf8() + "\n");
                this->printToConsole("   Положите файл requirements.txt в папку projects/z1 вашего репозитория.\n");
                if (this->statusBar()) this->statusBar()->showMessage("Ошибка: Файл requirements.txt не найден", 5000);
                return;
            }

            if (!QFile::exists(venvPythonPath)) {
                this->printToConsole("❌ [ОШИБКА АВТОМАТИКИ] Исполняемый файл venv/bin/python не появился на диске.\n");
                return;
            }

            // --- ШАГ В: ЗАПУСК ФОНОВОЙ УСТАНОВКИ ПАКЕТОВ PYTORCH ---
            this->printToConsole(">>> [ИИ АВТОМАТИКА] Начинаю фоновую установку ИИ-библиотек по паспорту требований...\n");
            this->printToConsole("    Файл конфигурации: " + templateReqPath.toUtf8() + "\n");
            this->installPackagesFromRequirements(projectPath, venvPythonPath, templateReqPath);
        });

        QStringList args;
        args << "-m" << "venv" << "venv";
        createProc->start("/usr/bin/python", args);
        return;
    }
    // ---------------------------------------------------------------------
    // ВЕТКА 3: ИСПОЛЬЗОВАТЬ СИСТЕМНЫЙ PYTHON
    // ---------------------------------------------------------------------
    else {
        this->venvPythonBinary = "/usr/bin/python";
        this->printToConsole("⚠ Настройка venv отменена. Переключаю среду разработки на глобальный интерпретатор.\n");
        this->initLspServer();
        return;
    }
}

void Neuro_programm::installPackagesFromRequirements(const QString &workingDir, const QString &pythonPath, const QString &reqPath)
{
    if (!QFile::exists(reqPath) || !QFile::exists(pythonPath)) {
        this->printToConsole("❌ [ИИ СБОЙ] Файл требований или интерпретатор Python не найден.\n");
        return;
    }

    if (this->statusBar()) {
        this->statusBar()->showMessage("PyTorch Studio: Обновление пакетного менеджера...", 0);
        this->statusBar()->setStyleSheet("QStatusBar { color: #e67e22; font-weight: bold; }");
    }

    if (panelOther) {
        panelOther->setInstallProgressRange(0, 100);
        panelOther->setInstallProgressValue(5);
        panelOther->setInstallProgressVisible(true);
    }

    // =========================================================================
    // ШАГ 1: ПРИНУДИТЕЛЬНОЕ ОБНОВЛЕНИЕ ВНУТРЕННЕГО PIP ВНУТРИ ЧИСТОГО VENV
    // Это на 100% решает проблему мгновенного вылета с Кодом 1!
    // =========================================================================
    this->printToConsole(">>> [ИИ АВТОМАТИКА] Шаг 1: Обновляю ядро pip внутри venv до актуальной версии...\n");

    QProcess *pipUpdateProc = new QProcess(this);
    pipUpdateProc->setWorkingDirectory(workingDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TMPDIR", "/tmp");
    env.insert("PIP_CACHE_DIR", "/tmp/pip-cache-elf");
    env.insert("PYTHONUNBUFFERED", "1");
    pipUpdateProc->setProcessEnvironment(env);

    // Подключаем чтение логов обновления ядра pip во встроенную консоль
    connect(pipUpdateProc, &QProcess::readyReadStandardOutput, this, [this, pipUpdateProc]() {
        this->printToConsole(pipUpdateProc->readAllStandardOutput());
    });
    connect(pipUpdateProc, &QProcess::readyReadStandardError, this, [this, pipUpdateProc]() {
        this->printToConsole(pipUpdateProc->readAllStandardOutput());
    });

    // Как только ядро pip успешно обновилось, запускаем Шаг 2 (тяжелый PyTorch)
    connect(pipUpdateProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, workingDir, pythonPath, reqPath, env, pipUpdateProc](int updateExitCode, QProcess::ExitStatus status)
    {
        pipUpdateProc->deleteLater();

        if (updateExitCode != 0 || status == QProcess::CrashExit) {
            this->printToConsole("❌ [КРИТИЧЕСКИЙ СБОЙ] Не удалось обновить базовый pip внутри venv. Пропуск основной установки.\n");
            if (panelOther) panelOther->setInstallProgressVisible(false);
            return;
        }

        // =====================================================================
        // ШАГ 2: ЗАПУСК ОСНОВНОЙ УСТАНОВКИ ИИ-БИБЛИОТЕК ПО REQUIREMENTS.TXT
        // =====================================================================
        this->printToConsole("\n>>> [ИИ АВТОМАТИКА] Шаг 2: Ядро pip успешно обновлено. Накатываю PyTorch / LSP зависимости...\n");
        if (this->statusBar()) this->statusBar()->showMessage("PyTorch Studio: Установка ИИ библиотек...", 0);

        QProcess *pipMainProc = new QProcess(this);
        pipMainProc->setWorkingDirectory(workingDir);
        pipMainProc->setProcessEnvironment(env); // Передаем безопасное /tmp окружение

        auto parsePipOutput = [this](const QString &output) {
            if (!panelOther) return;
            static QRegularExpression progressRegex(R"(Progress\s+(\d+)\s+of\s+(\d+))");
            QRegularExpressionMatch match = progressRegex.match(output);

            if (match.hasMatch()) {
                double downloadedBytes = match.captured(1).toDouble();
                double totalBytes = match.captured(2).toDouble();
                if (totalBytes > 0) {
                    int percent = static_cast<int>((downloadedBytes / totalBytes) * 100.0);
                    panelOther->setInstallProgressRange(0, 100);
                    panelOther->setInstallProgressValue(percent);
                }
            } else {
                this->printToConsole(output.toUtf8()); // Выводим только чистые текстовые логи
                QString cleanOut = output.trimmed();
                if (cleanOut.contains("Installing collected packages") || cleanOut.contains("Running setup.py")) {
                    panelOther->setInstallProgressRange(0, 100);
                    panelOther->setInstallProgressValue(90);
                }
            }
        };

        connect(pipMainProc, &QProcess::readyReadStandardOutput, this, [pipMainProc, parsePipOutput]() {
            parsePipOutput(QString::fromUtf8(pipMainProc->readAllStandardOutput()));
        });
        connect(pipMainProc, &QProcess::readyReadStandardError, this, [pipMainProc, parsePipOutput]() {
            parsePipOutput(QString::fromUtf8(pipMainProc->readAllStandardError()));
        });

        connect(pipMainProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, pythonPath, pipMainProc](int mainExitCode, QProcess::ExitStatus mainStatus)
        {
            pipMainProc->deleteLater();

            if (panelOther) {
                panelOther->setInstallProgressRange(0, 100);
                panelOther->setInstallProgressValue(100);
                panelOther->setInstallProgressVisible(false);
            }

            if (mainExitCode == 0 && mainStatus == QProcess::NormalExit) {
                this->venvPythonBinary = pythonPath;
                this->printToConsole("✨ [УСПЕХ] Все зависимости проверены. Окружение PyTorch Studio готово!\n");
                this->sendSystemNotification("Окружение ИИ", "Синхронизация завершена. Все пакеты в актуальном состоянии.");
                if (this->statusBar()) {
                    this->statusBar()->showMessage("PyTorch Studio: Библиотеки синхронизированы", 4000);
                    this->statusBar()->setStyleSheet("QStatusBar { color: #00ff00; font-weight: normal; }");
                }
                this->initLspServer();
            }
            else {
                this->printToConsole(("\n❌ [ИИ КАТАСТРОФА] Установка пакетов PyTorch оборвалась. Код ошибки: " + QString::number(mainExitCode) + "\n").toUtf8());
                if (this->statusBar()) this->statusBar()->showMessage("PyTorch Studio: Ошибка установки зависимостей", 5000);
            }
        });

        // Запуск основной установки пакетов
        QStringList mainArgs;
        mainArgs << "-m" << "pip" << "install" << "--upgrade" << "--no-cache-dir" << "--progress-bar" << "raw" << "-r" << reqPath;
        pipMainProc->start(pythonPath, mainArgs);
    });

    // Запуск Шага 1: Команда безопасного обновления самого pip без использования тяжелых кэш-директорий
    QStringList updateArgs;
    updateArgs << "-m" << "pip" << "install" << "--upgrade" << "--no-cache-dir" << "pip";
    pipUpdateProc->start(pythonPath, updateArgs);
}

void Neuro_programm::printToConsole(const QByteArray &rawBytes)
{
    // Напрямую отправляем байты в парсер ANSI/VT100 и кареток \r
    if (this->panelOther && this->panelOther->ui && this->panelOther->ui->consoleOutput) {
        this->panelOther->ui->consoleOutput->appendTerminalData(rawBytes);
    }
}

void Neuro_programm::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);

    QPainter painter(this);
    // Для теста оставляем красный, потом замените на QColor("#4d5455")
    QPen pen(QColor(0, 0, 0), 1); // Наш черный карандаш толщиной 1px
    painter.setPen(pen);

    // Рисуем прямоугольник строго по внутреннему краю окна
    painter.drawRect(0, 0, this->width() - 1, this->height() - 1);
}

void Neuro_programm::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    // Если окно развернули, свернули или вернули в оконный режим
    if (event->type() == QEvent::WindowStateChange) {
        // Принудительно заставляем Qt перерисовать QPainter рамку
        this->update();
    }
}

void Neuro_programm::saveProjectAs()
{
    // 1. Восстанавливаем полный путь к текущему открытому файлу проекта.
    // Если у вас в currentOpenProjectPath хранится только папка, собираем путь к файлу:
    QString sourceFile = this->currentOpenProjectPath + "/z1.pystudio"; // Замените на вашу переменную файла, если она есть

    // 2. АВТОМАТИЧЕСКАЯ ПРОВЕРКА И ПРЕДВАРИТЕЛЬНОЕ СОХРАНЕНИЕ
    // Если проект изменен (горит звездочка) или файл физически еще не создан на диске
    if (this->isWindowModified() || !QFile::exists(sourceFile))
    {
        qDebug() << "[SAVE_AS] Обнаружены несохраненные изменения. Запускаю принудительное сохранение...";

        // Вызываем вашу штатную функцию сохранения текущего файла.
        // Она запишет актуальные настройки из GUI (эпохи, батчи и т.д.) на диск.
        this->saveCurrentActiveFile();

        // Даем операционной системе Linux долю секунды на сброс дискового буфера
        QCoreApplication::processEvents();
    }

    // Жесткая защита: если после попытки сохранения исходный файл так и не появился
    if (this->currentOpenProjectPath.isEmpty() || !QFile::exists(sourceFile)) {
        if (this->statusBar()) {
            this->statusBar()->showMessage("Ошибка: Нет активного файла проекта для копирования", 4000);
        }
        return;
    }

    // 3. Открываем окно Linux-проводника для выбора НАЗНАЧЕНИЯ (куда скопировать)
    QString destinationFile = QFileDialog::getSaveFileName(
                this,
                "Сохранить копию проекта в новое место...",
                QDir::homePath(),
                "Файлы проекта PyTorch Studio (*.pystudio);;All Files (*)"
                );

    if (destinationFile.isEmpty()) return; // Пользователь нажал "Отмена"

    // Автоматически дописываем расширение, если пользователь забыл его ввести
    if (!destinationFile.endsWith(".pystudio", Qt::CaseInsensitive)) {
        destinationFile += ".pystudio";
    }

    // Защита: нельзя скопировать файл сам в себя (это сотрет его)
    if (QFileInfo(sourceFile) == QFileInfo(destinationFile)) {
        if (this->statusBar()) this->statusBar()->showMessage("Ошибка: Путь назначения совпадает с исходным файлом", 4000);
        return;
    }

    // 4. Если файл по новому адресу уже существует, удаляем его перед копированием
    if (QFile::exists(destinationFile)) {
        QFile::remove(destinationFile);
    }

    // 5. ФИЗИЧЕСКОЕ КОПИРОВАНИЕ НА ДИСКЕ СИЛАМИ СИСТЕМЫ
    if (QFile::copy(sourceFile, destinationFile))
    {
        // Переключаем IDE на работу с этим новым файлом
        QFileInfo newFileInfo(destinationFile);
        this->currentOpenProjectPath = newFileInfo.absoluteDir().absolutePath();

        // Обновляем заголовки, сбрасываем звездочку модификации и пишем в историю .config
        this->setWindowTitle(QString("PyTorch Studio - %1 [%2]").arg(newFileInfo.baseName(), destinationFile));
        this->setWindowModified(false);

        addProjectToRecent(destinationFile);

        sendSystemNotification("PyTorch Studio", "✔ Файл проекта успешно сохранен и скопирован в новое место");
    }
    else {
        sendSystemNotification("Ошибка системы", "Не удалось скопировать файл. Проверьте права доступа папки.");
    }
}

void Neuro_programm::onInstallSinglePackageTriggered()
{
    // 1. ПРОВЕРКА АКТИВНОГО ПРОЕКТА
    if (this->currentOpenProjectPath.isEmpty()) {
        sendSystemNotification("Внимание", "Сначала откройте или создайте ИИ-проект (*.pystudio)"); //
        return;
    }

    // Проверяем, существует ли локальный интерпретатор venv
    QString venvPythonPath = this->currentOpenProjectPath + "/venv/bin/python"; //
    if (!QFile::exists(venvPythonPath)) {
        // Если venv не найден, используем глобальный системный Python Arch Linux
        venvPythonPath = "/usr/bin/python"; //
    }

    // 2. ВЫЗОВ ДИАЛОГОВОГО ОКНА ДЛЯ ВВОДА ИМЕНИ ПАКЕТА
    bool ok;
    QString packageName = QInputDialog::getText(
                this,
                "Установка пакета Python",
                "Введите точное имя библиотеки для pip (например, scipy):",
                QLineEdit::Normal,
                "",
                &ok
                );

    // Если пользователь нажал "Отмена" или ввёл пустую строку — выходим
    if (!ok || packageName.trimmed().isEmpty()) return;

    // 3. АВТО-РАСКРЫТИЕ ИНТЕРФЕЙСА НИЖНЕЙ ПАНЕЛИ КОНСОЛИ
    if (this->panelOther) {
        this->panelOther->setVisible(true);
        this->panelOther->setTerminalPageActive();
    }

    // Синхронизируем кнопки управления статус-бара
    if (btnTerminal) btnTerminal->setChecked(true);
    if (btnAIChat) btnAIChat->setChecked(false);

    // Раздвигаем центральный сплиттер на фиксированные 250 пикселей под консоль вывода
    if (mainVerticalSplitter)
    {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250})); //
    }

    // Выводим стартовый маркер начала установки в ваш бинарный терминал
    this->printToConsole(QString("\n>>> [PIP АВТОМАТИКА] Запуск установки пакета: %1...\n").arg(packageName).toUtf8());

    // 4. ЗАПУСК ФОНОВОГО ПРОЦЕССА УСТАНОВКИ ЧЕРЕЗ QPROCESS
    QProcess *pipInstallProc = new QProcess(this);
    pipInstallProc->setWorkingDirectory(this->currentOpenProjectPath); //

    // Настраиваем изолированное окружение без буферизации данных
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment(); //
    env.insert("PYTHONUNBUFFERED", "1"); //
    env.insert("PYTHONIOENCODING", "UTF-8"); //
    pipInstallProc->setProcessEnvironment(env); //

    // Подключаем вывод логов pip напрямую в метод printToConsole
    connect(pipInstallProc, &QProcess::readyReadStandardOutput, this, [this, pipInstallProc]() {
        this->printToConsole(pipInstallProc->readAllStandardOutput());
    });
    connect(pipInstallProc, &QProcess::readyReadStandardError, this, [this, pipInstallProc]() {
        this->printToConsole(pipInstallProc->readAllStandardError());
    });

    // Настраиваем вежливую очистку памяти после завершения работы pip
    connect(pipInstallProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, packageName, pipInstallProc](int exitCode, QProcess::ExitStatus status) {
        pipInstallProc->deleteLater(); // Освобождаем оперативную память подпроцесса

        if (exitCode == 0 && status == QProcess::NormalExit) {
            this->printToConsole(QString("✔ [PIP УСПЕХ] Библиотека %1 успешно добавлена в окружение.\n").arg(packageName).toUtf8());
            sendSystemNotification("Менеджер пакетов", QString("Пакет %1 успешно установлен").arg(packageName)); //
        } else {
            this->printToConsole(QString("❌ [PIP СБОЙ] Ошибка установки пакета %1. Код вылета: %2\n").arg(packageName).arg(exitCode).toUtf8());
        }
    });

    // Формируем бинарные аргументы запуска пакетного менеджера
    QStringList pipArgs;
    pipArgs << "-m" << "pip" << "install" << packageName.trimmed();

    // Асинхронно стартуем pip, не блокируя работу интерфейса IDE
    pipInstallProc->start(venvPythonPath, pipArgs); //
}

void Neuro_programm::install_from_requirements()
{
    // 1. ПРОВЕРКА АКТИВНОГО ПРОЕКТА
    if (this->currentOpenProjectPath.isEmpty()) {
        sendSystemNotification("Внимание", "Сначала откройте или создайте ИИ-проект (*.pystudio)"); //
        return;
    }

    // Вычисляем путь к Python внутри виртуального окружения venv
    QString venvPythonPath = this->currentOpenProjectPath + "/venv/bin/python"; //
    if (!QFile::exists(venvPythonPath)) {
        venvPythonPath = "/usr/bin/python"; // Фоллбэк на глобальный Python, если venv нет
    }

    // 2. АВТОМАТИЧЕСКИЙ ПОИСК ФАЙЛА REQUIREMENTS В КОРНЕ
    QString targetReqPath = this->currentOpenProjectPath + "/requirements.txt";

    // Если в корне проекта файла нет — даем пользователю выбрать его вручную через QFileDialog
    if (!QFile::exists(targetReqPath)) {
        this->printToConsole("⚠ [PIP] Файл requirements.txt не найден в корне проекта. Открываю проводник...\n");
        targetReqPath = QFileDialog::getOpenFileName(
                    this,
                    "Выберите файл зависимостей проекта",
                    this->currentOpenProjectPath,
                    "Файлы требований (*.txt);;Все файлы (*)"
                    );
    }

    // Если пользователь закрыл диалог выбора файла или нажал "Отмена" — выходим
    if (targetReqPath.isEmpty() || !QFile::exists(targetReqPath)) {
        this->printToConsole("❌ [PIP] Операция установки отменена пользователем.\n");
        return;
    }

    // 3. АВТО-РАСКРЫТИЕ ИНТЕРФЕЙСА КОНСОЛИ
    if (this->panelOther) {
        this->panelOther->setVisible(true); //
        this->panelOther->setTerminalPageActive(); //
    }

    // Включаем подсветку кнопки в статус-баре
    if (btnTerminal) btnTerminal->setChecked(true); //
    if (btnAIChat) btnAIChat->setChecked(false); //

    // Выделяем фиксированные 250 пикселей под вывод консоли
    if (mainVerticalSplitter) {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250})); //
    }

    this->printToConsole(QString("\n>>> [PIP АВТОМАТИКА] Считывание манифеста: %1\n").arg(targetReqPath).toUtf8());
    this->printToConsole(">>> Запуск пакетной установки ИИ-зависимостей PyTorch...\n");

    // 4. ЗАПУСК ФОНОВОГО ПРОЦЕССА PIP
    QProcess *pipBatchProc = new QProcess(this);
    pipBatchProc->setWorkingDirectory(this->currentOpenProjectPath); //

    // Настраиваем небуферизированное окружение Linux для tqdm прогресс-баров
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment(); //
    env.insert("PYTHONUNBUFFERED", "1"); //
    env.insert("PYTHONIOENCODING", "UTF-8"); //
    pipBatchProc->setProcessEnvironment(env); //

    // Связываем вывод pip напрямую с вашим TerminalWidget через бинарный метод printToConsole
    connect(pipBatchProc, &QProcess::readyReadStandardOutput, this, [this, pipBatchProc]() {
        this->printToConsole(pipBatchProc->readAllStandardOutput());
    });
    connect(pipBatchProc, &QProcess::readyReadStandardError, this, [this, pipBatchProc]() {
        this->printToConsole(pipBatchProc->readAllStandardError());
    });

    // Обработчик успешного или аварийного завершения установки
    connect(pipBatchProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pipBatchProc](int exitCode, QProcess::ExitStatus status) {
        pipBatchProc->deleteLater(); // Очищаем память

        if (exitCode == 0 && status == QProcess::NormalExit) {
            this->printToConsole("\n✔ [PIP УСПЕХ] Все пакеты из файла требований успешно развёрнуты.\n");
            sendSystemNotification("Менеджер окружения", "✔ Зависимости PyTorch успешно обновлены"); //
        } else {
            this->printToConsole(QString("\n❌ [PIP СБОЙ] Ошибка при пакетной установке библиотек. Код вылета Linux: %1\n").arg(exitCode).toUtf8());
        }
    });

    // Формируем стандартную команду пакетной установки: python -m pip install -r requirements.txt
    QStringList pipArgs;
    pipArgs << "-m" << "pip" << "install" << "-r" << targetReqPath;

    // Асинхронно стартуем pip в фоновом режиме, сохраняя отзывчивость интерфейса
    pipBatchProc->start(venvPythonPath, pipArgs); //
}

bool Neuro_programm::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    // Проверяем, что видеосервер Linux использует протокол X11/XCB (включая XWayland)
    if (eventType == "xcb_generic_event_t")
    {
        // Используем базовый указатель на структуру данных, чтобы не зависеть от приватных библиотек Qt
        unsigned char *ev = static_cast<unsigned char*>(message);

        // В протоколе XCB тип события всегда хранится в самом первом байте структуры
        uint8_t responseType = ev[0] & ~0x80;

        // 0x22 — это аппаратный код системного запроса Linux (XCB_GE_GENERIC)
        if (responseType == 0x22 || responseType == 4) // Перехватываем опрос координат мыши
        {
            // Переводим глобальные координаты мыши в локальные координаты окна
            QPoint localPos = this->mapFromGlobal(QCursor::pos());

            // Если мышка находится в пределах вашей 75-пиксельной шапки
            if (localPos.y() >= 0 && localPos.y() <= 75)
            {
                // Защита: если курсор над кнопками управления или меню, отдаем управление Qt
                QWidget *child = this->childAt(localPos);
                if (child && (child->inherits("QPushButton") || child->inherits("QMenuBar") || child->inherits("QComboBox"))) {
                    return QMainWindow::nativeEvent(eventType, message, result);
                }

                // МАГИЯ LINUX: Сообщаем оконному менеджеру KWin, что эта зона — Заголовок Окна!
                // Число 2 — это нативный маркер HTCAPTION.
                // После этого ОС сама включает и идеальный двойной клик (сжатие/разворачивание), и перетаскивание!
                *result = 2;
                return true; // Завершаем обработку, Linux всё сделает за нас
            }
        }
    }

    // Во всех остальных случаях отдаем управление стандартному движку Qt
    return QMainWindow::nativeEvent(eventType, message, result);
}

// 1. НАЖАТИЕ МЫШИ И ДВОЙНОЙ ЩЕЛЧОК
void Neuro_programm::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Ловим строго ДВОЙНОЙ КЛИК ядра Qt
        if (event->type() == QEvent::MouseButtonDblClick)
        {
            QPoint localPos = event->pos();
            // Если кликнули в районе верхней шапки приложения
            if (localPos.y() >= 0 && localPos.y() <= 100)
            {
                if (this->isMaximized()) {
                    this->showNormal();   // Сжимаем в окно
                } else {
                    this->showMaximized(); // Разворачиваем во весь экран
                }
                event->accept();
                this->m_isDragging = false; // Страховка от залипания
                return;
            }
        }
    }
    QMainWindow::mousePressEvent(event);
}


// 2. ФИЗИЧЕСКОЕ ПЛАВНОЕ ПЕРЕМЕЩЕНИЕ ОКНА
void Neuro_programm::mouseMoveEvent(QMouseEvent *event)
{
    // Если левая кнопка зажата в шапке и мы потащили мышь
    if (this->m_isDragging && (event->buttons() & Qt::LeftButton))
    {
        // Двигаем окно в новые координаты с учетом сохраненной дельты
        this->move(event->globalPosition().toPoint() - this->m_dragPosition);
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

// 3. ОТПУСКАНИЕ МЫШИ
void Neuro_programm::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        this->m_isDragging = false; // Сбрасываем флаг перетаскивания
    }
    QMainWindow::mouseReleaseEvent(event);
}

void Neuro_programm::saveSettings()
{
    QSettings settings;

    if (!currentOpenProjectPath.isEmpty())
    {
        // Если проект открыт — берем его venv
        QString currentVenvPath = currentOpenProjectPath + "/venv";
        settings.setValue("python/venv_path", currentVenvPath);
    } else {
        // ЕСЛИ ПРОЕКТ НЕ ОТКРЫТ: Выходим из папки build наружу, в корень программы
        QDir baseDir(QCoreApplication::applicationDirPath());

        // Обычно папка build лежит на одном уровне с каталогом Projects.
        // Поднимаемся на 1 или 2 уровня вверх (в зависимости от вашей структуры сборки)
        baseDir.cdUp(); // вышли из Debug/Release
        baseDir.cdUp(); // вышли из папки build_... в корень программы

        // Собираем дефолтный путь: корень_программы / Projects / (имя дефолтной папки) / venv
        // Замените "DefaultProject", если папка по умолчанию называется иначе
        QString defaultVenvPath = baseDir.absolutePath() + "/Projects/DefaultProject/venv";

        settings.setValue("python/venv_path", defaultVenvPath);
    }

    settings.setValue("interface/font_size", 10);
    settings.setValue("interface/dark_theme", true);

    settings.sync();
}

void Neuro_programm::btnStartDebug_clicked() {
    // Останавливаем старый процесс отладки, если он был запущен ранее
    if (debuggedScriptProcess->state() == QProcess::Running) {
        debuggedScriptProcess->terminate();
        debuggedScriptProcess->waitForFinished(1000);
    }

    // 1. Считываем правильные пути к venv и файлу скрипта
    QSettings settings;
    QString venvPath = settings.value("python/venv_path", "").toString();
    QString scriptPath = currentOpenProjectPath + "/test_debug.py"; // Убедитесь, что имя файла совпадает!

#if defined(Q_OS_WIN)
    QString pythonExec = venvPath + "/Scripts/python.exe";
#else
    QString pythonExec = venvPath + "/bin/python";
#endif

    // Отладочный лог в консоль IDE (для проверки путей)
    qDebug() << "Исполняемый файл Python:" << pythonExec;
    qDebug() << "Путь к тестовому скрипту:" << scriptPath;

    // 2. Настраиваем перенаправление вывода (stdout/stderr) в левую консоль
    disconnect(debuggedScriptProcess, &QProcess::readyReadStandardOutput, nullptr, nullptr);
    disconnect(debuggedScriptProcess, &QProcess::readyReadStandardError, nullptr, nullptr);

    // В файле neuro_programm.cpp внутри метода кнопки отладки:

    // 1. Настраиваем логи вывода (просто печатаем текст, без ручных проверок строк)
    connect(debuggedScriptProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        if (panelOther) {
            panelOther->appendLogText(QString::fromUtf8(debuggedScriptProcess->readAllStandardOutput()));
        }
    });
    connect(debuggedScriptProcess, &QProcess::readyReadStandardError, this, [this]() {
        if (panelOther) {
            panelOther->appendLogText(QString::fromUtf8(debuggedScriptProcess->readAllStandardError()));
        }
    });

    // 2. Пишем стартовый лог в дебаг-окно
    if (panelOther) {
        panelOther->appendDebugLog("⏳ Скрипт запущен. Ожидание инициализации порта...");
    }

    // 3. Запускаем процесс
    QStringList args;
    args << "-Xfrozen_modules=off" << "-u" << scriptPath;
    debuggedScriptProcess->start(pythonExec, args);

    // 4. 🔥 СИНХРОНИЗАЦИЯ ПО СИГНАЛУ СТАРТА ПРОЦЕССА:
    // Отключаем старые коннекты, чтобы не дублировались
    disconnect(debuggedScriptProcess, &QProcess::started, nullptr, nullptr);

    // Ловим сигнал, когда ОС РЕАЛЬНО запустила интерпретатор (после всех проверок bash)
    connect(debuggedScriptProcess, &QProcess::started, this, [this]() {
        // Как только скрипт физически пошел выполняться, даем ему ровно 1.5 секунды
        // на импорт torch и открытие порта 5678, а затем бьем сокетом со 100% точностью!
        QTimer::singleShot(1500, this, [this]() {
            if (panelOther) {
                panelOther->appendDebugLog("🔌 Подключаем Консоль отладки C++ к Python...");
                panelOther->connectToDebugger();
            }
        });
    });

}

void Neuro_programm::startTensorBoard(const QString &logDir) {
    // 1. Создаем объект QProcess как поле класса (чтобы он не удалился при выходе из функции)
    if (!tensorboardProcess) {
        tensorboardProcess = new QProcess(this);
    } else {
        tensorboardProcess->kill(); // Если сервер уже работал, перезапускаем его
        tensorboardProcess->waitForFinished();
    }

    // 2. Настраиваем аргументы запуска
    // Указываем фиксированный порт, например, 6006, и путь к логам PyTorch
    QStringList arguments;
    arguments << "-m" << "tensorboard.main"
              << "--logdir" << QDir::toNativeSeparators(logDir)
              << "--port" << "6006";

    // 3. Ловим логи запуска сервера (для отладки в вашей консоли вывода)
    connect(tensorboardProcess, &QProcess::readyReadStandardOutput, this, [=]() {
        qDebug() << "TensorBoard Output:" << tensorboardProcess->readAllStandardOutput();
    });

    connect(tensorboardProcess, &QProcess::readyReadStandardError, this, [=]() {
        qDebug() << "TensorBoard Error/Status:" << tensorboardProcess->readAllStandardError();
    });

    // 4. Запускаем процесс через системный интерпретатор Python
    // (Убедитесь, что в используемом окружении Python установлен пакет tensorboard)
    tensorboardProcess->start("python", arguments);

    // 5. Даем серверу 1.5–2 секунды на инициализацию, прежде чем загружать страницу
    QTimer::singleShot(2000, this, [=]() {
        if (ui->tensorboardWebView) {
            ui->tensorboardWebView->setUrl(QUrl("http://localhost:6006"));
        }
    });
}

void Neuro_programm::onTorchCacheProcessFinished() {
    if (!torchCacheProc) return;

    QString confPath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";
    QSettings settings(confPath, QSettings::IniFormat);

    QString out = QString::fromUtf8(torchCacheProc->readAllStandardOutput()).trimmed();
    QString err = QString::fromUtf8(torchCacheProc->readAllStandardError()).trimmed();

    QString torchVersion;

    if (!out.isEmpty() && !out.contains("Error")) {
        // ИСПРАВЛЕНО: Зашиваем черный цвет (#000000) для вывода в HTML
        torchVersion = QString("<span style='color: #000000;'>%1</span>").arg(out);
    } else if (!err.isEmpty()) {
        // Ошибки Python выделим темно-красным для контраста
        torchVersion = QString("<span style='color: #880000;'>Ошибка Python: %1</span>").arg(err.split('\n').constFirst().left(60));
    } else {
        torchVersion = "<span style='color: #000000;'>Ошибка: Интерпретатор вернул пустой ответ</span>";
    }

    // Сохраняем готовую HTML-строку в кэш конфигурации
    settings.setValue("cache/ai_stack", torchVersion);
    settings.setValue("cache/ollama_version", cachedOllamaVersion);
    settings.sync();

    torchCacheProc->deleteLater();
    torchCacheProc = nullptr;
}

void Neuro_programm::showTreeViewContextMenu(const QPoint &pos)
{
    // 1. Получаем индекс элемента, по которому кликнули
    QModelIndex proxyIndex = ui->treeView->indexAt(pos);
    QModelIndex sourceIndex;

    if (proxyIndex.isValid() && this->projectProxyModel != nullptr) {
        sourceIndex = this->projectProxyModel->mapToSource(proxyIndex);
    }

    // Вычисляем путь к элементу на диске
    QString clickedPath = (sourceIndex.isValid() && projectModel != nullptr) ?
                projectModel->filePath(sourceIndex) : currentOpenProjectPath;
    QFileInfo clickedInfo(clickedPath);
    QString parentDir = clickedInfo.isDir() ? clickedPath : clickedInfo.absolutePath();

    // Создаем и стилизуем современное меню IDE
    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
                "QMenu { background-color: #252526; color: #CCCCCC; border: 1px solid #3C3C3C; padding: 4px; }"
                "QMenu::item { padding: 4px 24px 4px 28px; }"
                "QMenu::item:selected { background-color: #094771; color: #FFFFFF; }"
                "QMenu::separator { height: 1px; background-color: #3C3C3C; margin: 4px 0px; }"
                );

    // Считываем текущую конфигурацию venv из QSettings системы
    QSettings settings("PyTorchStudio", "IDE");
    QString globalVenvPath = settings.value("python/global_venv_path", "").toString();

    // ПРОВЕРКА: Кликнули по главному корневому узлу ИЛИ по файлу .pystudio
    bool isMainProjectNode = (clickedPath == currentOpenProjectPath ||
                              clickedInfo.suffix() == "pystudio" ||
                              !proxyIndex.isValid());

    if (isMainProjectNode)
    {
        // =========================================================================
        // БЛОК 1: РАБОТА С VENV
        // =========================================================================
        QAction *actSync = new QAction("🔄 Синхронизировать зависимости venv", &contextMenu);
        QAction *actRunTrain = new QAction("🚀 Запустить обучение (train.py)", &contextMenu);
        actRunTrain->setFont(QFont(actRunTrain->font().family(), -1, QFont::Bold)); // Выделяем жирным
        QAction *actTerminal = new QAction("🖥 Открыть терминал в среде venv", &contextMenu);

        contextMenu.addAction(actSync);
        contextMenu.addAction(actRunTrain);
        contextMenu.addAction(actTerminal);
        contextMenu.addSeparator();

        // =========================================================================
        // БЛОК 2: ADD NEW (Файлы, Папки, Шаблоны ИИ-модулей)
        // =========================================================================
        QMenu *menuAddNew = contextMenu.addMenu("📄 Add New...");
        QAction *actNewFile = menuAddNew->addAction("Новый файл...");
        QAction *actNewFolder = menuAddNew->addAction("Новая папка...");
        menuAddNew->addSeparator();
        QAction *actTemplateTrain = menuAddNew->addAction("Создать train.py по шаблону");
        QAction *actTemplateModel = menuAddNew->addAction("Создать модуль Python (Сеть PyTorch)");
        QAction *actTemplateClass = menuAddNew->addAction("Создать базовый Python класс");

        contextMenu.addSeparator();

        // =========================================================================
        // ЗАКРЫТЬ ПРОЕКТ (Изолированный пункт между блоками)
        // =========================================================================
        QAction *actCloseProject = new QAction(QString("❌ Закрыть проект «%1»").arg(QDir(currentOpenProjectPath).dirName()), &contextMenu);
        contextMenu.addAction(actCloseProject);
        contextMenu.addSeparator();

        // =========================================================================
        // БЛОК 3: УПРАВЛЕНИЕ ДЕРЕВОМ (Развернуть/Свернуть)
        // =========================================================================
        QAction *actExpandNode = new QAction("🔍 Развернуть узел", &contextMenu);
        QAction *actExpandAll  = new QAction("↕ Развернуть все", &contextMenu);
        QAction *actCollapseAll = new QAction("🧱 Свернуть все", &contextMenu);

        contextMenu.addAction(actExpandNode);
        contextMenu.addAction(actExpandAll);
        contextMenu.addAction(actCollapseAll);
        contextMenu.addSeparator();

        // =========================================================================
        // БЛОК 4: GIT
        // =========================================================================
        QMenu *menuGit = contextMenu.addMenu("📁 Git");
        QAction *actGitStatus = menuGit->addAction("Проверить статус (status)");
        QAction *actGitCommit = menuGit->addAction("Зафиксировать изменения (Commit)...");
        QAction *actGitPush   = menuGit->addAction("🚀 Отправить на GitHub (Push)");

        // =========================================================================
        // ПРИВЯЗКА СИГНАЛОВ ДЛЯ ГЛАВНОГО УЗЛА (Лямбда-вызовы)
        // =========================================================================

        // Блок 1
        connect(actSync, &QAction::triggered, this, [this]() {
            this->processEnvironmentAndSync(currentOpenProjectPath, "AUTO");
        });
        connect(actRunTrain, &QAction::triggered, this, [this]() {
            onExecuteScriptRequested(currentOpenProjectPath + "/train.py");
        });
        connect(actTerminal, &QAction::triggered, this, [this, globalVenvPath]() {
            if (panelOther) {
                panelOther->setVisible(true);
#if defined(Q_OS_WIN)
                panelOther->appendLogText("🖥 Терминал venv: " + globalVenvPath + "/Scripts/activate.bat");
#else
                panelOther->appendLogText("🖥 Терминал venv: source " + globalVenvPath + "/bin/activate");
#endif
            }
        });

        // Блок 2 (Стандартное создание)
        connect(actNewFile, &QAction::triggered, this, [this, parentDir]() { onCreateFileRequested(parentDir); });
        connect(actNewFolder, &QAction::triggered, this, [this, parentDir]() { onCreateFolderRequested(parentDir); });

        // Блок 2 (Генерация ИИ-шаблонов кода на диске Arch Linux)
        connect(actTemplateTrain, &QAction::triggered, this, [this, parentDir]() {
            QFile file(parentDir + "/train.py");
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "import torch\nimport torch.nn as nn\nimport torch.optim as optim\n\ndef train():\n    print('Starting PyTorch training loop...')\n\nif __name__ == '__main__':\n    train()\n";
                file.close();
                if (panelOther) panelOther->appendLogText("✔ Шаблон train.py успешно сгенерирован.");
            }
        });
        connect(actTemplateModel, &QAction::triggered, this, [this, parentDir]() {
            bool ok;
            QString name = QInputDialog::getText(this, "Новая модель", "Имя модуля (например, custom_net):", QLineEdit::Normal, "", &ok);
            if (ok && !name.trimmed().isEmpty()) {
                QFile file(parentDir + "/" + name.trimmed() + ".py");
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << "import torch\nimport torch.nn as nn\n\nclass CustomNet(nn.Module):\n    def __init__(self):\n        super().__init__()\n        self.fc = nn.Linear(10, 2)\n\n    def forward(self, x):\n        return self.fc(x)\n";
                    file.close();
                }
            }
        });
        connect(actTemplateClass, &QAction::triggered, this, [this, parentDir]() {
            bool ok;
            QString name = QInputDialog::getText(this, "Новый класс", "Имя Python-файла:", QLineEdit::Normal, "", &ok);
            if (ok && !name.trimmed().isEmpty()) {
                QFile file(parentDir + "/" + name.trimmed() + ".py");
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << "class BaseAgent:\n    def __init__(self):\n        pass\n\n    def step(self):\n        internal_state = {}\n";
                    file.close();
                }
            }
        });

        // Закрыть проект
        connect(actCloseProject, &QAction::triggered, this, [this]() {
            this->syncVenvToRequirements();
            if (projectModel) projectModel->setRootPath("");
            ui->treeView->setModel(nullptr);
            this->setWindowTitle("PyTorch Studio");
        });

        // Блок 3 (Дерево)
        connect(actExpandNode, &QAction::triggered, this, [this, proxyIndex]() {
            if (proxyIndex.isValid()) ui->treeView->expand(proxyIndex);
        });
        connect(actExpandAll, &QAction::triggered, this, [this]() {
            ui->treeView->expandAll();
        });
        connect(actCollapseAll, &QAction::triggered, this, [this]() {
            ui->treeView->collapseAll();
        });

        // Блок 4 (Git)
        connect(actGitStatus, &QAction::triggered, this, [this]() { onGitStatusRequested(); });
        connect(actGitCommit, &QAction::triggered, this, [this]() { onGitCommitRequested(); });
        connect(actGitPush,   &QAction::triggered, this, [this]() { onGitPushRequested(); });

        // Показываем меню на экране
        contextMenu.exec(ui->treeView->viewport()->mapToGlobal(pos));
        return;
    }

    // =========================================================================
    // СЦЕНАРИЙ 2: Стандартный контекстный клик по внутренним файлам и папкам
    // =========================================================================
    QMenu *menuNew = contextMenu.addMenu("Создать...");
    QAction *actInnerFile = menuNew->addAction("Новый файл...");
    QAction *actInnerFolder = menuNew->addAction("Новая папка...");
    contextMenu.addSeparator();

    if (proxyIndex.isValid() && !clickedInfo.isDir() && clickedInfo.suffix() == "py")
    {
        QAction *actRunScript = new QAction("▶ Запустить скрипт в venv", &contextMenu);
        // ИСПРАВЛЕНИЕ: Правильный захват clickedPath внутри [] и один указатель this
        connect(actRunScript, &QAction::triggered, this, [this, clickedPath]() {
            onExecuteScriptRequested(clickedPath);
        });
        contextMenu.addAction(actRunScript);
        contextMenu.addSeparator();
    }

    QAction *actCopyPath = new QAction("Копировать абсолютный путь", &contextMenu);
    QAction *actRename = new QAction("Переименовать (F2)", &contextMenu);
    QAction *actDelete = new QAction("Удалить с диска", &contextMenu);
    contextMenu.addAction(actCopyPath);
    contextMenu.addAction(actRename);
    contextMenu.addAction(actDelete);

    // ИСПРАВЛЕНИЕ: Исправлены все лямбды. Переменные передаются по значению внутри []
    connect(actInnerFile, &QAction::triggered, this, [this, parentDir]() {
        onCreateFileRequested(parentDir);
    });

    connect(actInnerFolder, &QAction::triggered, this, [this, parentDir]() {
        onCreateFolderRequested(parentDir);
    });

    connect(actCopyPath, &QAction::triggered, this, [clickedPath]() {
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(clickedPath));
    });

    connect(actRename, &QAction::triggered, this, [this, proxyIndex]() {
        if (proxyIndex.isValid()) ui->treeView->edit(proxyIndex);
    });

    connect(actDelete, &QAction::triggered, this, [this, clickedPath]()
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Удаление",
                                                                  QString("Удалить безвозвратно '%1'?").arg(QFileInfo(clickedPath).fileName()),
                                                                  QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            if (QFileInfo(clickedPath).isDir()) {
                QDir(clickedPath).removeRecursively();
            } else {
                QFile::remove(clickedPath);
            }
        }
    });

    // Вызываем контекстное меню строго в глобальных координатах экрана
    contextMenu.exec(ui->treeView->viewport()->mapToGlobal(pos));
}

// Обработчик создания нового файла
void Neuro_programm::onCreateFileRequested(const QString &parentPath)
{
    bool ok;
    QString fileName = QInputDialog::getText(this, "Новый файл", "Введите имя файла:", QLineEdit::Normal, "", &ok);
    if (!ok || fileName.trimmed().isEmpty()) return;

    QFile file(parentPath + "/" + fileName.trimmed());
    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        if (panelOther) panelOther->appendLogText("📄 Создан файл: " + file.fileName());
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл на диске.");
    }
}

// Обработчик создания новой папки
void Neuro_programm::onCreateFolderRequested(const QString &parentPath)
{
    bool ok;
    QString folderName = QInputDialog::getText(this, "Новая папка", "Введите имя папки:", QLineEdit::Normal, "", &ok);
    if (!ok || folderName.trimmed().isEmpty()) return;

    QDir dir(parentPath);
    if (dir.mkdir(folderName.trimmed())) {
        if (panelOther) panelOther->appendLogText("📁 Создана директория: " + parentPath + "/" + folderName.trimmed());
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать папку.");
    }
}

// Обработчик асинхронного запуска Python скриптов внутри вашего venv
void Neuro_programm::onExecuteScriptRequested(const QString &scriptPath)
{
    QSettings settings("PyTorchStudio", "IDE");
    QString globalVenvPath = settings.value("python/global_venv_path", "").toString();
    bool useSystemPython = settings.value("python/use_system", false).toBool();

    QString pythonExec;
    if (useSystemPython) {
        pythonExec = "python";
    } else {
#if defined(Q_OS_WIN)
        pythonExec = globalVenvPath + "/Scripts/python.exe";
#else
        pythonExec = globalVenvPath + "/bin/python";
#endif
    }

    if (panelOther) {
        panelOther->setVisible(true);
        panelOther->appendLogText("\n🚀 Запуск процесса: " + pythonExec + " " + scriptPath);
        // Здесь передаем выполнение в ваш встроенный терминал/QProcess
        // Например: panelOther->runPythonProcess(pythonExec, scriptPath);
    }
}

#include <QActionGroup>
#include <QCryptographicHash>

// Вспомогательная функция для расчета MD5-хэша (вставьте её перед методом)
QString Neuro_programm::calculateFileMd5(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (hash.addData(&file)) {
        return hash.result().toHex();
    }
    return QString();
}

// Сам метод сквозной проверки окружения и синхронизации pip
void Neuro_programm::processEnvironmentAndSync(const QString &projectPath, const QString &architecture)
{
    QSettings settings("PyTorchStudio", "IDE");
    QString globalVenvPath = settings.value("python/global_venv_path", "").toString();
    bool useSystemPython = settings.value("python/use_system", false).toBool();
    QString finalPythonExec;

    // Формируем пути исполняемого файла Python
    if (useSystemPython) {
        finalPythonExec = "python";
    } else {
#if defined(Q_OS_WIN)
        finalPythonExec = globalVenvPath + "/Scripts/python.exe";
#else
        finalPythonExec = globalVenvPath + "/bin/python";
#endif
        settings.setValue("python/venv_path", globalVenvPath);
    }

    // Сверка хэшей требований и Двусторонняя синхронизация пакетов
    QString reqFilePath = projectPath + "/requirements.txt";
    if (QFile::exists(reqFilePath))
    {
        QString currentHash = calculateFileMd5(reqFilePath);
        QString savedHash = settings.value("python/last_requirements_hash", "").toString();

        // Если файл изменился или это первое открытие проекта для текущего venv
        if (currentHash.isEmpty() || currentHash != savedHash)
        {
            if (panelOther) panelOther->appendLogText("🔄 Обнаружены изменения в конфигурации проекта. Выравнивание окружения...");

            QProcess pipInstall;

            // Если в проекте жестко задана архитектура CPU, используем официальное whl-зеркало PyTorch
            if (architecture == "CPU") {
                pipInstall.start(finalPythonExec, QStringList() << "-m" << "pip" << "install"
                                 << "--index-url" << "https://pytorch.org"
                                 << "-r" << reqFilePath);
            } else {
                // Стандартная установка для CUDA систем (Arch Linux / Windows)
                pipInstall.start(finalPythonExec, QStringList() << "-m" << "pip" << "install" << "-r" << reqFilePath);
            }

            pipInstall.waitForFinished(-1);

            // Обратная фиксация имен и точных версий пакетов (pip freeze)
            QProcess pipFreeze;
            pipFreeze.setStandardOutputFile(reqFilePath);
            pipFreeze.start(finalPythonExec, QStringList() << "-m" << "pip" << "freeze");
            pipFreeze.waitForFinished(-1);

            // Фиксируем новое состояние кэша requirements.txt
            QString finalHash = calculateFileMd5(reqFilePath);
            settings.setValue("python/last_requirements_hash", finalHash);
            if (panelOther) panelOther->appendLogText("✔ Системный venv и requirements.txt синхронизированы под архитектуру: " + architecture);
        }
        else
        {
            if (panelOther) panelOther->appendLogText("⚡ Зависимости проекта не изменялись. Пропуск синхронизации pip.");
        }
    }

    // Инициализируем LSP сервер, так как pip теперь гарантированно готов
    this->initLspServer();
}

void Neuro_programm::syncVenvToRequirements()
{
    // Если проект не открыт, синхронизировать нечего
    if (currentOpenProjectPath.isEmpty()) return;

    QSettings settings("PyTorchStudio", "IDE");
    bool useSystemPython = settings.value("python/use_system", false).toBool();
    QString globalVenvPath = settings.value("python/global_venv_path", "").toString();
    QString reqFilePath = currentOpenProjectPath + "/requirements.txt";

    // Формируем путь к исполняемому файлу Python среды
    QString pythonExec;
    if (useSystemPython) {
        pythonExec = "python";
    } else {
#if defined(Q_OS_WIN)
        pythonExec = globalVenvPath + "/Scripts/python.exe";
#else
        pythonExec = globalVenvPath + "/bin/python";
#endif
    }

    // Запускаем выгрузку установленных имен пакетов обратно в файл проекта (pip freeze)
    QProcess pipFreeze;
    pipFreeze.setStandardOutputFile(reqFilePath); // Перенаправляем вывод консоли прямо в файл
    pipFreeze.start(pythonExec, QStringList() << "-m" << "pip" << "freeze");

    // Даем процессу до 5 секунд на завершение, чтобы не вешать программу намертво
    if (pipFreeze.waitForFinished(5000)) {
        // Пересчитываем и обновляем MD5 хэш, чтобы при следующем запуске Студия знала,
        // что venv и файл требований идеально синхронизированы, и пропустила pip install
        QString newHash = calculateFileMd5(reqFilePath);
        settings.setValue("python/last_requirements_hash", newHash);

        if (panelOther) {
            panelOther->appendLogText("💾 Пакеты проекта автоматически сохранены в requirements.txt перед закрытием.");
        }
    }
}

void Neuro_programm::onGitStatusRequested()
{
    // ... код проверки статуса репозитория ...
}

void Neuro_programm::onGitCommitRequested()
{
    // ... код ввода commit message и фиксации изменений ...
}

void Neuro_programm::onGitPushRequested()
{
    // ... код отправки изменений на GitHub ...
}

void Neuro_programm::updateCodeSearch()
{
    // 1. Получаем активный редактор кода текущей страницы stackedWidget
    CodeEditor* editor = getCurrentEditor();
    if (!editor) return;

    // 2. Получаем текст и состояние кнопок-триггеров из панели поиска
    QString textToFind = ui->search_panel->getSearchText();
    bool matchCase     = ui->search_panel->isMatchCase();
    bool wholeWords    = ui->search_panel->isWholeWords();
    bool isRegex       = ui->search_panel->isRegex();

    qDebug() << "Ищем текст:" << textToFind << "Регистр:" << matchCase << "Целое слово:" << wholeWords << "Regex:" << isRegex;

    // СВЕРХВАЖНЫЙ ФИКС: Если поле пустое (пользователь очистил его крестиком),
    // мгновенно убираем всю желтую подсветку из файла и выходим.
    if (textToFind.isEmpty()) {
        editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
        return;
    }

    // 3. Создаем контейнер для хранения всех найденных фрагментов выделения
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Настраиваем флаги поиска Qt
    QTextDocument::FindFlags flags = QTextDocument::FindFlags();
    if (matchCase)  flags |= QTextDocument::FindCaseSensitively;
    if (wholeWords) flags |= QTextDocument::FindWholeWords;

    // Настраиваем внешний вид выделения (мягкий пастельный желтый цвет)
    QTextCharFormat format;
    format.setBackground(QColor(255, 235, 156)); // Мягкий желтый (#FFEBB4)
    format.setForeground(QColor(124, 77, 0));    // Темно-коричневый текст для контраста

    QTextDocument *doc = editor->document();
    QTextCursor cursor(doc);

    // Блокируем отрисовку интерфейса на долю секунды, чтобы экран не мерцал при вводе букв
    editor->setUpdatesEnabled(false);

    // 4. Запускаем цикл сканирования документа
    if (isRegex) {
        // Режим РЕГУЛЯРНЫХ ВЫРАЖЕНИЙ
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!matchCase) options |= QRegularExpression::CaseInsensitiveOption;

        QRegularExpression regex(textToFind, options);
        if (regex.isValid()) {
            while (!cursor.isNull() && !cursor.atEnd()) {
                cursor = doc->find(regex, cursor, flags);
                if (!cursor.isNull()) {
                    QTextEdit::ExtraSelection selection;
                    selection.format = format;
                    selection.cursor = cursor;
                    extraSelections.append(selection);
                }
            }
        }
    } else {
        // Режим ОБЫЧНОГО ТЕКСТА
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(textToFind, cursor, flags);
            if (!cursor.isNull()) {
                QTextEdit::ExtraSelection selection;
                selection.format = format;
                selection.cursor = cursor;
                extraSelections.append(selection);
            }
        }
    }

    // 5. Накатываем сформированную желтую разметку на редактор и включаем экран обратно
    editor->setExtraSelections(extraSelections);
    editor->setUpdatesEnabled(true);
}

void Neuro_programm::onFindNext() {
    CodeEditor* editor = getCurrentEditor();
    if (!editor) return;

    QString textToFind = ui->search_panel->getSearchText();
    if (textToFind.isEmpty()) return;

    QTextDocument::FindFlags flags = QTextDocument::FindFlags();
    if (ui->search_panel->isMatchCase()) flags |= QTextDocument::FindCaseSensitively;
    if (ui->search_panel->isWholeWords()) flags |= QTextDocument::FindWholeWords;

    bool found = false;
    QTextCursor foundCursor;

    if (ui->search_panel->isRegex()) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!ui->search_panel->isMatchCase()) options |= QRegularExpression::CaseInsensitiveOption;

        QRegularExpression regex(textToFind, options);
        if (!regex.isValid()) return;

        // Ищем регулярку вперед
        foundCursor = editor->document()->find(regex, editor->textCursor(), flags);
        if (!foundCursor.isNull()) {
            editor->setTextCursor(foundCursor); // Перемещаем каретку
            found = true;
        } else {
            // Зацикливание: ищем с самого начала документа
            QTextCursor startCursor(editor->document());
            foundCursor = editor->document()->find(regex, startCursor, flags);
            if (!foundCursor.isNull()) {
                editor->setTextCursor(foundCursor);
                found = true;
            }
        }
    } else {
        // Обычный поиск вперед
        foundCursor = editor->document()->find(textToFind, editor->textCursor(), flags);
        if (!foundCursor.isNull()) {
            editor->setTextCursor(foundCursor);
            found = true;
        } else {
            QTextCursor startCursor(editor->document());
            foundCursor = editor->document()->find(textToFind, startCursor, flags);
            if (!foundCursor.isNull()) {
                editor->setTextCursor(foundCursor);
                found = true;
            }
        }
    }

    // Передаем найденные координаты курсора в метод желтой подсветки
    if (found) {
        highlightCurrentMatch(editor->textCursor());
    }
}


void Neuro_programm::onFindPrev() {
    CodeEditor* editor = getCurrentEditor();
    if (!editor) return;

    QString textToFind = ui->search_panel->getSearchText();
    if (textToFind.isEmpty()) return;

    // Включаем флаг поиска назад
    QTextDocument::FindFlags flags = QTextDocument::FindBackward;
    if (ui->search_panel->isMatchCase()) flags |= QTextDocument::FindCaseSensitively;
    if (ui->search_panel->isWholeWords()) flags |= QTextDocument::FindWholeWords;

    bool found = false;
    QTextCursor foundCursor;

    if (ui->search_panel->isRegex()) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!ui->search_panel->isMatchCase()) options |= QRegularExpression::CaseInsensitiveOption;

        QRegularExpression regex(textToFind, options);
        if (!regex.isValid()) return;

        foundCursor = editor->document()->find(regex, editor->textCursor(), flags);
        if (!foundCursor.isNull()) {
            editor->setTextCursor(foundCursor);
            found = true;
        } else {
            // Зацикливание: переносим виртуальный поиск в самый конец файла
            QTextCursor endCursor(editor->document());
            endCursor.movePosition(QTextCursor::End);
            foundCursor = editor->document()->find(regex, endCursor, flags);
            if (!foundCursor.isNull()) {
                editor->setTextCursor(foundCursor);
                found = true;
            }
        }
    } else {
        foundCursor = editor->document()->find(textToFind, editor->textCursor(), flags);
        if (!foundCursor.isNull()) {
            editor->setTextCursor(foundCursor);
            found = true;
        } else {
            QTextCursor endCursor(editor->document());
            endCursor.movePosition(QTextCursor::End);
            foundCursor = editor->document()->find(textToFind, endCursor, flags);
            if (!foundCursor.isNull()) {
                editor->setTextCursor(foundCursor);
                found = true;
            }
        }
    }

    if (found) {
        highlightCurrentMatch(editor->textCursor());
    }
}

void Neuro_programm::onSelectAll() {
    // 1. Получаем активный редактор кода
    QPlainTextEdit* editor = qobject_cast<QPlainTextEdit*>(getCurrentEditor());
    if (!editor) return;

    // 2. Получаем искомое словосочетание из панели поиска
    QString textToFind = ui->search_panel->getSearchText();
    if (textToFind.isEmpty()) return;

    // 3. Создаем список для хранения выделенных фрагментов
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Настраиваем правила поиска (регистр, целое слово, регулярка)
    QTextDocument::FindFlags flags = QTextDocument::FindFlags();
    if (ui->search_panel->isMatchCase()) flags |= QTextDocument::FindCaseSensitively;
    if (ui->search_panel->isWholeWords()) flags |= QTextDocument::FindWholeWords;

    // Настраиваем внешний вид выделения (например, желтый фон текста)
    QTextCharFormat format;
    format.setBackground(QColor(255, 235, 156)); // Мягкий желтый цвет (Hex: #FFEBB4)
    format.setForeground(QColor(124, 77, 0));

    // Сохраняем текущий документ для поиска
    QTextDocument *doc = editor->document();
    QTextCursor cursor(doc);

    // 4. Цикл поиска всех вхождений в файле
    if (ui->search_panel->isRegex()) {
        // Поиск по регулярному выражению
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!ui->search_panel->isMatchCase()) options |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(textToFind, options);

        if (!regex.isValid()) return;

        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(regex, cursor, flags);
            if (!cursor.isNull()) {
                QTextEdit::ExtraSelection selection;
                selection.format = format;
                selection.cursor = cursor;
                extraSelections.append(selection);
            }
        }
    } else {
        // Обычный поиск слова/фразы
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(textToFind, cursor, flags);
            if (!cursor.isNull()) {
                QTextEdit::ExtraSelection selection;
                selection.format = format;
                selection.cursor = cursor;
                extraSelections.append(selection);
            }
        }
    }

    // 5. Применяем множественное выделение к редактору
    editor->setExtraSelections(extraSelections);
    editor->setFocus();
}

CodeEditor* Neuro_programm::getCurrentEditor() {
    if (!ui->centralStackedWidget || ui->centralStackedWidget->count() == 0) {
        return nullptr;
    }

    // 1. Берем виджет ТЕКУЩЕЙ активной страницы, которую видит пользователь
    QWidget* currentPage = ui->centralStackedWidget->currentWidget();
    if (!currentPage) return nullptr;

    // 2. Ищем строго ваш кастомный CodeEditor на этой странице
    CodeEditor* editor = currentPage->findChild<CodeEditor*>();
    return editor;
}

void Neuro_programm::highlightCurrentMatch(QTextCursor matchCursor) {
    CodeEditor* editor = getCurrentEditor();
    if (!editor) return;

    // 1. Сначала запускаем полное фоновое сканирование (заполнит блекло-желтым цветом)
    updateCodeSearch();

    // 2. Берем список уже созданных фоновых выделений
    QList<QTextEdit::ExtraSelection> selections = editor->extraSelections();

    // 3. Создаем новое, максимально яркое выделение для ТЕКУЩЕГО слова
    QTextEdit::ExtraSelection currentSelection;
    currentSelection.cursor = matchCursor;

    QTextCharFormat format;
    format.setBackground(QColor(255, 210, 0));  // Насыщенный золотисто-желтый цвет
    format.setForeground(Qt::black);            // Черный текст для контраста
    format.setFontWeight(QFont::Bold);          // Можно сделать текст жирным, чтобы он выделялся
    currentSelection.format = format;

    // Добавляем текущее яркое слово поверх фоновых
    selections.append(currentSelection);

    // Применяем комбинированную разметку к редактору
    editor->setExtraSelections(selections);
}

void Neuro_programm::onReplaceCurrent() {
    CodeEditor* editor = getCurrentEditor();
    if (!editor) return;

    QString textToFind = ui->search_panel->getSearchText();
    QString replaceText = ui->search_panel->getReplaceText();
    if (textToFind.isEmpty()) return;

    QTextCursor cursor = editor->textCursor();

    // Проверяем, выделено ли что-то прямо сейчас
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        bool isMatch = false;

        // Проверяем соответствие выделенного текста условиям (с учетом регулярки или регистра)
        if (ui->search_panel->isRegex()) {
            QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
            if (!ui->search_panel->isMatchCase()) options |= QRegularExpression::CaseInsensitiveOption;
            QRegularExpression regex(textToFind, options);
            isMatch = regex.match(selectedText).hasMatch();
        } else {
            Qt::CaseSensitivity cs = ui->search_panel->isMatchCase() ? Qt::CaseSensitive : Qt::CaseInsensitive;
            isMatch = (selectedText.compare(textToFind, cs) == 0);
        }

        // Если выделенный текст — это то, что мы искали, заменяем его
        if (isMatch) {
            cursor.insertText(replaceText);
            // Обновляем желтые маркеры на экране, так как текст изменился
            updateCodeSearch();
        }
    }
}

void Neuro_programm::onReplaceAndFindNext() {
    // 1. Сначала делаем замену текущего выделенного фрагмента
    onReplaceCurrent();

    // 2. Сразу же ищем и подсвечиваем следующее совпадение по тексту
    onFindNext();
}

void Neuro_programm::onReplaceAll() {
    CodeEditor* editor = getCurrentEditor();
    if (!editor) return;

    QString textToFind = ui->search_panel->getSearchText();
    QString replaceText = ui->search_panel->getReplaceText();
    if (textToFind.isEmpty()) return;

    // Сохраняем исходную позицию курсора пользователя
    QTextCursor originalCursor = editor->textCursor();

    // Отключаем обновление экрана для моментальной скорости работы
    editor->setUpdatesEnabled(false);

    // Создаем рабочий курсор и переносим его в абсолютное начало файла
    QTextCursor searchCursor(editor->document());
    searchCursor.movePosition(QTextCursor::Start);

    // Настраиваем базовые флаги поиска
    QTextDocument::FindFlags flags = QTextDocument::FindFlags();
    if (ui->search_panel->isMatchCase())  flags |= QTextDocument::FindCaseSensitively;
    if (ui->search_panel->isWholeWords()) flags |= QTextDocument::FindWholeWords;

    int replaceCount = 0;

    // Объединяем все замены в одну транзакцию (чтобы работал Ctrl+Z для всей массовой замены сразу)
    searchCursor.beginEditBlock();

    if (ui->search_panel->isRegex()) {
        // --- РЕЖИМ РЕГУЛЯРНЫХ ВЫРАЖЕНИЙ ---
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!ui->search_panel->isMatchCase()) options |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression regex(textToFind, options);

        if (!regex.isValid()) {
            searchCursor.endEditBlock();
            editor->setUpdatesEnabled(true);
            return;
        }

        while (true) {
            // Ищем совпадение от текущей позиции рабочего курсора
            searchCursor = editor->document()->find(regex, searchCursor, flags);
            if (searchCursor.isNull()) break; // Если совпадений больше нет — выходим из цикла

            // Производим замену текста
            searchCursor.insertText(replaceText);
            replaceCount++;

            // ЖЕЛЕЗНЫЙ ФИКС ЗАЦИКЛИВАНИЯ:
            // Если текст замены пустой или совпадает с искомым, find() может застрять.
            // Принудительно сдвигаем позицию курсора в конец вставленного фрагмента.
            searchCursor.setPosition(searchCursor.position());
        }
    } else {
        // --- РЕЖИМ ОБЫЧНОГО ТЕКСТА ---
        while (true) {
            searchCursor = editor->document()->find(textToFind, searchCursor, flags);
            if (searchCursor.isNull()) break;

            searchCursor.insertText(replaceText);
            replaceCount++;

            // ЖЕЛЕЗНЫЙ ФИКС ЗАЦИКЛИВАНИЯ для обычного текста:
            searchCursor.setPosition(searchCursor.position());
        }
    }

    // Закрываем транзакцию правок
    searchCursor.endEditBlock();

    // Возвращаем курсор пользователя на его исходное место
    editor->setTextCursor(originalCursor);

    // Включаем отрисовку графики обратно
    editor->setUpdatesEnabled(true);

    // Перерисовываем актуальную желтую разметку для оставшихся совпадений
    updateCodeSearch();

    // Выводим отчет в статусбар
    ui->statusbar->showMessage(QString("✔ Успешно заменено совпадений: %1").arg(replaceCount), 4000);
}

void Neuro_programm::updateCustomTitle(const QString &fileName) {
    // ПРОВЕРКА №1: Проверяем, существует ли сам графический элемент на форме
    if (!titleLabel) {
        qDebug() << "❌ КРИТИЧЕСКАЯ ОШИБКА: Указатель titleLabel равен nullptr!";
        return;
    }

    QString projectName = "";
    if (!currentOpenProjectPath.isEmpty()) {
        QFileInfo projectInfo(currentOpenProjectPath);
        projectName = projectInfo.fileName();
    }

    QString finalTitleText = "";
    if (fileName.isEmpty()) {
        if (!projectName.isEmpty()) finalTitleText = QString("%1 — PyTorch Studio").arg(projectName);
        else finalTitleText = "PyTorch Studio";
    } else {
        if (!projectName.isEmpty()) finalTitleText = QString("%1@%2 — PyTorch Studio").arg(fileName, projectName);
        else finalTitleText = QString("%1 — PyTorch Studio").arg(fileName);
    }

    // Выводим в лог то, что сейчас попытаемся нарисовать на экране
    qDebug() << "🎯 Успешно устанавливаем текст в кастомную шапку:" << finalTitleText;

    // Физически меняем текст на экране приложения
    titleLabel->setText(finalTitleText);
}

void Neuro_programm::updateProjectsListFromSettings()
{
    if (!ui->projectListWidget) return;
    ui->projectListWidget->clear(); // Очищаем старый список перед заполнением

    // =========================================================================
    // ПРЯМОЙ СЧИТ КУРСА ИЗ ВАШЕГО СУЩЕСТВУЮЩЕГО МЕНЮ "ОТКРЫТЬ НЕДАВНИЕ"
    // =========================================================================
    // Замените ui->menuRecentFiles на точное имя вашего подменю недавних файлов!
    if (!recentProjectsMenu) return;

    // Извлекаем список всех экшенов (строк) из вашего готового меню
    QList<QAction*> recentActions = recentProjectsMenu->actions();

    qDebug() << ">>> [СИНХРОНИЗАЦИЯ] Найдено рабочих строк в меню недавних:" << recentActions.size();

    for (QAction *action : recentActions) {
        if (!action || action->isSeparator() || action->text().isEmpty()) continue;

        // В Qt недавние пути обычно хранятся прямо в тексте экшена или в его data()
        QString fullPath = action->data().toString();
        if (fullPath.isEmpty()) {
            fullPath = action->text(); // Резервный случай, если путь записан в текст
        }

        // Очищаем от возможных системных горячих клавиш или номеров (например, "1. /path/to...")
        fullPath.remove(QRegularExpression("^\\d+\\.\\s*"));
        fullPath = fullPath.trimmed();

        if (fullPath.isEmpty() || !QFile::exists(fullPath)) continue;

        QFileInfo fileInfo(fullPath);

        // Создаем элемент списка. Выводим пользователю красивое имя файла (z1.pystudio)
        QListWidgetItem *item = new QListWidgetItem(fileInfo.fileName(), ui->projectListWidget);

        // НАМЕРТВО сохраняем чистый абсолютный путь в скрытые метаданные элемента
        item->setData(Qt::UserRole, fullPath);

        // Ставим иконку
        item->setIcon(this->style()->standardIcon(QStyle::SP_FileIcon));
        item->setToolTip(fullPath);

        ui->projectListWidget->addItem(item);
    }
}


void Neuro_programm::loadProjectFromSettingsList(QListWidgetItem *item)
{
    if (!item) return;

    // 1. Восстанавливаем сохраненный путь к файлу проекта
    QString fullPath = item->data(Qt::UserRole).toString();

    // =========================================================================
    // ЖЕЛЕЗНЫЙ ФИКС: Извлекаем индекс экшена из метаданных (Qt::UserRole + 1)
    // =========================================================================
    int actionIndex = item->data(Qt::UserRole + 1).toInt();

    // 2. РАЗВОРАЧИВАЕМ ЦЕНТРАЛЬНЫЙ СТЭК: Показываем рабочую область кода
    this->setIDEInStartMode(false);

    // 3. Запускаем аппаратный триггер загрузки проекта через ваш массив
    if (actionIndex >= 0 && actionIndex < MaxRecentFiles && recentProjectActions[actionIndex] != nullptr)
    {
        qDebug() << ">>> [БЫСТРЫЙ КЛИК] Запуск проекта из истории под индексом:" << actionIndex;

        // Программно имитируем клик по меню "Открыть недавние" для полной загрузки GUI панели параметров
        recentProjectActions[actionIndex]->trigger();

        if (this->statusBar()) {
            this->statusBar()->showMessage(QString("Проект %1 успешно загружен").arg(QFileInfo(fullPath).baseName()), 3000);
        }
        return;
    }

    // Резервный случай: если массив дал сбой, принудительно открываем хотя бы текст кода
    if (!fullPath.isEmpty() && QFile::exists(fullPath)) {
        this->openNewFileInEditor(fullPath);
    }
}

void Neuro_programm::setIDEInStartMode(bool isStartMode)
{
    // =========================================================================
    // ГЛАВНЫЙ UX ТРИГГЕР НАВИГАТОРА ФУНКЦИЙ (comboDevice ПОД ФАЙЛОВЫМ МЕНЮ)
    // =========================================================================
    if (ui->comboDevice) {
        // Если isStartMode равен true (файлы закрыты) -> !isStartMode станет false и скроет комбобокс.
        // Если isStartMode равен false (файл открылся) -> !isStartMode станет true и выведет его на экран!
        ui->comboDevice->setVisible(!isStartMode);
    }
    // =========================================================================

    if (isStartMode)
    {
        // РЕЖИМ СТАРТА:
        int placeholderIndex = this->property("placeholderIndex").toInt();
        if (ui->centralStackedWidget && placeholderIndex > 0)
        {
            ui->centralStackedWidget->setCurrentIndex(placeholderIndex);
        }

        if (ui->leftDockWidget) ui->leftDockWidget->setVisible(true);
        if (actProject) actProject->setChecked(true);

        // Принудительно гасим лейбл координат в стартовом режиме
        if (ui && ui->cursorPosLabel)
        {
            ui->cursorPosLabel->hide();
        }
    }
    else {
        // РЕЖИМ ПРОЕКТА: Просто даем системе работать
        if (ui->centralStackedWidget)
        {
            ui->centralStackedWidget->setVisible(true);
        }
    }
}


void Neuro_programm::updateCursorPositionIndicator()
{
    // Проверяем, что графический интерфейс и виджет из Designer существуют в памяти
    if (!ui || !ui->cursorPosLabel) return;

    // 1. Извлекаем указатель на текущую открытую страницу stackedWidget
    QWidget *currentPage = ui->centralStackedWidget->currentWidget();
    if (!currentPage) {
        ui->cursorPosLabel->hide(); // ФАЙЛОВ НЕТ: Полностью скрываем лэйбл с панели!
        return;
    }

    // 2. Ищем активный текстовый редактор на этой странице
    CodeEditor *activeEditor = currentPage->findChild<CodeEditor*>();

    // =========================================================================
    // СТРОГАЯ ЗАЩИТА: Скрываем лэйбл на сервисных экранах и плейсхолдере шорткатов
    // =========================================================================
    if (!activeEditor ||
            activeEditor->objectName() == "MAIN_SCREEN" ||
            activeEditor->objectName() == "AI_CHAT_SCREEN" ||
            activeEditor->objectName() == "JETBRAINS_PLACEHOLDER" ||
            currentPage->objectName() == "JETBRAINS_PLACEHOLDER")
    {
        ui->cursorPosLabel->hide(); // Скрываем лэйбл, чтобы он не висел пустым
        return;
    }

    // =========================================================================
    // РАБОЧИЙ РЕЖИМ: Если код дошел сюда — файл открыт. Показываем индикатор!
    // =========================================================================
    ui->cursorPosLabel->show();

    // 3. МАТЕМАТИЧЕСКИЙ РАСЧЕТ КООРДИНАТ КУРСOРА QT
    QTextCursor cursor = activeEditor->textCursor();

    // Блоки и символы в Qt нумеруются с 0, прибавляем 1 для привычного IDE-вида
    int line = cursor.blockNumber() + 1;
    int column = cursor.position() - cursor.block().position() + 1;

    // 4. ВЫВОД В ДИЗАЙНЕРСКИЙ ВИДЖЕТ В НOВOМ ФOРМАТЕ
    ui->cursorPosLabel->setText(QString("Строка: %1, Столбец: %2").arg(line).arg(column));
    ui->cursorPosLabel->adjustSize();
}

void Neuro_programm::updateJediStatusText(const QString &message, bool isError)
{
    if (statusLogLabel) {
        // 1. По умолчанию ставим оригинальный текст
        QString finalMessage = message;

        if (isError) {
            // Выделяем текст ярко-красным цветом для ошибок синтаксиса Jedi
            statusLogLabel->setStyleSheet("color: #ef5350; font-weight: bold; padding-left: 5px;");

            // Профессиональный UX: если это ошибка, добавляем перед ней значок-индикатор ❌
            finalMessage = "❌ " + message;
        } else {
            // Выделяем зеленым цветом, если код чист
            statusLogLabel->setStyleSheet("color: #4caf50; font-weight: bold; padding-left: 5px;");
            finalMessage = "✔ " + message;
        }

        // 2. Отправляем итоговую строку в наш усекаемый ElidedLabel
        statusLogLabel->setFullText(finalMessage);
    }
}

void Neuro_programm::updateJediStatusTextFromLsp(int errorCount)
{
    // ЗАЩИТА: Проверяем, существует ли кастомный лейбл в памяти ОЗУ
    if (!statusLogLabel) return;

    if (errorCount > 0) {
        // Зажигаем красный маркер и выводим ТОЧНОЕ количество синтаксических ошибок
        statusLogLabel->setStyleSheet("color: #ef5350; font-weight: bold; padding-left: 5px;");
        statusLogLabel->setFullText(QString("❌ Jedi: Найдено ошибок: %1").arg(errorCount));
    } else {
        // Если ошибок нет — возвращаем красивый зеленый статус
        statusLogLabel->setStyleSheet("color: #4caf50; font-weight: bold; padding-left: 5px;");
        statusLogLabel->setFullText("✔ Jedi: Код успешно проверен. Ошибок нет.");
    }

    // Принудительно заставляем виджет перерисовать QSS стиль на экране Linux
    statusLogLabel->style()->unpolish(statusLogLabel);
    statusLogLabel->style()->polish(statusLogLabel);
    statusLogLabel->update();
}

void Neuro_programm::updateFunctionNavigator(CodeEditor *editor)
{
    if (!editor || !ui->comboDevice) return;

    // Временно блокируем сигналы, чтобы комбобокс не генерировал события при очистке
    ui->comboDevice->blockSignals(true);
    ui->comboDevice->clear();
    ui->comboDevice->addItem("🔍 Навигация по функциям...");

    QString text = editor->toPlainText();
    // Разбиваем текст на строки
    QStringList lines = text.split('\n');

    // Регулярное выражение для поиска классов и функций Python
    static const QRegularExpression pyRegex("^[ ]*(def|class)[ ]+([a-zA-Z0-9_]+)");

    for (int i = 0; i < lines.size(); ++i) {
        QRegularExpressionMatch match = pyRegex.match(lines[i]);
        if (match.hasMatch()) {
            QString type = match.captured(1); // "def" или "class"
            QString name = match.captured(2); // Имя функции/класса

            // Формируем красивое имя для отображения
            QString displayName = (type == "class") ? "🔶 class " + name : "🔷 " + name + "()";

            // Добавляем в комбобокс, а в скрытую роль Qt::UserRole сохраняем реальный номер строки (i)
            ui->comboDevice->addItem(displayName, i);
        }
    }

    ui->comboDevice->blockSignals(false);
}

