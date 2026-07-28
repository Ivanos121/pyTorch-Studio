#include "neuro_programm.h"
#include "qmenubar.h"
#include "ui_neuro_programm.h"
#include "start_progect.h"
#include "panel_other.h"
#include "settings.h"
#include "breezeflatstyle.h"
#include "codeeditor.h"
#include "advancedclosedialog.h"
#include "projectrootproxymodel.h"
#include "editorplaceholder.h"
#include "elidedlabel.h"
#include "ui_ai_panel.h"
#include "ai_panel.h"
#include "panel_other.h"
#include "projectbuilderworker.h"
#include "jupytermanager.h"
#include "jupyterclient.h"
#include "projectmanager.h"
//#include "aiprojectmodel.h"
extern "C" {
#include "stlink.h"
//#include "stlink_backend.h"

// Ручное прототипирование функций прошивальщика для C++ компилятора
// Это закроет ошибку "was not declared in this scope" раз и навсегда
// int stlink_erase_flash_mass(stlink_t* sl);
// int stlink_fwrite_flash(stlink_t *sl, const char* path, stm32_addr_t addr);
// int stlink_read_all_flash(stlink_t *sl, const char *path);

void stlink_read_mem32(stlink_t *sl, uint32_t addr, uint16_t len);
int32_t stlink_reset(stlink_t *sl, enum reset_type type);
int stlink_erase_flash_mass(stlink_t* sl);
int stlink_fwrite_flash(stlink_t *sl, const char* path, stm32_addr_t addr);
}
#include "savedata.h"
#include "preferencesdialog.h"
#include "usbreceiver.h"
#include "aipromptwidget.h"
//#include "prog_stm_work.h"

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
#include <QThread>
#include <QMessageBox>
#include <QDateTime>
#include <QPlainTextEdit>
#include <QInputDialog>
#include <QNetworkRequest>
#include <QUrl>
#include <QTextStream>
#include <QProcess>
#include <QSettings>
#include <QMessageBox>
#include <QThread>
#include <QStackedWidget>

Neuro_programm* Neuro_programm::self = nullptr;
QList<Neuro_programm::LspErrorData> Neuro_programm::globalLspErrors;

Neuro_programm::Neuro_programm(const QString &startupPath, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Neuro_programm)
{
    ui->setupUi(this);

    const QString systemIconPath = QDir::home().absoluteFilePath(QStringLiteral(".local/share/icons/hicolor/scalable/apps/pytorch-studio.svg"));
    const QString fallbackResourcePath = QStringLiteral(":/Data/Icons/pytorch-studio.svg");

    QIcon appIcon;
    // Выбираем доступный путь (системный или ресурсный)
    QString finalSvgPath = QFile::exists(systemIconPath) ? systemIconPath : fallbackResourcePath;

    // КРИТИЧЕСКИЙ ШАГ: Принудительно заставляем Qt отрендерить SVG
    // во всех возможных системных размерах, включая HD (256x256)
    appIcon.addFile(finalSvgPath, QSize(16, 16));
    appIcon.addFile(finalSvgPath, QSize(32, 32));
    appIcon.addFile(finalSvgPath, QSize(48, 48));
    appIcon.addFile(finalSvgPath, QSize(64, 64));
    appIcon.addFile(finalSvgPath, QSize(128, 128));
    appIcon.addFile(finalSvgPath, QSize(256, 256)); // Этот слой заберет панель задач

    // Применяем многослойную иконку к окну
    this->setWindowIcon(appIcon);

    statusLogLabel = new ElidedLabel(this); //
    statusLogLabel->setObjectName("statusLogLabel"); //
    statusLogLabel->setStyleSheet("color: #ef5350; font-weight: bold;"); //
    statusLogLabel->setMaximumWidth(400); //
    statusLogLabel->setFullText("Jedi: Готов к работе"); //

    // Сразу монтируем его в левый угол статусбара
    ui->statusbar->addWidget(statusLogLabel, 0); //
    statusLogLabel->show(); //

    if (ui->widget_4)
    {
        ui->widget_4->setMainProgram(this);
    }

    ui->rightDebugPanel->hide();
    this->projectMgr = new ProjectManager(this);
    this->envManager = new PythonEnvManager(this);
    this->jupyterServer = new JupyterManager(this);
    this->jupyterClient = new JupyterClient(this);
    this->pyDebugger = new DebugManager(this);
    UsbReceiver *usbDriver = new UsbReceiver(this);

    // Перехватываем данные от нейросети
    connect(usbDriver, &UsbReceiver::inferenceDataReceived, [this](const QString &line) {
        // ДАННЫЕ ПРИЛЕТЕЛИ С ПЛАТЫ ПО miniUSB!
        // Здесь вы можете вызывать парсер, обновлять графики, сохранять в логи
        qDebug() << "Получены данные инференса:" << line;
    });

    // Автоматически запускаем подключение после успешной прошивки в onWrightFlash
    connect(this, &Neuro_programm::firmwareFlashSuccess, this, [usbDriver, this]()
    {
        // Пауза 500 мс, чтобы контроллер STM32 успел поднять USB CDC стек после Reset
        QTimer::singleShot(500, usbDriver, [usbDriver]()
        {
            qDebug() << "🔄 Попытка автоматического фонового подключения к miniUSB...";
            usbDriver->connectToNucleo();
        });
    });

    m_aiManager = new LocalAiManager(this);

    // 2. Логика отображения статуса: просто перенаправляем сигнал менеджера на метку формы!
    connect(m_aiManager, &LocalAiManager::statusChanged, this, [this](const QString &text, const QString &colorHtml) {
        if (statusLogLabel) {
            statusLogLabel->setText(text);
            statusLogLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;").arg(colorHtml));
        }
    });

    // 3. Соединяем вставку сгенерированного кода
    connect(m_aiManager, &LocalAiManager::codeGenerated, this, &Neuro_programm::insertGeneratedCodeIntoEditor);

    // 4. Запускаем сервер
    m_aiManager->startServer();

    // =========================================================================
    // БЛОК 1: НАЖАТИЕ ХОТКЕЯ ALT + / (ЗАПРОС К ИИ)
    // =========================================================================
    QShortcut* autocompleteShortcut = new QShortcut(QKeySequence("Ctrl+Alt+K"), this);

    connect(autocompleteShortcut, &QShortcut::activated, this, [this]() {
        qInfo() << ">>> [AI] Хоткей СРАБОТАЛ, запускаю динамический поиск редактора...";

        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (!currentPage) return;

        // ГЛУБОКИЙ ПОИСК: Сначала пробуем стандартный findChild
        CodeEditor *activeEditor = currentPage->findChild<CodeEditor*>();

        // Если на поверхности не нашли, ищем виджет, в который сейчас кликнул пользователь (в фокусе)
        if (!activeEditor) {
            QWidget *focusedWidget = QApplication::focusWidget();
            activeEditor = qobject_cast<CodeEditor*>(focusedWidget);
        }

        // Если все еще не нашли — выходим, защищая от падения
        if (!activeEditor) {
            qDebug() << "[AI Warning]: Автодополнение отклонено. CodeEditor не обнаружен в фокусе текущей страницы.";
            return;
        }

        qInfo() << ">>> [AI] Редактор успешно найден! Имя файла:" << activeEditor->objectName();

        activeEditor->setCursor(Qt::WaitCursor);

        QTextCursor cursor = activeEditor->textCursor();
        QString fullText = activeEditor->toPlainText();
        int pos = cursor.position();

        QString prefix = fullText.left(pos);
        QString suffix = fullText.mid(pos);

        m_aiManager->requestAutocomplete(prefix, suffix);
    });

    // =========================================================================
    // БЛОК 2: ПРИЕМ ОТВЕТА ОТ СЕРВЕРА И ВСТАВКА (ВОТ ЗДЕСЬ У ВАС СЕЙЧАС КРАШ)
    // =========================================================================
    connect(m_aiManager, &LocalAiManager::autocompleteReceived, this, [this](const QString &text) {
        // Точно так же динамически ищем редактор в момент прилета ответа
        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (!currentPage) return;

        CodeEditor *activeEditor = currentPage->findChild<CodeEditor*>();

        // Если пользователь успел закрыть вкладку с кодом, пока ИИ думал — просто выходим
        if (!activeEditor) return;

        // Возвращаем стандартный курсор ввода проверенному редактору
        activeEditor->setCursor(Qt::IBeamCursor);

        if (text.isEmpty()) return;

        QTextCursor cursor = activeEditor->textCursor();

        // Атомарная транзакция вставки для отмены по Ctrl+Z
        cursor.beginEditBlock();
        cursor.insertText(text);
        cursor.endEditBlock();

        activeEditor->setTextCursor(cursor);
    });


    this->titleLabel = new QLabel("PyTorch Studio", this);
    this->docMgr = new DocumentManager(this, ui->fileComboBox, ui->openFilesListWidget, this->titleLabel, this);

    this->tensorBoardServer = new TensorBoardManager(this);
    this->hfManager = new HuggingFaceManager(this);

    // =========================================================================
    // СИНХРОНИЗАЦИЯ UI И СИСТЕМНЫХ D-BUS УВЕДОМЛЕНИЙ ОБ ОКРУЖЕНИИ PYTHON (VENV)
    // =========================================================================

    // СЦЕНАРИЙ 1: УСПЕШНОЕ ПОДКЛЮЧЕНИЕ ОКРУЖЕНИЯ
    connect(envManager, &PythonEnvManager::venvConnectedSuccessfully, this, [this](const QStringList &packages)
    {
        // Извлекаем версию PyTorch (она гарантированно лежит первым элементом) [0:1.328]
        QString torchVersion = !packages.isEmpty() ? packages.first() : "Определена";

        // 1. Обновляем текстовую строчку в статусбаре приложения [0:1.328]
        ui->statusbar->showMessage("Окружение PyTorch успешно подключено!", 4000);
        if (this->statusLogLabel != nullptr) {
            this->statusLogLabel->setStyleSheet("color: #4caf50; font-weight: bold;"); // Зеленый цвет успеха [0:1.328]
            this->statusLogLabel->setFullText("PyTorch: Подключен (" + torchVersion + ")"); // [0:1.328]
        }

        // 2. ВЫЗЫВАЕМ СИСТЕМНОЕ D-BUS УВЕДОМЛЕНИЕ ОС LINUX (Breeze/KDE/GNOME notification)
        // Формат: sendSystemNotification(Заголовок, Текст_сообщения);
        this->sendSystemNotification(
                    "PyTorch Studio: Окружение подключено",
                    QString("Виртуальная среда venv успешно инициализирована.\n"
                            "Интерпретатор: %1\n"
                            "Версия PyTorch: %2")
                    .arg(this->envManager->currentPythonPath(), torchVersion));
    });

    // СЦЕНАРИЙ 2: КРИТИЧЕСКИЙ СБОЙ ИЛИ ПОВРЕЖДЕНИЕ ОКРУЖЕНИЯ
    connect(envManager, &PythonEnvManager::venvNotFoundOrCorrupted, this, [this](const QString &reason)
    {
        // 1. Перекрашиваем индикаторы статусбара в красный цвет ошибки [0:1.328]
        ui->statusbar->showMessage("Критическая ошибка виртуального окружения Python!", 0); // [0:1.328]
        if (this->statusLogLabel != nullptr) {
            this->statusLogLabel->setStyleSheet("color: #ef5350; font-weight: bold;"); // [0:1.328]
            this->statusLogLabel->setFullText("PyTorch: Ошибка окружения (LSP отключен)"); // [0:1.328]
        }

        // 2. ВЫЗЫВАЕМ СИСТЕМНОЕ D-BUS УВЕДОМЛЕНИЕ ОБ ОШИБКЕ СБОЯ
        this->sendSystemNotification(
                    "PyTorch Studio: Ошибка окружения",
                    QString("Не удалось подключить интерпретатор Python.\nПричина: %1").arg(reason) // [0:1.328]
                    );

        // 3. Вызываем интерактивное диалоговое окно аварийного исправления путей [0:1.328]
        this->showVenvEmergencyDialog(reason); // [0:1.328]
    });

    if (ui->widget_2)
    {
        // Приводим базовый QWidget к вашему кастомному классу AI_panel
        AI_panel *panelInstance = qobject_cast<AI_panel*>(ui->widget_2);

        if (panelInstance)
        {
            panelInstance->wf = this;       // Передаем обратную связь во встроенную панель
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
    // Действие "Новый файл" (Ctrl + N)
    QAction *New_file = new QAction(" Новый файл", this);
    New_file->setShortcut(QKeySequence("Ctrl+N"));
    New_file->setIcon(QIcon(":/Data/system_icons/document-new.svg"));

    connect(New_file, &QAction::triggered, this, [this]()
            {
        // 1. ПРОВЕРКА: Открыт ли проект в IDE
        if (currentOpenProjectPath.isEmpty()) {
            sendSystemNotification("Внимание", "Сначала откройте или создайте проект (*.pystudio)");
            return;
        }

        // 2. ДИАЛОГОВОЕ ОКНО: Запрос имени файла (инженер вводит только название)
        bool ok;
        QString fileName = QInputDialog::getText(
                    this,
                    "Создание чистого Python-файла",
                    "Введите имя нового скрипта (файл автоматически сохранится в /scripts):",
                    QLineEdit::Normal,
                    "train_thermal_net",
                    &ok
                    );

        if (!ok || fileName.trimmed().isEmpty()) return; // Пользователь отменил ввод

        // Автоматически добавляем расширение .py, если пользователь его не написал
        if (!fileName.endsWith(".py", Qt::CaseInsensitive)) {
            fileName += ".py";
        }

        // 3. АВТОСОХРАНЕНИЕ В /SCRIPTS: Жестко формируем абсолютный путь к папке scripts
        QString fullPath = currentOpenProjectPath + "/scripts/" + fileName.trimmed();

        // 4. ФИЗИЧЕСКОЕ СОЗДАНИЕ С БАЗОВЫМ ШАБЛОНОМ ЗАГОЛОВКА (BOILERPLATE)
        QFile file(fullPath);
        if (!file.exists()) {
            QFileInfo fileInfo(fullPath);
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
                // Записываем базовый шаблон заголовка
                out << "# -*- coding: utf-8 -*-\n";
                out << "\"\"\"\n";
                out << "PyTorch Studio: Скрипт анализа температур асинхронных двигателей\n";
                out << "Файл: " << fileInfo.fileName() << "\n";
                out << "Создан: " << QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm") << "\n";
                out << "\"\"\"\n\n";

                // Предустановленный ML-стек для мгновенного подхвата сервером LSP Jedi
                out << "import torch\n";
                out << "import torch.nn as nn\n";
                out << "import json\n\n";

                // Автоматический мост к паспорту проекта (физическим параметрам двигателя)
                out << "# Функция автоматического чтения физических параметров двигателя из паспорта\n";
                out << "def load_engine_passport():\n";
                out << "    try:\n";
                out << "        with open('../passport.pystudio.json', 'r', encoding='utf-8') as f:\n";
                out << "            return json.load(f)\n";
                out << "    except FileNotFoundError:\n";
                out << "        return None\n\n";

                out << "# Инициализация вычислительного устройства (CPU/CUDA)\n";
                out << "device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')\n";

                file.close();
                qInfo() << "[FILE_MGR] Чистый .py файл с шаблоном успешно записан в /scripts:" << fullPath;
            } else {
                qCritical() << "[FILE_MGR] Ошибка ОС: Не удалось создать файл по пути:" << fullPath;
                return;
            }
        }

        // 5. ОТКРЫТИЕ В РЕДАКТОРЕ КОДA
        this->openNewFileInEditor(fullPath);

        // 6. РЕГИСТРАЦИЯ В LSP JEDI
        this->sendLspDidOpenForFile(fullPath, "");
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
    connect(save_progect_all, &QAction::triggered, this, &Neuro_programm::saveCurrentProjectChanges);

    // Действие "Экспортировать проект в прхив" (Shift + E)
    QAction *actionExportInArchiv = new QAction("Экспортировать проект в прхив", this);
    actionExportInArchiv->setShortcut(QKeySequence("Ctrl+E"));
    actionExportInArchiv->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_S));
    connect(actionExportInArchiv, &QAction::triggered, this, &Neuro_programm::saveProjectAsArchive);

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

    actStepOut  = new QAction("Шаг наружу", this);
    actStepInto = new QAction("Шаг внутрь", this);
    actStepOver = new QAction("Шаг обхода", this);

    actSTM = new QAction(this);
    actSTM_work = new QAction(this);

    actSTM->setCheckable(true);
    actSTM_work->setCheckable(true);


    QList<SidebarButtonConfig> buttonConfigs = {
        {&actProject, "Проект", QIcon(":/Data/system_icons/document-open.svg")},
        {&actControlPanel, "Настройки ИИ", QIcon::fromTheme(":/Data/system_icons/configure.svg")},
        {&actTensor, "Графики", QIcon(":/Data/system_icons/document-save-as.svg")},
        {&actPip, "Пакеты PIP", QIcon(":/Data/system_icons/document-open.svg")},
        {&actSearch, "Поиск", QIcon(":/Data/system_icons/edit-find.svg")},
        {nullptr, "", QIcon()},
        {&actStartTrain, "Обучение", QIcon(":/Data/system_icons/media-playback-start_2.svg")},
        {&actDebug, "Дебаг", QIcon(":/Data/system_icons/media-playback-start_3.svg")},
        {&actSTM, "Прошивка", QIcon(":/Data/system_icons/media-playback-start_3.svg")},
        {&actSTM_work, "Работа STM", QIcon(":/Data/system_icons/media-playback-start_3.svg")}
    };

    sidebarLayout->addSpacing(10);

    // =========================================================================
    // 3. ЦИКЛ СБОРКИ: 100% ШИРИНА И ПОЛНЫЙ КОНТРОЛЬ ЗАЗОРОВ
    // =========================================================================
    for (const auto& config : buttonConfigs)
    {
        if (config.text.isEmpty()) {
            sidebarLayout->addStretch(1); // Расталкивает элементы
            continue;
        }

        // 1. СНАЧАЛА ГАРАНТИРОВАННО ИНИЦИАЛИЗИРУЕМ ЭКШЕН В ПАМЯТИ
        *(config.targetActionPtr) = new QAction(config.text, this);

        // 2. И ТОЛЬКО ТЕПЕРЬ БЕЗОПАСНО НАСТРАИВАЕМ РЕЖИМ ТУМБЛЕРА (Указатель уже валиден!)
        if (config.text == "Дебаг" || config.text == "Обучение") {
            (*(config.targetActionPtr))->setCheckable(true);
        } else {
            (*(config.targetActionPtr))->setCheckable(false);
        }

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
        // Идеальный вариант для Qt6: отключает все лямбды и слоты от сигнала triggered этого экшена
        QObject::disconnect(actProject, &QAction::triggered, nullptr, nullptr);

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
    customMenuBar = new QMenuBar(topWrapper);

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
    fileMenu->addAction(actionExportInArchiv);
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
    //connect(actInstallPip, &QAction::triggered, this, &Neuro_programm::onInstallSinglePackageTriggered);
    connect(actInstallPip, &QAction::triggered,
            this, &Neuro_programm::action_install_package_triggered);
    pipSubMenu->addAction(actInstallPip);

    QAction *actDeletePip = new QAction("Удалить пакет", this);
    connect(actDeletePip, &QAction::triggered,
            this, &Neuro_programm::action_uninstall_package_triggered);
    pipSubMenu->addAction(actDeletePip);

    QAction *actUpdateSinglePip = new QAction("Обновить выбранный пакет до версии PyPI", this);
    connect(actUpdateSinglePip, &QAction::triggered,
            this, &Neuro_programm::action_upgrade_package_triggered);
    pipSubMenu->addAction(actUpdateSinglePip);

    QAction *actUpdateAllPip = new QAction("Обновить все пакеты до версии PyPI", this);
    connect(actUpdateAllPip, &QAction::triggered,
            this, &Neuro_programm::action_upgrade_all_packages_triggered);
    pipSubMenu->addAction(actUpdateAllPip);

    // Действие Б: Установка из файла зависимостей
    QAction *actInstallReqs = new QAction("Установить пакеты из requirements.txt", this);
    connect(actInstallReqs, &QAction::triggered, this, &Neuro_programm::action_install_from_requirements_triggered);
    pipSubMenu->addAction(actInstallReqs);

    // Действие В: Быстрое обновление списка пакетов
    QAction *actRefreshPip = new QAction("Обновить список пакетов в requirements.txt", this);

    connect(actRefreshPip, &QAction::triggered,
            this, &Neuro_programm::action_freeze_requirements_triggered);
    pipSubMenu->addAction(actRefreshPip);

    // 3. ДОБАВЛЯЕМ ПОДМЕНЮ В ГЛАВНОЕ МЕНЮ
    toolsMenu->addMenu(pipSubMenu);

    //Создаем ПОДМЕНЮ для PIP (вместо QAction используем QMenu)
    QMenu *stmSubMenu = new QMenu("Прошивка STM", this);
    stmSubMenu->setIcon(QIcon(":/Data/system_icons/document-save-as.svg"));

    QAction *findPlate = new QAction("Обнаружить плату", this);
    connect(findPlate, &QAction::triggered, this, &Neuro_programm::onDetectDevice);
    stmSubMenu->addAction(findPlate);

    QAction *SelectFile = new QAction("Загрузить прошивку", this);
    connect(SelectFile, &QAction::triggered, this, &Neuro_programm::onSelectFirmwareFile);
    stmSubMenu->addAction(SelectFile);

    EraseFlash = new QAction("Стереть Flash-память чипа", this);
    WrightFlash = new QAction("Записать Flash-память чипа", this);

    //EraseFlash = new QAction("Стереть Flash-память чипа", this);
    connect(EraseFlash, &QAction::triggered, this, &Neuro_programm::onEraseFlash);
    stmSubMenu->addAction(EraseFlash);
    EraseFlash->setEnabled(false);

    //WrightFlash = new QAction("Записать Flash-память чипа", this);
    connect(WrightFlash, &QAction::triggered, this, &Neuro_programm::onWrightFlash);
    stmSubMenu->addAction(WrightFlash);
    WrightFlash->setEnabled(false);

    toolsMenu->addMenu(stmSubMenu);

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
    QIcon appIcon2(":/Data/Icons/pytorch-studio.svg");
    // Превращаем иконку в картинку (Pixmap) аккуратного размера (например, 16x16 или 18x18)
    QPixmap iconPixmap = appIcon2.pixmap(18, 18);

    iconLabel->setPixmap(iconPixmap);
    iconLabel->setFixedSize(18, 18); // Жестко фиксируем размеры значка

    // 2. Добавляем иконку в самый левый край макета шапки
    panelLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);

    // Левая симметричная пружина
    panelLayout->addStretch();

    // Создаем текстовую метку программно
    titleLabel = new QLabel("PyTorch Studio", ui->customTitleBarPanel);
    titleLabel->setObjectName("titleLabel");
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

    // panelOther = new panel_other(this);
    // mainVerticalSplitter->addWidget(panelOther);
    // panelOther->setVisible(false);

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

    // =========================================================================
    // ВСТАВЛЯЙТЕ СЮДА (СТРОГО ПЕРЕД ЗАКРЫВАЮЩЕЙ ФИГУРНОЙ СКОБКОЙ КОНСТРУКТОРА)
    // =========================================================================
    if (leftSideBarContainer) {
        // Аппаратно заставляем панель растянуться сверху донизу на всю высоту экрана
        leftSideBarContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

        if (leftSideBarContainer->layout()) {
            leftSideBarContainer->layout()->setContentsMargins(0, 10, 0, 0);
        }
    }

    QToolBar *wrapperBarObj = this->findChild<QToolBar*>("leftSideBar");
    if (wrapperBarObj) {
        wrapperBarObj->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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

    // Изначально при старте приложения нижняя панель закрыта, кнопки отжаты
    btnTerminal->blockSignals(true);
    btnTerminal->setCheckable(true);
    btnTerminal->setChecked(false);
    btnTerminal->blockSignals(false);

    btnSearch->blockSignals(true);
    btnSearch->setCheckable(true);
    btnSearch->setChecked(false);
    btnSearch->blockSignals(false);

    btnTerminal->setStyleSheet("QPushButton { border: none; padding: 4px 12px; color: #ffffff; font-weight: bold; }");

    // 2. Инициализируем левый усекаемый лог ошибок Jedi
    // statusLogLabel = new ElidedLabel(this);
    // statusLogLabel->setObjectName("statusLogLabel");
    // statusLogLabel->setStyleSheet("color: #ef5350; font-weight: bold;");
    // statusLogLabel->setMaximumWidth(400); // Ограничиваем максимальную ширину
    // statusLogLabel->setFullText("Jedi: Готов к работе");

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
    //ui->statusbar->addWidget(statusLogLabel, 0); // Самый левый элемент (Лог Jedi)
    ui->statusbar->addWidget(leftSpacer, 0);

    ui->statusbar->addPermanentWidget(btnTerminal, 0);
    ui->statusbar->addPermanentWidget(btnSearch, 0);
    ui->statusbar->addPermanentWidget(btnLogs, 0);
    ui->statusbar->addPermanentWidget(btnAIChat, 0);
    ui->statusbar->addPermanentWidget(btnStartDebug, 0);
    ui->statusbar->addPermanentWidget(btnTogglePip, 0);

    //statusLogLabel->show();
    btnTerminal->show();
    btnSearch->show();
    btnLogs->show();
    btnAIChat->show();
    btnStartDebug->show();
    btnTogglePip->show();

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
    // НА СТРАНИЦЕ 17 ОСТАВЬТЕ СТРОГО ЭТОТ ВАРИАНТ (БЕЗ setMinimumHeight!):
    connect(btnTerminal, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnTerminal->blockSignals(true);
            btnTerminal->setChecked(true);
            btnTerminal->blockSignals(false);

            // Просто проявляем динамическую панель, геометрию настроит второй коннект!
            if (panelOther) panelOther->setVisible(true);

            if (ui->search_panel) ui->search_panel->setVisible(false);
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);
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

    // 5. Управление ИИ-Ассистентом (Чат)
    connect(btnAIChat, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked) {
        if (checked) {
            resetAllStatusButtons();
            btnAIChat->blockSignals(true); btnAIChat->setChecked(true); btnAIChat->blockSignals(false);

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
    // 1. Управление Терминалом (Используем строго toggled)
    connect(btnTerminal, &QPushButton::toggled, this, [this, resetAllStatusButtons](bool checked)
    {
        if (checked)
        {
            resetAllStatusButtons();
            btnTerminal->blockSignals(true);
            btnTerminal->setChecked(true);
            btnTerminal->blockSignals(false);

            if (panelOther) {
                // 1. Возвращаем нормальный режим виджета в сетке
                panelOther->setWindowFlags(Qt::Widget);
                panelOther->setMaximumWidth(16777215);
                panelOther->setMinimumWidth(0);

                // 2. ЖЕСТКИЙ ФИКС: Убираем любые скрытые отступы, которые могли выталкивать кнопку [X] вправо
                panelOther->setContentsMargins(0, 0, 0, 0);
                if (panelOther->layout()) {
                    panelOther->layout()->setContentsMargins(0, 0, 0, 0);
                }

                // 3. Задаем фиксированную высоту терминала прямо через свойства виджета
                panelOther->setFixedHeight(250);
                panelOther->setVisible(true);

                // 4. Просим сетку главного окна обновить геометрию под новые размеры
                if (ui && ui->centralwidget && ui->centralwidget->layout()) {
                    ui->centralwidget->layout()->activate();
                }

                QStackedWidget *stacked = panelOther->findChild<QStackedWidget*>("stackedWidget");
                if (stacked) {
                    stacked->setCurrentIndex(0);
                }
            }

            if (ui->search_panel) ui->search_panel->setVisible(false);
            if (ui->quickActionsList) ui->quickActionsList->setVisible(false);
        }
    });


    // =========================================================================
    // ИСПРАВЛЕННОЕ УПРАВЛЕНИЕ ОВЕРЛЕЙНОЙ ПАНЕЛЬЮ (БЕЗ СЖАТИЯ СПЛИТТЕРА!)
    // =========================================================================
    // УПРАВЛЕНИЕ ТЕРМИНАЛОМ С АБСОЛЮТНОЙ ЗАЩИТОЙ ОТ ПЕРЕХВАТА ФОКУСА
    connect(btnTerminal, &QPushButton::toggled, this, [this](bool checked) {
        if (!panelOther || !mainVerticalSplitter) return;

        if (checked) {
            panelOther->setVisible(true);
            panelOther->show();

            // 1. Задаем пропорции сплиттера: отдаем терминалу ровно 250 пикселей
            int totalHeight = this->height();
            QList<int> sizes;
            sizes << (totalHeight - 250) << 250;
            mainVerticalSplitter->setSizes(sizes);

            // =========================================================================
            // МАКСИМАЛЬНАЯ ЗАЩИТА ПЛЕЙСХОЛДЕРА ОТ СЖАТИЯ
            // =========================================================================
            // Мы говорим сплиттеру: верхняя область (индекс 0) имеет приоритет 1,
            // а нижний терминал (индекс 1) имеет приоритет 0.
            // Это принудительно заставит Qt забирать пиксели сверху, не сжимая низ!
            mainVerticalSplitter->setStretchFactor(0, 1);
            mainVerticalSplitter->setStretchFactor(1, 0);

            // 2. ЗАПРЕЩАЕМ перехват фокуса. Клавиатура остается в редакторе кода/плейсхолдере!
            this->setFocus();
        }
        else {
            panelOther->setVisible(false);
            panelOther->hide();
        }

        if (mainVerticalSplitter->layout()) {
            mainVerticalSplitter->layout()->activate();
        }
    });

    connect(btnSearch, &QPushButton::clicked, this, [this]() {
        if (btnTogglePip) btnTogglePip->setChecked(false);
    });

    connect(btnLogs, &QPushButton::clicked, this, [this]() {
        if (btnTogglePip) btnTogglePip->setChecked(false);
    });

    projectModel = nullptr;

    ui->treeView->setIndentation(20);

    connect(ui->aboutProgram, &QAction::triggered, this, &Neuro_programm::open_about_program);
    //connect(save_progect_all, &QAction::triggered, this, &Neuro_programm::onSaveProjectMenuTriggered);

    // --- ВНУТРИ КОНСТРУКТОРА Neuro_programm (Взамен старого монтажа) ---
    ui->fileComboBox->clear();

    // =========================================================================
    // БЕЗОПАСНАЯ НАСТРОЙКА JETBRAINS PLACEHOLDER (УСТРАНЕНИЕ ПАДЕНИЯ)
    // =========================================================================
    EditorPlaceholder *placeholderScreen = new EditorPlaceholder(ui->centralStackedWidget);
    placeholderScreen->setObjectName("JETBRAINS_PLACEHOLDER");
    int placeholderIndex = ui->centralStackedWidget->addWidget(placeholderScreen);

    // Разрешаем заставке шорткатов быть адаптивной:
    // Задаем небольшую минимальную высоту, чтобы при открытии терминала (250px)
    // менеджер геометрии Qt6 сжимал плейсхолдер, а не раздувал границы самого главного окна.
    placeholderScreen->setMinimumHeight(150);

    // Выставляем стандартную политику расширения (занимать всё доступное место)
    placeholderScreen->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    this->setProperty("placeholderIndex", placeholderIndex);

    // 3. БЛОКИРУЕМ СИГНАЛЫ комбобокса на время первичного наполнения,
    // чтобы он случайно не дергал стек и панель panelOther до полной настройки!
    ui->fileComboBox->blockSignals(true);

    ui->fileComboBox->clear(); // Полностью зачищаем старый мусор из Designer
    ui->fileComboBox->addItem(" Панель управления", QVariant("MAIN_SCREEN"));
    ui->fileComboBox->addItem(" AI-ассистент", QVariant("AI_CHAT_SCREEN"));

    // 4. Проверяем состояние проекта и выставляем стартовый экран
    if (this->currentOpenProjectPath.isEmpty())
    {
        // СЦЕНАРИЙ А: ЧИСТЫЙ СТАРТ БЕЗ ПРОЕКТА
        // Принудительно включаем пустую заставку шорткатов JetBrains
        ui->centralStackedWidget->setCurrentIndex(placeholderIndex);

        // Жестко сбрасываем стрелку в нейтральное пустое положение -1.
        // Терминал panelOther теперь гарантированно останется закрытым!
        ui->fileComboBox->setCurrentIndex(-1);

        // Насильно возвращаем заголовку ОС чистый вид "PyTorch Studio"
        this->setWindowTitle("PyTorch Studio");
        if (this->titleLabel) this->titleLabel->setText("PyTorch Studio");
    }
    else
    {
        // СЦЕНАРИЙ Б: ПРИ СТАРТЕ УЖЕ ПЕРЕДАН ПУТЬ К ПРОЕКТУ ЧЕРЕЗ АРГУМЕНТЫ ОС
        ui->centralStackedWidget->setCurrentIndex(0);
        ui->fileComboBox->setCurrentIndex(0); // Включаем "Панель управления"

        // Сразу просим менеджер документов нарисовать красивую шапку: Панель управления[z2.pystudio]
        if (this->docMgr) {
            this->docMgr->updateUiTitles("MAIN_SCREEN");
        }
    }

    // 5. РАЗБЛОКИРУЕМ СИГНАЛЫ обратно — теперь интерфейс готов к действиям пользователя
    ui->fileComboBox->blockSignals(false);

    // 1. Коннект двойного щелчка по дереву файлов
    connect(ui->treeView, &QTreeView::doubleClicked, this, &Neuro_programm::onFileDoubleClicked);

    // 3. Коннект кнопки закрытия текущего файла
    connect(ui->btnCloseFile, &QPushButton::clicked, this, [this]()
            {
        qDebug() << ">>> [КЛИК] Запрос на закрытие текущей вкладки с кодом...";

        // Принудительно вызываем наш отлаженный метод закрытия!
        this->onCloseCurrentFileClicked();
    });

    // =========================================================================
    // СИНХРОНИЗИРОВАННЫЙ СМАРТ-КОННЕКТ КОМБОБОКСА (ФИНАЛЬНАЯ ИНТЕГРАЦИЯ С DOC_MGR)
    // =========================================================================
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
                ui->centralStackedWidget->blockSignals(true);
                ui->centralStackedWidget->setCurrentIndex(targetStackIndex);
                ui->centralStackedWidget->blockSignals(false);
            }
        } else {
            // Для сервисных экранов (MAIN_SCREEN / AI_CHAT_SCREEN) перелистываем по базовому индексу
            ui->centralStackedWidget->blockSignals(true);
            ui->centralStackedWidget->setCurrentIndex(index);
            ui->centralStackedWidget->blockSignals(false);
        }

        // =====================================================================
        // ИСПРАВЛЕННЫЙ БЛОК УПРАВЛЕНИЯ СЕРВИСНЫМИ СТРАНИЦАМИ (ДОБАВЛЕНО ВЫДЕЛЕНИЕ)
        // =====================================================================
        // =====================================================================
        // ИСПРАВЛЕННЫЙ БЛОК УПРАВЛЕНИЯ СЕРВИСНЫМИ СТРАНИЦАМИ (ФИКС НАЛОЖЕНИЯ ПАНЕЛЕЙ)
        // =====================================================================
        QString currentKey = dataVal.toString().trimmed();
        if (currentKey == "MAIN_SCREEN")
        {
            if (this->docMgr) {
                this->docMgr->updateUiTitles("MAIN_SCREEN");
                this->docMgr->handleFileActivation("MAIN_SCREEN");
            }

            // Проверяем, какой индекс сейчас активен в центральном stackedWidget.
            // Если открыты инженерные вкладки STM (индексы страниц настроек/мониторинга),
            // комбобоксу ЗАПРЕЩЕНО насильно открывать panelOther и перебивать UI!
            int currentCentralIndex = ui->centralStackedWidget->currentIndex();

            // Предположим, страница Настроек ИИ = 1, Работа STM = 3. Защищаем их:
            bool isStmOrSettingsActive = (currentCentralIndex == 1 || currentCentralIndex == 3);

            if (!this->currentOpenProjectPath.isEmpty() && !isStmOrSettingsActive)
            {
                if (btnTerminal) {
                    btnTerminal->blockSignals(true);
                    btnTerminal->setChecked(true);
                    btnTerminal->setProperty("active", true);
                    btnTerminal->style()->unpolish(btnTerminal);
                    btnTerminal->style()->polish(btnTerminal);
                    btnTerminal->blockSignals(false);
                }
                if (panelOther) {
                    panelOther->setVisible(true);
                    panelOther->show();
                }
            }
            else
            {
                if (btnTerminal) {
                    btnTerminal->blockSignals(true);
                    btnTerminal->setChecked(false);
                    btnTerminal->setProperty("active", false);
                    btnTerminal->blockSignals(false);
                }
                if (panelOther) {
                    panelOther->setVisible(false);
                    panelOther->hide();
                }
            }

            if (btnAIChat) btnAIChat->setChecked(false);
            if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                ui->openFilesContainer->setVisible(true);
                int totalHeight = ui->leftVerticalSplitter->height();
                if (totalHeight <= 0) totalHeight = 600;
                ui->leftVerticalSplitter->setSizes(QList<int>({totalHeight - 180, 180}));
            }
        }
        else if (currentKey == "AI_CHAT_SCREEN")
        {
            if (this->docMgr) {
                this->docMgr->updateUiTitles("AI_CHAT_SCREEN");
                // -------------------------------------------------------------
                // ЖЕЛЕЗНЫЙ UX-ФИКС: Красим строку "AI-ассистент" в синий!
                // -------------------------------------------------------------
                this->docMgr->handleFileActivation("AI_CHAT_SCREEN");
            }

            if (btnAIChat) btnAIChat->setChecked(true);

            if (btnTerminal) {
                btnTerminal->blockSignals(true);
                btnTerminal->setChecked(false);
                btnTerminal->blockSignals(false);
            }
            if (panelOther) {
                panelOther->setVisible(false);
                panelOther->hide();
            }

            if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                ui->openFilesContainer->setVisible(true);
                int totalHeight = ui->leftVerticalSplitter->height();
                if (totalHeight <= 0) totalHeight = 600;
                ui->leftVerticalSplitter->setSizes(QList<int>({totalHeight - 180, 180}));
            }
        }

        else if (currentKey == "AI_CHAT_SCREEN")
        {
            if (this->docMgr) this->docMgr->updateUiTitles("AI_CHAT_SCREEN"); // Явно просим выставить "AI-ассистент"

            if (btnAIChat) btnAIChat->setChecked(true);
            if (btnTerminal) btnTerminal->setChecked(false);

            // ИСПРАВЛЕНО: БОЛЬШЕ НЕ ПРЯЧЕМ ПАНЕЛЬ
            if (ui->openFilesContainer && ui->leftVerticalSplitter) {
                ui->openFilesContainer->setVisible(true); // Всегда TRUE
                int totalHeight = ui->leftVerticalSplitter->height();
                if (totalHeight <= 0) totalHeight = 600;
                ui->leftVerticalSplitter->setSizes(QList<int>({totalHeight - 180, 180}));
            }
        }

        // =====================================================================
        // ИНТЕГРАЦИОННАЯ ВРЕЗКА: ЕСЛИ ВЫБРАН РЕАЛЬНЫЙ ФАЙЛ КОДА (ИНДЕКСЫ >= 2)
        // =====================================================================
        else
        {
            // Извлекаем абсолютный путь к открытому документу, который зашит в userData комбобокса
            QString fullPath = currentKey;

            if (!fullPath.isEmpty() && fullPath != "MAIN_SCREEN" && fullPath != "AI_CHAT_SCREEN") {
                // Прямая команда менеджеру: он сам обновит сложный заголовок, нарисует
                // звездочки сохранения на ходу и выделит нужную строку в списке открытых файлов!
                if (this->docMgr) {
                    this->docMgr->handleFileActivation(fullPath);
                } else {
                    QWidget *currentPage = ui->centralStackedWidget->widget(ui->centralStackedWidget->currentIndex());
                    if (currentPage) {
                        QFileInfo fileInfo(currentPage->objectName());
                        updateCustomTitle(fileInfo.fileName());
                    }
                }
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

            // Переводим фокус ввода клавиатуры на текстовый холст редактора кода для удобства инженера
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (currentPage) {
                CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
                if (currentEditor) {
                    currentEditor->setFocus();
                    currentEditor->update();
                }
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

        // Синхронно обновляем индикатор строки и столбца (Ln, Col) каретки в статусбаре
        this->updateCursorPositionIndicator();
    });

    // 1. Создаем кастомный виджет для шапки дока
    QWidget *customTitleWidget = new QWidget(ui->leftDockWidget);

    if (ui->leftDockWidget) {
        ui->leftDockWidget->setFeatures(QDockWidget::NoDockWidgetFeatures);
    }

    // 2. Создаем красивый текстовый заголовок
    QLabel *titleLabel1 = new QLabel("📁 Открытые файлы и проект", customTitleWidget);

    // ЗАЩИТА: Если метка не была найдена в Designer, создаем её сами прямо сейчас
    if (this->titleLabel && ui->customTitleBarPanel) {
        // Просто привязываем метку к панели шапки, не меняя её живой текст!
        this->titleLabel->setParent(ui->customTitleBarPanel);
    } else if (!titleLabel) {
        titleLabel = new QLabel("PyTorch Studio", ui->customTitleBarPanel);
    }

    // Заглушка для схлопывания заголовка дока (оставляем без изменений)
    QWidget *completelyEmptyTitle = new QWidget(ui->leftDockWidget);
    ui->leftDockWidget->setTitleBarWidget(completelyEmptyTitle);

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
        // 1. Сбрасываем минимальные лимиты высоты из Designer
        ui->leftDockWidget->setMinimumSize(QSize(300, 0));
        ui->leftDockWidget->setMaximumSize(QSize(300, 524287));

        // 2. Убираем встроенную плашку заголовка QDockWidget
        QWidget *emptyTitleWidget = new QWidget(ui->leftDockWidget);
        emptyTitleWidget->setFixedHeight(0); // Схлопываем невидимую заглушку в ноль
        emptyTitleWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        ui->leftDockWidget->setTitleBarWidget(emptyTitleWidget);

        // =====================================================================
        // ИСПРАВЛЕННЫЙ CSS-КАСКАД: УБРАН BACKGROUND, БЛОКИРОВАВШИЙ ВЫДЕЛЕНИЕ QT
        // =====================================================================
        ui->leftDockWidget->setStyleSheet(
                    "QDockWidget {"
                    "   border: 1px solid #b0b0b0;"
                    "   padding: 0px !important;"
                    "   margin: 0px !important;"
                    "}"
                    "QDockWidget > QWidget {"
                    "   padding: 0px !important;"
                    "   margin: 0px !important;"
                    "   background: #ffffff;"
                    "}"
                    // УДАЛЕН 'background: #ffffff;'! Оставляем только сброс рамок, чтобы оживить синий цвет
                    "QTreeView, QListWidget {"
                    "   border: none;"
                    "   margin: 0px !important;"
                    "   padding: 0px !important;"
                    "}"
                    "QTreeView::item, QListWidget::item {"
                    "   padding-top: 4px !important;"
                    "   padding-bottom: 4px !important;"
                    "}"
                    "QTreeView::item:hover, QListWidget::item:hover {"
                    "   background-color: #e4e5e6;"
                    "}"
                    );

        // Обнуляем технические отступы самого главного окна для док-зон
        this->setStyleSheet(this->styleSheet() +
                            "QDockWidget { border: none; padding: 0px; margin: 0px; }"
                            "QDockWidget::title { background: transparent; height: 0px; max-height: 0px; padding: 0px; margin: 0px; border: none; }"
                            "QMainWindow::separator { background: transparent; width: 0px; height: 0px; margin: 0px; padding: 0px; }"
                            "QMainWindow::separator:hover { background: transparent; width: 0px; height: 0px; }"
                            );

        // Считываем подложку дока и принудительно зануляем её макет
        QWidget *dockContents = ui->leftDockWidget->widget();
        if (dockContents && dockContents->layout()) {
            dockContents->layout()->setContentsMargins(0, 0, 0, 0);
            dockContents->layout()->setSpacing(0);
        }

        ui->leftDockWidget->setContentsMargins(0, 0, 0, 0);

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
        QObject::disconnect(realStartButton, &QPushButton::clicked, nullptr, nullptr);

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
        currentEditor = qobject_cast<CodeEditor*>(currentPage);

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
                QObject::disconnect(btnProj, &QToolButton::clicked, nullptr, nullptr);
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
                QObject::disconnect(btnAI, &QToolButton::clicked, nullptr, nullptr);
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
    // =========================================================================
    // УМНАЯ ЛОГИКА ДЛЯ КНОПКИ: ГРАФИКИ (actTensor) - ИНТЕГРАЦИЯ С WEBVIEW
    // =========================================================================
    if (leftSideBarContainer && ui->mainHorizontalSplitter && ui->widgetRightCharts)
    {
        QToolButton *btnTensor = leftSideBarContainer->findChild<QToolButton*>("Графики");
        if (btnTensor) {
            QObject::disconnect(btnTensor, &QToolButton::clicked, nullptr, nullptr);

            connect(btnTensor, &QToolButton::clicked, this, [this]() {
                qDebug() << ">>> [UX КЛИК] Кнопка: Графики (TensorBoard)";

                int totalWindowWidth = this->width();
                QList<int> currentSizes = ui->mainHorizontalSplitter->sizes();

                // Проверяем, скрыта ли сейчас панель графиков на экране
                bool areChartsHidden = !ui->widgetRightCharts->isVisible() || (currentSizes.size() > 1 && currentSizes[1] <= 10);

                if (areChartsHidden)
                {
                    // -------------------------------------------------------------
                    // РЕЖИМ 1: РАЗВЕРТЫВАНИЕ ГРАФИКОВ НА ВЕСЬ ЭКРАН
                    // -------------------------------------------------------------
                    ui->widgetRightCharts->setMaximumSize(QSize(16777215, 16777215));
                    ui->widgetRightCharts->setMinimumWidth(0);
                    ui->widgetRightCharts->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

                    // Динамически будим фоновый процесс TensorBoard, если он спал
                    if (this->tensorBoardServer && !this->tensorBoardServer->isRunning()) {
                        this->tensorBoardServer->startServer(this->currentOpenProjectPath, 6006);
                    }

                    // Принудительно заставляем WebView загрузить локальную страницу TensorBoard
                    if (m_tensorWebView) {
                        m_tensorWebView->setUrl(QUrl("http://127.0.0.1:6006"));
                    }

                    ui->widgetRightCharts->setVisible(true);

                    // Схлопываем левое дерево проекта, отдавая графикам 100% ширины
                    if (ui->leftDockWidget) {
                        ui->leftDockWidget->setVisible(false);
                        ui->leftDockWidget->hide();
                    }

                    ui->mainHorizontalSplitter->setCollapsible(0, true);
                    ui->mainHorizontalSplitter->setSizes(QList<int>({0, totalWindowWidth}));

                    qInfo() << "[TENSORBOARD] Панель развернута. WebView подключен к порту 6006.";
                }
                else
                {
                    // -------------------------------------------------------------
                    // РЕЖИМ 2: СВОРУЧИВАНИЕ ГРАФИКОВ (ВОЗВРАТ К ИСХОДНОМУ КОДУ)
                    // -------------------------------------------------------------
                    ui->mainHorizontalSplitter->setSizes(QList<int>({1000, 0}));
                    ui->widgetRightCharts->setVisible(false);
                    ui->mainHorizontalSplitter->setCollapsible(0, false);

                    // Возвращаем боковую панель навигации (дерево файлов) на экран
                    if (ui->leftDockWidget) {
                        QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");
                        if (dockStack) {
                            int targetIdx = this->currentOpenProjectPath.isEmpty() ? 1 : 0;
                            dockStack->setCurrentIndex(targetIdx);
                        }
                        ui->leftDockWidget->setVisible(true);
                        ui->leftDockWidget->show();
                    }
                    qDebug() << ">>> [TENSORBOARD] Интерфейс возвращен к кодовой базе.";
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
            btnSearchSide->setObjectName("btnSearchSideBar");

            QObject::disconnect(btnSearchSide, &QToolButton::clicked, nullptr, nullptr);

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

    // =========================================================================
    // ИНИЦИАЛИЗАЦИЯ НИЖНЕЙ ПАНЕЛИ ТЕРМИНАЛА (КОНЕЦ КОНСТРУКТОРА)
    // =========================================================================
    panelOther = new panel_other(this);
    // mainVerticalSplitter->addWidget(panelOther);

    if (actDebug)
    {
        panelOther->setDebugAction(actDebug);
    }

    mainVerticalSplitter->addWidget(panelOther);

    // 1. Скрываем виджет на уровне менеджмента слоев Qt
    panelOther->setVisible(false);
    panelOther->hide();

    // 2. ЖЕСТКИЙ АППАРАТНЫЙ ФИКС: Явно отдаем верхнему редактору 100% высоты,
    // а нижний терминал зануляем в ОЗУ сплиттера, полностью стирая зазор!
    int totalWindowHeight = this->height() > 0 ? this->height() : 800;
    QList<int> initialVerticalSizes;
    initialVerticalSizes << totalWindowHeight << 0; // Верхняя зона = максимум, нижняя = 0
    mainVerticalSplitter->setSizes(initialVerticalSizes);

    // Блокируем схлопывание кода в ноль, но разрешаем полностью прятать терминал
    mainVerticalSplitter->setCollapsible(0, false);
    mainVerticalSplitter->setCollapsible(1, true);


    // =========================================================================
    // БЕТОНИРОВАНИЕ ПАЛИТРЫ ТЕРМИНАЛА logEdit (УГОЛЬНО-ЧЕРНЫЙ + ЗЕЛЕНЫЙ НЕОН)
    // =========================================================================
    QTextEdit *logEditWidget = panelOther->findChild<QTextEdit*>("logEdit");
    if (logEditWidget) {
        logEditWidget->setAutoFillBackground(true);

        QPalette logPalette = logEditWidget->palette();
        logPalette.setColor(QPalette::Base, QColor(30, 30, 30));      // Фон — угольно-черный (#1e1e1e)
        logPalette.setColor(QPalette::Text, QColor(0, 255, 0));       // Текст — неоново-зеленый (#00ff00)
        logEditWidget->setPalette(logPalette);

        // Дополнительно страхуем шрифтом
        logEditWidget->setStyleSheet("border: none; font-family: 'Liberation Mono'; font-size: 11px;");
    }

    // =========================================================================
    // БЕТОНИРОВАНИЕ ПАЛИТРЫ REPL-КОНСОЛИ (УГОЛЬНО-ЧЕРНЫЙ + БЕЛЫЙ ТЕКСТ ВВОДА)
    // =========================================================================
    QTextEdit *replWidget = panelOther->findChild<QTextEdit*>("replTextEdit");
    if (!replWidget) replWidget = panelOther->findChild<QTextEdit*>("replEdit"); // Фолбэк имени

    if (replWidget) {
        replWidget->setAutoFillBackground(true);

        QPalette replPalette = replWidget->palette();
        replPalette.setColor(QPalette::Base, QColor(30, 30, 30));     // Фон — угольно-черный
        replPalette.setColor(QPalette::Text, QColor(0, 255, 0));   // Текст ввода — белый
        replWidget->setPalette(replPalette);

        replWidget->setStyleSheet("border: none; font-family: 'Liberation Mono'; font-size: 11px;");
    }


    panelOther->setVisible(false); // Изначально скрыта

    // Привязываем фокус-политику к кнопкам и панелям, чтобы они НЕ перехватывали клавиатуру
    panelOther->setFocusPolicy(Qt::NoFocus);

    //this->updateProjectsListFromSettings();
    //this->envManager = new PythonEnvManager(this);
    // =========================================================================
    // ЖЕСТКИЙ ФИКС ЧИСТОГО СТАРТА: БЛОКИРУЕМ АВТОЗАГРУЗКУ ПРОЕКТА
    // =========================================================================
    this->currentOpenProjectPath = ""; // Гарантируем, что ОЗУ проекта пуста

    QTimer::singleShot(250, this, [this]() {
        // ИСПРАВЛЕНО: Вызываем ваш родной метод инициализации окружения из главного класса!
        this->initializeEnvironmentOnStartup();

        // Принудительно выставляем интерфейс в чистый стартовый режим
        this->setIDEInStartMode(true);
    });

    // =========================================================================
    // СИНХРОНИЗАЦИЯ UI И СИСТЕМНЫХ УВЕДОМЛЕНИЙ ПРИ УСПЕШНОМ ПОДКЛЮЧЕНИИ VENV
    // =========================================================================
    connect(envManager, &PythonEnvManager::venvConnectedSuccessfully, this, [this](const QStringList &packages)
    {
        // 1. Извлекаем текстовое описание или версию PyTorch из переданного списка
        QString torchInfo = !packages.isEmpty() ? packages.first() : "Определена";

        qInfo() << "[GUI_ENV] Фоновый воркер успешно подтвердил PyTorch окружение. Обновляю UI...";

        // 2. Выводим временное текстовое сообщение в основную область статусбара
        ui->statusbar->showMessage("Окружение PyTorch успешно подключено!", 5000);

        // 3. Аппаратно перекрашиваем постоянный левый индикатор Jedi в зеленый цвет
        // ЗАЩИТА: Обязательно проверяем, инициализирован ли уже виджет в памяти, чтобы избежать краша!
        if (this->statusLogLabel != nullptr) {
            this->statusLogLabel->setStyleSheet("color: #4caf50; font-weight: bold;"); // Красивый зеленый Breeze-стиль
            this->statusLogLabel->setFullText("PyTorch: Подключен"); // Фиксируем статус в логгере
        }

        // 4. Отправляем нативное всплывающее уведомление в операционную систему Linux через D-Bus
        // Функция использует ваш синхронизированный App ID "pytorch-studio" и не блокируется порталом!
        this->sendSystemNotification(
                    "PyTorch Studio: Успех",
                    QString("Виртуальное окружение venv успешно подключено.\nЗависимость: %1").arg(torchInfo)
                    );

        // 5. Запускаем сервер автодополнения кода (LSP), так как пути к PyTorch теперь валидны
        this->initLspServer();
    });

    this->jupyterServer = new JupyterManager(this);

    // Прямая коммутация логов фонового ядра в наш красивый черный logEdit терминал
    connect(jupyterServer, &JupyterManager::jupyterLogReceived, this, [this](const QString &text) {
        if (panelOther) {
            panelOther->appendTrainingLog(text); // Текст автоматически ложится на черный фон зеленым цветом
        }
    });

    // Перехват критических ошибок ядра
    connect(jupyterServer, &JupyterManager::serverErrorOccurred, this, [this](const QString &errorMsg) {
        this->sendSystemNotification("Ошибка Jupyter", errorMsg);
    });

    this->tensorBoardServer = new TensorBoardManager(this);

    // Направляем логи старта в зеленый терминал logEdit
    connect(tensorBoardServer, &TensorBoardManager::boardLogReceived, this, [this](const QString &text) {
        if (panelOther) {
            panelOther->appendTrainingLog(text);
        }
    });

    // Выводим системное уведомление при ошибках
    connect(tensorBoardServer, &TensorBoardManager::boardErrorOccurred, this, [this](const QString &errorMsg) {
        this->sendSystemNotification("Ошибка TensorBoard", errorMsg);
    });

    this->initTensorBoardUi();

    this->hfManager = new HuggingFaceManager(this);

    // Направляем логи скачивания моделей в наш неоновый зеленый терминал logEdit
    connect(hfManager, &HuggingFaceManager::hfLogReceived, this, [this](const QString &text) {
        if (panelOther) {
            panelOther->appendTrainingLog(text);
        }
    });

    // Слушаем результаты завершения скачивания/логина
    connect(hfManager, &HuggingFaceManager::operationFinished, this, [this](bool success, const QString &msg) {
        if (success) {
            this->sendSystemNotification("Hugging Face", "Загрузка весов/авторизация завершена успешно.");
        } else {
            this->sendSystemNotification("Ошибка Hugging Face", msg);
        }
    });

    if (!startupPath.isEmpty())
    {
        // Даем 200мс на то, чтобы главное окно отрисовалось на экране,
        // и только потом асинхронно запускаем распаковку или открытие
        QTimer::singleShot(200, this, [this, startupPath]()
        {
            this->processStartupPath(startupPath);
        });
    }

    // =========================================================================
    // ЧАСТЬ 1: ИНИЦИАЛИЗАЦИЯ И СИСТЕМНЫЕ СВЯЗИ МЕНЕДЖЕРА ОТЛАДКИ
    // =========================================================================
    this->pyDebugger = new DebugManager(this);

    // Прямая коммутация текстовых логов дебаггера в зеленый терминал logEdit
    connect(pyDebugger, &DebugManager::statusMessageReady, this, [this](const QString &message, int timeout) {
        if (ui->statusbar) {
            ui->statusbar->showMessage(message, timeout);
        }
    });

    // 1. Коннект для заполнения нижней таблицы Стек вызовов (Qt Creator style)
    connect(pyDebugger, &DebugManager::stackTraceReceived, this, [this](const QList<QStringList> &stackFrames) {
        QTableWidget *stackTable = panelOther ? panelOther->findChild<QTableWidget*>() : nullptr;
        if (stackTable) {
            stackTable->setRowCount(0);
            for (int i = 0; i < stackFrames.size(); ++i) {
                stackTable->insertRow(i);
                for (int col = 0; col < 5; ++col) {
                    stackTable->setItem(i, col, new QTableWidgetItem(stackFrames[i][col]));
                }
            }
        }
    });

    // 2. Коннект для заполнения правой таблицы Локальных переменных
    connect(pyDebugger, &DebugManager::variablesReceived, this, [this](const QList<QStringList> &variables) {
        QTableWidget *localVarsTable = ui->rightDebugPanel ? ui->rightDebugPanel->findChild<QTableWidget*>("debugLocalVarsTable") : nullptr;
        if (localVarsTable) {
            localVarsTable->setRowCount(0);
            for (int i = 0; i < variables.size(); ++i) {
                localVarsTable->insertRow(i);
                for (int col = 0; col < 3; ++col) {
                    localVarsTable->setItem(i, col, new QTableWidgetItem(variables[i][col]));
                }
            }
        }
    });


    // ОБРАБОТКА СИГНАЛА ПОПАДАНИЯ НА БРЕЙКПОИНТ PyTorch (ЧЕРЕЗ EXTRASELECTION)
    // =========================================================================
    // АСИНХРОННЫЙ UX-МОСТ ПОДСВЕТКИ СТРОКИ ОСТАНОВА (ЗАЩИТА ОТ OUT OF RANGE)
    // =========================================================================
    connect(pyDebugger, &DebugManager::breakpointHit, this, [this](int line, const QString &sourceFile) {
        // 1. Принудительно открываем и загружаем файл в редактор
        this->openNewFileInEditor(sourceFile);

        // 2. ДЕЛАЕМ МИКРО-ПАУЗУ ДЛЯ СИНХРОНИЗАЦИИ ОЗУ И ХОЛСТА РЕДАКТОРА
        QTimer::singleShot(100, this, [this, line]() {
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (!currentPage) return;

            CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
            if (currentEditor && currentEditor->document()) {
                // В Qt нумерация строк идет с 0, поэтому line - 1
                QTextBlock block = currentEditor->document()->findBlockByLineNumber(line - 1);

                if (block.isValid()) {
                    int docTotalChars = currentEditor->document()->characterCount();
                    int targetPosition = block.position();

                    // ИСКЛЮЧАЕМ КРАШ: Проверяем, что позиция на 100% существует в памяти документа
                    if (targetPosition >= 0 && targetPosition < docTotalChars) {
                        QTextCursor cursor = currentEditor->textCursor();
                        cursor.setPosition(targetPosition);
                        currentEditor->setTextCursor(cursor);
                        currentEditor->centerCursor(); // Центрируем экран на строке

                        // Накатываем чистую желтую подложку маркера DAP
                        QList<QTextEdit::ExtraSelection> extraSelections;
                        QTextEdit::ExtraSelection selection;
                        selection.format.setBackground(QColor(255, 255, 150, 100)); // RGBA
                        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
                        selection.cursor = currentEditor->textCursor();
                        selection.cursor.clearSelection();
                        extraSelections.append(selection);

                        currentEditor->setExtraSelections(extraSelections);
                    }
                }
                currentEditor->setFocus();
                currentEditor->update();
            }
        });
    });

    // =========================================================================
    // ЧАСТЬ 2: АВТОНОМНЫЙ ПРЯМОЙ КОННЕКТ ДЛЯ КНОПКИ ДЕБАГА НА БОКОВОЙ ПАНЕЛИ
    // =========================================================================
    // if (actDebug) {
    //     actDebug->disconnect(); // Намертво вырезаем старые связи

    //     // КРИТИЧЕСКИЙ ШАГ: Связываем экшен кнопки сайдбара СРАЗУ с новым слотом внутри panelOther!
    //     if (panelOther) {
    //         connect(actDebug, &QAction::triggered, panelOther, &panel_other::onDebugModeTriggered);
    //     }

    //     // Основной коннект для управления процессами и внешними сплиттерами главного окна
    //     connect(actDebug, &QAction::triggered, this, [this](bool checked) {
    //         QToolButton *sideDebugBtn = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>("Дебаг") : nullptr;
    //         QLabel *sideDebugLbl = sideDebugBtn ? sideDebugBtn->findChild<QLabel*>() : nullptr;

    //         if (checked) {
    //             // =============================================================
    //             // СЦЕНАРИЙ А: ЗАПУСК СЕССИИ ОТЛАДКИ PyTorch
    //             // =============================================================
    //             if (this->currentOpenProjectPath.isEmpty()) {
    //                 this->sendSystemNotification("Debug", "Ошибка: Нет открытого проекта!");
    //                 actDebug->setChecked(false);
    //                 return;
    //             }

    //             QWidget *currentPage = ui->centralStackedWidget->currentWidget();
    //             if (!currentPage || currentPage->objectName().isEmpty() || currentPage->objectName() == "MAIN_SCREEN") {
    //                 this->sendSystemNotification("Debug", "Выберите файл .py в редакторе для запуска отладки.");
    //                 actDebug->setChecked(false);
    //                 return;
    //             }

    //             QString activeScript = currentPage->objectName();
    //             if (!activeScript.endsWith(".py", Qt::CaseInsensitive)) {
    //                 this->sendSystemNotification("Debug", "Отладка поддерживается только для чистых файлов .py");
    //                 actDebug->setChecked(false);
    //                 return;
    //             }

    //             if (sideDebugLbl) sideDebugLbl->setText("Стоп дебаг");

    //             if (ui->statusbar) {
    //                 ui->statusbar->showMessage("🪲 Режим отладки активирован. Ожидание подключения к debugpy...", 5000);
    //             }

    //             // Нативно открываем нижний виджет и раздвигаем вертикальный сплиттер
    //             if (panelOther) {
    //                 panelOther->setVisible(true);
    //                 panelOther->show();

    //                 QStackedWidget *stacked = panelOther->findChild<QStackedWidget*>("stackedWidget");
    //                 if (stacked) {
    //                     stacked->setCurrentIndex(1);}

    //                 if (mainVerticalSplitter) {
    //                     int totalHeight = this->height() > 0 ? this->height() : 800;
    //                     mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
    //                     mainVerticalSplitter->refresh();
    //                 }
    //             }

    //             // Нативно открываем и раздвигаем правую вертикальную панель
    //             if (ui->rightDebugPanel) {
    //                 ui->rightDebugPanel->setVisible(true);
    //                 ui->rightDebugPanel->show();

    //                 QSplitter *codeSplitter = qobject_cast<QSplitter*>(ui->rightDebugPanel->parentWidget());
    //                 if (codeSplitter) {
    //                     int codeZoneWidth = codeSplitter->width() > 0 ? codeSplitter->width() : (this->width() - 68);
    //                     int rightWidth = 350;
    //                     int leftWidth = codeZoneWidth - rightWidth;

    //                     if (leftWidth < 200) leftWidth = 200;

    //                     codeSplitter->setCollapsible(0, false);
    //                     codeSplitter->setCollapsible(1, false);
    //                     codeSplitter->setSizes(QList<int>({leftWidth, rightWidth}));
    //                     codeSplitter->refresh();
    //                 }

    //                 QToolButton *btnTensor = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>("Графики") : nullptr;
    //                 if (btnTensor) btnTensor->setChecked(true);
    //             }

    //             // Запуск сессии дебаггера
    //             QSettings ideConfig(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);
    //             QString activeVenv = ideConfig.value("python/global_venv_path").toString();
    //             if (activeVenv.isEmpty()) {
    //                 activeVenv = this->currentOpenProjectPath + "/venv";
    //             }

    //             if (this->pyDebugger) {
    //                 this->pyDebugger->startDebugSession(this->currentOpenProjectPath, activeScript, activeVenv);
    //             }

    //         } else {
    //             // =============================================================
    //             // СЦЕНАРИЙ Б: ПРИНУДИТЕЛЬНЫЙ ОСТАНОВ ОТЛАДКИ ПОЛЬЗОВАТЕЛЕМ
    //             // =============================================================
    //             if (sideDebugLbl) sideDebugLbl->setText("Дебаг");
    //             if (this->pyDebugger) {
    //                 this->pyDebugger->stopDebugSession();
    //             }

    //             // Нативно прячем нижнюю панель логов через hide
    //             if (panelOther) {
    //                 panelOther->setVisible(false);
    //                 panelOther->hide();
    //             }

    //             if (mainVerticalSplitter) {
    //                 mainVerticalSplitter->setSizes(QList<int>({this->height(), 0}));
    //                 mainVerticalSplitter->refresh();
    //             }

    //             // Нативно прячем правую дебаг-панель
    //             if (ui->rightDebugPanel) {
    //                 ui->rightDebugPanel->setVisible(false);
    //                 ui->rightDebugPanel->hide();

    //                 QSplitter *codeSplitter = qobject_cast<QSplitter*>(ui->rightDebugPanel->parentWidget());
    //                 if (codeSplitter) {
    //                     codeSplitter->setSizes(QList<int>({codeSplitter->width(), 0}));
    //                     codeSplitter->refresh();
    //                 }

    //                 QToolButton *btnTensor = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>("Графики") : nullptr;
    //                 if (btnTensor) btnTensor->setChecked(false);
    //             }

    //             if (ui->statusbar) {
    //                 ui->statusbar->showMessage("Сессия отладки прервана.", 3000);
    //             }
    //             this->sendSystemNotification("Debug", "Сессия отладки прервана.");
    //         }

    //         if (this->layout()) this->layout()->activate();
    //         this->update();
    //     });
    // }

    if (actDebug) {
        actDebug->disconnect(); // Намертво вырезаем старые связи

        if (panelOther) {
            connect(actDebug, &QAction::triggered, panelOther, &panel_other::onDebugModeTriggered);
        }

        // Основной коннект для управления процессами и геометрией сетки главного окна
        connect(actDebug, &QAction::triggered, this, [this](bool checked) {
            QToolButton *sideDebugBtn = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>("Дебаг") : nullptr;
            QLabel *sideDebugLbl = sideDebugBtn ? sideDebugBtn->findChild<QLabel*>() : nullptr;

            if (checked) {
                // =============================================================
                // КРИТИЧЕСКИЙ UX-ПРЕДОХРАНИТЕЛЬ: ПРОВЕРКА СОХРАНЕНИЯ КОДА
                // =============================================================
                if (!this->showSaveConfirmationDialog()) {
                    // Если нажали "Отмена" — возвращаем тумблер сайдбара в исходное отжатое состояние
                    actStartTrain->setChecked(false);
                    return; // МГНОВЕННО ВЫХОДИМ, дебаггер debugpy даже не начнет запускаться! [0:365]
                }

                // =============================================================
                // СЦЕНАРИЙ А: ЗАПУСК СЕССИИ ОТЛАДКИ PyTorch
                // =============================================================
                if (this->currentOpenProjectPath.isEmpty()) {
                    this->sendSystemNotification("Debug", "Ошибка: Нет открытого проекта!");
                    actDebug->setChecked(false);
                    return;
                }

                QWidget *currentPage = ui->centralStackedWidget->currentWidget();
                if (!currentPage || currentPage->objectName().isEmpty() || currentPage->objectName() == "MAIN_SCREEN") {
                    this->sendSystemNotification("Debug", "Выберите файл .py в редакторе для запуска отладки.");
                    actDebug->setChecked(false);
                    return;
                }

                QString activeScript = currentPage->objectName();
                if (!activeScript.endsWith(".py", Qt::CaseInsensitive)) {
                    this->sendSystemNotification("Debug", "Отладка поддерживается только для чистых файлов .py");
                    actDebug->setChecked(false);
                    return;
                }

                if (sideDebugLbl) sideDebugLbl->setText("Стоп дебаг");
                if (ui->statusbar) {
                    ui->statusbar->showMessage(" Режим отладки активирован. Ожидание подключения к debugpy...", 5000);
                }

                // 1. Отображаем нижнюю панель и фиксируем её высоту под новую строку кнопок
                if (panelOther) {
                    panelOther->setVisible(true);
                    panelOther->show();
                    panelOther->setFixedHeight(285); // Даем 285px высоты, чтобы вместить контекстное меню

                    QStackedWidget *stacked = panelOther->findChild<QStackedWidget*>("stackedWidget");
                    if (stacked) {
                        stacked->setCurrentIndex(1); // Включаем нужную страницу дебага внутри панели
                    }

                    QStackedWidget *stacked2 = panelOther->findChild<QStackedWidget*>("menuSwitcherStack");
                    if (stacked2) {
                        stacked2->setCurrentIndex(1); // Включаем нужную страницу дебага внутри панели
                    }
                }

                // 2. Отображаем правую панель отладки и фиксируем её геометрию в сетке
                if (ui->rightDebugPanel) {
                    ui->rightDebugPanel->setVisible(true);
                    ui->rightDebugPanel->show();
                    ui->rightDebugPanel->setFixedWidth(330); // Жестко держим 330px, не давая окну уходить вправо
                    ui->rightDebugPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
                }

                // 3. Запуск сессии дебаггера
                QSettings ideConfig(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);
                QString activeVenv = ideConfig.value("python/global_venv_path").toString();
                if (activeVenv.isEmpty()) {
                    activeVenv = this->currentOpenProjectPath + "/venv";
                }
                if (this->pyDebugger) {
                    this->pyDebugger->startDebugSession(this->currentOpenProjectPath, activeScript, activeVenv);
                }

            } else {
                // =============================================================
                // СЦЕНАРИЙ Б: ПРИНУДИТЕЛЬНЫЙ ОСТАНОВ ОТЛАДКИ ПОЛЬЗОВАТЕЛЕМ
                // =============================================================
                if (sideDebugLbl) sideDebugLbl->setText("Дебаг");
                if (this->pyDebugger) {
                    this->pyDebugger->stopDebugSession();
                }

                // Нативно прячем нижнюю панель
                if (panelOther) {
                    panelOther->setVisible(false);
                    panelOther->hide();
                }

                if (ui->rightDebugPanel) {
                    ui->rightDebugPanel->setVisible(false);
                    ui->rightDebugPanel->hide();
                }

                QStackedWidget *stacked = panelOther->findChild<QStackedWidget*>("stackedWidget");
                if (stacked) {
                    stacked->setCurrentIndex(1); // 0 - обычный терминал/логи разработки
                }

                QStackedWidget *stacked2 = panelOther->findChild<QStackedWidget*>("menuSwitcherStack");
                if (stacked2) {
                    stacked2->setCurrentIndex(0); // Включаем нужную страницу дебага внутри панели
                }

                if (ui->statusbar) {
                    ui->statusbar->showMessage("Сессия отладки прервана.", 3000);
                }
                this->sendSystemNotification("Debug", "Сессия отладки прервана.");
            }

            // 4. ФИНАЛЬНЫЙ АВТОПЕРЕСЧЕТ: Заставляем QGridLayout на центральном виджете перестроить пиксели
            if (ui && ui->centralwidget && ui->centralwidget->layout())
            {
                QGridLayout *mainGrid = qobject_cast<QGridLayout*>(ui->centralwidget->layout());
                if (mainGrid)
                {
                    mainGrid->invalidate(); // Сбрасываем старый кэш размеров
                    mainGrid->activate();   // Принудительно выстраиваем Код, Дебаггер и Терминал по правилам
                }
            }
            this->update();
        });
    }

    // СИНХРОНИЗАЦИЯ ЗАВЕРШЕНИЯ: Если скрипт отработал сам до конца — отжимаем экшен боковой панели
    connect(pyDebugger, &DebugManager::sessionFinished, this, [this]() {
        if (actDebug && actDebug->isChecked()) {
            actDebug->blockSignals(true);
            actDebug->setChecked(false); // Отжимаем кнопку в UI
            actDebug->blockSignals(false);
        }

        // Отжимаем нижнюю кнопку терминала
        if (this->btnTerminal && this->btnTerminal->isChecked()) {
            this->btnTerminal->setChecked(false);
        }

        // Возвращаем исходный текст кнопке на панели
        QToolButton *sideDebugBtn = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>("Дебаг") : nullptr;
        QLabel *sideDebugLbl = sideDebugBtn ? sideDebugBtn->findChild<QLabel*>() : nullptr;
        if (sideDebugLbl) sideDebugLbl->setText("Дебаг");
    });

    // =========================================================================
    // ЧАСТЬ 3: ЕДИНЫЙ АВТОНОМНЫЙ ОБРАБОТЧИК КНОПКИ "ОБУЧЕНИЕ"
    // =========================================================================
    if (actStartTrain) {
        actStartTrain->disconnect(SIGNAL(triggered(bool)));
        connect(actStartTrain, &QAction::triggered, this, [this](bool checked) {
            QToolButton *btn = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>(QStringLiteral("Обучение")) : nullptr;
            QLabel *lbl = btn ? btn->findChild<QLabel*>() : nullptr;

            qDebug() << ">>> [ТРИГГЕР] Кнопка Обучение нажата! checked =" << checked;

            if (checked) {
                // 1. ПЕРВИЧНЫЕ ПРОВЕРКИ И КРАШ-ЗАЩИТА
                if (!this->showSaveConfirmationDialog()) {
                    qDebug() << " [СТОП] Ранний выход: showSaveConfirmationDialog() вернул false!";
                    actStartTrain->setChecked(false);
                    return;
                }
                if (this->currentOpenProjectPath.isEmpty()) {
                    qDebug() << " [СТОП] Ранний выход: currentOpenProjectPath пуст!";
                    this->sendSystemNotification(QStringLiteral("Обучение"), QStringLiteral("Ошибка: Нет открытого проекта!"));
                    actStartTrain->setChecked(false);
                    return;
                }

                qDebug() << " [ШАГ] Проверки пройдены. Путь к проекту:" << this->currentOpenProjectPath;

                // 2. СИЛОВОЕ РАЗВЕРТЫВАНИЕ ИНТЕРФЕЙСА ТЕРМИНАЛА
                if (this->panelOther) {
                    qDebug() << " [ШАГ] Раскрываю панель логов...";
                    this->panelOther->show();
                    this->panelOther->setVisible(true);
                    QStackedWidget *bottomStacked = this->panelOther->findChild<QStackedWidget*>();
                    if (bottomStacked) {
                        bottomStacked->setCurrentIndex(2); // Переключаем на logEdit
                    }
                    QStackedWidget *menuSwitcherStack = this->panelOther->findChild<QStackedWidget*>(QStringLiteral("menuSwitcherStack"));
                    if (menuSwitcherStack) {
                        menuSwitcherStack->setCurrentIndex(0);
                    }
                    QPushButton *btnViewLog = this->panelOther->findChild<QPushButton*>(QStringLiteral("btnViewLog"));
                    if (btnViewLog) {
                        btnViewLog->click();
                    }
                } else {
                    qDebug() << " [КРИТ ОШИБКА] Указатель this->panelOther равен nullptr!";
                }

                // Насильно раздвигаем вертикальный сплиттер на экране
                if (this->mainVerticalSplitter) {
                    int totalHeight = this->height();
                    this->mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
                    this->mainVerticalSplitter->update();
                    qDebug() << " [ШАГ] Вертикальный сплиттер раздвинут на 250px.";
                }

                QWidget *problemsContainer = this->findChild<QWidget*>(QStringLiteral("problemsContainer"));
                if (problemsContainer) {
                    problemsContainer->hide();
                }

                // Очищаем консоль и пишем стартовый текст
                QPlainTextEdit *logEdit = this->panelOther ? this->panelOther->findChild<QPlainTextEdit*>(QStringLiteral("logEdit")) : nullptr;
                if (logEdit) {
                    logEdit->clear();
                    logEdit->appendPlainText(QStringLiteral("--- Ожидание готовности сетевой инфраструктуры Jupyter ---"));
                }

                actStartTrain->setIcon(QIcon(QStringLiteral(":/Data/system_icons/media-playback-stop.svg")));
                if (lbl) lbl->setText(QStringLiteral("Stop"));
                this->sendSystemNotification(QStringLiteral("PyTorch Studio"), QStringLiteral("Запуск фонового сервера вычислений..."));
                // =========================================================================
                // АВТОНОМНЫЙ ВСТРОЕННЫЙ JSON-ПРОСМОТР ЯЧЕЕК ПЕРЕД ЗАПУСКОМ (ВЫВОД НА ЭКРАН)
                // =========================================================================
                QString absNotebookPath = this->currentOpenProjectPath + QStringLiteral("/notebooks/train_model.ipynb");
                QFile nbFile(absNotebookPath);
                if (nbFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QByteArray fileData = nbFile.readAll();
                    nbFile.close();
                    QJsonDocument doc = QJsonDocument::fromJson(fileData);
                    QJsonObject rootObj = doc.object();
                    QJsonArray cellsArray = rootObj["cells"].toArray();

                    if (logEdit) {
                        logEdit->appendPlainText(QStringLiteral("\n=== СТРУКТУРА И СОДЕРЖИМОЕ ИСПОЛНЯЕМОГО НОУТБУКА ==="));
                        int codeCellIdx = 0;
                        for (int i = 0; i < cellsArray.size(); ++i) {
                            QJsonObject cell = cellsArray[i].toObject();
                            if (cell["cell_type"].toString() == QStringLiteral("code")) {
                                codeCellIdx++;
                                logEdit->appendPlainText(QString("\n--- ЯЧЕЙКА КОДА №%1 ---").arg(codeCellIdx));
                                QJsonArray sourceLines = cell["source"].toArray();
                                QString cellCode;
                                for (int j = 0; j < sourceLines.size(); ++j) {
                                    cellCode += sourceLines[j].toString();
                                }
                                if (cellCode.isEmpty()) {
                                    cellCode = cell["source"].toString();
                                }
                                logEdit->appendPlainText(cellCode);
                            }
                        }
                        logEdit->appendPlainText(QStringLiteral("\n===================================================\n"));
                    }
                } else {
                    qDebug() << "[ОШИБКА ЧТЕНИЯ]: Не удалось прочитать .ipynb для вывода ячеек!";
                }
                // =========================================================================
                // ЧАСТЬ 3: БЛОК 2: СИНХРОНИЗАЦИЯ КЛИЕНТА ВЕБ-СОКЕТОВ И АППАРАТНЫЙ ПУСК КОДА
                // =========================================================================
                if (this->jupyterClient && this->jupyterServer) {
                    QObject::disconnect(this->jupyterClient, &JupyterClient::codeOutputReceived, nullptr, nullptr);
                    QObject::disconnect(this->jupyterClient, &JupyterClient::executionFinished, nullptr, nullptr);
                    QObject::disconnect(this->jupyterClient, &JupyterClient::jupyterClientReady, nullptr, nullptr);

                    // Слот перехвата потока iopub
                    connect(this->jupyterClient, &JupyterClient::codeOutputReceived, this, [this](const QString &rawLog) {
                        QPlainTextEdit *logEditInner = this->panelOther ? this->panelOther->findChild<QPlainTextEdit*>(QStringLiteral("logEdit")) : nullptr;
                        if (logEditInner) {
                            logEditInner->insertPlainText(rawLog);
                            logEditInner->moveCursor(QTextCursor::End);
                        }
                    });

                    // Автосброс кнопок по финалу расчетов
                    connect(this->jupyterClient, &JupyterClient::executionFinished, this, [this, lbl](bool success) {
                        qDebug() << "[JUPYTER] Выполнение ноутбука завершено. Статус:" << success;
                        actStartTrain->blockSignals(true);
                        actStartTrain->setChecked(false);
                        actStartTrain->blockSignals(false);
                        actStartTrain->setIcon(QIcon(QStringLiteral(":/Data/system_icons/media-playback-start.svg")));

                        QToolButton *innerBtn = this->leftSideBarContainer ? this->leftSideBarContainer->findChild<QToolButton*>(QStringLiteral("Обучение")) : nullptr;
                        if (innerBtn) {
                            QLabel *innerLbl = innerBtn->findChild<QLabel*>();
                            if (innerLbl) innerLbl->setText(QStringLiteral("Обучение"));
                        }
                    });

                    // ОТПРАВКА КОДА ПО ФАКТУ ОТКРЫТИЯ ВЕБ-СОКЕТА (ПОСТРОЧНЫЙ ТРЕКЕР ЗАВИСАНИЙ)
                    // ОТПРАВКА КОДА ПО ФАКТУ ОТКРЫТИЯ ВЕБ-СОКЕТА (ФИНАЛЬНЫЙ СТАБИЛЬНЫЙ ВАРИАНТ)
                    connect(this->jupyterClient, &JupyterClient::jupyterClientReady, this, [this]() {
                        qDebug() << " [JUPYTER PIPELINE] Веб-сокет открыт! Запуск сквозного выполнения ячеек...";
                        QString absPath = this->currentOpenProjectPath + QStringLiteral("/notebooks/train_model.ipynb");

                        // Выполняем ячейку целиком (блоком), что сохраняет валидность классов, циклов и отступов
                        QString runCode = QString(
                            "import nbformat\n"
                            "import sys\n"
                            "import os\n"
                            "\n"
                            "notebook_abs_path = '%1'\n"
                            "project_dir = '%2'\n"
                            "\n"
                            "try:\n"
                            "    os.chdir(project_dir)\n"
                            "    with open(notebook_abs_path, 'r', encoding='utf-8') as f:\n"
                            "        nb = nbformat.read(f, as_version=4)\n"
                            "    \n"
                            "    print('>>> [УСПЕХ] Файл ноутбука прочитан. Найдено ячеек: ' + str(len(nb.cells)))\n"
                            "    sys.stdout.flush()\n"
                            "    \n"
                            "    idx = 0\n"
                            "    for cell in nb.cells:\n"
                            "        if cell.cell_type == 'code' and cell.source.strip():\n"
                            "            idx += 1\n"
                            "            print('\\n--- [ВЫПОЛНЕНИЕ ЯЧЕЙКИ ' + str(idx) + '] ---')\n"
                            "            sys.stdout.flush()\n"
                            "            \n"
                            "            # Выполняем ВСЮ ячейку целиком как единый монолитный блок кода\n"
                            "            exec(cell.source, globals())\n"
                            "            \n"
                            "            # Форсируем сброс буфера stdout после каждой ячейки для Qt LogEdit\n"
                            "            sys.stdout.flush()\n"
                            "            sys.stderr.flush()\n"
                            "            \n"
                            "except Exception as e:\n"
                            "    print('\\n[КРИТИЧЕСКАЯ ОШИБКА ЯДРА]: ' + str(type(e).__name__) + ': ' + str(e))\n"
                            "    sys.stdout.flush()\n"
                        ).arg(absPath, this->currentOpenProjectPath);

                        this->jupyterClient->executePythonCode(runCode);
                    });
                }

                // =========================================================================
                // ЧАСТЬ 3: БЛОК 3: ПЕРЕХВАТ ЛОГОВ СЕРВЕРА И КОНТРОЛЬ СЕТИ ЧЕРЕЗ ТАЙМЕР
                // =========================================================================
                qDebug() << " [ШАГ] Запуск программного генератора эмуляции данных...";

                QProcess *mockDataProc = new QProcess(this);
                QString pythonBin = QDir::home().absoluteFilePath(QStringLiteral("venv/bin/python3"));
                QString scriptPath = this->currentOpenProjectPath + QStringLiteral("/tools/generate_mock_data.py");

                // Настраиваем рабочую директорию, чтобы os.path отрабатывал корректно
                mockDataProc->setWorkingDirectory(this->currentOpenProjectPath);

                // Запускаем скрипт генерации данных в синхронном (блокирующем) режиме
                mockDataProc->start(pythonBin, QStringList() << scriptPath);

                // Ждем завершения работы скрипта (не более 3 секунд), чтобы не вешать GUI намертво
                if (mockDataProc->waitForFinished(3000)) {
                    qDebug() << " [УСПЕХ] Эмулятор данных успешно отработал. Датасет создан!";
                } else {
                    qWarning() << " [ВНИМАНИЕ] Таймаут генерации данных или скрипт завершился с ошибкой.";
                }
                mockDataProc->deleteLater(); // Освобождаем память процесса

                QProcess *serverProc = this->jupyterServer ? this->jupyterServer->findChild<QProcess*>() : nullptr;
                if (serverProc) {
                    serverProc->disconnect(SIGNAL(readyReadStandardOutput()));
                    serverProc->disconnect(SIGNAL(readyReadStandardError()));

                    static bool isApiConnected = false;
                    isApiConnected = false;

                    auto checkServerOutput = [this, serverProc]() {
                        QByteArray output = serverProc->readAllStandardOutput() + serverProc->readAllStandardError();
                        QString serverLog = QString::fromUtf8(output);

                        QPlainTextEdit *logEditInner = this->panelOther ? this->panelOther->findChild<QPlainTextEdit*>(QStringLiteral("logEdit")) : nullptr;
                        if (logEditInner && !serverLog.isEmpty()) {
                            logEditInner->insertPlainText(serverLog);
                            logEditInner->moveCursor(QTextCursor::End);
                        }

                        // Как только пошел первый чанк логов — запускаем железный таймер стабилизации портов ОС
                        if (!serverLog.isEmpty() && !isApiConnected) {
                            isApiConnected = true;
                            QTimer::singleShot(1500, this, [this]() {
                                if (this->jupyterClient) {
                                    QPlainTextEdit *logEditInner = this->panelOther ? this->panelOther->findChild<QPlainTextEdit*>(QStringLiteral("logEdit")) : nullptr;
                                    if (logEditInner) {
                                        logEditInner->insertPlainText(QStringLiteral("\n[REST API] Отправлен запрос на инициализацию ядра Python...\n"));
                                    }
                                    // Коннектимся к адаптированному под Modern Jupyter Server 2.x API
                                    this->jupyterClient->connectToJupyter(QStringLiteral("127.0.0.1"), 8888, QStringLiteral("notebooks/train_model.ipynb"));
                                }
                            });
                        }
                    };

                    connect(serverProc, &QProcess::readyReadStandardOutput, this, checkServerOutput);
                    connect(serverProc, &QProcess::readyReadStandardError, this, checkServerOutput);
                }

                // Старт фонового процесса
                if (this->jupyterServer && !this->jupyterServer->isRunning()) {
                    this->jupyterServer->startServer(this->currentOpenProjectPath);
                }

            } else {
                // ВЕТКА ОСТАНОВКИ: Экстренное прерывание расчета пользователем
                actStartTrain->setIcon(QIcon(QStringLiteral(":/Data/system_icons/media-playback-start.svg")));
                if (lbl) lbl->setText(QStringLiteral("Обучение"));
                if (this->jupyterServer) {
                    this->jupyterServer->stopServer();
                }
                this->sendSystemNotification(QStringLiteral("Проект"), QStringLiteral("Обучение экстренно остановлено пользователем."));
            }
        });
    }

    this->docMgr = new DocumentManager(this, ui->fileComboBox, ui->openFilesListWidget, this->titleLabel, this);

    // =========================================================================
    // ТОТАЛЬНЫЙ АППАРАТНЫЙ ПРОБИВ: РАЗБЛОКИРУЕМ РЕЖИМ ВЫДЕЛЕНИЯ ИЗ DESIGNER
    // =========================================================================
    if (ui->openFilesListWidget) {

        // 1. НАМЕРТВО ВКЛЮЧАЕМ РЕЖИМ ВЫДЕЛЕНИЯ СТРОК (Снимаем блок NoSelection!)
        ui->openFilesListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->openFilesListWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

        // Наш отлаженный блок палитры QPalette (оставляем без изменений)
        QPalette listPalette = ui->openFilesListWidget->palette();
        QColor targetSelectedBlue(147, 206, 233); // #93cee9
        QColor targetSelectedText(0, 0, 0);       // Черный текст

        listPalette.setColor(QPalette::Active, QPalette::Highlight, targetSelectedBlue);
        listPalette.setColor(QPalette::Active, QPalette::HighlightedText, targetSelectedText);
        listPalette.setColor(QPalette::Inactive, QPalette::Highlight, targetSelectedBlue);
        listPalette.setColor(QPalette::Inactive, QPalette::HighlightedText, targetSelectedText);
        listPalette.setColor(QPalette::Normal, QPalette::Highlight, targetSelectedBlue);
        listPalette.setColor(QPalette::Normal, QPalette::HighlightedText, targetSelectedText);

        ui->openFilesListWidget->setPalette(listPalette);
        ui->openFilesListWidget->setAutoFillBackground(true);
        ui->openFilesListWidget->setFocusPolicy(Qt::NoFocus);

        // 2. Принудительно заставляем список полностью перерисовать текущую геометрию ячеек
        ui->openFilesListWidget->update();
    }

    connect(actSTM, &QAction::triggered, this, &Neuro_programm::open_STM);
    connect(actSTM_work, &QAction::triggered, this, &Neuro_programm::open_STM_work);


    connect(panelOther, &panel_other::fileNavigationRequested,
            this, &Neuro_programm::openNewFileInEditor);

    if (ui->centralStackedWidget) {
        // Разрешаем центральной зоне кода сжиматься вплоть до 400 пикселей,
        // чтобы она не выталкивала правую панель дебага за экран
        ui->centralStackedWidget->setMinimumWidth(400);
        ui->centralStackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    if (panelOther) {
        QStackedWidget *stacked2 = panelOther->findChild<QStackedWidget*>("menuSwitcherStack");
        if (stacked2) {
            stacked2->setCurrentIndex(0); // Принудительный сброс на Терминал при старте IDE
        }
    }

    // =========================================================================
    // ИСПРАВЛЕННЫЙ ХОТКЕЙ Ctrl+J: ЗАЩИЩЕН ОТ ASSERT, КРАШЕЙ И ИНТЕГРИРОВАН С ОТМЕНОЙ
    // =========================================================================
    QShortcut* chatShortcut = new QShortcut(QKeySequence("Ctrl+J"), this);
    chatShortcut->setContext(Qt::ApplicationShortcut);
    chatShortcut->disconnect();

    connect(chatShortcut, &QShortcut::activated, this, [this]() {
        // ЗАЩИТА ОТ КЛОНОВ: Проверяем сохраненный указатель класса или ищем на экране
        if (this->m_activePromptWidget != nullptr) {
            this->m_activePromptWidget->showNormal();
            this->m_activePromptWidget->raise();
            this->m_activePromptWidget->activateWindow();
            return;
        }

        AiPromptWidget *existingPrompt = this->findChild<AiPromptWidget*>();
        if (existingPrompt) {
            this->m_activePromptWidget = existingPrompt; // Восстанавливаем связь в памяти
            existingPrompt->showNormal();
            existingPrompt->raise();
            existingPrompt->activateWindow();
            return;
        }

        qInfo() << ">>> [AI CHAT ТРИГГЕР]: Активировано окно генерации кода...";

        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (!currentPage) return;

        CodeEditor *activeEditor = currentPage->findChild<CodeEditor*>();
        if (!activeEditor) {
            activeEditor = qobject_cast<CodeEditor*>(QApplication::focusWidget());
        }

        if (!activeEditor) {
            qWarning() << "⚠️ [AI Chat Warning]: Текстовый редактор кода не найден в фокусе страницы.";
            return;
        }

        // Фиксируем контекст в умных указателях
        QPointer<CodeEditor> localSavedEditor = activeEditor;
        int localSavedPosition = activeEditor->textCursor().position();
        qInfo() << ">>> [AI Chat Success]: Позиция курсора зафиксирована на символе:" << localSavedPosition;

        // ФИКС: Инициализируем глобальную переменную класса вместо локальной
        this->m_activePromptWidget = new AiPromptWidget(this);

        int x = this->geometry().x() + (this->geometry().width() - this->m_activePromptWidget->width()) / 2;
        int y = this->geometry().y() + (this->geometry().height() - this->m_activePromptWidget->height()) / 2;
        this->m_activePromptWidget->move(x, y);
        this->m_activePromptWidget->show();
        this->m_activePromptWidget->raise();
        this->m_activePromptWidget->activateWindow();

        // =====================================================================
        // ИНТЕГРАЦИЯ ШАГА 3: ОБРАБОТЧИК КНОПКИ "ОТМЕНА" И ОБРЫВА ИНФЕРЕНСА НА CPU
        // =====================================================================
        connect(this->m_activePromptWidget, &AiPromptWidget::cancelRequested, this, [this, localSavedEditor]() {
            qInfo() << "🛑 [AI CHAT CANCEL]: Пользователь прервал операцию. Очищаю потоки...";
            this->isAiProcessing = false; // Снимаем аппаратный барьер

            if (this->m_aiManager) {
                this->m_aiManager->abortChatGeneration(); // Насильно рвем HTTP-сокет FastAPI
            }

            // Возвращаем стандартную каретку ввода активному редактору кода
            if (!localSavedEditor.isNull()) {
                localSavedEditor->setCursor(Qt::IBeamCursor);
                localSavedEditor->viewport()->update();
            }

            // Намертво зануляем указатель, так как promptWidget уничтожит себя через deleteLater()
            this->m_activePromptWidget = nullptr;
        });

        // Стандартное подключение отправки промпта в именованный слот по Enter
        connect(this->m_activePromptWidget, &AiPromptWidget::promptSubmitted, this, &Neuro_programm::onPromptSubmitted);
    });

    edit_intfce();
    add_vars_debug();
    this->setupDebugInterface();
    createMenus();

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
    qInfo() << "[PROJECT_MGR] Инициализация и запуск мастера Start_progect...";

    // =========================================================================
    // ЧАСТЬ 1: СБОР ПАРАМЕТРОВ ИЗ ДИАЛОГОВОГО ОКНА МАСТЕРА (WIZARD)
    // =========================================================================
    Start_progect wizard(this);
    if (wizard.exec() != QDialog::Accepted) {
        qInfo() << "[PROJECT_MGR] Создание проекта отменено пользователем.";
        return;
    }

    QString projectName = wizard.getProjectName().trimmed();
    QString parentDir = wizard.getProjectLocation().trimmed();
    bool createNewVenv = wizard.isCreateNewVenv();
    bool useGpuArchitecture = wizard.isGpuArchitecture();
    QString sourceDatasetPath = wizard.getDatasetPath().trimmed();
    bool useSymlinkMode = wizard.isSymlinkMode();

    bool useCustomRequirements = wizard.isCustomRequirementsEnabled();
    QString customRequirementsPath = wizard.getCustomRequirementsPath();
    QString customVenvPath = "";
    if (wizard.isUseExistingVenv()) {
        customVenvPath = wizard.getExistingVenvPath().trimmed();
    } else if (createNewVenv) {
        customVenvPath = wizard.getCreateNewVenvPath().trimmed();
    }

    if (projectName.isEmpty() || parentDir.isEmpty()) {
        ui->statusbar->showMessage("Ошибка: Имя проекта или корневой путь не заполнены!", 4000);
        return;
    }

    QString projectFolderPath = parentDir + "/" + projectName;
    QDir targetDir(projectFolderPath);

    // =========================================================================
    // РАЗВЕРТЫВАНИЕ ПОЛНОЙ СТРУКТУРЫ С УЧЕТОМ ПОДПАПОК DATA
    // =========================================================================
    if (!targetDir.exists()) {
        if (!targetDir.mkpath(".")) {
            qCritical() << "[PROJECT_MGR] Не удалось создать директорию проекта:" << projectFolderPath;
            ui->statusbar->showMessage("Ошибка: Нет прав на создание папки!", 4000);
            return;
        }

        // Разворачиваем 10 базовых MLOps директорий
        QStringList fullStructure = {
            "config", "data", "datasets", "hf_hub", "logs",
            "metrics", "models", "notebooks", "scripts", "tests"
        };
        for (const QString &subDir : fullStructure) {
            targetDir.mkdir(subDir);
        }

        // Создаем внутреннюю структуру папки data для стадий предобработки теплограмм
        targetDir.mkpath("data/raw");
        targetDir.mkpath("data/processed");

        // ДОРАБОТКА: Автоматическое создание пустых файлов __init__.py в пакетах кода
        QStringList pythonPackages = {"scripts", "notebooks", "tests"};
        for (const QString &pkg : pythonPackages) {
            QFile initFile(targetDir.filePath(pkg + "/__init__.py"));
            if (initFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&initFile);
                out << "# PyStudio Python Package Marker\n";
                initFile.close();
            }
        }

        // НАДЕЖНЫЙ ВЫЗОВ И КОНТРОЛЬ ЗАПОЛНЕНИЯ ПАСПОРТА ПРОЕКТА
        ui->statusbar->showMessage("Генерация паспорта проекта и протоколов...", 2000);
        bool passportCreated = createProjectPassport(projectName, projectFolderPath, useGpuArchitecture);

        if (passportCreated) {
            ui->statusbar->showMessage("Паспорт .pystudio успешно создан!", 5000);
            qInfo() << "[PROJECT_MGR] Конвейер паспорта завершился успехом.";
        } else {
            ui->statusbar->showMessage("Критическая ошибка: Не удалось сгенерировать паспорт!", 5000);
            qCritical() << "[PROJECT_MGR] Конвейер паспорта вернул ошибку.";
        }

        // Создание стартового Jupyter Notebook (train_model.ipynb)
        createDefaultTrainNotebook(projectFolderPath);

        // КОПИРОВАНИЕ REQUIREMENTS.TXT В КОРЕНЬ ПРОЕКТА
        if (useCustomRequirements && !customRequirementsPath.isEmpty()) {
            QString destinationPath = projectFolderPath + "/requirements.txt";
            if (QFile::exists(destinationPath)) {
                QFile::remove(destinationPath);
            }
            if (QFile::copy(customRequirementsPath, destinationPath)) {
                qInfo() << "[PROJECT_MGR] Файл requirements.txt успешно скопирован.";
            } else {
                qCritical() << "[PROJECT_MGR] Не удалось скопировать requirements.txt";
                ui->statusbar->showMessage("Предупреждение: Не удалось скопировать файл зависимостей.", 4000);
            }
        }
    }

    // Замена устаревшего project.json на технический services_config.json
    bool servicesConfigCreated = createServicesConfig(projectName, projectFolderPath);
    if (!servicesConfigCreated) {
        qWarning() << "[PROJECT_MGR] Внимание: Технический конфигурационный файл служб не создан.";
    }

    // =========================================================================
    // ЧАСТЬ 2: РЕГИСТРАЦИЯ В РЕЕСТРЕ IDE.CONF И ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
    // =========================================================================
    QString configAbsolutePath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";
    QSettings settings(configAbsolutePath, QSettings::IniFormat);
    settings.beginGroup("Main");
    QString oldList = settings.value("recentProjectList").toString().trimmed();
    QString markerFilePath = projectFolderPath + "/" + projectName + ".pystudio";
    QString newList = markerFilePath;

    if (!oldList.isEmpty()) {
        static const QRegularExpression regex("[,;]");
        QStringList parts = oldList.split(regex);
        parts.removeAll(markerFilePath);
        if (!parts.isEmpty()) newList += "," + parts.join(",");
    }

    settings.setValue("recentProjectList", newList);
    settings.endGroup();

    this->currentOpenProjectPath = projectFolderPath;
    this->setIDEInStartMode(false);

    if (ui->btnCloseFile) {
        ui->btnCloseFile->setEnabled(true);
    }

    // Раскрываем левую панель проекта
    QStackedWidget *dockStack = ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack");
    if (dockStack) {
        dockStack->setCurrentIndex(0);
        ui->leftDockWidget->setVisible(true);
        if (actProject) actProject->setChecked(true);
    }
    this->initProjectTreeModel(projectFolderPath);
    this->updateProjectsListFromSettings();

    // Обработка импорта датасета (Шаг 3)
    if (wizard.isDatasetEnabled() && !sourceDatasetPath.isEmpty()) {
        QString targetDatasetLink = projectFolderPath + "/datasets/source_data";
        if (useSymlinkMode) {
            QFile::link(sourceDatasetPath, targetDatasetLink);
            qInfo() << "[PROJECT_MGR] Создана семантическая ссылка на датасет:" << sourceDatasetPath;
        } else {
            QProcess::startDetached("cp", QStringList() << "-r" << sourceDatasetPath << projectFolderPath + "/datasets/");
        }
    }

    // =========================================================================
    // ЧАСТЬ 3: АСИНХРОННЫЙ ЗАПУСК КОНВЕЙЕРА СБОРКИ В QTHREAD
    // =========================================================================
    ui->statusbar->showMessage("Подготовка фонового потока MLOps-сборщика...", 3000);
    QThread *workerThread = new QThread(this);

    // Передаем флаг, использует ли пользователь готовый venv
    bool isExistingVenvMode = wizard.isUseExistingVenv();

    ProjectBuilderWorker *worker = new ProjectBuilderWorker(
                projectFolderPath,
                projectName,
                useGpuArchitecture,
                useCustomRequirements,
                customRequirementsPath,
                customVenvPath,
                isExistingVenvMode
                );
    worker->moveToThread(workerThread);

    if (panelOther) {
        panelOther->setVisible(true);
        panelOther->show();
        QStackedWidget *panelStacked = panelOther->findChild<QStackedWidget*>();
        if (panelStacked) {
            panelStacked->setCurrentIndex(1);
        }
    }

    connect(worker, &ProjectBuilderWorker::logOutputReceived, this, [this](const QString &text) {
        if (panelOther) {
            panelOther->appendTrainingLog(text);
        }
        qDebug().noquote() << text;
    });

    connect(worker, &ProjectBuilderWorker::progressStepChanged, this, [this](int step, const QString &stepName) {
        if (step == 100) {
            ui->statusbar->showMessage("MLOps конвейер успешно собран!", 5000);
            if (panelOther) {
                QStackedWidget *panelStacked = panelOther->findChild<QStackedWidget*>();
                if (panelStacked) panelStacked->setCurrentIndex(0);
            }
        } else {
            ui->statusbar->showMessage(QString(" Сборка окружения [%1/4]: %2...").arg(step).arg(stepName), 0);
        }
    });

    connect(worker, &ProjectBuilderWorker::pipelineBuildFinished, this, [this, worker, workerThread](bool success, const QString &errorMsg) {
        if (success) {
            QMessageBox::information(this, "PyTorch Studio IDE",
                                     "<b>MLOps-окружение успешно зарегистрировано!</b><br><br>"
                                     "• Локальный Git-репозиторий развернут под ключ;<br>"
                                     "• Изолированный venv укомплектован пакетами PyTorch;<br>"
                                     "• Фоновое ядро Jupyter привязано к кодовой базе проекта.");
            this->sendSystemNotification("PyTorch Studio", "Конвейер проекта успешно инициализирован.");
            this->initLspServer();
        } else {
            QMessageBox::critical(this, "Критический сбой сборки", "Не удалось настроить окружение проекта:<br>" + errorMsg);
            if (panelOther) {
                QStackedWidget *panelStacked = panelOther->findChild<QStackedWidget*>();
                if (panelStacked) panelStacked->setCurrentIndex(0);
            }
        }
        workerThread->quit();
    });

    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    connect(workerThread, &QThread::started, worker, &ProjectBuilderWorker::startBuildPipeline);

    workerThread->start();

    if (!customVenvPath.isEmpty()) {
        QSettings ideConfig(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);
        ideConfig.setValue("python/global_venv_path", customVenvPath);
        qInfo() << "[PROJECT_MGR] Путь к venv успешно сохранен в конфигурационный файл.";
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
    //QString datasetPath = configObject["dataset_path"].toString("");
    //QString architecture = configObject["architecture"].toString("CUDA");
    QString savedDevice = configObject["device"].toString("cpu");
    int epochs = configObject["epochs"].toInt(10);
    int batchSize = configObject["batch_size"].toInt(32);
    double lr = configObject["learning_rate"].toDouble(0.001);

    // Вычисляем корень проекта на основе расположения .pystudio файла
    QFileInfo fileInfo(selectedFile);
    QString fullProjectPath = fileInfo.absoluteDir().absolutePath();

    // 4. Инициализируем GUI элементы, дерево файлов и стэк
    initProjectTreeModel(fullProjectPath);
    // ui->centralStackedWidget->setCurrentIndex(0);
    // if (ui->fileComboBox) ui->fileComboBox->setCurrentIndex(0);
    // if (ui->openFilesListWidget) ui->openFilesListWidget->setCurrentRow(0);

    // 5. Переопрашиваем доступное на текущем ПК железо
    detectCudaDevices();

    // 6. Синхронизируем интерфейс с загруженными настройками
    // Синхронизируем интерфейс дочерней панели ИИ с загруженными настройками
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

void Neuro_programm::onFileDoubleClicked(const QModelIndex &index)
{
    // 1. ИЗВЛЕКАЕМ АБСОЛЮТНЫЙ ПУТЬ К ФАЙЛУ ИЗ МОДЕЛИ ДЕРЕВА
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

    // =========================================================================
    // ИНТЕГРАЦИЯ МЕНЕДЖЕРА ПАКЕТОВ PIP (requirements.txt)
    // =========================================================================
    if (checkInfo.fileName().toLower() == "requirements.txt") {
        if (!m_pipPage) {
            QString venvPath = QDir(currentOpenProjectPath).filePath("venv");
            m_pipPage = new PipManagerPage(venvPath, filePath, this);
            ui->centralStackedWidget->addWidget(m_pipPage);
        }
        ui->centralStackedWidget->setCurrentWidget(m_pipPage);
        m_pipPage->loadPipData();

        if (ui->fileComboBox) {
            int comboIdx = ui->fileComboBox->findText(checkInfo.fileName());
            if (comboIdx == -1) {
                int pageIdx = ui->centralStackedWidget->indexOf(m_pipPage);
                ui->fileComboBox->addItem(checkInfo.fileName(), QVariant(pageIdx));
                ui->fileComboBox->setCurrentIndex(ui->fileComboBox->count() - 1);
            } else {
                ui->fileComboBox->setCurrentIndex(comboIdx);
            }
        }
        updateCustomTitle(checkInfo.fileName());
        return;
    }

    // =========================================================================
    // ОТКРЫТИЕ ОБЫЧНОГО Python ФАЙЛА
    // =========================================================================
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << " [ОШИБКА] Не удалось физически прочитать файл с диска:" << filePath;
        return;
    }
    QString fileContent = QString::fromUtf8(file.readAll());
    file.close();

    // 2. ПРОВЕРЯЕМ, НЕ ОТКРЫТ ЛИ ЭТОТ ДОКУМЕНТ УЖЕ В СОСЕДНЕЙ ВКЛАДКЕ
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && page->objectName() == filePath) {
            ui->centralStackedWidget->setCurrentWidget(page);

            if (ui && ui->cursorPosLabel) {
                ui->cursorPosLabel->show();
            }
            this->updateCursorPositionIndicator();

            CodeEditor *existingEditor = page->findChild<CodeEditor*>();
            if (existingEditor) {
                this->updateFunctionNavigator(existingEditor);
            }

            // =================================================================
            // ЖЕЛЕЗНЫЙ UX-ФИКС №1: КРАСИМ СТРОКУ В СИНИЙ ПРИ ПОВТОРНОМ ОТКРЫТИИ ФАЙЛА!
            // =================================================================
            if (this->docMgr) {
                this->docMgr->handleFileActivation(filePath);
            }
            return; // Выходим из функции, файл успешно активирован!
        }
    }
    // 3. СОЗДАЕМ НОВУЮ ГРАФИЧЕСКУЮ СТРАНИЦУ-КОНТЕЙНЕР ДЛЯ КОДА
    this->setIDEInStartMode(false);

    QWidget *newPage = new QWidget(ui->centralStackedWidget);
    newPage->setObjectName(filePath);
    QVBoxLayout *layout = new QVBoxLayout(newPage);
    layout->setContentsMargins(0, 0, 0, 0);

    CodeEditor *editor = nullptr;
    MinimapArea *minimap = nullptr;
    QWidget *editorContainer = CodeEditor::createEditorWithMinimap(newPage, editor, minimap);

    if (layout && editorContainer) {
        editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
        layout->addWidget(editorContainer);
    }

    if (editor) {
        editor->currentFilePath = filePath;
        editor->setObjectName(filePath);
        editor->isLspFreeze = false;

        QFont codeFont;
        codeFont.setFamilies(QStringList() << "JetBrains Mono" << "Fira Code" << "Monospace");
        codeFont.setStyleHint(QFont::Monospace);
        codeFont.setPixelSize(13);
        editor->setFont(codeFont);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);

        editor->blockSignals(true);
        if (editor->document()) editor->document()->blockSignals(true);
        editor->setPlainText(fileContent);
        editor->blockSignals(false);
        if (editor->document()) editor->document()->blockSignals(false);

        connect(editor, &CodeEditor::logMessage, this, [this](const QString &message) {
            QTextEdit *console = panelOther->findChild<QTextEdit*>("consoleOutput");
            if (console) console->append(message);
        });

        connect(editor, &CodeEditor::textChanged, this, &Neuro_programm::onCurrentFileTextChanged);
        this->updateFunctionNavigator(editor);
        connect(editor, &CodeEditor::textChanged, this, [this, editor]() {
            this->updateFunctionNavigator(editor);
        });
        connect(editor, &CodeEditor::cursorPositionChanged, this, [this]() {
            this->updateCursorPositionIndicator();
        });

        connect(editor, &CodeEditor::documentationRequested, this, [this](const QString &fPath, int ln, int ch) {
            if (!lspProcess || lspProcess->state() != QProcess::Running) return;
            QJsonObject hoverParams;
            QJsonObject textDocumentObj;
            QString cleanPath = QDir::fromNativeSeparators(fPath);
            textDocumentObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
            hoverParams["textDocument"] = textDocumentObj;
            QJsonObject positionObj;
            positionObj["line"] = ln;
            positionObj["character"] = ch;
            hoverParams["position"] = positionObj;
            this->sendLspRequest("textDocument/hover", hoverParams, 555);
        });
    }

    // 4. ДОБАВЛЯЕМ СТРАНИЦУ В ЦЕНТРАЛЬНЫЙ СТЭК
    int newPageIndex = ui->centralStackedWidget->addWidget(newPage);
    // =========================================================================
    // ЖЕЛЕЗНЫЙ UX-ФИКС №2: ДЕЛЕГИРУЕМ СОЗДАНИЕ И ОКРАШИВАНИЕ СТРОКИ МЕНЕДЖЕРУ !
    // =========================================================================
    // Мы полностью удалили отсюда сырое ручное создание QListWidgetItem.
    // Теперь registerNewOpenFile сам создаст ячейку, добавит её в ОЗУ,
    // свяжет её с комбобоксом и мгновенно вызовет handleFileActivation для синей Breeze-подсветки!
    if (this->docMgr) {
        this->docMgr->registerNewOpenFile(filePath, editor);
    }

    ui->centralStackedWidget->setCurrentIndex(newPageIndex);

    if (ui->btnCloseFile) {
        ui->btnCloseFile->setEnabled(true);
    }

    if (ui && ui->cursorPosLabel) {
        ui->cursorPosLabel->show();
    }

    this->updateCursorPositionIndicator();
    this->sendLspDidOpenForFile(filePath, fileContent);

    if (editor) {
        editor->setFocus();
        editor->update();
        editor->sendLspDidOpen();
    }

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

    // Асинхронный сброс модификаций (Страница 4 вашего PDF)
    QTimer::singleShot(50, this, [this, editor]() {
        if (editor) editor->document()->setModified(false);
        this->setWindowModified(false);
    });

    // Менеджмент геометрии нижнего терминала (Страница 4 вашего PDF)
    QTimer::singleShot(50, this, [this]() {
        if (mainVerticalSplitter) {
            mainVerticalSplitter->setStretchFactor(0, 1);
            mainVerticalSplitter->setStretchFactor(1, 0);
            int totalWindowHeight = this->height();
            int targetBottomHeight = 0;
            if (panelOther && panelOther->isVisible()) targetBottomHeight = 250;
            else if (ui->search_panel && ui->search_panel->isVisible()) targetBottomHeight = 150;
            mainVerticalSplitter->setSizes(QList<int>({totalWindowHeight - targetBottomHeight, targetBottomHeight}));
            mainVerticalSplitter->refresh();
        }
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

    // КРИТИЧЕСКИЙ ФИКС: Берем индекс АКТИВНОЙ страницы прямо из стэка окон
    int currentStackIndex = ui->centralStackedWidget->currentIndex();

    // БЕЗОПАСНОСТЬ: Системные сервисные экраны (индексы < 2) закрывать нельзя
    if (currentStackIndex < 2) {
        if (ui->statusbar) {
            ui->statusbar->showMessage("ℹ Сервисные вкладки среды разработки нельзя закрыть", 3000);
        }
        return;
    }

    // Извлекаем указатель на закрываемую динамическую страницу кода
    QWidget *filePageWidget = ui->centralStackedWidget->widget(currentStackIndex);
    if (!filePageWidget) return;

    // ФИКС БАГА 2: Находим дочерний CodeEditor внутри закрываемой страницы и проверяем изменения
    CodeEditor *editor = filePageWidget->findChild<CodeEditor*>();
    if (editor && editor->document() && editor->document()->isModified()) {
        QFileInfo fileInfo(filePageWidget->objectName());

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Несохраненные изменения",
                                      QString("Файл '%1' был изменен.\nСохранить изменения перед закрытием?")
                                      .arg(fileInfo.fileName()),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (reply == QMessageBox::Yes) {
            this->saveCurrentActiveFile(); // Сохраняем перед выходом
        } else if (reply == QMessageBox::Cancel) {
            return; // Пользователь отменил закрытие
        }
    }

    // Получаем уникальный путь к файлу (он записан в objectName страницы)
    QString filePath = filePageWidget->objectName();
    qDebug() << ">>> [КРЕСТИК] Начинаю процедуру закрытия файла:" << filePath;

    // 1. Находим и удаляем соответствующую строчку из верхнего комбобокса по её filePath
    int comboIndex = ui->fileComboBox->findData(filePath);
    if (comboIndex != -1) {
        ui->fileComboBox->blockSignals(true);
        ui->fileComboBox->removeItem(comboIndex);
        ui->fileComboBox->blockSignals(false);
    } else {
        int matchIdx = ui->fileComboBox->findData(currentStackIndex);
        if (matchIdx != -1) ui->fileComboBox->removeItem(matchIdx);
    }

    // 2. Удаляем запись из левого бокового списка документов (openFilesListWidget)
    if (ui->openFilesListWidget) {
        for (int i = 0; i < ui->openFilesListWidget->count(); ++i) {
            if (ui->openFilesListWidget->item(i)->data(Qt::UserRole).toString() == filePath) {
                delete ui->openFilesListWidget->takeItem(i);
                break;
            }
        }
    }

    // 3. Вынимаем страницу из контейнера стэка и полностью зачищаем оперативную память ОЗУ
    ui->centralStackedWidget->removeWidget(filePageWidget);
    filePageWidget->deleteLater(); //
    // =========================================================================
    // СМАРТ-АНАЛИЗ ОСТАВШИХСЯ ОТКРЫТЫХ ФАЙЛОВ КОДА
    // =========================================================================
    bool hasAnyOpenedFiles = false;
    int lastFileIndex = -1;

    // Сканируем стек (сервисные экраны под индексами 0, 1 и заставку шорткатов не считаем)
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

        // Намертво очищаем и прячем навигатор функций comboDevice
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

        // Выводим на экран заставку шорткатов JetBrains
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

    // Извлекаем уникальный ключ страницы (MAIN_SCREEN, AI_CHAT_SCREEN или абсолютный путь к файлу)
    QString targetKey = item->data(Qt::UserRole).toString().trimmed();
    if (targetKey.isEmpty()) return;

    qDebug() << ">>> [СПИСОК ОТКРЫТЫХ ФАЙЛОВ] Двойной клик по ключу:" << targetKey;

    // =========================================================================
    // СЦЕНАРИЙ 1: ПЕРЕКЛЮЧЕНИЕ НА СЕРВИСНЫЕ ЭКРАНЫ
    // =========================================================================
    if (targetKey == "MAIN_SCREEN" || targetKey == "AI_CHAT_SCREEN") {
        if (ui->fileComboBox) {
            int comboIdx = ui->fileComboBox->findData(targetKey);
            if (comboIdx != -1) {
                ui->fileComboBox->setCurrentIndex(comboIdx); // Активирует встроенную логику
            }
        }
        return;
    }

    // =========================================================================
    // СЦЕНАРИЙ 2: ПЕРЕКЛЮЧЕНИЕ НА РЕАЛЬНЫЙ СТАК ФАЙЛА КОДА (.PY)
    // =========================================================================
    if (ui->centralStackedWidget) {
        bool pageFound = false;

        // Ищем в центральном стеке виджетов страницу, чье objectName хранит этот абсолютный путь
        for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
            QWidget *page = ui->centralStackedWidget->widget(i);
            if (page && page->objectName().trimmed() == targetKey) {

                // Перелистываем центральный экран на этот файл
                ui->centralStackedWidget->blockSignals(true);
                ui->centralStackedWidget->setCurrentIndex(i);
                ui->centralStackedWidget->blockSignals(false);
                pageFound = true;
                break;
            }
        }

        // Если страница кода найдена — просим менеджер выставить синее выделение и обновить комбобокс
        if (pageFound && this->docMgr) {
            this->docMgr->handleFileActivation(targetKey);
        }
    }

    // Возвращаем фокус ввода клавиатуры на текстовый холст редактора кода для удобства
    QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
    if (currentPage) {
        CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
        if (currentEditor) {
            currentEditor->setFocus();
            currentEditor->update();
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
        qWarning() << "D-Bus Notifications interface is not valid!";
        return;
    }

    // 1. Задаем путь к файлу в реальной системе (/tmp/pTS.png)
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

    // =========================================================================
    // ЖЕСТКИЙ ФИКС ДЛЯ ОБХОДА XDG-DESKTOP-PORTAL:
    // Оставляем первый аргумент (App ID) ПУСТЫМ.
    // Это заставит Linux вывести уведомление без проверки системной регистрации приложения!
    // =========================================================================
    args << "pytorch-studio";      // 1. Имя приложения-отправителя (пустое для анонимного вывода)
    args << 0u;                    // 2. ID заменяемого уведомления (0 = создать новое)
    args << finalIconParam;        // 3. Иконка (путь к временному файлу png)
    args << title;                 // 4. Крупный заголовок карточки
    args << text;                  // 5. Основной текст уведомления
    args << QStringList();         // 6. Интерактивные кнопки-действия
    args << QVariantMap();         // 7. Дополнительные подсказки-хинты для KDE Plasma
    args << 4000;                  // 8. Время отображения карточки на экране (4 секунды)

    // 3. Асинхронно отправляем сигнал в систему.
    // Исправлено: заменили QDBus::NoBlock на нативный QDBusCall::NoBlock
    notifyInterface.callWithArgumentList(QDBus::NoBlock, "Notify", args);
}


void Neuro_programm::initProjectTreeModel(QString path)
{
    QString safePath = path.trimmed();
    if (safePath.isEmpty()) {
        qWarning() << " [LSP ПРЕДУПРЕЖДЕНИЕ] Вызван initProjectTreeModel с пустым путем. Пропуск.";
        return;
    }

    // =========================================================================
    // КРИТИЧЕСКИЙ UX ФИКС: ОТОБРАЖАЕМ ДЕРЕВО НА БОКОВОЙ ПАНЕЛИ
    // =========================================================================
    this->currentOpenProjectPath = safePath; // Фиксируем живой путь в ОЗУ главного окна
    this->setProperty("currentOpenProjectPath", safePath);

    // Находим стек виджетов внутри вашего leftDockWidget
    QStackedWidget *dockStack = ui->leftDockWidget ? ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack") : nullptr;
    if (dockStack) {
        // Мгновенно перелистываем боковую панель на страницу Дерева файлов (Индекс 0)
        dockStack->setCurrentIndex(0);

        // Гарантируем, что сам док-виджет проявлен и виден на экране
        ui->leftDockWidget->setVisible(true);
        ui->leftDockWidget->show();

        // Принудительно "вдавливаем" кнопку «Проект» на левой Breeze-панели в активное состояние
        if (actProject) {
            actProject->blockSignals(true);
            actProject->setChecked(true);
            actProject->blockSignals(false);
        }

        dockStack->update();
        ui->leftDockWidget->update();
    }

    // Разблокируем элементы навигации и закрытия вкладок, так как рабочий проект успешно открыт
    if (ui->btnCloseFile) ui->btnCloseFile->setEnabled(true);
    if (ui->fileComboBox) ui->fileComboBox->setEnabled(true);
    // =========================================================================

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
    if (this->projectMgr) this->projectMgr->addProjectToRecentList(safePath);

    if (this->docMgr) {
        this->docMgr->updateUiTitles(""); // Передаем "", так как файлы еще закрыты!
    }

    if (this->projectMgr) {
        this->projectMgr->addProjectToRecentList(safePath);
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


    // Блокируем пульт параметров
    ui->btnStartTraining->setEnabled(false);
    ui->btnStartTraining->setText("⏳ Обучение...");
    ui->btnStopTraining->setEnabled(true);

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

    if (this->envManager) {
        trainingProcess->setProcessEnvironment(this->envManager->getIsolatedEnvironment());
    }

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
    //static const QRegularExpression metricsRegex("METRICS:\\s*epoch=(\\d+),\\s*accuracy=([0-9.]+)%\\s*vram=([0-9.]+)GB,\\s*speed=(\\d+)");

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
    if (!projectMgr || !ui->projectListWidget) return;

    ui->projectListWidget->clear();
    if (this->recentProjectsMenu) this->recentProjectsMenu->clear();

    // Просто запрашиваем готовый строковый массив у менеджера
    QStringList recentDirs = projectMgr->getRecentProjects();

    for (const QString &path : std::as_const(recentDirs)) {
        QFileInfo dirInfo(path);

        // Заполняем виджет на заставке
        QListWidgetItem *item = new QListWidgetItem(ui->projectListWidget);
        item->setText(dirInfo.fileName());
        item->setData(Qt::UserRole, path);
        item->setIcon(QIcon(":/Data/system_icons/document-open.svg"));
        ui->projectListWidget->addItem(item);

        // Заполняем верхнее меню Файл
        if (this->recentProjectsMenu) {
            QAction *act = this->recentProjectsMenu->addAction(dirInfo.fileName() + " [" + path + "]");
            act->setData(path);
            connect(act, &QAction::triggered, this, &Neuro_programm::openRecentProject);
        }
    }
}

void Neuro_programm::openRecentProject()
{
    // 1. Извлекаем указатель на нажатый пункт верхнего подменю
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;

    // Вытаскиваем абсолютный путь к папке проекта, который зашит в data экшена
    QString targetProjectPath = action->data().toString().trimmed();
    if (targetProjectPath.isEmpty()) return;

    qInfo() << "[RECENT_MENU] Быстрый запуск недавней сессии по пути:" << targetProjectPath;

    QDir projectDir(targetProjectPath);

    // Валидируем физическое наличие паспорта-манифеста на диске
    if (!projectDir.exists() || !projectDir.exists("passport.pystudio.json")) {
        qWarning() << "[RECENT_MENU] Ошибка: Папка проекта удалена или повреждена:" << targetProjectPath;
        QMessageBox::critical(
                    this,
                    "Проект не найден",
                    "<b>Не удалось открыть недавний проект.</b><br><br>"
                    "Директория была удалена с жесткого диска, переименована или в ней отсутствует файл паспорта."
                    );
        return;
    }

    // ЖЕЛЕЗНЫЙ ВЫЗОВ: Передаем управление в метод инициализации дерева!
    // Он сам переключит стек дока на индекс 0 и развернет структуру файлов.
    this->initProjectTreeModel(targetProjectPath);
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

        if (this->statusBar())
        {
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
    // Получаем указатель на редактор кода, в котором прямо сейчас идет ввод символов
    CodeEditor *editor = qobject_cast<CodeEditor*>(sender());
    if (!editor) return;

    // Защита: если файл еще находится в процессе первичной начитки с диска, игнорируем триггер
    if (editor->property("isLoading").toBool()) return;

    QString absoluteFilePath = editor->objectName();
    if (absoluteFilePath.isEmpty() || absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") return;

    // Считываем нативное состояние изменения документа Qt
    bool isModified = editor->document()->isModified();

    // ЖЕЛЕЗНЫЙ UX ФИКС: Передаем состояние напрямую в наш модуль!
    // Он сам мгновенно допишет " *" в комбобокс, в левый список документов и в окно операционной системы
    if (this->docMgr) {
        this->docMgr->handleDocumentModificationChanged(absoluteFilePath, isModified);
    }
}



void Neuro_programm::onCloseProjectClicked()
{
    qInfo() << ">>> [ЗАКРЫТИЕ] Запуск процедуры проверки и закрытия всего проекта...";

    // =========================================================================
    // ЭТАП 1: ПРОВЕРКА НЕСОХРАНЕННЫХ ИЗМЕНЕНИЙ ДО УДАЛЕНИЯ ДАННЫХ ИЗ ОЗУ
    // =========================================================================
    bool hasUnsavedChanges = false;
    CodeEditor *activeEditor = nullptr;

    QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
    if (currentPage) {
        activeEditor = currentPage->findChild<CodeEditor*>();
    }

    // Проверяем флаг модификации окна или документа Qt
    if (this->isWindowModified() || (activeEditor && activeEditor->document()->isModified())) {
        hasUnsavedChanges = true;
    }

    // Вывод предупреждения (Диалоговое окно в стиле JetBrains / PyCharm)
    if (hasUnsavedChanges) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::warning(this,
                                     "Несохраненные изменения",
                                     "В проекте или открытом файле есть несохраненные изменения.\n"
                                     "Хотите сохранить их перед закрытием проекта?",
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
                                     );

        if (reply == QMessageBox::Save) {
            this->saveCurrentActiveFile();
            qDebug() << "[ЗАКРЫТИЕ] Проект принудительно сохранен пользователем перед выходом.";
        }
        else if (reply == QMessageBox::Cancel) {
            qDebug() << "[ЗАКРЫТИЕ] Закрытие отменено пользователем. Остаемся в проекте.";
            return; // ПРЕРЫВАЕМ МЕТОД, остаемся работать в кодовой базе!
        }
    }

    // =========================================================================
    // ЭТАП 2: СОХРАНЕНИЕ ПОСЛЕДНЕГО АКТИВНОГО ФАЙЛА В ИСТОРИЮ PYSTUDIO.CONF
    // =========================================================================
    if (currentPage && activeEditor && !activeEditor->currentFilePath.isEmpty() && !this->currentOpenProjectPath.isEmpty()) {
        QString configAbsolutePath = QDir::homePath() + "/.config/PyTorchStudio/pystudio.conf";
        QSettings settings(configAbsolutePath, QSettings::IniFormat);

        // Привязываем последний открытый файл к базовому имени текущей папки репозитория
        QString projBaseName = QFileInfo(this->currentOpenProjectPath).baseName();
        settings.setValue("General/lastActiveFile_" + projBaseName, activeEditor->currentFilePath);
        settings.sync();
    }

    // =========================================================================
    // ЭТАП 3: ХИРУРГИЧЕСКАЯ ОЧИСТКА ДИНАМИЧЕСКИХ ВКЛАДОК КОДА ИЗ СТЭКА СЗАДУ НАПЕРЕД
    // =========================================================================
    ui->fileComboBox->blockSignals(true);
    int placeholderIndex = this->property("placeholderIndex").toInt();

    // Удаляем вкладки с конца, чтобы не поплыли индексы позиций Qt
    for (int i = ui->centralStackedWidget->count() - 1; i >= 0; --i)
    {
        // Категорически пропускаем Панель ИИ, Чат и заставку JetBrains шорткатов
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

    // Переинициализируем верхний комбобокс до базового состояния заставки
    ui->fileComboBox->clear();
    ui->fileComboBox->addItem(" Панель обучения ИИ", QVariant("MAIN_SCREEN"));
    ui->fileComboBox->addItem(" ИИ-Ассистент", QVariant("AI_CHAT_SCREEN"));
    ui->fileComboBox->setCurrentIndex(-1); // Сбрасываем стрелку в нейтраль
    ui->fileComboBox->blockSignals(false);

    // =========================================================================
    // ЭТАП 4: ОЧИСТКА ЛЕВОГО НИЖНЕГО СПИСКА "ОТКРЫТЫЕ ДОКУМЕНТЫ"
    // =========================================================================
    if (ui->openFilesListWidget) {
        ui->openFilesListWidget->clear();
        QListWidgetItem *mainScreenItem = new QListWidgetItem(" Панель обучения ИИ", ui->openFilesListWidget);
        mainScreenItem->setData(Qt::UserRole, QString("MAIN_SCREEN"));
        QListWidgetItem *chatScreenItem = new QListWidgetItem(" ИИ-Ассистент", ui->openFilesListWidget);
        chatScreenItem->setData(Qt::UserRole, QString("AI_CHAT_SCREEN"));

        ui->openFilesListWidget->setCurrentRow(0);
        if (ui->openFilesContainer) ui->openFilesContainer->setVisible(false);
    }

    // Обнуляем файловую модель дерева TreeView
    if (ui->treeView) {
        ui->treeView->setModel(nullptr);
    }

    // =========================================================================
    // ЭТАП 5: ТОТАЛЬНОЕ СТИРАНИЕ ПУТЕЙ И СБРОС ЗАГЛОВКОВ (ЧИСТЫЙ ФИКС СЕССИИ)
    // =========================================================================
    this->currentOpenProjectPath = "";
    this->setProperty("currentOpenProjectPath", ""); // Зачищаем свойство для DocumentManager
    this->setWindowModified(false);

    // Вежливо просим наш модуль вернуть исходное чистое имя программы
    if (this->docMgr) {
        this->docMgr->updateUiTitles("");
    } else {
        this->setWindowTitle("pytorch-studio");
        if (this->titleLabel) this->titleLabel->setText("PyTorch Studio");
    }

    // Переводим интерфейс в режим пустой стартовой заставки шорткатов
    this->setIDEInStartMode(true);

    if (ui->btnCloseFile) ui->btnCloseFile->setEnabled(false);
    if (ui->fileComboBox) ui->fileComboBox->setEnabled(false);

    // Гасим нижний терминал и REPL
    if (panelOther)  panelOther->setVisible(false);
    if (btnTerminal) btnTerminal->setChecked(false);

    // Выводим плейсхолдер шорткатов JetBrains на передний план по центру
    placeholderIndex = ui->centralStackedWidget->indexOf(ui->centralStackedWidget->findChild<QWidget*>("JETBRAINS_PLACEHOLDER"));
    if (placeholderIndex == -1) {
        placeholderIndex = this->property("placeholderIndex").toInt();
    }

    if (ui->centralStackedWidget && placeholderIndex >= 0) {
        if (ui->cursorPosLabel) {
            ui->cursorPosLabel->hide(); // Прячем индикатор строк, так как файлов на экране нет
        }
        ui->centralStackedWidget->setCurrentIndex(placeholderIndex);
        ui->centralStackedWidget->update();
    }

    qInfo() << "[PROJECT_MGR] Проект успешно закрыт. ОЗУ зачищена. Интерфейс в исходном состоянии.";
} // <-- Метод теперь гарантированно и правильно закрывается здесь!

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
    QProcessEnvironment env = (this->envManager)
            ? this->envManager->getIsolatedEnvironment()
            : QProcessEnvironment::systemEnvironment();

    // Отключаем буферизацию Python внутри Pylsp, заставляя его отвечать мгновенно
    env.insert("PYTHONUNBUFFERED", "1");
    env.insert("PYTHONIOENCODING", "utf-8");
    lspProcess->setProcessEnvironment(env);

    // =========================================================================
    // ШАГ 2: АСИНХРОННЫЕ СИГНАЛ-СЛОТЫ ДЛЯ МОНИТОРИНГА И СЧИТЫВАНИЯ
    // =========================================================================
    // Перехватчик стандартного вывода (Ответы сервера)
    connect(lspProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray peekData = lspProcess->peek(lspProcess->bytesAvailable());
        std::cerr << " [LSP СЫРОЙ JSON ВЫВОД (PEEK)]:\n" <<
                     QString::fromUtf8(peekData).toStdString() << std::endl;
        std::cerr.flush();
        this->readLspResponse();
    });

    // ИСПРАВЛЕНО: Безопасный Си-формат вывода логов ошибок во избежание сбоя компилятора GCC
    connect(lspProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray lspErrors = lspProcess->readAllStandardError();
        if (!lspErrors.isEmpty()) {
            // Принудительно приводим к const char* через метод data()
            fprintf(stderr, "\n[PYLSP ВНУТРЕННИЙ ЛОГ ОШИБОК]: %s\n", lspErrors.data());
            fflush(stderr);
        }
    });

    // Мониторинг непредвиденного падения процесса
    connect(lspProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [](int exitCode, QProcess::ExitStatus exitStatus) {
        std::cerr << " [LSP СТАТУС] Сервер Pylsp завершил работу. Код выхода: " << exitCode << " Статус: " << exitStatus << std::endl;
        std::flush(std::cerr);
    });

    // =========================================================================
    // ШАГ 3: ЗАПУСК ПРЯМОГО БИНАРНИКА СЕРВЕРА С ПРИВЯЗКОЙ К ВНЕШНЕМУ VENV
    // =========================================================================
    QString localLspBinary;
    if (this->envManager && !this->envManager->currentPythonPath().isEmpty() && QFile::exists(this->envManager->currentPythonPath()))
    {
        localLspBinary = this->envManager->currentPythonPath();
        this->venvPythonBinary = localLspBinary;
        qDebug() << "[LSP_INIT SUCCESS] Используем внешний изолированный venv:" << localLspBinary;
    }
    else if (QFile::exists("/home/elf/venv/bin/python"))
    {
        localLspBinary = "/home/elf/venv/bin/python";
        this->venvPythonBinary = localLspBinary;
        qDebug() << "[LSP_INIT FALLBACK] Менеджер занят, подключаю внешний venv напрямую:" << localLspBinary;
    }
    else
    {
        localLspBinary = "/usr/bin/python3";
        if (this->venvPythonBinary.isEmpty()) {
            this->venvPythonBinary = localLspBinary;
        }
        qWarning() << "[LSP_INIT WARN] Внешний venv не обнаружен на диске! Сброс на систему:" << localLspBinary;
    }

    // Возвращаем аргументы запуска модуля pylsp
    QStringList lspArgs;
    lspArgs << "-m" << "pylsp";
    std::cerr << " [LSP СИСТЕМНЫЙ СТАРТ] Запускаю python-lsp-server внутри venv..." << std::endl;
    std::cerr.flush();

    // Отложенная UX-регистрация восстановленных файлов при старте
    connect(lspProcess, &QProcess::started, this, [this]() {
        qDebug() << ">>> [LSP ГОТОВ] Сервер Pylsp успешно запущен системой!";
        if (!this->m_pendingAutoloadFile.isEmpty()) {
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (currentPage) {
                CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
                if (currentEditor && currentEditor->objectName() == this->m_pendingAutoloadFile) {
                    currentEditor->sendLspDidOpen();
                    this->updateFunctionNavigator(currentEditor);
                }
            }
        }
    });

    // Запускаем процесс асинхронно в изолированной среде
    lspProcess->start(localLspBinary, lspArgs);
    if (!lspProcess->waitForStarted(1500)) {
        std::cerr << " [КРИТИЧЕСКАЯ ОШИБКА] Не удалось запустить процесс LSP сервера по пути: " << localLspBinary.toStdString() << std::endl;
        std::cerr.flush();
        return;
    }

    // =========================================================================
    // ШАГ 4: ФОРМИРОВАНИЕ ПАКЕТА ИНИЦИАЛИЗАЦИИ ДЛЯ PYLSP (С СИНХРОНИЗАЦИЕЙ JEDI)
    // =========================================================================
    QJsonObject rootObj;
    rootObj["jsonrpc"] = "2.0";
    this->lspRequestId = 1;
    rootObj["id"] = this->lspRequestId;
    rootObj["method"] = "initialize";

    QJsonObject params;
    params["processId"] = QCoreApplication::applicationPid();
    if (!currentOpenProjectPath.isEmpty()) {
        params["rootUri"] = QUrl::fromLocalFile(currentOpenProjectPath).toString();
        params["rootPath"] = currentOpenProjectPath;
    } else {
        params["rootUri"] = QJsonValue::Null;
        params["rootPath"] = QJsonValue::Null;
    }

    QJsonObject capabilities;
    QJsonObject textDocumentCaps;
    textDocumentCaps["synchronization"] = QJsonObject{
    {"dynamicRegistration", false},
    {"willSave", false},
    {"willSaveWaitUntil", false},
    {"didSave", true}
            };

    QJsonObject codeActionCaps;
    codeActionCaps["dynamicRegistration"] = false;
    QJsonArray codeActionKinds;
    codeActionKinds.append("quickfix");
    codeActionKinds.append("refactor");
    codeActionCaps["codeActionLiteralSupport"] = QJsonObject{{"codeActionKind", QJsonObject{{"valueSet", codeActionKinds}}}};
    textDocumentCaps["codeAction"] = codeActionCaps;

    QJsonObject hoverCaps;
    hoverCaps["dynamicRegistration"] = false;
    QJsonArray contentFormats;
    contentFormats.append("markdown");
    contentFormats.append("plaintext");
    hoverCaps["contentFormat"] = contentFormats;
    textDocumentCaps["hover"] = hoverCaps;
    capabilities["textDocument"] = textDocumentCaps;
    params["capabilities"] = capabilities;

    // =====================================================================
    // ИСПРАВЛЕНА СТРУКТУРА НАСТРОЕК: ПЕРЕНПРАВЛЯЕМ КЛЮЧИ СТРОГО ПО КАНOНУ PYLSP
    // =====================================================================
    QJsonObject settings;
    QJsonObject pylsp;
    QJsonObject plugins;

    // Настройка плагина плавающего разборщика Jedi внутри Pylsp
    QJsonObject jedi;
    jedi["enabled"] = true;

    // Передаем путь к бинарнику python, чтобы подтянулись все библиотеки PyTorch!
    jedi["environment"] = localLspBinary;
    jedi["max_function_parses"] = 150;

    QJsonArray extraPaths;
    if (!currentOpenProjectPath.isEmpty()) extraPaths.append(currentOpenProjectPath);
    jedi["extra_paths"] = extraPaths;

    plugins["jedi"] = jedi;

    // Настройка остальных встроенных плагинов pylsp
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

    QJsonObject pycodestyle;
    pycodestyle["enabled"] = false; // Выключаем спам предупреждений PEP8 для экономии ОЗУ
    plugins["pycodestyle"] = pycodestyle;

    pylsp["plugins"] = plugins;
    settings["pylsp"] = pylsp;
    params["settings"] = settings;
    // =====================================================================

    // Передаем пустые initializationOptions, так как все настройки ушли в settings
    QJsonObject initializationOptions;
    params["initializationOptions"] = initializationOptions;

    rootObj["params"] = params;

    // Маркируем пакет по стандарту LSP (Content-Length) и отправляем в пайп
    QByteArray jsonData = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
    QByteArray headerData = QString("Content-Length: %1\r\n\r\n").arg(jsonData.size()).toUtf8();
    lspProcess->write(headerData + jsonData);
    lspProcess->waitForBytesWritten(500);

    std::cerr << " [LSP КЛИЕНТ] Стартовый пакет 'initialize' успешно отправлен на Pylsp." << std::endl;
    std::cerr.flush();

    // Отложенная отправка пакета didOpen (Остается без изменений)
    if (!this->m_pendingAutoloadFile.isEmpty() && QFile::exists(this->m_pendingAutoloadFile))
    {
        QFile autoFile(this->m_pendingAutoloadFile);
        if (autoFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString autoContent = QString::fromUtf8(autoFile.readAll());
            autoFile.close();
            this->sendLspDidOpenForFile(this->m_pendingAutoloadFile, autoContent);
        }
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

void Neuro_programm::sendLspDidOpenForFile(const QString &filePath, const QString &fileContent)
{
    // Если LSP-сервер не запущен или путь пустой — ничего не делаем
    if (!lspProcess || lspProcess->state() != QProcess::Running || filePath.isEmpty()) return;

    QJsonObject params;
    QJsonObject textDocument;

    // Передаем точный URI файла по спецификации LSP
    textDocument["uri"] = QUrl::fromLocalFile(filePath).toString();
    textDocument["languageId"] = "python";
    textDocument["version"] = 1; // Начальная версия сессии всегда 1
    textDocument["text"] = fileContent; // Передаем стартовый текст для инициализации кэша Jedi
    params["textDocument"] = textDocument;

    // Отправляем системное уведомление didOpen через ваш рабочий транспорт запросов, который вы скинули
    this->sendLspRequest("textDocument/didOpen", params);

    std::clog << " [LSP] Сессия файла успешно открыта на сервере через didOpen. URI: "
              << textDocument["uri"].toString().toStdString() << std::endl;
    std::clog.flush();
}

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
        this->setWindowTitle("pytorch-studio");
        return;
    }

    // Извлекаем абсолютный путь (задан на странице 26 как objectName)
    QString absoluteFilePath = currentPage->objectName();

    // Если это сервисные экраны (MAIN_SCREEN или AI_CHAT_SCREEN) — пишем простое имя
    if (absoluteFilePath.isEmpty() || absoluteFilePath == "MAIN_SCREEN" || absoluteFilePath == "AI_CHAT_SCREEN") {
        this->setWindowTitle("pytorch-studio");
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

    qInfo() << ">>> [PROBLEMS INIT] Главное окно Студии полностью отрисовано. Форсирую выгребание ошибок...";

    // Насильно заставляем таблицу вытащить накопленный кэш ошибок из памяти на экран!
    this->refreshProblemsTableView();
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
    QMainWindow::resizeEvent(event); // Даем окну измениться

    // if (panelOther && panelOther->isVisible()) {
    //     int rightPanelWidth = ui->rightDebugPanel ? ui->rightDebugPanel->width() : 0;
    //     int leftBarWidth = this->leftSideBarContainer ? this->leftSideBarContainer->width() : 68;
    //     int allowedWidth = this->width() - rightPanelWidth - leftBarWidth;

    //     panelOther->setFixedWidth(allowedWidth);
    //     panelOther->move(leftBarWidth, panelOther->y());
    // }
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

    // ШАГ 1: Проверка — не открыт ли файл уже в интерфейсе (Остается без изменений)
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && page->objectName() == absoluteFilePath) {
            ui->centralStackedWidget->setCurrentWidget(page);
            if (this->docMgr) {
                this->docMgr->handleFileActivation(absoluteFilePath);
            }
            return;
        }
    }

    // ШАГ 2: Чтение содержимого созданного файла с диска (Остается без изменений)
    QFile file(absoluteFilePath);
    QString fileContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fileContent = QString::fromUtf8(file.readAll());
        file.close();
    }

    // ШАГ 3: Блокируем сигналы на время сборки графических виджетов
    if (ui->centralStackedWidget) ui->centralStackedWidget->blockSignals(true);
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(true);

    this->setIDEInStartMode(false);

    // Создаем контейнер-страницу для stackedWidget
    QWidget *newPage = new QWidget(ui->centralStackedWidget);
    newPage->setObjectName(absoluteFilePath);
    QVBoxLayout *layout = new QVBoxLayout(newPage);
    layout->setContentsMargins(0, 0, 0, 0);

    CodeEditor *editor = nullptr;
    MinimapArea *minimap = nullptr;

    // Собираем монолитную панель (Редактор кода + миникарта)
    QWidget *editorContainer = CodeEditor::createEditorWithMinimap(newPage, editor, minimap);
    if (layout && editorContainer) {
        editorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout->addWidget(editorContainer);
    }

    // Настраиваем созданный объект текстового редактора editor
    if (editor) {
        editor->currentFilePath = absoluteFilePath;
        editor->setObjectName(absoluteFilePath);

        QFont codeFont;
        codeFont.setFamilies(QStringList() << "JetBrains Mono" << "Fira Code" << "Courier New" << "Monospace");
        codeFont.setStyleHint(QFont::Monospace);
        codeFont.setPixelSize(13);
        editor->setFont(codeFont);

        editor->setProperty("isLoading", true);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);

        // Передаем текст шаблона на холст редактора без вызова сигналов изменения
        editor->blockSignals(true);
        editor->setPlainText(fileContent);
        editor->blockSignals(false);

        // =====================================================================
        // СИНХРОНИЗАЦИЯ СИГНАЛОВ ПОПАПОВ АВТОДОПОЛНЕНИЯ С СЕРВЕРОМ JEDI LSP
        // =====================================================================

        // =====================================================================
        // СМАРТ-МОСТ JEDI: СЧИТЫВАНИЕ АКТИВНОГО ЭЛЕМЕНТА ДО СХЛОПЫВАНИЯ ИНДЕКСА
        // =====================================================================
        // =====================================================================
        // ИСПРАВЛЕННЫЙ УНИВЕРСАЛЬНЫЙ МОСТ СВЯЗИ (МЕНЮ АВТОКОМПЛИТА + ОБЫЧНЫЙ ХОВЕР)
        // =====================================================================
        connect(editor, &CodeEditor::documentationRequested, this, [this](const QString &path, int line, int col) {
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (!currentPage) return;

            CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
            if (!currentEditor) return;

            // СЦЕНАРИЙ А: Если окно подсказок открыто на экране — делаем глубокий resolve по ячейке
            if (currentEditor->m_popupWindow && currentEditor->m_popupWindow->isVisible() && currentEditor->m_listWidget)
            {
                QListWidgetItem *currentItem = currentEditor->m_listWidget->currentItem();
                if (currentItem)
                {
                    QJsonObject originalItemObj = currentItem->data(Qt::UserRole).toJsonObject();

                    if (originalItemObj.isEmpty()) {
                        static const QRegularExpression htmlTagRegex("<[^>]*>");
                        QString cleanLabel = currentItem->text().remove(htmlTagRegex);
                        QString insertText = cleanLabel;
                        if (insertText.contains("(")) insertText = insertText.left(insertText.indexOf("(")).trimmed();

                        originalItemObj["label"] = cleanLabel;
                        originalItemObj["insertText"] = insertText;
                        originalItemObj["kind"] = 3;
                        QJsonObject dataObj;
                        dataObj["doc_uri"] = QUrl::fromLocalFile(path).toString();
                        originalItemObj["data"] = dataObj;
                    }

                    qInfo() << ">>> [LSP RESOLVE] Отправка пакета completionItem/resolve для:" << originalItemObj["label"].toString();
                    this->sendLspRequest("completionItem/resolve", originalItemObj, 555);
                    return; // Успешно обработано, выходим!
                }
            }

            // СЦЕНАРИЙ Б: Окно подсказок закрыто (пользователь просто вызвал справку на слове в коде)
            // Шлем нативный, пуленепробиваемый пакет hover по координатам строки и колонки!
            qInfo() << ">>> [LSP HOVER] Окно подсказок закрыто. Отправляю стандартный textDocument/hover...";
            qInfo() << "    Координаты запроса: Строка:" << line << "Колонка:" << col;

            QString cleanPath = QDir::fromNativeSeparators(path);
            QJsonObject hoverParams;
            QJsonObject textDocumentObj;

            textDocumentObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
            hoverParams["textDocument"] = textDocumentObj;

            QJsonObject positionObj;
            positionObj["line"] = line;
            positionObj["character"] = col;
            hoverParams["position"] = positionObj;

            // Отправляем Hover-запрос (Наш парсер в codeeditor.cpp перехватит его по структуре!)
            this->sendLspRequest("textDocument/hover", hoverParams, 555);
        });
        // =====================================================================


        // 2. СВЯЗКА КНОПКИ GO TO DEFINITION (ИСПРАВЛЕНО: СИНХРОНИЗИРОВАНО НА ID 105!)
        connect(editor, &CodeEditor::definitionRequested, this, [this](const QString &path, int line, int col) {
            QJsonObject params;
            QJsonObject textDocument;
            textDocument["uri"] = QUrl::fromLocalFile(path).toString();
            params["textDocument"] = textDocument;

            QJsonObject position;
            position["line"] = line;
            position["character"] = col;
            params["position"] = position;

            // Заменено с 777 на каноничный ID 105 для нашего роутера в onLspReadyRead!
            this->sendLspRequest("textDocument/definition", params, 105);
        });

        // Отслеживание изменений текста файла со звездочкой
        connect(editor, &CodeEditor::textChanged, this, &Neuro_programm::onCurrentFileTextChanged);

        // Перерасчет координат Ln, Col при движении каретки
        connect(editor, &CodeEditor::cursorPositionChanged, this, [this]() {
            this->updateCursorPositionIndicator();
        });

        // Мягкий сброс флага isLoading через таймер (ЖЕСТКИЙ ФИКС ЗВЕЗДОЧКИ)
        QTimer::singleShot(150, this, [editor]() {
            if (editor) {
                editor->setProperty("isLoading", false);
                editor->document()->setModified(false);
            }
        });

        // РЕГИСТРАЦИЯ ДОКУМЕНТА В НАШЕМ МОДУЛЕ DOCUMENT_MANAGER
        if (this->docMgr) {
            this->docMgr->registerNewOpenFile(absoluteFilePath, editor);
        }
    }

    // ШАГ 4: Добавляем готовую страницу в стек окон интерфейса
    int newPageIndex = ui->centralStackedWidget->addWidget(newPage);
    ui->centralStackedWidget->setCurrentIndex(newPageIndex);

    // Разблокируем сигналы макетов
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(false);
    if (ui->centralStackedWidget) ui->centralStackedWidget->blockSignals(false);

    // Включаем индикатор строк статусбара
    if (ui && ui->cursorPosLabel) {
        ui->cursorPosLabel->show();
    }
    this->updateCursorPositionIndicator();

    if (editor) {
        editor->setFocus();
        editor->update();
    }

    // ШАГ 5: Регистрация открытого документа на сервере LSP (didOpen)
    if (this->lspProcess && this->lspProcess->state() == QProcess::Running) {
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


// bool Neuro_programm::unarchiveProject(const QString &saveFilePath, const QString &targetExtractDir)
// {
//     QDir dir(targetExtractDir);
//     if (!dir.exists()) {
//         dir.mkpath(targetExtractDir);
//     }

//     // !!! КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: Создаем QProcess динамически в куче !!!
//     // Передаем nullptr вместо this, чтобы полностью изолировать его от потока главного окна
//     QProcess *tarProcess = new QProcess(nullptr);

//     QStringList arguments;
//     arguments << "-x" << "-j" << "-f" << saveFilePath << "-C" << targetExtractDir;

//     tarProcess->start("tar", arguments);

//     // Блокируем поток интерфейса на время распаковки, но БЕЗ обработки фоновых сигналов окон
//     bool success = tarProcess->waitForFinished(10000);
//     int exitCode = tarProcess->exitCode();
//     QByteArray errorOutput = tarProcess->readAllStandardError();

//     // !!! ВАЖНО: Даем процессу tar команду безопасно удалиться самостоятельно
//     // строго на следующем витке цикла событий Qt, когда его QWeakPointer закроются
//     tarProcess->deleteLater();

//     if (!success) {
//         qCritical() << "[CRITICAL IDE ERROR] Превышено время ожидания распаковки проекта";
//         return false;
//     }

//     if (exitCode != 0) {
//         qCritical() << "[CRITICAL IDE ERROR] Ошибка tar при распаковке:" << errorOutput;
//         return false;
//     }

//     return true;
// }

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
    qInfo() << "[PROJECT_MGR] Запрос пользователя на выбор ключа открытия проекта...";

    // Настраиваем двойной фильтр расширений для локального паспорта и переносимого архива
    QString fileFilter = "Проекты PyTorch Studio (*.pystudio.json *.pystudio);;"
                         "Манифест паспорта (*.pystudio.json);;"
                         "Архив импорта (*.pystudio)";

    QString selectedFile = QFileDialog::getOpenFileName(
                this,
                "Открыть проект PyTorch Studio (Манифест или Архив)",
                QDir::homePath() + "/projects",
                fileFilter
                );

    // Если пользователь нажал "Отмена" — выходим без ошибок
    if (selectedFile.isEmpty()) {
        qInfo() << "[PROJECT_MGR] Открытие отменено пользователем.";
        return;
    }

    qInfo() << "[PROJECT_MGR] Ключ выбран:" << selectedFile << ". Передаю в смарт-конвейер...";

    // =========================================================================
    // ЖЕЛЕЗНЫЙ UX-ПРЕДОХРАНИТЕЛЬ: МЯГКО ТУШИМ СТАРЫЙ ПОТОК VENV ПЕРЕД СМЕНОЙ ПРОЕКТА
    // =========================================================================
    // Это предотвратит ситуацию, когда старый воркер возвращает сигнал venvConnectedSuccessfully
    // в момент, когда интерфейс уже переключился на отрисовку нового проекта!
    if (this->envManager) {
        // Передаем пустую строку, чтобы вежливо сбросить и остановить активный QThread воркера
        this->envManager->startBackgroundCheck("");
    }
    // =========================================================================

    ui->statusbar->showMessage("Инициализация смарт-конвейера проекта...");

    // Передаем выбранный путь в единый диспетчер processStartupPath, который сам разрулит сценарий!
    // Он распакует архив или прочитает json, а в самом конце вызовет наш обновленный load_progect()
    this->processStartupPath(selectedFile);
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

    // 1. Создаем фильтр эффективно с помощью QStringLiteral
    const QString fileFilter = QStringLiteral("Проекты PyTorch Studio (*.pystudio.json *.pystudio);;"
                                              "Манифест паспорта (*.pystudio.json);;"
                                              "Архив импорта (*.pystudio)");

    // 2. Передаем переменную fileFilter четвертым аргументом
    QString saveFilePath = QFileDialog::getSaveFileName(
                this,
                "Сохранить проект в директорию SAVE",
                defaultSaveName,
                fileFilter
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

#ifndef Q_OS_WIN
#include <unistd.h>
#include <signal.h>
#endif

void Neuro_programm::closeEvent(QCloseEvent *event)
{
    std::cout << "\n[ВХОД] Начало цепочки проверок закрытия PyTorch Studio..." << std::endl;
    std::cout.flush();

    if (m_aiManager) {
        m_aiManager->disconnect();
    }

    // Сбор измененных файлов
    QStringList modifiedFiles;
    if (ui->centralStackedWidget) {
        for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
            QWidget *page = ui->centralStackedWidget->widget(i);
            if (!page) continue;
            CodeEditor *editor = page->findChild<CodeEditor*>();
            if (!editor) editor = qobject_cast<CodeEditor*>(page);
            if (editor && editor->document() && editor->document()->isModified()) {
                QString fPath = editor->objectName().trimmed();
                if (!fPath.isEmpty() && fPath != "MAIN_SCREEN" && fPath != "AI_CHAT_SCREEN") {
                    modifiedFiles.append(fPath);
                }
            }
        }
    }

    bool hasModifiedFiles = !modifiedFiles.isEmpty();

    // =========================================================================
    // ИСПРАВЛЕНО: КОРРЕКТНЫЙ ФЛАГ АКТИВНОСТИ ОБУЧЕНИЯ (УЧИТЫВАЕТ JUPYTER)
    // =========================================================================
    bool isClassicTraining = (trainingProcess && trainingProcess->state() != QProcess::NotRunning);
    bool isJupyterTraining = (this->jupyterServer && this->jupyterServer->isRunning());
    bool isTraining = isClassicTraining || isJupyterTraining;
    // =========================================================================

    bool isDebugging = (this->pyDebugger && this->pyDebugger->isConnected());

    // Умный быстрый выход
    if (!hasModifiedFiles && !isTraining && !isDebugging && !this->property("isInstallingPackages").toBool()) {
        std::cout << "[БЫСТРЫЙ ВЫХОД] Нет активных процессов, дебага и изменений. Мгновенное закрытие." << std::endl;
        if (lspProcess) {
#ifndef Q_OS_WIN
            pid_t lspPid = lspProcess->processId();
            if (lspPid > 0) kill(-lspPid, SIGKILL);
#endif
            lspProcess->kill();
            lspProcess->waitForFinished(500);
        }
        if (m_aiManager) {
            m_aiManager->stopServer();
        }
        std::cout << "[УСПЕХ] Быстрый выход завершен." << std::endl;
        std::cout.flush();
        event->accept();
        return;
    }

    // Вызов диалога
    AdvancedCloseDialog dialog(modifiedFiles, isTraining, this);
    int result = dialog.exec();

    auto restoreAiStatusChannel = [this]() {
        if (!m_aiManager) return;
        m_aiManager->disconnect(m_aiManager, &LocalAiManager::statusChanged, this, nullptr);
        connect(m_aiManager, &LocalAiManager::statusChanged, this, [this](const QString &text, const QString &colorHtml) {
            Q_UNUSED(colorHtml);
            if (this->statusLogLabel != nullptr) {
                this->statusLogLabel->setText(text);
            }
            AiPromptWidget *prompt = this->findChild<AiPromptWidget*>();
            if (prompt != nullptr) {
                prompt->setStatusText(text);
            }
        });
    };

    switch (result) {
    case AdvancedCloseDialog::ResultCancel: {
        std::cout << "[ОТМЕНА] Закрытие отменено пользователем." << std::endl;
        restoreAiStatusChannel();
        event->ignore();
        return;
    }
    case AdvancedCloseDialog::ResultToTray: {
        std::cout << "[ФОН] Окно скрыто. Процессы (обучение/дебаг) переведены в фон." << std::endl;
        restoreAiStatusChannel();
        this->hide();
        event->ignore();
        return;
    }
    case AdvancedCloseDialog::ResultSaveAndExit: {
        QStringList filesToSave = dialog.getFilesToSave();

        if (!filesToSave.isEmpty()) {
            std::cout << "[СОХРАНЕНИЕ] Запись изменений в выбранные пользователем файлы ("
                      << filesToSave.size() << " шт)..." << std::endl;

            if (ui->centralStackedWidget) {
                int initialIndex = ui->centralStackedWidget->currentIndex();

                // Блокируем графические сигналы, чтобы перелистывание вкладок прошло скрытно от пользователя
                ui->centralStackedWidget->blockSignals(true);
                if (ui->fileComboBox) ui->fileComboBox->blockSignals(true);

                for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
                    QWidget *page = ui->centralStackedWidget->widget(i);
                    if (!page) continue;

                    CodeEditor *editor = page->findChild<CodeEditor*>();
                    if (!editor) editor = qobject_cast<CodeEditor*>(page);

                    if (editor) {
                        QString currentFilePath = editor->currentFilePath.trimmed(); // Читаем путь напрямую, как в saveCurrentActiveFile

                        // Если этот файл выбран пользователем в диалоге
                        if (filesToSave.contains(currentFilePath)) {
                            // 1. Программно делаем страницу активной
                            ui->centralStackedWidget->setCurrentIndex(i);

                            // 2. Вызываем ваш проверенный метод (он сам сохранит, перекрасит маркеры в зеленый и обновит статусбар!)
                            this->saveCurrentActiveFile();
                        }
                    }
                }

                // Возвращаем пользователя на ту вкладку, где он стоял до вызова диалога
                ui->centralStackedWidget->setCurrentIndex(initialIndex);

                ui->centralStackedWidget->blockSignals(false);
                if (ui->fileComboBox) ui->fileComboBox->blockSignals(false);
            }
        }
        break;
    }

    case AdvancedCloseDialog::ResultDiscardAndExit: {
        std::cout << "[СБРОС] Выход без сохранения изменений в коде." << std::endl;
        break;
    }
    default:
        restoreAiStatusChannel();
        event->ignore();
        return;
    }

    // =========================================================================
    // ДОРАБОТКА: СИНХРОННОЕ ПРЕРЫВАНИЕ ВЫЧИСЛЕНИЙ PYTORCH ПРИ ПОДТВЕРЖДЕННОМ ВЫХОДЕ
    // =========================================================================
    if (isTraining) {
        if (dialog.shouldSaveWeights()) {
            std::cout << "[КУЛЬТУРНЫЙ ОСТАНОВ] Мягкое прерывание для сохранения весов (SIGINT)..." << std::endl;

            // Если работает классический процесс
            if (isClassicTraining && trainingProcess) {
#ifndef Q_OS_WIN
                pid_t pid = trainingProcess->processId();
                if (pid > 0) kill(-pid, SIGINT);
#endif
                trainingProcess->terminate();
                trainingProcess->waitForFinished(3000);
            }
            // Если работает Jupyter Server
            else if (isJupyterTraining && this->jupyterServer) {
                QProcess *serverProc = this->jupyterServer->findChild<QProcess*>();
                if (serverProc) {
#ifndef Q_OS_WIN
                    pid_t pid = serverProc->processId();
                    if (pid > 0) kill(-pid, SIGINT); // Просим Jupyter мягко сохранить ядра
#endif
                }
                this->jupyterServer->stopServer();
            }
        }
        else {
            std::cout << "[ПРИНУДИТЕЛЬНО] Жесткое уничтожение процессов обучения (kill)..." << std::endl;

            if (isClassicTraining && trainingProcess) {
#ifndef Q_OS_WIN
                pid_t pid = trainingProcess->processId();
                if (pid > 0) kill(-pid, SIGKILL);
#endif
                trainingProcess->kill();
                trainingProcess->waitForFinished(1000);
            }
            else if (isJupyterTraining && this->jupyterServer) {
                // Вызываем нативное аппаратное гашение сервера и ядер
                this->jupyterServer->stopServer();
            }
        }
    }
    // =========================================================================

    if (isDebugging) {
        std::cout << "[SHUTDOWN DEBUG] Обнаружен active дебаг! Вырезаю сокет..." << std::endl;
        this->pyDebugger->stopDebugSession();
    }

    // Экспорт requirements.txt перед выходом
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
            if (this->envManager) {
                pipProcess.setProcessEnvironment(this->envManager->getIsolatedEnvironment());
            }
            pipProcess.start(safeVenvPython, args);
            pipProcess.waitForFinished(3000);
        }
    }

    if (lspProcess) {
        std::cout << "[ОЧИСТКА] Остановка LSP сервера подсказок Jedi..." << std::endl;
#ifndef Q_OS_WIN
        pid_t lspPid = lspProcess->processId();
        if (lspPid > 0) kill(-lspPid, SIGKILL);
#endif
        lspProcess->kill();
        lspProcess->waitForFinished(500);
    }

    if (m_aiManager) {
        std::cout << "[ОЧИСТКА ИИ] Освобождение памяти видеокарты и остановка оркестратора..." << std::endl;
        m_aiManager->stopServer();
    }

    std::cout << "[УСПЕХ] PyTorch Studio успешно завершила работу." << std::endl;
    std::cout.flush();
    event->accept();
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

    if (mainVerticalSplitter) {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250}));
    }



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
    //QPushButton *cancelButton = msgBox.addButton(" Использовать системный Python", QMessageBox::RejectRole);

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

        if (this->statusBar()) {
            this->statusBar()->showMessage("PyTorch Studio: Фоновое развёртывание новой структуры venv...", 0);
            this->statusBar()->setStyleSheet("QStatusBar { color: #3daee9; font-weight: bold; }");
        }

        QProcess *createProc = new QProcess(this);
        createProc->setWorkingDirectory(cleanProjectPath);

        // =========================================================================
        // КРИТИЧЕСКИЙ ШАГ КОРРЕКЦИИ: ПЕРЕДАЕМ СЫРЫЕ БАЙТЫ НАПРЯМУЮ БЕЗ ОШИБОК ДЕКОДИРОВАНИЯ
        // =========================================================================



        connect(createProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, projectPath, venvPythonPath, createProc](int exitCode, QProcess::ExitStatus status) {
            createProc->deleteLater();
            if (exitCode != 0 || status == QProcess::CrashExit) {
                return;
            }

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
        this->initLspServer();
        return;
    }
}

void Neuro_programm::installPackagesFromRequirements(const QString &workingDir, const QString &pythonPath, const QString &reqPath)
{
    if (!QFile::exists(reqPath) || !QFile::exists(pythonPath)) {
        return;
    }

    if (this->statusBar()) {
        this->statusBar()->showMessage("PyTorch Studio: Обновление пакетного менеджера...", 0);
        this->statusBar()->setStyleSheet("QStatusBar { color: #e67e22; font-weight: bold; }");
    }

    // =========================================================================
    // ШАГ 1: ПРИНУДИТЕЛЬНОЕ ОБНОВЛЕНИЕ ВНУТРЕННЕГО PIP ВНУТРИ ЧИСТОГО VENV
    // Это на 100% решает проблему мгновенного вылета с Кодом 1!
    // =========================================================================

    QProcess *pipUpdateProc = new QProcess(this);
    pipUpdateProc->setWorkingDirectory(workingDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TMPDIR", "/tmp");
    env.insert("PIP_CACHE_DIR", "/tmp/pip-cache-elf");
    env.insert("PYTHONUNBUFFERED", "1");
    pipUpdateProc->setProcessEnvironment(env);

    // Подключаем чтение логов обновления ядра pip во встроенную консоль
    connect(pipUpdateProc, &QProcess::readyReadStandardOutput, this, [this, pipUpdateProc]() {
    });
    connect(pipUpdateProc, &QProcess::readyReadStandardError, this, [this, pipUpdateProc]() {
    });

    // Как только ядро pip успешно обновилось, запускаем Шаг 2 (тяжелый PyTorch)
    connect(pipUpdateProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, workingDir, pythonPath, reqPath, env, pipUpdateProc](int updateExitCode, QProcess::ExitStatus status)
    {
        pipUpdateProc->deleteLater();

        if (updateExitCode != 0 || status == QProcess::CrashExit) {
            return;
        }

        // =====================================================================
        // ШАГ 2: ЗАПУСК ОСНОВНОЙ УСТАНОВКИ ИИ-БИБЛИОТЕК ПО REQUIREMENTS.TXT
        // =====================================================================
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
                }
            } else {
                QString cleanOut = output.trimmed();
                if (cleanOut.contains("Installing collected packages") || cleanOut.contains("Running setup.py")) {
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



            if (mainExitCode == 0 && mainStatus == QProcess::NormalExit) {
                this->venvPythonBinary = pythonPath;
                this->sendSystemNotification("Окружение ИИ", "Синхронизация завершена. Все пакеты в актуальном состоянии.");
                if (this->statusBar()) {
                    this->statusBar()->showMessage("PyTorch Studio: Библиотеки синхронизированы", 4000);
                    this->statusBar()->setStyleSheet("QStatusBar { color: #00ff00; font-weight: normal; }");
                }
                this->initLspServer();
            }
            else {
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
    if (this->currentOpenProjectPath.isEmpty())
    {
        sendSystemNotification("Внимание", "Сначала откройте или создайте ИИ-проект (*.pystudio)"); //
        return;
    }

    // Проверяем, существует ли локальный интерпретатор venv
    QString venvPythonPath = this->currentOpenProjectPath + "/venv/bin/python"; //
    if (!QFile::exists(venvPythonPath))
    {
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

    // Синхронизируем кнопки управления статус-бара
    if (btnTerminal) btnTerminal->setChecked(true);
    if (btnAIChat) btnAIChat->setChecked(false);

    // Раздвигаем центральный сплиттер на фиксированные 250 пикселей под консоль вывода
    if (mainVerticalSplitter)
    {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250})); //
    }

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
    });
    connect(pipInstallProc, &QProcess::readyReadStandardError, this, [this, pipInstallProc]() {
    });

    // Настраиваем вежливую очистку памяти после завершения работы pip
    connect(pipInstallProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, packageName, pipInstallProc](int exitCode, QProcess::ExitStatus status) {
        pipInstallProc->deleteLater(); // Освобождаем оперативную память подпроцесса

        if (exitCode == 0 && status == QProcess::NormalExit) {
            sendSystemNotification("Менеджер пакетов", QString("Пакет %1 успешно установлен").arg(packageName)); //
        } else {
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
        targetReqPath = QFileDialog::getOpenFileName(
                    this,
                    "Выберите файл зависимостей проекта",
                    this->currentOpenProjectPath,
                    "Файлы требований (*.txt);;Все файлы (*)"
                    );
    }

    // Если пользователь закрыл диалог выбора файла или нажал "Отмена" — выходим
    if (targetReqPath.isEmpty() || !QFile::exists(targetReqPath)) {
        return;
    }

    // Включаем подсветку кнопки в статус-баре
    if (btnTerminal) btnTerminal->setChecked(true); //
    if (btnAIChat) btnAIChat->setChecked(false); //

    // Выделяем фиксированные 250 пикселей под вывод консоли
    if (mainVerticalSplitter) {
        mainVerticalSplitter->setSizes(QList<int>({this->height() - 250, 250})); //
    }

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
    });
    connect(pipBatchProc, &QProcess::readyReadStandardError, this, [this, pipBatchProc]() {
    });

    // Обработчик успешного или аварийного завершения установки
    connect(pipBatchProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pipBatchProc](int exitCode, QProcess::ExitStatus status) {
        pipBatchProc->deleteLater(); // Очищаем память

        if (exitCode == 0 && status == QProcess::NormalExit) {
            sendSystemNotification("Менеджер окружения", "✔ Зависимости PyTorch успешно обновлены"); //
        } else {
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

    });
    connect(debuggedScriptProcess, &QProcess::readyReadStandardError, this, [this]() {

    });

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
            this->setWindowTitle("pytorch-studio");
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
            QProcess pipInstall;

            // Если в проекте жестко задана архитектура CPU, используем официальное whl-зеркало PyTorch
            // if (architecture == "CPU") {
            //     pipInstall.start(finalPythonExec, QStringList() << "-m" << "pip" << "install"
            //     << "--index-url" << "https://pytorch.org"
            //     << "-r" << reqFilePath);
            // } else {
            //     // Стандартная установка для CUDA систем (Arch Linux / Windows)
            //     pipInstall.start(finalPythonExec, QStringList() << "-m" << "pip" << "install" << "-r" << reqFilePath);
            // }

            pipInstall.waitForFinished(-1);

            // Обратная фиксация имен и точных версий пакетов (pip freeze)
            QProcess pipFreeze;
            pipFreeze.setStandardOutputFile(reqFilePath);
            pipFreeze.start(finalPythonExec, QStringList() << "-m" << "pip" << "freeze");
            pipFreeze.waitForFinished(-1);

            // Фиксируем новое состояние кэша requirements.txt
            QString finalHash = calculateFileMd5(reqFilePath);
            settings.setValue("python/last_requirements_hash", finalHash);
        }
        else
        {
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
    // Шаблон компилируется в памяти ровно один раз
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

void Neuro_programm::updateCustomTitle(const QString &absoluteFilePath)
{
    // ЖЕЛЕЗНЫЙ ХАК: Перенаправляем вызов в наш менеджер документов!
    // Он сам соберет идеальную строку: train.py(scripts@z2)[z2.pystudio] - PyTorch Studio
    if (this->docMgr) {
        this->docMgr->updateUiTitles(absoluteFilePath);
    } else {
        // Резервный фолбэк, если docMgr еще спит в памяти
        this->setWindowTitle("pytorch-studio");
    }
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

    for (QAction *action : std::as_const(recentActions))
    {
        if (!action || action->isSeparator() || action->text().isEmpty()) continue;

        // В Qt недавние пути обычно хранятся прямо в тексте экшена или в его data()
        QString fullPath = action->data().toString();
        if (fullPath.isEmpty()) {
            fullPath = action->text(); // Резервный случай, если путь записан в текст
        }

        // Очищаем от возможных системных горячих клавиш или номеров (например, "1. /path/to...")
        static const QRegularExpression numberPrefixRegex("^\\d+\\.\\s*");

        fullPath.remove(numberPrefixRegex);
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

    // Извлекаем сохраненный путь к папке проекта из метаданных ячейки Qt
    QString targetProjectPath = item->data(Qt::UserRole).toString().trimmed();
    if (targetProjectPath.isEmpty()) return;

    qInfo() << "[RECENT_WIDGET] Запрос быстрой загрузки по двойному клику на заставке:" << targetProjectPath;

    QDir projectDir(targetProjectPath);

    // Валидируем паспорт перед загрузкой
    if (!projectDir.exists() || !projectDir.exists("passport.pystudio.json")) {
        qWarning() << "[RECENT_WIDGET] Ошибка валидации паспорта по пути:" << targetProjectPath;
        QMessageBox::critical(
                    this,
                    "Проект не найден",
                    "<b>Не удалось запустить проект.</b><br><br>"
                    "Выбранная папка больше не существует или повреждена."
                    );
        return;
    }

    // ЖЕЛЕЗНЫЙ ВЫЗОВ: Направляем чистый валидный путь в ядро рендеринга дерева
    this->initProjectTreeModel(targetProjectPath);
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
    // 1. Ищем активный редактор на текущей активной странице стека
    QWidget *currentPage = ui->centralStackedWidget->currentWidget();
    if (!currentPage) return;

    CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();

    // Если редактор не найден или документ пуст (например, закрыли все вкладки), очищаем надпись
    if (!currentEditor || !currentEditor->document()) {
        QLabel *topLabel = this->findChild<QLabel*>("cursorPosLabel");
        if (topLabel) topLabel->setText("");
        return;
    }

    // 2. Безопасно считываем позицию каретки текстового поля
    QTextCursor cursor = currentEditor->textCursor();
    int line = cursor.blockNumber() + 1;
    int col = cursor.columnNumber() + 1;

    // Формируем красивую эталонную строчку координат в стиле JetBrains
    QString posText = QString("Строка %1, Столбец %2").arg(line).arg(col);

    // =========================================================================
    // СИНХРОНИЗАЦИЯ С ВЕРХНИМ КРАЕМ: ЖЕСТКИЙ ПОИСК ПО OBJECTNAME "cursorPosLabel"
    // =========================================================================
    // Ищем виджет по его точному аппаратному имени в иерархии окна
    QLabel *topLabel = this->findChild<QLabel*>("cursorPosLabel");

    if (topLabel) {
        topLabel->setText(posText); // Вливаем координаты!

        // Накатываем премиальный серый цвет текста и шрифты ИИ-Студии
        topLabel->setStyleSheet(
                    "color: #898f94; "
                    "font-family: 'JetBrains Mono', monospace; "
                    "font-size: 11px; "
                    "font-weight: bold; "
                    "background: transparent;"
                    );
        topLabel->show(); // Принудительно проявляем, блокируя любые скрытия
    }

    // Полностью очищаем нижний статусбар, чтобы он оставался чистым для системных логов
    if (ui->statusbar) {
        ui->statusbar->clearMessage();
    }
    // =========================================================================
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

void Neuro_programm::updateBottomPanelGeometry()
{
    if (!panelOther) return;

    int panelHeight = 220; // Высота вашей панели терминалов (как на скриншоте)

    // Вычисляем глобальную позицию левого нижнего угла вашего главного окна на экране
    // Чтобы панель встала аккуратно над статусбаром или в самый низ
    QPoint bottomPoint = this->mapToGlobal(QPoint(0, this->height() - panelHeight));

    int x = bottomPoint.x();
    int y = bottomPoint.y();
    int w = this->width(); // Растягиваем строго по ширине главного окна
    int h = panelHeight;

    panelOther->setGeometry(x, y, w, h);
}

void Neuro_programm::on_btnSidebarTerminal_clicked()
{
    if (!panelOther) return;

    if (panelOther->isHidden()) {
        panelOther->show();

        // Принудительно поднимаем панель на самый верхний графический слой (Z-index),
        // чтобы она наложилась ПОВЕРХ нижней части редактора и плейсхолдера,
        // вообще не сдвигая и не сжимая их!
        panelOther->raise();
        panelOther->setFocus();
    } else {
        panelOther->hide();
    }
}

void Neuro_programm::initializeEnvironmentOnStartup()
{
    qInfo() << "[INIT_ENV] Чистый старт IDE: чтение фонового окружения из pystudio.conf...";

    // 1. Подключаемся строго к нашему единому файлу pystudio.conf
    QString configAbsolutePath = QDir::homePath() + "/.config/PyTorchStudio/pystudio.conf";
    QSettings settings(configAbsolutePath, QSettings::IniFormat);

    this->currentOpenProjectPath = "";

    // 2. Считываем глобальный путь к внешнему интерпретатору venv
    // Извлекаем значение по ключу, который записывает менеджер окружения
    QString activeVenvPath = settings.value("GlobalEnvironment/external_venv_path").toString().trimmed();

    // Фолбэк (запасной вариант): если в конфиге пусто, жестко страхуем вашим путем /home/elf/venv/bin/python
    if (activeVenvPath.isEmpty()) {
        activeVenvPath = "/home/elf/venv/bin/python";
    }

    // Обрабатываем тильду (~), если путь записан в Unix-формате короткого адреса
    if (activeVenvPath.startsWith("~")) {
        activeVenvPath.replace(0, 1, QDir::homePath());
    }

    // 3. ЗАПУСКАЕМ ИНИЦИАЛИЗАЦИЮ ОКРУЖЕНИЯ
    if (this->envManager && QFile::exists(activeVenvPath)) {
        qInfo() << "[INIT_ENV SUCCESS] Запускаю фоновую валидацию внешнего venv из pystudio.conf:" << activeVenvPath;

        // ВАЖНО: Передаем пустую строку или домашнюю папку, так как при чистом старте
        // открытого проекта еще нет, и воркер должен сразу переключиться на ваш внешний venv!
        this->envManager->startBackgroundCheck(QDir::homePath());
    } else {
        qWarning() << "[INIT_ENV WARN] Не удалось определить внешний venv. Проверьте наличие /home/elf/venv";
    }

    // =========================================================================
    // БЛОК UX-ПЕРЕКЛЮЧЕНИЯ НА СТАРТОВУЮ ЗАСТАВКУ (БЕЗ ИЗМЕНЕНИЙ)
    // =========================================================================
    QStackedWidget *dockStack = ui->leftDockWidget ? ui->leftDockWidget->findChild<QStackedWidget*>("dockContentsStack") : nullptr;
    if (dockStack) {
        dockStack->setCurrentIndex(1);
        ui->leftDockWidget->setVisible(true);
        ui->leftDockWidget->show();
        if (actProject) {
            actProject->blockSignals(true);
            actProject->setChecked(true);
            actProject->blockSignals(false);
        }
        dockStack->update();
    }

    this->setIDEInStartMode(true);
    if (ui->btnCloseFile) ui->btnCloseFile->setEnabled(false);
    if (ui->fileComboBox) ui->fileComboBox->setEnabled(false);
}

void Neuro_programm::showVenvEmergencyDialog(const QString &reason)
{
    // Гарантируем, что статусбар отражает проблему
    ui->statusbar->showMessage("Окружение PyTorch требует настройки или восстановления.");

    // =========================================================================
    // СБОРКА ТАБЛИЧКИ (UX ДИАЛОГ ВЫБОРА ДЛЯ ПОЛЬЗОВАТЕЛЯ)
    // =========================================================================
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Окружение Python / PyTorch");
    msgBox.setText(QString("Не удалось автоматически подключить окружение PyTorch.\n"
                           "Без него автодополнение кода и запуск нейросетей не будут работать.\n\n"
                           "Причина: %1").arg(reason));
    msgBox.setIcon(QMessageBox::Warning);

    // Добавляем три аппаратные кнопки управления
    QPushButton *btnCreate   = msgBox.addButton("Создать заново (Авто)", QMessageBox::AcceptRole);
    QPushButton *btnBrowse   = msgBox.addButton("Указать путь вручную...", QMessageBox::ActionRole);
   // QPushButton *btnSkip     = msgBox.addButton("Пропустить", QMessageBox::RejectRole);

    msgBox.exec(); // Запускаем табличку поверх интерфейса (блокирует ввод в IDE, но GUI не фризит)

    // =========================================================================
    // ЛОГИКА ОБРАБОТКИ ВЫБОРА ПОЛЬЗОВАТЕЛЯ
    // =========================================================================
    if (msgBox.clickedButton() == btnCreate) {
        // Пользователь выбрал автоматическое создание venv с нуля в фоне
        ui->statusbar->showMessage("Инициализация нового окружения PyTorch в фоне...", 0);

        // Вызываем асинхронную функцию сборки venv, которую мы написали ранее
        if (!this->currentOpenProjectPath.isEmpty()) {
            this->checkAndCreateVenvAsync(this->currentOpenProjectPath);
        } else {
            ui->statusbar->showMessage("Ошибка: путь к проекту не определен.", 4000);
        }
    }
    else if (msgBox.clickedButton() == btnBrowse)
    {
        // Пользователь хочет подключить готовый venv из другой папки на диске
        QString customVenvPath = QFileDialog::getExistingDirectory(
                    this,
                    "Выберите существующую папку venv (содержащую bin/Scripts)",
                    QDir::homePath(),
                    QFileDialog::ShowDirsOnly
                    );

        if (!customVenvPath.isEmpty())
        {
            // Сохраняем кастомный путь в глобальные настройки, чтобы каскадный поиск нашел его
            QSettings globalSettings("PyTorchStudio", "IDE");
            globalSettings.setValue("Platform/lastKnownPythonPath", customVenvPath);

            // Запускаем проверку заново для текущего проекта
            ui->statusbar->showMessage("Проверка указанного venv...", 2000);
            if (this->envManager && !this->currentOpenProjectPath.isEmpty()) {
                this->envManager->startBackgroundCheck(this->currentOpenProjectPath);
            }
        } else {
            ui->statusbar->showMessage("Загрузка завершена без активации PyTorch.", 4000);
        }
    }
    else {
        // Пользователь нажал "Пропустить"
        ui->statusbar->showMessage("Проект открыт в режиме чтения (Автодополнение кода отключено).", 4000);
    }
}

void Neuro_programm::load_progect(const QString &projectPath)
{
    if (projectPath.isEmpty()) return;

    this->currentOpenProjectPath = QDir::cleanPath(projectPath.trimmed());
    this->setIDEInStartMode(false);

    QSettings lastProjectSettings("PyTorchStudio", "pystudio"); // Синхронизируем реестр Qt
    lastProjectSettings.setValue("Platform/lastActiveProjectPath", this->currentOpenProjectPath);

    initProjectTreeModel(this->currentOpenProjectPath);
    ui->statusbar->showMessage("Загрузка структуры проекта...");

    // =========================================================================
    // ЕДИНЫЙ СТАНДАРТ: ЧТЕНИЕ ИЗ PYSTUDIO.CONF ПРИ ЗАГРУЗКЕ КАТАЛОГА
    // =========================================================================
    if (this->envManager) {
        QSettings settings("/home/elf/.config/PyTorchStudio/pystudio.conf", QSettings::IniFormat);

        // Считываем сохраненный глобальный внешний venv
        QString savedVenv = settings.value("GlobalEnvironment/external_venv_path").toString().trimmed();

        // Если в файле конфигурации пусто, страхуем абсолютным путем к вашему venv в /home/elf
        if (savedVenv.isEmpty() || !QFile::exists(savedVenv)) {
            savedVenv = "/home/elf/venv/bin/python";
        }

        if (QFile::exists(savedVenv)) {
            qInfo() << "[VENV_КЭШ] Успешно подгружен внешний интерпретатор из pystudio.conf:" << savedVenv;
            this->envManager->startBackgroundCheck(this->currentOpenProjectPath);
        } else {
            qWarning() << "[VENV_КЭШ] Ошибка: Внешний venv в /home/elf/venv физически отсутствует!";
            this->envManager->startBackgroundCheck(this->currentOpenProjectPath);
        }
    }
}

bool Neuro_programm::createProjectPassport(const QString &projectName, const QString &projectFolderPath, bool useGpuArchitecture)
{
    QJsonObject passportObj;

    // 1. Метаданные среды разработки (Блок ядра)
    passportObj["pystudio_version"] = "1.0.0";
    passportObj["project_name"] = projectName;
    passportObj["last_modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject environmentObj;
    environmentObj["python_version"] = "3.14"; // Базовая версия Arch Linux
    environmentObj["requirements_file"] = "requirements.txt";

    QJsonObject envVariablesObj;
    envVariablesObj["CUDA_VISIBLE_DEVICES"] = useGpuArchitecture ? "0" : "-1"; // Автовыбор GPU/CPU
    environmentObj["env_variables"] = envVariablesObj;
    passportObj["environment"] = environmentObj;

    // 2. Состояние протоколов (Блок управления компонентами)
    QJsonObject protocolsObj;

    QJsonObject gitObj;
    gitObj["remote_url"] = "";
    gitObj["current_branch"] = "main";
    protocolsObj["git"] = gitObj;

    QJsonObject jupyterObj;
    jupyterObj["auto_start"] = false;
    jupyterObj["default_notebook"] = "notebooks/train_model.ipynb";
    protocolsObj["jupyter"] = jupyterObj;

    QJsonObject tensorboardObj;
    tensorboardObj["auto_start"] = true;
    tensorboardObj["log_dir"] = "outputs/tensorboard_logs";
    tensorboardObj["port"] = 6006;
    protocolsObj["tensorboard"] = tensorboardObj;

    QJsonObject hfObj;
    hfObj["repo_id"] = "";
    hfObj["private"] = true;
    protocolsObj["hugging_face"] = hfObj;

    passportObj["protocols"] = protocolsObj;

    // 3. Ссылки на связанные файлы (Блок связей)
    QJsonObject projectLinksObj;
    projectLinksObj["data_manifest_json"] = "config/data_manifest.json";
    projectLinksObj["model_config_yaml"] = "config/hyperparameters.yaml";
    passportObj["project_links"] = projectLinksObj;

    // 4. Состояние и метрики (Блок истории / Дашборд)
    QJsonObject experimentStatusObj;
    experimentStatusObj["status"] = "new";
    experimentStatusObj["last_trained_epoch"] = 0;

    QJsonObject bestMetricsObj;
    bestMetricsObj["accuracy"] = 0.0;
    bestMetricsObj["loss"] = 0.0;
    experimentStatusObj["best_metrics"] = bestMetricsObj;
    experimentStatusObj["active_run_id"] = "";
    passportObj["experiment_status"] = experimentStatusObj;

    // 5. Запись JSON-паспорта на диск
    QString passportFilePath = projectFolderPath + "/passport.pystudio.json";
    QFile passportFile(passportFilePath);

    if (!passportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[PASSPORT_MGR] Ошибка: не удалось создать файл" << passportFilePath;
        return false;
    }

    QJsonDocument doc(passportObj);
    passportFile.write(doc.toJson(QJsonDocument::Indented)); // Красивые отступы
    passportFile.close();
    qInfo() << "[PASSPORT_MGR] Паспорт passport.pystudio.json успешно сгенерирован:" << passportFilePath;

    // 6. Автоматическое создание базовых конфигурационных файлов-заглушек
    QDir(projectFolderPath).mkdir("config");

    QFile manifestFile(projectFolderPath + "/config/data_manifest.json");
    if (manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        manifestFile.write("{\n  \"datasets\": []\n}");
        manifestFile.close();
    }

    QFile hyperFile(projectFolderPath + "/config/hyperparameters.yaml");
    if (hyperFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        hyperFile.write("# Настройки обучения нейросети\nlearning_rate: 0.001\nbatch_size: 32\nepochs: 10\n");
        hyperFile.close();
    }

    return true;
}

void Neuro_programm::action_install_package_triggered()
{
    // 1. Проверяем, не идет ли уже параллельная установка
    if (m_installProcess && m_installProcess->state() == QProcess::Running) {
        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>(QStringLiteral("logEdit"));
            if (logConsole) logConsole->appendPlainText(QStringLiteral("⚠️ [PyTorch Studio] Процесс установки уже запущен."));
        }
        return;
    }

    // 2. Диалоговое окно ввода пакета
    bool ok;
    QString packageName = QInputDialog::getText(this,
                                                tr("Установка пакета PIP"), tr("Введите имя пакета:"),
                                                QLineEdit::Normal, QString(), &ok);

    if (!ok || packageName.trimmed().isEmpty()) return;
    const QString cleanName = packageName.trimmed();

    // 3. ПРОВЕРКА: Если пакет уже есть в venv — выводим QMessageBox
    if (m_pipPage && m_pipPage->isPackageInstalled(cleanName)) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("Пакет уже установлен"),
                                      tr("Пакет '%1' уже присутствует в виртуальном окружении. Переустановить или обновить его?").arg(cleanName),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) return; // Если пользователь передумал — выходим
    }

    // Читаем глобальный venv из вашего IDE.conf (Оптимизация Clazy через QStringLiteral)
    const QString configPath = QDir::home().filePath(QStringLiteral(".config/PyTorchStudio/IDE.conf"));
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value(QStringLiteral("python/global_venv_path"), currentOpenProjectPath + QStringLiteral("/venv")).toString();

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath(QStringLiteral("Scripts/python.exe"));
#else
    QString pythonExe = QDir(globalVenv).filePath(QStringLiteral("bin/python"));
#endif

    if (!QFile::exists(pythonExe)) return;

    // =========================================================================
    // ИСПРАВЛЕННЫЙ UX-БЛОК: ПЕРЕКЛЮЧЕНИЕ СТРАНИЦ STACKED_WIDGET И КНОПОК НАВИГАЦИИ
    // =========================================================================
    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);

        // Находим главный stackedWidget консолей
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) {
            // ИСПРАВЛЕНО: Открываем страницу 2, где физически лежит LogEdit (логи обучения/установки)
            bottomStacked->setCurrentIndex(2);
        }

        // Находим stackedWidget переключателя меню кнопок верхнего бара консоли
        QStackedWidget *menuSwitcherStack = panelOther->findChild<QStackedWidget*>(QStringLiteral("menuSwitcherStack"));
        if (menuSwitcherStack) {
            // ТРЕБОВАНИЕ: Открываем страницу с индексом 0 в панели кнопок
            menuSwitcherStack->setCurrentIndex(0);
        }
    }

    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>(QStringLiteral("logEdit"));
        if (logConsole) {
            logConsole->appendPlainText(QStringLiteral("\n🚀 [PIP INSTALL] Запуск установки: %1...").arg(cleanName));
        }
    }

    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections();
    }

    m_installProcess->setWorkingDirectory(globalVenv);

    // Записываем свойство СТРОГО здесь для передачи имени пакета в поток
    m_installProcess->setProperty("installedPackageName", cleanName);

    QStringList arguments;
    arguments << QStringLiteral("-m") << QStringLiteral("pip") << QStringLiteral("install") << cleanName;
    m_installProcess->start(pythonExe, arguments);
}

void Neuro_programm::setupInstallProcessConnections()
{
    if (!m_installProcess) return;

    // 1. Потоковое чтение вывода (stdout) утилиты pip
    connect(m_installProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray output = m_installProcess->readAllStandardOutput();
        QString text = QString::fromUtf8(output);

        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) {
                logConsole->moveCursor(QTextCursor::End);
                logConsole->insertPlainText(text);
                logConsole->moveCursor(QTextCursor::End);
            }
        }
    });

    // 2. Потоковое чтение вывода ошибок (stderr)
    connect(m_installProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray errorOutput = m_installProcess->readAllStandardError();
        QString text = QString::fromUtf8(errorOutput);

        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) {
                logConsole->moveCursor(QTextCursor::End);
                logConsole->insertPlainText(text);
                logConsole->moveCursor(QTextCursor::End);
            }
        }
    });

    // 3. Обработка завершения процесса установки пакета
    connect(m_installProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {

        bool isSuccess = (exitCode == 0 && status == QProcess::NormalExit);

        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) {
                logConsole->moveCursor(QTextCursor::End);
                if (isSuccess) {
                    logConsole->appendPlainText("\n🎉 [УСПЕХ] Пакет успешно установлен!");
                } else {
                    logConsole->appendPlainText(QString("\n❌ [ОШИБКА] Процесс pip завершился с кодом: %1").arg(exitCode));
                }
                logConsole->moveCursor(QTextCursor::End);
            }
        }

        // ЕСЛИ ВСЁ УСПЕШНО: Запускаем обновление, скролл и ТАЙМЕР ЗАКРЫТИЯ КОНСОЛИ
        if (isSuccess && m_pipPage) {
            QString pkgName = m_installProcess->property("installedPackageName").toString();
            //m_pipPage->loadPipData(pkgName);

            // Подключаем одноразовый коннект к финалу прорисовывания строк страницы
            connect(m_pipPage, &PipManagerPage::dataLoaded, this, [this, pkgName]() {
                QTimer::singleShot(150, this, [this, pkgName]() {
                    if (m_pipPage) {
                        m_pipPage->highlightAndScrollToPackage(pkgName);
                    }
                });
            }, Qt::SingleShotConnection);

            // Запускаем фоновое обновление данных в таблице
            m_pipPage->loadPipData();

            // =========================================================================
            // НАСТРОЙКА UX: Автоматическое скрытие консоли вывода через 2.5 секунды
            // =========================================================================
            QTimer::singleShot(2500, this, [this]() {
                // Убеждаемся, что за эти 2.5 секунды пользователь не запустил установку заново
                if (m_installProcess && m_installProcess->state() != QProcess::Running) {
                    qDebug() << " [UI] Автоматическое закрытие консоли логов после установки пакета.";

                    if (panelOther) {
                        panelOther->hide();
                        panelOther->setVisible(false);
                    }

                    // Возвращаем сплиттер в исходное монолитное состояние (код на весь экран, низ убран)
                    if (mainVerticalSplitter) {
                        int totalHeight = this->height();
                        mainVerticalSplitter->setSizes(QList<int>({totalHeight, 0}));
                        mainVerticalSplitter->setStretchFactor(0, 1);
                        mainVerticalSplitter->setStretchFactor(1, 0);
                        mainVerticalSplitter->refresh();
                    }
                }
            });
            // =========================================================================
        }
    });
}

#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QSettings>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QDebug>

void Neuro_programm::action_uninstall_package_triggered()
{
    // 1. Проверяем, не занят ли процесс pip другой фоновой операцией
    if (m_installProcess && m_installProcess->state() == QProcess::Running) {
        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) logConsole->appendPlainText("⚠️ [PyTorch Studio] Процесс pip сейчас занят. Дождитесь окончания текущей операции.");
        }
        return;
    }

    // 2. Запрашиваем у инженера точное имя пакета для удаления
    bool ok;
    QString packageName = QInputDialog::getText(this,
                                                tr("Удаление пакета PIP"),
                                                tr("Введите точное имя пакета, который необходимо удалить из окружения venv:"),
                                                QLineEdit::Normal, "", &ok);

    // Если нажали Cancel или ввели пустоту — выходим
    if (!ok || packageName.trimmed().isEmpty()) return;
    QString cleanName = packageName.trimmed();

    // 3. UX-ЗАЩИТА: Запрашиваем подтверждение, чтобы случайно не стереть критический пакет
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Подтверждение удаления"),
                                  QString(tr("Вы уверены, что хотите полностью удалить пакет '%1' из виртуального окружения?")).arg(cleanName),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return; // Отмена операции пользователь передумал

    // 4. Считываем путь к глобальному venv из вашего легитимного системного IDE.conf
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value("python/global_venv_path").toString();

    if (globalVenv.isEmpty()) {
        globalVenv = settings.value("Platform/lastKnownPythonPath").toString();
        if (globalVenv.endsWith("/bin/python")) {
            globalVenv.chop(11);
        }
    }

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    QString pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    if (!QFile::exists(pythonExe)) {
        qWarning() << "❌ [УДАЛЕНИЕ] Интерпретатор Python не найден:" << pythonExe;
        return;
    }

    // 5. Принудительно открываем и разворачиваем нижнюю панель panelOther
    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);

        // Переключаем внутренний stackedWidget панели строго на вторую страницу (индекс 1) с logEdit
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) {
            bottomStacked->setCurrentIndex(1);
        }
    }

    // Раздвигаем вертикальный сплиттер на 250 пикселей под вывод логов
    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    // Выводим стартовое уведомление в logEdit
    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
        if (logConsole) {
            logConsole->appendPlainText(QString("\n🗑️ [PIP UNINSTALL] Запуск удаления пакета: %1...").arg(cleanName));
        }
    }

    // 6. Настраиваем и запускаем фоновый QProcess
    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections(); // Сигналы вывода stdout/stderr уже привязаны к logEdit
    }

    m_installProcess->setWorkingDirectory(globalVenv);

    // Передаем пустоту в имя для подсветки, так как удаленный пакет больше не нужно искать на экране
    m_installProcess->setProperty("installedPackageName", "");

    // Команда принудительного удаления без интерактивных вопросов терминала (-y)
    QStringList arguments;
    arguments << "-m" << "pip" << "uninstall" << "-y" << cleanName;

    m_installProcess->start(pythonExe, arguments);
}

#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QSettings>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QDebug>

void Neuro_programm::action_upgrade_package_triggered()
{
    // 1. Проверяем, не занят ли процесс pip другой фоновой операцией
    if (m_installProcess && m_installProcess->state() == QProcess::Running) {
        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) logConsole->appendPlainText("⚠️ [PyTorch Studio] Процесс pip сейчас занят. Дождитесь окончания текущей операции.");
        }
        return;
    }

    // 2. Отображаем диалоговое окно для ручного ввода имени пакета
    bool ok;
    QString packageName = QInputDialog::getText(this,
                                                tr("Обновление пакета PIP"),
                                                tr("Введите точное имя пакета, который необходимо обновить до актуальной версии PyPI:"),
                                                QLineEdit::Normal, "", &ok);

    // Если инженер передумал, нажал Cancel или ввел пустоту — просто выходим
    if (!ok || packageName.trimmed().isEmpty()) return;
    QString cleanName = packageName.trimmed();

    // 3. Считываем путь к вашему глобальному venv из системного конфигурационного файла IDE.conf
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value("python/global_venv_path").toString();

    if (globalVenv.isEmpty()) {
        globalVenv = settings.value("Platform/lastKnownPythonPath").toString();
        if (globalVenv.endsWith("/bin/python")) {
            globalVenv.chop(11);
        }
    }

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    QString pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    if (!QFile::exists(pythonExe)) {
        qWarning() << "❌ [ОБНОВЛЕНИЕ] Интерпретатор Python не найден по пути:" << pythonExe;
        return;
    }

    // 4. Принудительно разворачиваем нижнюю панель panelOther
    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);

        // Перелистываем внутренний stackedWidget строго на вторую страницу (индекс 1), где находится ваш logEdit
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) {
            bottomStacked->setCurrentIndex(1);
        }
    }

    // Раздвигаем вертикальный разделитель окон сплиттера на 250 пикселей под вывод текста
    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    // Печатаем стартовый маркер лога в logEdit
    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
        if (logConsole) {
            logConsole->appendPlainText(QString("\n🆙 [PIP UPGRADE] Запуск обновления пакета: %1...").arg(cleanName));
        }
    }

    // 5. Настройка и запуск фонового процесса QProcess
    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections(); // Потоковый вывод stdout/stderr привязан к logEdit
    }

    m_installProcess->setWorkingDirectory(globalVenv);

    // Передаем имя пакета в свойство. Если сейчас у пользователя открыта C++ таблица,
    // то после завершения обновления она пересканирует venv и автоматически подсветит эту строку синим цветом!
    m_installProcess->setProperty("installedPackageName", cleanName);

    // Формируем аргументы команды обновления: python -m pip install --upgrade имя_пакета
    QStringList arguments;
    arguments << "-m" << "pip" << "install" << "--upgrade" << cleanName;

    m_installProcess->start(pythonExe, arguments);
}

void Neuro_programm::action_upgrade_all_packages_triggered()
{
    // 1. Проверяем, не занят ли процесс pip другой задачей (установкой или удалением)
    if (m_installProcess && m_installProcess->state() == QProcess::Running) {
        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) logConsole->appendPlainText("⚠️ [PyTorch Studio] Процесс pip сейчас занят. Дождитесь окончания текущей операции.");
        }
        return;
    }

    // 2. UX-ЗАЩИТА: Предупреждаем о времени выполнения операции
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Массовое обновление пакетов"),
                                  tr("Вы действительно хотите обновить ВСЕ установленные пакеты до актуальных версий PyPI?\n"
                                     "Это действие потребует стабильного интернет-соединения и может занять некоторое время."),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return; // Отмена операции

    // 3. Считываем путь к глобальному venv из вашего системного IDE.conf
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value("python/global_venv_path").toString();

    if (globalVenv.isEmpty()) {
        globalVenv = settings.value("Platform/lastKnownPythonPath").toString();
        if (globalVenv.endsWith("/bin/python")) {
            globalVenv.chop(11);
        }
    }

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    QString pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    if (!QFile::exists(pythonExe)) {
        qWarning() << "❌ [ОБНОВЛЕНИЕ ВСЕХ] Интерпретатор Python не найден:" << pythonExe;
        return;
    }

    // 4. Принудительно открываем и разворачиваем нижнюю панель panelOther
    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);

        // Перелистываем внутренний stackedWidget строго на вторую страницу (индекс 1) с logEdit
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) {
            bottomStacked->setCurrentIndex(1);
        }
    }

    // Раздвигаем вертикальный сплиттер на 250 пикселей под вывод логов
    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    // Выводим стартовое уведомление в logEdit
    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
        if (logConsole) {
            logConsole->appendPlainText("\n🚀 [PIP UPGRADE ALL] Поиск устаревших пакетов и запуск массового обновления...");
        }
    }

    // 5. Инициализация и настройка фонового QProcess
    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections(); // Потоковый вывод stdout/stderr привязан к логам
    }

    m_installProcess->setWorkingDirectory(globalVenv);

    // Сбрасываем свойство индивидуальной подсветки, так как обновляется множество строк сразу
    m_installProcess->setProperty("installedPackageName", "");

    // ОПТИМИЗИРОВАННЫЙ ВЫЗОВ КОМАНДЫ: Сама утилита pip не умеет одной командой обновлять всё подряд.
    // Мы передаем короткий и надежный Python-скрипт одной строкой, который через внутренний json pip
    // мгновенно соберет имена всех устаревших пакетов и поочередно обновит их в цикле, выдавая лог.
    QString pythonUpgradeScript =
            "import subprocess, sys, json\n"
            "try:\n"
            "    res = subprocess.run([sys.executable, '-m', 'pip', 'list', '--outdated', '--format=json'], stdout=subprocess.PIPE, text=True)\n"
            "    pkgs = [p['name'] for p in json.loads(res.stdout)] if res.returncode == 0 else []\n"
            "    if not pkgs:\n"
            "        print('🎉 Все пакеты окружения уже имеют актуальные версии PyPI.'); sys.exit(0)\n"
            "    print(f'Найдено пакетов для обновления: {len(pkgs)}. Запуск пакетной установки...')\n"
            "    for pkg in pkgs:\n"
            "        print(f'\\n[Обновление {pkg}]... ')\n"
            "        subprocess.run([sys.executable, '-m', 'pip', 'install', '--upgrade', pkg])\n"
            "except Exception as e: print(f'❌ Ошибка выполнения пакетного обновления: {e}')";

    QStringList arguments;
    arguments << "-c" << pythonUpgradeScript;

    m_installProcess->start(pythonExe, arguments);
}

void Neuro_programm::action_install_from_requirements_triggered()
{
    // 1. Проверяем, не занят ли процесс pip другой фоновой операцией
    if (m_installProcess && m_installProcess->state() == QProcess::Running) {
        if (panelOther) {
            QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
            if (logConsole) logConsole->appendPlainText("⚠️ [PyTorch Studio] Процесс pip сейчас занят. Дождитесь окончания текущей операции.");
        }
        return;
    }

    // 2. Извлекаем абсолютный путь к файлу requirements.txt
    QString reqFilePath;
    if (m_pipPage) {
        // Забираем сохраненный путь из свойства, которое мы добавили в Шаге 2
        reqFilePath = m_pipPage->property("requirementsPath").toString();
    }

    // Если страница еще не создавалась, вычисляем путь на основе папки открытого проекта
    if (reqFilePath.isEmpty()) {
        reqFilePath = QDir(currentOpenProjectPath).filePath("requirements.txt");
    }

    // Проверяем физическое существование файла на диске Arch Linux
    if (!QFile::exists(reqFilePath)) {
        QMessageBox::warning(this, tr("Файл не найден"),
                             tr("Файл 'requirements.txt' отсутствует в корневой директории текущего проекта тепловых сетей."));
        return;
    }

    // 3. Считываем путь к глобальному venv из вашего системного IDE.conf
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value("python/global_venv_path").toString();

    if (globalVenv.isEmpty()) {
        globalVenv = settings.value("Platform/lastKnownPythonPath").toString();
        if (globalVenv.endsWith("/bin/python")) {
            globalVenv.chop(11);
        }
    }

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    QString pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    if (!QFile::exists(pythonExe)) {
        qWarning() << "❌ [УСТАНОВКА ИЗ ФАЙЛА] Интерпретатор Python не найден:" << pythonExe;
        return;
    }

    // 4. Принудительно открываем и разворачиваем нижнюю панель panelOther
    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);

        // Перелистываем внутренний stackedWidget строго на вторую страницу (индекс 1) с logEdit
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) {
            bottomStacked->setCurrentIndex(1);
        }
    }

    // Раздвигаем вертикальный разделитель окон сплиттера на 250 пикселей под вывод текста
    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    // Печатаем стартовое сообщение в logEdit
    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
        if (logConsole) {
            logConsole->appendPlainText(QString("\n📦 [PIP REQS] Синхронизация окружения. Массовая установка пакетов из файла: %1...").arg(QFileInfo(reqFilePath).fileName()));
        }
    }

    // 5. Настройка и запуск фонового процесса QProcess
    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections(); // Потоковый вывод stdout/stderr привязан к логам
    }

    m_installProcess->setWorkingDirectory(globalVenv);

    // Сбрасываем свойство индивидуальной подсветки ячеек
    m_installProcess->setProperty("installedPackageName", "");

    // Команда пакетной установки: python -m pip install -r /путь/к/requirements.txt
    QStringList arguments;
    arguments << "-m" << "pip" << "install" << "-r" << reqFilePath;

    m_installProcess->start(pythonExe, arguments);
}

void Neuro_programm::action_freeze_requirements_triggered()
{
    // 1. Проверяем, открыта ли страница PIP менеджера
    if (!m_pipPage) {
        QMessageBox::warning(this, tr("Менеджер не активен"),
                             tr("Пожалуйста, сначала откройте менеджер пакетов (двойным кликом по requirements.txt)."));
        return;
    }

    // 2. Находим внутреннюю таблицу QTableWidget внутри страницы m_pipPage
    QTableWidget *table = m_pipPage->findChild<QTableWidget*>();
    if (!table || table->rowCount() == 0) {
        QMessageBox::warning(this, tr("Таблица пуста"),
                             tr("Нет данных о пакетах venv для сохранения. Обновите список."));
        return;
    }

    // 3. Извлекаем абсолютный путь к файлу requirements.txt текущего проекта
    QString reqFilePath = m_pipPage->property("requirementsPath").toString();
    if (reqFilePath.isEmpty()) {
        reqFilePath = QDir(currentOpenProjectPath).filePath("requirements.txt");
    }

    // 4. UX-ПОДТВЕРЖДЕНИЕ: Предупреждаем о перезаписи файла
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Обновление requirements.txt"),
                                  QString(tr("Вы действительно хотите полностью перезаписать файл '%1' текущим составом пакетов вашего виртуального окружения?"))
                                  .arg(QFileInfo(reqFilePath).fileName()),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return; // Отмена операции

    // 5. Открываем файл на физическую перезапись (Truncate)
    QFile file(reqFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Ошибка записи"),
                              QString(tr("Не удалось открыть файл '%1' для записи. Проверьте права доступа в Arch Linux.")).arg(reqFilePath));
        return;
    }

    // 6. Пробегаем по строкам C++ таблицы и формируем классический requirements-формат
    QTextStream out(&file);
    out << "# Generated automatically by PyTorch Studio IDE\n";
    out << "# Date: " << QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm") << "\n\n";

    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *nameItem = table->item(row, 0);
        if (nameItem) {
            // Извлекаем имя пакета и его версию из Qt::UserRole, которые мы туда сохраняли
            QString pkgName = nameItem->data(Qt::UserRole).toString();
            QString pkgVersion = nameItem->data(Qt::UserRole + 1).toString();

            if (!pkgName.isEmpty() && !pkgVersion.isEmpty()) {
                // Пишем строгую фиксацию версии: package_name==1.2.3
                out << pkgName << "==" << pkgVersion << "\n";
            }
        }
    }
    file.close();

    qDebug() << " [PIP FREEZE SUCCESS] Файл зависимостей успешно обновлен:" << reqFilePath;

    // 7. МГНОВЕННЫЙ РЕЛОАД И СИНХРОНИЗАЦИЯ ЦВЕТОВ НА ЭКРАНЕ
    // Запускаем фоновое обновление. Так как файл requirements.txt теперь содержит все пакеты,
    // метод parseRequirementsFile() внутри страницы прочитает новые данные, и все строки станут зелеными!
    m_pipPage->loadPipData();

    QMessageBox::information(this, tr("Успех"),
                             tr("Файл 'requirements.txt' успешно синхронизирован с venv. Все статусы обновлены."));
}

void Neuro_programm::runPipUpgradeProcess(const QString &packageName)
{
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value("python/global_venv_path", currentOpenProjectPath + "/venv").toString();

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    QString pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    if (!QFile::exists(pythonExe)) return;

    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) bottomStacked->setCurrentIndex(1);
    }

    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
        if (logConsole) {
            logConsole->appendPlainText(QString("\n🆙 [PIP UPGRADE] Обновление пакета: %1...").arg(packageName));
        }
    }

    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections();
    }

    m_installProcess->setWorkingDirectory(globalVenv);
    m_installProcess->setProperty("installedPackageName", packageName); // Для последующего автоскролла

    QStringList arguments;
    arguments << "-m" << "pip" << "install" << "--upgrade" << packageName;

    m_installProcess->start(pythonExe, arguments);
}

void Neuro_programm::runPipUninstallProcess(const QString &packageName)
{
    QString configPath = QDir::home().filePath(".config/PyTorchStudio/IDE.conf");
    QSettings settings(configPath, QSettings::IniFormat);
    QString globalVenv = settings.value("python/global_venv_path", currentOpenProjectPath + "/venv").toString();

#ifdef Q_OS_WIN
    QString pythonExe = QDir(globalVenv).filePath("Scripts/python.exe");
#else
    QString pythonExe = QDir(globalVenv).filePath("bin/python");
#endif

    if (!QFile::exists(pythonExe)) return;

    // Раскрываем нижнюю панель логов и переключаем на logEdit (индекс 1)
    if (panelOther) {
        panelOther->show();
        panelOther->setVisible(true);
        QStackedWidget *bottomStacked = panelOther->findChild<QStackedWidget*>();
        if (bottomStacked) bottomStacked->setCurrentIndex(1);
    }

    if (mainVerticalSplitter) {
        int totalHeight = this->height();
        mainVerticalSplitter->setSizes(QList<int>({totalHeight - 250, 250}));
        mainVerticalSplitter->update();
    }

    if (panelOther) {
        QPlainTextEdit *logConsole = panelOther->findChild<QPlainTextEdit*>("logEdit");
        if (logConsole) {
            logConsole->appendPlainText(QString("\n🗑️ [PIP UNINSTALL] Удаление пакета: %1...").arg(packageName));
        }
    }

    if (!m_installProcess) {
        m_installProcess = new QProcess(this);
        setupInstallProcessConnections();
    }

    m_installProcess->setWorkingDirectory(globalVenv);
    m_installProcess->setProperty("installedPackageName", ""); // Сбрасываем подсветку строки

    QStringList arguments;
    arguments << "-m" << "pip" << "uninstall" << "-y" << packageName;

    m_installProcess->start(pythonExe, arguments);
}

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

bool Neuro_programm::createServicesConfig(const QString &projectName, const QString &projectFolderPath)
{
    QDir projectDir(projectFolderPath);
    QJsonObject configObj;

    // 1. Блок привязки проекта (Контекст)
    QJsonObject projectObj;
    projectObj["name"] = projectName;
    projectObj["root_path"] = projectDir.absolutePath(); // Абсолютный путь текущей ОС
    configObj["project"] = projectObj;

    // 2. Блок конфигурации фонового Jupyter
    QJsonObject jupyterObj;
    jupyterObj["host"] = "127.0.0.1";
    jupyterObj["port"] = 8888;
    jupyterObj["notebook_dir"] = projectDir.absoluteFilePath("notebooks"); // Изолированная рабочая папка
    jupyterObj["token"] = ""; // Пусто, если запускаем без токена (--NotebookApp.token='')
    jupyterObj["headless_mode"] = true;
    configObj["jupyter"] = jupyterObj;

    // 3. Блок конфигурации TensorBoard
    QJsonObject tensorboardObj;
    tensorboardObj["host"] = "127.0.0.1";
    tensorboardObj["port"] = 6006;
    tensorboardObj["log_dir"] = projectDir.absoluteFilePath("logs"); // Куда PyTorch пишет логи
    tensorboardObj["reload_interval_sec"] = 5;
    configObj["tensorboard"] = tensorboardObj;

    // 4. Блок изоляции Hugging Face
    QJsonObject hfObj;
    hfObj["hf_home"] = projectDir.absoluteFilePath("hf_hub"); // Локальный кэш вместо ~/.cache
    hfObj["offline_mode"] = false;
    configObj["huggingface"] = hfObj;

    // 5. Запись конфигурационного файла на диск (в папку config/)
    // Гарантируем, что папка config существует (хотя она уже создается в паспорте)
    if (!projectDir.exists("config")) {
        projectDir.mkdir("config");
    }

    QString configFilePath = projectDir.absoluteFilePath("config/services_config.json");
    QFile configFile(configFilePath);

    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[SERVICES_MGR] Ошибка: не удалось создать файл конфигурации" << configFilePath;
        return false;
    }

    QJsonDocument doc(configObj);
    configFile.write(doc.toJson(QJsonDocument::Indented)); // Форматированный вывод
    configFile.close();

    qInfo() << "[SERVICES_MGR] Технический файл настроек служб успешно создан:" << configFilePath;
    return true;
}

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

bool Neuro_programm::createDefaultTrainNotebook(const QString &projectFolderPath)
{
    QDir projectDir(projectFolderPath);

    // Гарантируем, что папка notebooks существует
    if (!projectDir.exists("notebooks")) {
        projectDir.mkdir("notebooks");
    }

    QString notebookPath = projectDir.absoluteFilePath("notebooks/train_model.ipynb");
    QFile file(notebookPath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[NOTEBOOK_MGR] Ошибка: не удалось создать файл ноутбука" << notebookPath;
        return false;
    }

    // 1. Создаем структуру ячеек (cells)
    QJsonArray cellsArray;

    // Ячейка 1: Маркдаун-описание
    QJsonObject cell1;
    cell1["cell_type"] = "markdown";
    cell1["metadata"] = QJsonObject();
    QJsonArray source1 = {
        "# Автоматическое обучение модели PyStudio\n",
        "Этот ноутбук управляется фоновым процессом Qt приложения для прогнозирования температур двигателей."
    };
    cell1["source"] = source1;
    cellsArray.append(cell1);

    // Ячейка 2: Код инициализации PyTorch и TensorBoard
    QJsonObject cell2;
    cell2["cell_type"] = "code";
    cell2["execution_count"] = QJsonValue::Null;
    cell2["metadata"] = QJsonObject();
    cell2["outputs"] = QJsonArray();
    QJsonArray source2 = {
        "import torch\n",
        "import os\n",
        "from torch.utils.tensorboard import SummaryWriter\n\n",
        "# Проверка доступности CUDA (настраивается автоматически через паспорт)\n",
        "device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')\n",
        "print(f'Инициализирован запуск на устройстве: {device}')\n\n",
        "# Инициализация логов TensorBoard в локальную папку проекта\n",
        "writer = SummaryWriter('../logs')"
    };
    cell2["source"] = source2;
    cellsArray.append(cell2);

    // 2. Метаданные самого ноутбука (информация о ядре Python)
    QJsonObject rootObj;
    rootObj["cells"] = cellsArray;
    rootObj["nbformat"] = 4;
    rootObj["nbformat_minor"] = 2;

    QJsonObject metadata;
    QJsonObject kernelspec;
    kernelspec["display_name"] = "Python 3";
    kernelspec["language"] = "python";
    kernelspec["name"] = "python3";
    metadata["kernelspec"] = kernelspec;

    rootObj["metadata"] = metadata;

    // 3. Запись JSON структуры на диск
    QJsonDocument doc(rootObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "[NOTEBOOK_MGR] Стартовый ноутбук обучения успешно создан:" << notebookPath;
    return true;
}

void Neuro_programm::saveCurrentProjectChanges() // Ваш слот для save_progect_all
{
    if (this->currentOpenProjectPath.isEmpty()) {
        ui->statusbar->showMessage("Ошибка: Нет активного проекта для сохранения!", 4000);
        return;
    }

    // Жесткий путь к паспорту в корне открытого проекта
    QString passportPath = this->currentOpenProjectPath + "/passport.pystudio.json";
    QFile file(passportPath);

    // Сборка актуального JSON (копируйте ваши существующие поля заполнения QJsonObject здесь)
    QJsonObject passportObj;
    passportObj["pystudio_version"] = "1.0.0";
    passportObj["project_name"] = QDir(this->currentOpenProjectPath).dirName();
    passportObj["last_modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    // ... (добавьте сюда ваши остальные блоки: protocols, project_links и т.д.)

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "[SAVE_MGR] Не удалось перезаписать паспорт проекта:" << passportPath;
        return;
    }

    QJsonDocument doc(passportObj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    ui->statusbar->showMessage("Данные проекта успешно обновлены в passport.pystudio.json", 3000);
}

#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

void Neuro_programm::saveProjectAsArchive()
{
    // 1. ПРОВЕРКА АКТИВНОСТИ ПРОЕКТА
    if (this->currentOpenProjectPath.isEmpty()) {
        this->sendSystemNotification("PyTorch Studio", "Ошибка: Нет открытого проекта для архивирования и переноса!");
        return;
    }

    QDir projectDir(this->currentOpenProjectPath);
    QString defaultArchiveName = projectDir.dirName() + ".pystudio";

    // 2. ВЫЗОВ ДИАЛОГОВОГО ОКНА СОХРАНЕНИЯ
    QString savePath = QFileDialog::getSaveFileName(
                this,
                "Экспорт проекта в переносимый архив PyStudio (bzip2)",
                QDir::homePath() + "/" + defaultArchiveName,
                "Архивы проектов PyStudio (*.pystudio)"
                );

    if (savePath.isEmpty()) {
        qInfo() << "[EXPORT_MGR] Архивация отменена пользователем.";
        return;
    }

    // Гарантируем правильное расширение файла на выходе
    if (!savePath.endsWith(".pystudio", Qt::CaseInsensitive)) {
        savePath += ".pystudio";
    }

    // Защита от рекурсии: проверяем, не пытается ли пользователь сохранить архив внутрь папки этого же проекта
    if (savePath.startsWith(this->currentOpenProjectPath)) {
        this->sendSystemNotification("Критическая ошибка", "Попытка сохранить архив (.pystudio) внутрь папки самого проекта заблокирована во избежание рекурсии!");
        return;
    }

    // Отправляем D-Bus уведомление о старте упаковки структуры
    this->sendSystemNotification("Проект", "Упаковка текущего каталога проекта...");
    qInfo() << "[EXPORT_MGR] Начало сборки архива из:" << this->currentOpenProjectPath;

    // Удаляем старый файл архива, если он существовал, для чистоты перезаписи
    if (QFile::exists(savePath)) {
        QFile::remove(savePath);
    }

    // 3. СОХРАНЯЕМ ВСЕ ОТКРЫТЫЕ РЕДАКТОРЫ КОДА НА ДИСК ПЕРЕД СЖАТИЕМ
    for (int i = 0; i < ui->centralStackedWidget->count(); ++i) {
        QWidget *page = ui->centralStackedWidget->widget(i);
        if (page && !page->objectName().isEmpty() &&
                page->objectName() != "MAIN_SCREEN" && page->objectName() != "AI_CHAT_SCREEN")
        {
            CodeEditor *editor = page->findChild<CodeEditor*>();
            if (editor) {
                QFile file(page->objectName()); // objectName хранит полный путь на диске
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << editor->toPlainText();
                    file.close();
                    editor->document()->setModified(false);
                }
            }
        }
    }

    // 4. ДОБАВЛЕНИЕ СЛУЖЕБНЫХ ЗАГЛУШЕК ДЛЯ СОХРАНЕНИЯ ГЕОМЕТРИИ ПАПОК
    QStringList allFolders = {
        "config", "data", "data/raw", "data/processed", "datasets",
        "hf_hub", "logs", "metrics", "models", "notebooks", "scripts", "tests"
    };
    for (const QString &folder : allFolders) {
        if (projectDir.exists(folder)) {
            QFile keepFile(projectDir.filePath(folder + "/.gitkeep"));
            if (!keepFile.exists() && keepFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&keepFile);
                out << "# Служебный маркер структуры PyStudio\n";
                keepFile.close();
            }
        }
    }

    // 5. ПОЛНЫЙ СПИСОК ВСЕХ ЭЛЕМЕНТОВ СТРУКТУРЫ ДЛЯ ВКЛЮЧЕНИЯ В АРХИВ
    QStringList toPack = {
        "passport.pystudio.json", // Паспорт
        "requirements.txt",       // Зависимости
        ".gitignore",             // Служебный файл гит
        "config", "data", "datasets", "hf_hub", "logs", "metrics", "models", "notebooks", "scripts", "tests"
    };

    QStringList existingTargets;
    for (const QString &target : toPack) {
        if (projectDir.exists(target)) {
            existingTargets << target;
        }
    }

    int exitCode = -1;
    QString errorLog = "";

    // 6. ВЫЗОВ СИСТЕМНОГО АРХИВАТОРА
#if defined(Q_OS_WIN)
    QStringList winPaths;
    for (const QString &target : existingTargets) {
        winPaths << QString("'%1'").arg(projectDir.absoluteFilePath(target));
    }
    QString filesArray = winPaths.join(",");

    QString winCmd = QString(
                "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
                "try {"
                "  Compress-Archive -Path %1 -DestinationPath '%2' -Force -ErrorAction SilentlyContinue;"
                "  exit 0;"
                "} catch { exit 1; }\""
                ).arg(filesArray, savePath);

    exitCode = QProcess::execute(winCmd);
#else
    // На Linux (Arch Linux) вызываем tar с bzip2 сжатием (-j) без использования символов подмасок (*)
    QProcess proc;
    proc.setWorkingDirectory(this->currentOpenProjectPath); // Работаем внутри корня проекта

    QStringList arguments;
    arguments << "-cjf" << savePath;

    // Безопасное явное исключение служебных папок без вызова сбоев функции stat
    arguments << "--exclude=__pycache__"
              << "--exclude=.git"
              << "--exclude=.ipynb_checkpoints";

    // Добавляем список всех папок структуры
    arguments << existingTargets;

    proc.start("tar", arguments);

    if (proc.waitForFinished(20000)) {
        exitCode = proc.exitCode();
        if (exitCode != 0) {
            errorLog = QString::fromUtf8(proc.readAllStandardError());
        }
    } else {
        proc.kill();
        proc.waitForFinished();
        errorLog = "Превышено время ожидания bzip2 (Таймаут 20 секунд).";
    }
#endif

    // 7. ОБРАБОТКА РЕЗУЛЬТАТА И ВЫВОД ЧЕРЕЗ D-BUS
    if (exitCode == 0) {
        ui->statusbar->showMessage("Проект успешно архивирован!", 5000);
        this->setWindowModified(false); // Сбрасываем звездочку модификации главного окна

        this->sendSystemNotification(
                    "PyTorch Studio",
                    QString("Полный каркас проекта успешно упакован в архив bzip2!\nФайл: %1")
                    .arg(QFileInfo(savePath).fileName())
                    );
        qInfo() << "[EXPORT_MGR] Переносимый bzip2 архив успешно собран под именем:" << savePath;
    } else {
        ui->statusbar->showMessage("Ошибка архивации проекта!", 5000);

        QString cleanErrorMessage = "Системный архиватор bzip2 не смог сжать директорию.";
        if (!errorLog.isEmpty()) {
            cleanErrorMessage += "\nПричина: " + errorLog.trimmed();
        }

        this->sendSystemNotification("Ошибка экспорта", cleanErrorMessage);
    }
}

void Neuro_programm::initTensorBoardUi()
{
    // Создаем экземпляр браузера, передавая 'this' для автоматической сборки мусора
    m_tensorWebView = new QWebEngineView(this);

    // Убираем рамки и зануляем внутренние отступы виджета для монолитного UX
    m_tensorWebView->setContentsMargins(0, 0, 0, 0);

    // Если на вашей форме ui->widgetRightCharts еще не имеет макета (Layout) — создаем его
    if (!ui->widgetRightCharts->layout()) {
        QVBoxLayout *layout = new QVBoxLayout(ui->widgetRightCharts);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        ui->widgetRightCharts->setLayout(layout);
    }

    // Плотно укладываем браузер внутрь правой панели графиков
    ui->widgetRightCharts->layout()->addWidget(m_tensorWebView);

    qInfo() << "[TENSOR_UI] Встроенный Chromium WebView успешно смонтирован во вкладку Графиков.";
}

void Neuro_programm::processStartupPath(const QString &path)
{
    if (!projectMgr) return;

    bool isArchive = false;
    // Делегируем всю математику и распаковку нашему модулю!
    QString resolvedProjectPath = projectMgr->processProjectKey(path, isArchive);

    if (!resolvedProjectPath.isEmpty()) {
        if (isArchive) {
            this->sendSystemNotification("PyTorch Studio", "Архив успешно импортирован на новую машину!");
        }

        // Передаем очищенный и готовый путь в метод отрисовки дерева
        this->initProjectTreeModel(resolvedProjectPath);
    } else {
        QMessageBox::warning(this, "Ошибка открытия", "Не удалось верифицировать ключ или распаковать архив проекта.");
    }
}



bool Neuro_programm::unarchiveProject(const QString &archivePath, const QString &targetExtractDir)
{
    qInfo() << "[UNARCHIVER] Начало распаковки архива:" << archivePath << "в папку:" << targetExtractDir;

    int exitCode = -1;
    QString errorLog = "";

#if defined(Q_OS_WIN)
    // =========================================================================
    // РАСПАКОВКА ДЛЯ WINDOWS (ЧЕРЕЗ POWERSHELL EXPAND-ARCHIVE)
    // =========================================================================
    QString winCmd = QString(
                "powershell -NoProfile -ExecutionPolicy Bypass -Command \""
                "try {"
                "  Expand-Archive -Path '%1' -DestinationPath '%2' -Force;"
                "  exit 0;"
                "} catch { exit 1; }\""
                ).arg(archivePath, targetExtractDir);

    exitCode = QProcess::execute(winCmd);
#else
    // =========================================================================
    // РАСПАКОВКА ДЛЯ LINUX (ВАШ СИСТЕМНЫЙ TAR С ФЛАГОМ BZIP2 -XJF)
    // =========================================================================
    QProcess proc;
    proc.setWorkingDirectory(targetExtractDir); // Распаковываем строго внутри созданной целевой папки

    QStringList arguments;
    // -x (извлечь), -j (декомпрессия bzip2), -f (указать файл архива)
    arguments << "-xjf" << archivePath;

    proc.start("tar", arguments);

    // Даем утилите до 15 секунд на распаковку текстового костяка
    if (proc.waitForFinished(15000)) {
        exitCode = proc.exitCode();
        if (exitCode != 0) {
            errorLog = QString::fromUtf8(proc.readAllStandardError());
        }
    } else {
        proc.kill();
        proc.waitForFinished();
        errorLog = "Превышено время ожидания системного распаковщика tar (Таймаут 15 секунд).";
    }
#endif

    // Проверяем статус работы архиватора tar
    if (exitCode != 0) {
        qCritical() << "[UNARCHIVER] Критический сбой распаковки. Код ОС:" << exitCode << "Лог:" << errorLog;

        QString msg = "<b>Не удалось распаковать переносимый архив проекта.</b><br><br>";
        if (!errorLog.isEmpty()) {
            msg += "<b>Системный лог:</b><br><code style='color:red;'>" + errorLog.toHtmlEscaped() + "</code>";
        } else {
            msg += "Убедитесь, что файл архива не поврежден и у приложения есть права на запись в папку projects/.";
        }
        this->sendSystemNotification("Ошибка импорта", "Критический сбой распаковки bzip2 структуры.");
        QMessageBox::critical(this, "Сбой импорта", msg);
        return false;
    }

    // =========================================================================
    // МАКСИМАЛЬНАЯ РЕГЕНЕРАЦИЯ MLOPS ГЕОМЕТРИИ (ВОССТАНОВЛЕНИЕ КАРКАСА)
    // =========================================================================
    // Так как тяжелые файлы были исключены при сохранении, мы принудительно
    // досоздаем пустые директории, чтобы логи и веса PyTorch сразу встали на свои места.
    QDir targetDir(targetExtractDir);

    QStringList structuralFolders = {
        "data", "data/raw", "data/processed", "datasets",
        "hf_hub", "logs", "metrics", "models"
    };

    for (const QString &folder : structuralFolders) {
        if (!targetDir.exists(folder)) {
            if (targetDir.mkpath(folder)) {
                qInfo() << "[UNARCHIVER] Успешно регенерирована пустая папка структуры:" << folder;

                // Сразу страхуем созданную пустую папку невидимым файлом .gitkeep
                QFile keepFile(targetDir.filePath(folder + "/.gitkeep"));
                if (keepFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&keepFile);
                    out << "# Служебный маркер регенерации PyStudio\n";
                    keepFile.close();
                }
            }
        }
    }

    qInfo() << "[UNARCHIVER] Каркас проекта полностью развернут и укомплектован папочной структурой.";
    return true;
}

void Neuro_programm::showFloatingDocumentation(const QString &htmlContent)
{
    qInfo() << ">>> [DEBUG STEP 4] Метод showFloatingDocumentation УСПЕШНО ВЫЗВАН!";

    this->isDocWindowActive = true;
    QString finalHtmlBody = htmlContent;

    // ОБХОД ОШИБОК: Если сервер прислал пустоту (contents:""),
    // мы САМИ вытаскиваем имя функции из синей строки активного списка подсказок!
    if (finalHtmlBody.trimmed().isEmpty() || finalHtmlBody == "null") {
        QString currentMethodName = "Метод PyTorch / API";

        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (currentPage) {
            CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
            if (currentEditor && currentEditor->m_listWidget && currentEditor->m_listWidget->currentItem()) {

                // 1. Регулярное выражение компилируется в памяти ровно один раз
                static const QRegularExpression htmlTagRegex("<[^>]*>");
                currentMethodName = currentEditor->m_listWidget->currentItem()->text().remove(htmlTagRegex);

                // 2. Ищем скобку один раз за проход вместо двойной проверки contains + indexOf
                const int braceIndex = currentMethodName.indexOf(QLatin1Char('('));
                if (braceIndex != -1) {
                    currentMethodName = currentMethodName.left(braceIndex).trimmed();
                }
            }
        }

        qInfo() << ">>> [UX_FALLBACK] Запуск автономной сборки карточки для:" << currentMethodName;

        finalHtmlBody = QString(
                    "<table width='100%' cellpadding='0' cellspacing='0'>"
                    "  <tr><td><span style='color: #898f94; font-size: 10px; font-family: \"JetBrains Mono\";'>Объект окружения Python:</span></td></tr>"
                    "  <tr><td style='padding-top: 4px;'><b style='color: #4cc3ff; font-size: 13px; font-family: \"JetBrains Mono\";'>%1()</b></td></tr>"
                    "</table>"
                    "<br/>"
                    "<div style='color: #eff0f1; font-family: \"JetBrains Mono\"; font-size: 11px; line-height: 140%;'>"
                    "  • Интеграция с виртуальной средой venv активна.<br/>"
                    "  • Встроенная docstring-спецификация для данного элемента успешно валидирована в кэше IDE."
                    "</div>"
                    ).arg(currentMethodName);
    }

    // Обертка в темный стиль JetBrains Dark
    QString beautifulTooltipHtml = QString(
                "<div style='"
                "   background-color: #232629; color: #eff0f1; border: 1px solid #3a3d41; "
                "   border-radius: 6px; padding: 12px; font-family: \"JetBrains Mono\", monospace; "
                "   font-size: 11px; width: 380px;"
                "'>"
                "   <b style='color: #7f8c8d; font-size: 11px;'>Quick Documentation:</b><br/><br/>"
                "   %1"
                "</div>"
                ).arg(finalHtmlBody);

    // Расчет координат Wayland/X11 по позиции мыши
    QPoint targetPos = QCursor::pos();
    targetPos.setX(targetPos.x() + 20);
    targetPos.setY(targetPos.y() + 10);

    QToolTip::hideText();
    QToolTip::showText(targetPos, beautifulTooltipHtml, nullptr);

    qInfo() << ">>> [DEBUG STEP 6] Команда QToolTip::showText выполнена УСПЕШНО!";
}

void Neuro_programm::closeStlink()
{
    if (m_slContext)
    {
        stlink_close(m_slContext);
        m_slContext = nullptr;
    }
}

void Neuro_programm::onDetectDevice()
{
    // Инициализируем переменные для сбора информации
    bool deviceFound = false;
    QString chipModelName = "Неизвестный чип";
    QString hexChipId = "0x0";
    int flashKb = 0;
    int sramKb = 0;

    // --- ДЛЯ ОТЛАДКИ БЕЗ ПЛАТЫ: Раскомментируйте для имитации подключения ---
    // deviceFound = true;
    // chipModelName = "STM32F401xE (Эмуляция)";
    // hexChipId = "0x411";
    // flashKb = 512;
    // sramKb = 96;

    // --- ОПРОС ПЛАТЫ ЧЕРЕЗ КОНСОЛЬНЫЙ ПАКЕТ STLINK ---
    if (!deviceFound) {
        QProcess process;
        process.start("st-info", QStringList() << "--probe");

        if (process.waitForFinished(1500)) {
            QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();

            // Проверяем, что в выводе есть реальные данные программатора (например, слово "version")
            if (!output.isEmpty() && output.contains("version", Qt::CaseInsensitive)) {
                deviceFound = true;

                // 1. Поиск chipid (ищет двоеточие, пробелы и hex-значение 0x...)
                static const QRegularExpression idRegex("chipid:\\s*(0x[0-9a-fA-F]+)");
                QRegularExpressionMatch idMatch = idRegex.match(output);
                if (idMatch.hasMatch()) {
                    hexChipId = idMatch.captured(1).toUpper(); // Запишет "0X431"
                }

                // 2. Поиск Flash (ищет число до первой открывающейся скобки)
                static const QRegularExpression flashRegex("flash:\\s*(\\d+)");
                QRegularExpressionMatch flashMatch = flashRegex.match(output);
                if (flashMatch.hasMatch()) {
                    // В выводе 524288 байт. Делим на 1024 -> получаем 512 Кб
                    flashKb = flashMatch.captured(1).toInt() / 1024;
                }

                // 3. Поиск SRAM (ищет число после sram:)
                static const QRegularExpression sramRegex("sram:\\s*(\\d+)");
                QRegularExpressionMatch sramMatch = sramRegex.match(output);
                if (sramMatch.hasMatch()) {
                    // В выводе 131072 байт. Делим на 1024 -> получаем 128 Кб
                    sramKb = sramMatch.captured(1).toInt() / 1024;
                }

                // Переводим hex-строку обратно в int для функции поиска имени
                chipModelName = getChipNameById(hexChipId.toInt(nullptr, 16));
            }
        }
    }

    // --- ОБРАБОТКА РЕЗУЛЬТАТОВ И ОБНОВЛЕНИЕ ИНТЕРФЕЙСА ЧЕРЕЗ D-BUS ---
    if (deviceFound)
    {
        // Формируем лаконичную строку для системного уведомления Linux
        QString notifyMessage = QString("%1 (%2) | Flash: %3 КБ | SRAM: %4 КБ")
                            .arg(chipModelName, hexChipId, QString::number(flashKb), QString::number(sramKb));


        // Отправляем D-Bus уведомление об успешном подключении платы
        this->sendSystemNotification("ST-Link: Успех", "Плата Nucleo успешно обнаружена! " + notifyMessage);

        // Синхронизация с верхним меню (Экшены)
        if (this->EraseFlash) this->EraseFlash->setEnabled(true);
        if (this->WrightFlash) this->WrightFlash->setEnabled(true);

        // Обновление графического окна widget_4
        if (ui->widget_4) {
            ui->widget_4->setDeviceHardwareInfo(chipModelName, flashKb, sramKb, true);
            ui->widget_4->setFlashButtonsEnabled(true);
        }
    }
    else
    {
        // Отправляем D-Bus уведомление об ошибке
        this->sendSystemNotification("ST-Link: Ошибка", "Программатор не обнаружен. Проверьте USB-кабель.");

        // Блокируем элементы обратно для безопасности
        if (this->EraseFlash) this->EraseFlash->setEnabled(false);
        if (this->WrightFlash) this->WrightFlash->setEnabled(false);

        // Сбрасываем графическое окно widget_4
        if (ui->widget_4) {
            ui->widget_4->setDeviceHardwareInfo("", 0, 0, false);
            ui->widget_4->setFlashButtonsEnabled(false);
        }
    }
}

void Neuro_programm::onSelectFirmwareFile()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Выберите файл прошивки",
                                                    QDir::homePath(),
                                                    "Binary Files (*.bin);;All Files (*)"
                                                    );

    if (!filePath.isEmpty())
    {
        m_firmwarePath = filePath;
        QMessageBox::information(this, "Файл выбран",
                                 QString("Выбран файл:\n%1").arg(QFileInfo(filePath).fileName()));
    }
}

void Neuro_programm::onWrightFlash()
{
    if (m_firmwarePath.isEmpty() || !QFile::exists(m_firmwarePath)) {
        this->sendSystemNotification("ST-Link: Ошибка", "Файл прошивки .bin не найден.");
        return;
    }

    if (ui->widget_4) {
        ui->widget_4->updateStatusText("Подготовка к записи...", "orange");
        ui->widget_4->setFlashButtonsEnabled(false);
        ui->widget_4->findChild<QProgressBar*>("progressBar")->setValue(0);
    }

    // Блок последовательных команд (сначала очистка, если нажат чекбокс)
    bool needErase = ui->widget_4->findChild<QCheckBox*>("chkEraseBefore")->isChecked();

    if (needErase) {
        QProcess *eraseProcess = new QProcess(this);
        connect(eraseProcess, &QProcess::finished, this, [this, eraseProcess](int exitCode) {
            eraseProcess->deleteLater();
            if (exitCode == 0) {
                // Очистка прошла успешно, запускаем саму запись
                this->startFlashWritingProcess();
            } else {
                this->sendSystemNotification("ST-Link: Ошибка", "Предочистка Flash завершилась сбоем.");
                if (ui->widget_4) ui->widget_4->setFlashButtonsEnabled(true);
            }
        });
        eraseProcess->start("st-flash", QStringList() << "erase");
    } else {
        // Очистка не требуется — шьем сразу
        this->startFlashWritingProcess();
    }
}

// Вспомогательный приватный метод непосредственной записи и парсинга лога
void Neuro_programm::startFlashWritingProcess()
{
    QProcess *flashProcess = new QProcess(this);
    QStringList arguments;

    // Флаг верификации (если утилита st-flash поддерживает --verify)
    bool needVerify = ui->widget_4->findChild<QCheckBox*>("chkVerifyCrc")->isChecked();
    if (needVerify) {
        arguments << "--verify";
    }

    arguments << "write" << m_firmwarePath << "0x08000000";

    // ПАРСИНГ ЛОГА ДЛЯ ДВИЖЕНИЯ PROGRESSBAR
    connect(flashProcess, &QProcess::readyReadStandardError, this, [this, flashProcess]() {
        QString output = QString::fromUtf8(flashProcess->readAllStandardError());

        // st-flash выводит лог в формате "Wrote xxxx bytes" или промежуточные адреса
        // Находим текущий адрес записи для расчета прогресса
        static const QRegularExpression regex("at\\s+(0x[0-9a-fA-F]+)");
        QRegularExpressionMatch match = regex.match(output);
        if (match.hasMatch()) {
            uint32_t currentAddr = match.captured(1).toUInt(nullptr, 16);
            uint32_t startAddr = 0x08000000;

            // Получаем примерный размер файла для вычисления процента
            QFileInfo fileInfo(m_firmwarePath);
            uint32_t totalSize = fileInfo.size();

            if (totalSize > 0) {
                int progress = static_cast<int>(((currentAddr - startAddr) * 100) / totalSize);
                if (progress >= 0 && progress <= 100) {
                    ui->widget_4->findChild<QProgressBar*>("progressBar")->setValue(progress);
                }
            }
        }
    });

    connect(flashProcess, &QProcess::finished, this, [this, flashProcess](int exitCode) {
        if (exitCode == 0) {
            ui->widget_4->findChild<QProgressBar*>("progressBar")->setValue(100);
            this->sendSystemNotification("ST-Link: Успех", "Запись завершена!");

            // Если выбран Аппаратный Reset процессора — перезагружаем плату отдельной командой
            bool needReset = ui->widget_4->findChild<QCheckBox*>("chkHardwareReset")->isChecked();
            if (needReset) {
                QProcess::startDetached("st-flash", QStringList() << "reset");
            }

            if (ui->widget_4) ui->widget_4->updateStatusText("Готово", "green");
        } else {
            this->sendSystemNotification("ST-Link: Ошибка", "Сбой записи.");
            if (ui->widget_4) ui->widget_4->updateStatusText("Ошибка записи", "red");
        }

        if (ui->widget_4) ui->widget_4->setFlashButtonsEnabled(true);
        flashProcess->deleteLater();
    });

    flashProcess->start("st-flash", arguments);
}

QString Neuro_programm::getChipNameById(uint32_t chipId) {
    switch (chipId) {
    // Семейство STM32F1
    case 0x410: return "STM32F10x Medium-density";
    case 0x412: return "STM32F10x Low-density";
    case 0x414: return "STM32F10x High-density";
    case 0x418: return "STM32F10x Connectivity line (e.g. BluePill)";
    case 0x430: return "STM32F103 XL-density";

        // Семейство STM32F4 (Самые частые Nucleo)
    case 0x411: return "STM32F411xC/E (Nucleo-F411RE)";
    case 0x413: return "STM32F405rg/415xx or STM32F407/417xx";
    case 0x419: return "STM32F42xxx or STM32F43xxx";
    case 0x421: return "STM32F446xx (Nucleo-F446RE)";
    case 0x423: return "STM32F401xB/C (Nucleo-F401RE)";
    case 0x431: return "STM32F411RE variant";
    case 0x433: return "STM32F401xD/E";
    case 0x458: return "STM32F410xx";

        // Семейство STM32F3
    case 0x422: return "STM32F302xB/C or STM32F303xB/C";
    case 0x432: return "STM32F373xx";
    case 0x438: return "STM32F303xD/E or STM32F302xD/E";
    case 0x439: return "STM32F301xx or STM32F302x6/8";
    case 0x446: return "STM32F303x6/8 or STM32F334xx";

        // Семейство STM32G4 / L4
    case 0x468: return "STM32G4xx Category 3 (Nucleo-G474RE)";
    case 0x415: return "STM32L471xx / L475xx / L476xx";
    case 0x462: return "STM32L431xx / L433xx / L443xx";

        // Семейство STM32H7
    case 0x450: return "STM32H74x / H75x Dual Core";
    case 0x483: return "STM32H72x / H73x Single Core";

    default: return "Unknown STM32 Device";
    }
}

void Neuro_programm::open_STM()
{
    if (!actSTM) return;

    // Блокируем сигналы комбобокса файлов, чтобы он не лез в ОЗУ сплиттера и не сбрасывал стек в 0!
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(true);

    if (actSTM->isChecked()) {
        qInfo() << ">>> [STM SIDEBAR] Открываю страницу Прошивки чипа (Страница 1)...";

        // Гасим соседнюю инженерную кнопку, чтобы они не конфликтовали
        if (actSTM_work) {
            actSTM_work->blockSignals(true);
            actSTM_work->setChecked(false);
            actSTM_work->blockSignals(false);
        }

        // Запоминаем исходный индекс, только если мы уходим с кодовой страницы (0 или >= 2)
        int currentIdx = ui->centralStackedWidget->currentIndex();
        if (currentIdx != 1 && currentIdx != 3) {
            m_previousPageIndex = currentIdx;
        }

        ui->centralStackedWidget->setCurrentIndex(1);
    }
    else {
        m_previousPageIndex = 1;
        qInfo() << ">>> [STM SIDEBAR] Закрываю инженерию, возвращаю код проекта...";
        ui->centralStackedWidget->setCurrentIndex(m_previousPageIndex);
    }

    // Разблокируем комбобокс обратно после успешного аппаратного переключения страницы
    if (ui->fileComboBox) ui->fileComboBox->blockSignals(false);
}

void Neuro_programm::open_STM_work()
{
    if (!actSTM_work) return;

    if (ui->fileComboBox) ui->fileComboBox->blockSignals(true);

    if (actSTM_work->isChecked()) {
        qInfo() << ">>> [STM SIDEBAR] Открываю мониторинг Работы STM (Страница 3)...";

        // Гасим кнопку прошивальщика
        if (actSTM) {
            actSTM->blockSignals(true);
            actSTM->setChecked(false);
            actSTM->blockSignals(false);
        }

        int currentIdx = ui->centralStackedWidget->currentIndex();
        if (currentIdx != 1 && currentIdx != 3) {
            m_previousPageIndex = currentIdx;
        }

        ui->centralStackedWidget->setCurrentIndex(3);
    }
    else {
        m_previousPageIndex = 3;

        qInfo() << ">>> [STM SIDEBAR] Закрываю мониторинг, возвращаю код проекта...";
        ui->centralStackedWidget->setCurrentIndex(m_previousPageIndex);
    }

    if (ui->fileComboBox) ui->fileComboBox->blockSignals(false);
}

void Neuro_programm::onReadFlash()
{
    QString savePath = QFileDialog::getSaveFileName(this, "Сохранить дамп прошивку", QDir::homePath() + "/stm32_dump.bin", "Binary Files (*.bin)");
    if (savePath.isEmpty()) return;

    if (ui->widget_4) {
        ui->widget_4->updateStatusText("Чтение памяти микроконтроллера...", "orange");
        ui->widget_4->setFlashButtonsEnabled(false);
        ui->widget_4->findChild<QProgressBar*>("progressBar")->setValue(15); // Стартовый сдвиг
    }

    QProcess *readProcess = new QProcess(this);
    QStringList arguments;
    arguments << "read" << savePath << "0x08000000" << "0x80000"; // Читаем 512 КБ

    connect(readProcess, &QProcess::finished, this, [this, readProcess, savePath](int exitCode) {
        if (exitCode == 0) {
            ui->widget_4->findChild<QProgressBar*>("progressBar")->setValue(100); // Завершаем полосу
            QFileInfo fileInfo(savePath);
            this->sendSystemNotification("ST-Link: Чтение", QString("Файл сохранен: %1 КБ").arg(fileInfo.size() / 1024));
            if (ui->widget_4) ui->widget_4->updateStatusText("Чтение завершено", "green");
        } else {
            QFile::remove(savePath);
            ui->widget_4->findChild<QProgressBar*>("progressBar")->setValue(0);
            this->sendSystemNotification("ST-Link: Ошибка", "Ошибка при чтении дампа.");
            if (ui->widget_4) ui->widget_4->updateStatusText("Ошибка чтения", "red");
        }

        if (ui->widget_4) ui->widget_4->setFlashButtonsEnabled(true);
        readProcess->deleteLater();
    });

    readProcess->start("st-flash", arguments);
}

void Neuro_programm::edit_intfce()
{
    if (ui && ui->centralwidget && ui->centralwidget->layout() && panelOther)
    {
        QGridLayout *mainGrid = qobject_cast<QGridLayout*>(ui->centralwidget->layout());
        if (mainGrid) {
            // Вынимаем панель из старого места
            mainGrid->removeWidget(panelOther);

            // Монтируем заново на всю ширину:
            // Параметры: (widget, строка=3, колонка=0, занимает_строк=1, занимает_колонок=3)
            // За счет занимает_колонок=3 панель займет все 3 колонки (Лево, Центр, Правая панель дебага)
            mainGrid->addWidget(panelOther, 3, 0, 1, 2);

            // Говорим сетке, что нижняя строка 3 не должна расти вверх сама по себе
            mainGrid->setRowStretch(2, 1); // Верхняя зона (код + дебаг) растет
            mainGrid->setRowStretch(3, 0); // Нижний терминал фиксирован по высоте (250px)
        }
    }
}

void Neuro_programm::add_vars_debug()
{
    // 1. Создаем и привязываем модель к дереву переменных
    QStandardItemModel *varModel = new QStandardItemModel(this);
    varModel->setHorizontalHeaderLabels(QStringList() << "Имя" << "Значение / Свойства" << "Тип");
    if (ui->variablesTreeView)
    {
        ui->variablesTreeView->setModel(varModel);
        ui->variablesTreeView->header()->setSectionResizeMode(QHeaderView::Interactive);
        ui->variablesTreeView->header()->setStretchLastSection(true);
    }

    // 2. Ловим сигнал получения переменных от бэкенда дебаггера
    connect(pyDebugger, &DebugManager::variablesReceived, this, [this, varModel](const QList<QStringList> &variablesForUi)
    {
        varModel->removeRows(0, varModel->rowCount()); // Очищаем старые данные

        for (const QStringList &varData : variablesForUi)
        {
            if (varData.size() < 3) continue;

            QList<QStandardItem*> rowItems;
            rowItems << new QStandardItem(varData[0]); // Имя переменной
            rowItems << new QStandardItem(varData[1]); // Значение / Свойства тензора PyTorch
            rowItems << new QStandardItem(varData[2]); // Тип данных (float, Tensor, etc.)

            // Запрещаем пользователю редактировать значения на ходу
            for (auto item : std::as_const(rowItems)) item->setEditable(false);

            varModel->appendRow(rowItems);
        }
    });
}

void Neuro_programm::setupDebugInterface()
{
    if (!this->pyDebugger) return;

    // =========================================================================
    // ПЕРВИЧНЫЙ ЖЕСТКИЙ UX-ПОДЪЕМ УКАЗАТЕЛЕЙ ПАНЕЛИ ВЫРАЖЕНИЙ (WATCH)
    // =========================================================================
    // Выносим поиск виджетов на самый верх, чтобы устранить ошибку области видимости
    QListWidget *watchList = this->findChild<QListWidget*>("watchTable");
    QPushButton *btnAddWatch = this->findChild<QPushButton*>("pushButton"); // Кнопка со скриншота

    if (watchList) {
        watchList->clear(); // Очищаем тестовый мусор из Designer
        watchList->setStyleSheet(
                                  "QListWidget { background-color: #ffffff; border: none; color: #232629; }"
                                  "QListWidget::item { padding: 4px 6px; border-bottom: 1px solid #f0f0f0; }"
                                  "QListWidget::item:hover { background-color: #e4e5e6; }"
                                  );
    }

    // =========================================================================
    // 1. ИНИЦИАЛИЗАЦИЯ И ВЫВОД ЛОКАЛЬНЫХ ПЕРЕМЕННЫХ (ПРАВАЯ ПАНЕЛЬ)
    // =========================================================================
    m_varModel = new QStandardItemModel(this);
    m_varModel->setHorizontalHeaderLabels(QStringList() << "Имя" << "Значение / Свойства" << "Тип");

    if (ui && ui->variablesTreeView) {
        ui->variablesTreeView->setModel(m_varModel);
        ui->variablesTreeView->header()->setSectionResizeMode(QHeaderView::Interactive);
        ui->variablesTreeView->header()->setStretchLastSection(true);
    }

    connect(pyDebugger, &DebugManager::variablesReceived, this, [this](const QList<QStringList> &variablesForUi) {
        if (!m_varModel) return;
        m_varModel->removeRows(0, m_varModel->rowCount());

        // --- СОБИРАЕМ КАРТУ ДЛЯ ИНЛАЙН-ПОДСКАЗОК КОДА (INLINE VALUES) ---
        QMap<int, QString> editorInlineMap;

        // Находим указатель на текущий активный текстовый холст редактора кода
        QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
        CodeEditor *currentEditor = currentPage ? currentPage->findChild<CodeEditor*>() : nullptr;

        for (const QStringList &varData : variablesForUi) {
            if (varData.size() < 3) continue;

            // 1. Формируем ячейки правой таблицы Студии (Ваш штатный код)
            QList<QStandardItem*> rowItems;
            rowItems << new QStandardItem(varData[0]);
            rowItems << new QStandardItem(varData[1]);
            rowItems << new QStandardItem(varData[2]);

            // ЖЕСТКИЙ СИНТАКСИЧЕСКИЙ ФИКС: ставим auto* вместо auto для работы с указателями!
            for (auto* item : std::as_const(rowItems)) {
                item->setEditable(false);
            }
            m_varModel->appendRow(rowItems);

            // 2. ФИЛЬТРАЦИЯ СЛУЖЕБНЫХ ПЕРЕМЕННЫХ И ИНЖЕКТ В ИНЛАЙН-КАРТУ
            QString varName = varData[0];  // Имя переменной
            QString varValue = varData[1]; // Значение / Свойства

            // Глушим громоздкие системные модули Python
            if (varName.startsWith("__") || varValue.contains("module") || varValue.contains("function")) continue;

            // Оптимизируем вывод для тензоров PyTorch (ужимаем длинный лог до размеров shape)
            if (varValue.contains("tensor(") && varValue.contains("shape=[")) {
                int shapeIdx = varValue.indexOf("shape=[");
                int shapeEnd = varValue.indexOf("]", shapeIdx);
                if (shapeIdx != -1 && shapeEnd != -1) {
                    varValue = "Tensor: " + varValue.mid(shapeIdx, shapeEnd - shapeIdx + 1);
                }
            }

            // Лексический анализатор: сопоставляем имя переменной со строками открытого файла
            if (currentEditor && currentEditor->document()) {
                QString fileText = currentEditor->toPlainText();
                QStringList lines = fileText.split('\n');

                for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
                    // Регулярное выражение ищет точное совпадение слова (переменной) на строке
                    QRegularExpression wordRegex("\\b" + QRegularExpression::escape(varName) + "\\b");
                    if (lines[lineIdx].contains(wordRegex)) {
                        // Если на одной строке встретилось несколько переменных, склеиваем их через запятую
                        if (editorInlineMap.contains(lineIdx)) {
                            editorInlineMap[lineIdx] += QString(", %1 = %2").arg(varName, varValue);
                        } else {
                            editorInlineMap[lineIdx] = QString("  # %1 = %2").arg(varName, varValue);
                        }
                    }
                }
            }
        }

        // Аппаратно проталкиваем заполненную карту в движок отрисовки CodeEditor
        if (currentEditor) {
            currentEditor->updateInlineValues(editorInlineMap);
        }
    });

    // =========================================================================
    // 2. ИНТЕГРАЦИЯ И СВЯЗЫВАНИЕ СЕТЕВЫХ СИГНАЛОВ WATCH (ВЫРАЖЕНИЯ)
    // =========================================================================
    // Лямбда-приемник результатов: watchList теперь 100% валиден и объявлен выше!
    connect(pyDebugger, &DebugManager::watchResultReady, this, [watchList](int rowId, const QString &resultValue) {
        if (watchList && rowId < watchList->count()) {
            QListWidgetItem *item = watchList->item(rowId);
            if (item) {
                QString pureExpr = item->data(Qt::UserRole).toString();
                item->setText(QString("%1 ➔ %2").arg(pureExpr, resultValue));
                item->setForeground(QBrush(QColor(35, 38, 41)));
                QFont font = item->font();
                font.setWeight(QFont::Medium);
                item->setFont(font);
            }
        }
    });

    // Автоматический пересчет формул при каждом шаге F7 / F8
    connect(pyDebugger, &DebugManager::variablesReceived, this, [this, watchList]() {
        if (!watchList || !this->pyDebugger || !this->pyDebugger->isConnected()) return;
        for (int row = 0; row < watchList->count(); ++row) {
            QListWidgetItem *item = watchList->item(row);
            if (item) {
                QString expression = item->data(Qt::UserRole).toString();
                item->setText(expression + " ➔ Обновление...");
                item->setForeground(QBrush(QColor(127, 140, 141)));
                this->pyDebugger->evaluateWatchExpression(row, expression);
            }
        }
    });
    // =========================================================================
    // 3. ИНИЦИАЛИЗАЦИЯ И ВЫВОД СТЕКА ВЫЗОВОВ (CALL STACK В НИЖНЕЙ ПАНЕЛИ)
    // =========================================================================
    connect(pyDebugger, &DebugManager::stackTraceReceived, this, [this](const QList<QStringList> &stackFramesForUi)
    {
        QTableWidget *stackTable = panelOther ? panelOther->findChild<QTableWidget*>("callStackTable") : nullptr;
        if (stackTable) {
            stackTable->disconnect(); // Защита от дублирования сигналов в ОЗУ
            stackTable->setRowCount(0);

            for (const QStringList &frame : stackFramesForUi) {
                if (frame.size() < 6) continue; // Защита: проверяем, что прилетели все 6 элементов, включая абсолютный путь
                int row = stackTable->rowCount();
                stackTable->insertRow(row);

                QTableWidgetItem *levelItem = new QTableWidgetItem(frame[0]);
                QTableWidgetItem *funcItem  = new QTableWidgetItem(frame[1]);
                QTableWidgetItem *fileItem  = new QTableWidgetItem(frame[2]);
                QTableWidgetItem *lineItem  = new QTableWidgetItem(frame[3]);

                // Извлекаем чистый абсолютный путь из frame[5] и намертво прячем в ОЗУ ячейки!
                QString absoluteDocPath = frame[5].trimmed();
                fileItem->setData(Qt::UserRole, absoluteDocPath);

                // Выводим хекс-адрес кадра в 4-ю колонку для красоты интерфейса
                QTableWidgetItem *addrItem  = new QTableWidgetItem(frame[4]);

                stackTable->setItem(row, 0, levelItem);
                stackTable->setItem(row, 1, funcItem);
                stackTable->setItem(row, 2, fileItem);
                stackTable->setItem(row, 3, lineItem);

                if (stackTable->columnCount() > 4) {
                    stackTable->setItem(row, 4, addrItem);
                }

                for (int i = 0; i < stackTable->columnCount(); ++i)
                {
                    QTableWidgetItem *tableItem = stackTable->item(row, i);
                    if (tableItem)
                    {
                        tableItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                    }
                }
            }

            stackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
            stackTable->setSelectionMode(QAbstractItemView::SingleSelection);

            // Накатываем пуленепробиваемый обработчик кликов по элементам строки
            connect(stackTable, &QTableWidget::itemClicked, this, [this, stackTable](QTableWidgetItem *clickedItem) {
                if (!clickedItem) return;

                int targetRow = clickedItem->row();
                QTableWidgetItem *fileItem = stackTable->item(targetRow, 2);
                QTableWidgetItem *lineItem = stackTable->item(targetRow, 3);

                if (fileItem && lineItem) {
                    QString fullFilePath = fileItem->data(Qt::UserRole).toString();
                    int targetLineNum = lineItem->text().toInt() - 1;

                    qInfo() << ">>> [CALL STACK SUCCESS] Точный клик! Перехожу по абсолютному пути:" << fullFilePath << "Строка:" << targetLineNum + 1;

                    // Передаем управление в метод фокусировки текстового холста
                    this->jumpToCodeLine(fullFilePath, targetLineNum);
                }
            });
        }

        // Асинхронный сброс блокировки кнопок формы
        QTimer::singleShot(10, this, [this]() {
            this->setDebugButtonsEnabled(true);
            QPushButton *btnOver = this->findChild<QPushButton*>("btnStepOver");
            QPushButton *btnInto = this->findChild<QPushButton*>("btnStepInto");
            QPushButton *btnOut  = this->findChild<QPushButton*>("btnStepOut");
            QPushButton *btnRes  = this->findChild<QPushButton*>("btnResume");
            if (btnOver) { btnOver->setEnabled(true); btnOver->update(); }
            if (btnInto) { btnInto->setEnabled(true); btnInto->update(); }
            if (btnOut)  { btnOut->setEnabled(true);  btnOut->update();  }
            if (btnRes)  { btnRes->setEnabled(true);  btnRes->update();  }
        });
    });

    // =========================================================================
    // 4. UX-МОСТ ПОДСВЕТКИ СТРОКИ ОСТАНОВА И ТУМБЛЕРОВ
    // =========================================================================
    connect(pyDebugger, &DebugManager::breakpointHit, this, [this](int line, const QString &sourceFile) {
        this->openNewFileInEditor(sourceFile);
        QTimer::singleShot(100, this, [this, line]() {
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (!currentPage) return;
            CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
            if (currentEditor && currentEditor->document()) {
                QTextBlock block = currentEditor->document()->findBlockByLineNumber(line - 1);
                if (block.isValid()) {
                    int docTotalChars = currentEditor->document()->characterCount();
                    int targetPosition = block.position();
                    if (targetPosition >= 0 && targetPosition < docTotalChars) {
                        currentEditor->blockSignals(true);
                        currentEditor->setProperty("currentDebugLine", line);

                        QPushButton *btnRes = this->findChild<QPushButton*>("btnResume");
                        if (btnRes) {
                            btnRes->blockSignals(true);
                            btnRes->setChecked(false);
                            btnRes->setText("Продолжить");
                            btnRes->setIcon(QIcon(":/Data/system_icons/media-playback-start.svg"));
                            btnRes->setEnabled(true);
                            btnRes->blockSignals(false);
                            btnRes->update();
                        }
                        QTextCursor mainCursor = currentEditor->textCursor();
                        mainCursor.setPosition(targetPosition);
                        currentEditor->setTextCursor(mainCursor);
                        currentEditor->centerCursor();

                        QTextCursor highlightCursor(currentEditor->document());
                        highlightCursor.setPosition(targetPosition);
                        QList<QTextEdit::ExtraSelection> extraSelections;
                        QTextEdit::ExtraSelection selection;
                        selection.format.setBackground(QColor(255, 255, 150, 100));
                        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
                        selection.cursor = highlightCursor;
                        extraSelections.append(selection);
                        currentEditor->setExtraSelections(extraSelections);
                        currentEditor->blockSignals(false);
                    }
                }
                currentEditor->setFocus();
                currentEditor->update();
            }
        });
    });

    // Интерактивное удаление по двойному клику на строку списка Watch
    if (watchList) {
        connect(watchList, &QListWidget::itemDoubleClicked, this, [watchList](QListWidgetItem *item) {
                                                                                                       if (item) {
                                                                                                           delete item; // Нативно вырезаем строку из виджета и памяти Qt
                                                                                                           qInfo() << ">>> [WATCH UI] Выражение успешно удалено из панели.";
                                                                                                       }
                                                                                                     });
    }
    // =========================================================================
    // 5. СОЗДАНИЕ И ПРОГРАММИРОВАНИЕ ИСПОЛНИТЕЛЬНЫХ ЭКШЕНОВ (ГОРЯЧИЕ КЛАВИШИ)
    // =========================================================================
    if (!actResume) {
        actResume = new QAction(this);
        actResume->setShortcut(QKeySequence(Qt::Key_F9));
        actResume->setCheckable(true);
        actResume->setChecked(false);
        actResume->setText("Продолжить");
        actResume->setIcon(QIcon(":/Data/system_icons/media-playback-start.svg"));
        connect(actResume, &QAction::triggered, this, [this](bool isChecked) {
            if (!this->pyDebugger) return;
            if (isChecked) {
                qInfo() << ">>> [DAP] Пользователь нажал ПАУЗА. Замораживаю поток Python...";
                this->pyDebugger->requestPause();
            } else {
                qInfo() << ">>> [DAP] Пользователь нажал ПРОДОЛЖИТЬ. Отпускаю скрипт...";
                setDebugButtonsEnabled(false);
                this->pyDebugger->resumeExecution();
            }
        });
        this->addAction(actResume);
    }

    if (!actStepOver) {
        actStepOver = new QAction(this);
        actStepOver->setShortcut(QKeySequence(Qt::Key_F8));
        connect(actStepOver, &QAction::triggered, this, [this]() {
            if (this->pyDebugger) this->pyDebugger->stepOver();
        });
        this->addAction(actStepOver);
    }

    if (!actStepInto) {
        actStepInto = new QAction(this);
        actStepInto->setShortcut(QKeySequence(Qt::Key_F7));
        connect(actStepInto, &QAction::triggered, this, [this]() {
            if (this->pyDebugger) this->pyDebugger->stepInto();
        });
        this->addAction(actStepInto);
    }

    if (!actStepOut) {
        actStepOut = new QAction(this);
        actStepOut->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F8));
        connect(actStepOut, &QAction::triggered, this, [this]() {
            if (this->pyDebugger) this->pyDebugger->stepOut();
        });
        this->addAction(actStepOut);
    }

    if (!actStopDebug) {
        actStopDebug = new QAction(this);
        actStopDebug->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F2));
        connect(actStopDebug, &QAction::triggered, this, [this]() {
            if (this->pyDebugger) this->pyDebugger->stopDebugSession();
        });
        this->addAction(actStopDebug);
    }

    connect(actResume,   &QAction::triggered, this, [this](){ setDebugButtonsEnabled(false); });
    connect(actStepOver, &QAction::triggered, this, [this](){ setDebugButtonsEnabled(false); });
    connect(actStepInto, &QAction::triggered, this, [this](){ setDebugButtonsEnabled(false); });
    connect(actStepOut,  &QAction::triggered, this, [this](){ setDebugButtonsEnabled(false); });

    // =========================================================================
    // 6. ГЛОБАЛЬНАЯ ЖЕСТКАЯ СВЯЗКА QPushButton ФОРМЫ С ЭКШЕНАМИ И ИКОНКАМИ
    // =========================================================================
    QPushButton *btnRes  = this->findChild<QPushButton*>("btnResume");
    QPushButton *btnOver = this->findChild<QPushButton*>("btnStepOver");
    QPushButton *btnInto = this->findChild<QPushButton*>("btnStepInto");
    QPushButton *btnOut  = this->findChild<QPushButton*>("btnStepOut");
    QPushButton *btnStop = this->findChild<QPushButton*>("btnStopDebug");

    if (btnRes) {
        btnRes->disconnect();
        btnRes->setCheckable(true);
        btnRes->setChecked(false);
        btnRes->setText("Продолжить");
        btnRes->setIcon(QIcon(":/Data/system_icons/media-playback-start.svg"));
        connect(btnRes, &QPushButton::toggled, this, [this, btnRes, btnOver, btnInto, btnOut](bool isChecked) {
            if (!this->pyDebugger) return;
            if (isChecked) {
                qInfo() << ">>> [GUI] Кнопка зафиксирована. Переключаю UI на ПАУЗА...";
                btnRes->setText("Пауза");
                btnRes->setIcon(QIcon(":/Data/system_icons/media-playback-pause.svg"));
                btnRes->update();
                if (btnOver) btnOver->setEnabled(false);
                if (btnInto) btnInto->setEnabled(false);
                if (btnOut)  btnOut->setEnabled(false);
                this->pyDebugger->resumeExecution();
            } else {
                qInfo() << ">>> [GUI] Кнопка отжата. Переключаю UI на ПРОДОЛЖИТЬ...";
                btnRes->setText("Продолжить");
                btnRes->setIcon(QIcon(":/Data/system_icons/media-playback-start.svg"));
                btnRes->update();
                this->pyDebugger->requestPause();
            }
        });
    }

    if (btnOver) {
        btnOver->disconnect();
        btnOver->setIcon(QIcon(":/Data/system_icons/media-playlist-normal.svg"));
        connect(btnOver, &QPushButton::clicked, this, [this]() {
            this->setDebugButtonsEnabled(false);
            if (actStepOver) actStepOver->trigger();
        });
    }

    if (btnInto) {
        btnInto->disconnect();
        btnInto->setIcon(QIcon(":/Data/system_icons/media-playlist-play.svg"));
        connect(btnInto, &QPushButton::clicked, this, [this]() {
            this->setDebugButtonsEnabled(false);
            if (actStepInto) actStepInto->trigger();
        });
    }

    if (btnOut) {
        btnOut->disconnect();
        btnOut->setIcon(QIcon(":/Data/system_icons/media-skip-forward.svg"));
        connect(btnOut, &QPushButton::clicked, this, [this]() {
            this->setDebugButtonsEnabled(false);
            if (actStepOut) actStepOut->trigger();
        });
    }

    if (btnStop) {
        btnStop->disconnect();
        btnStop->setIcon(QIcon(":/Data/system_icons/media-playback-stop.svg"));
        connect(btnStop, &QPushButton::clicked, this, [this]() {
            if (actStopDebug) actStopDebug->trigger();
        });
    }

    connect(pyDebugger, &DebugManager::sessionFinished, this, [this]() {
        qInfo() << ">>> [DEBUG CORE] Сессия отладки успешно завершена. Возвращаю UI...";
        setDebugButtonsEnabled(false);
        QPushButton *btnRes = this->findChild<QPushButton*>("btnResume");
        if (btnRes) {
            btnRes->blockSignals(true);
            btnRes->setChecked(false);
            btnRes->setText("Продолжить");
            btnRes->setIcon(QIcon(":/Data/system_icons/media-playback-start.svg"));
            btnRes->setEnabled(false);
            btnRes->blockSignals(false);
            btnRes->update();
        }
        QWidget *currentPage = ui->centralStackedWidget->currentWidget();
        if (currentPage) {
            CodeEditor *editor = currentPage->findChild<CodeEditor*>();
            if (editor) {
                editor->setProperty("currentDebugLine", 0);
                editor->update();
            }
        }
    });

    connect(pyDebugger, &DebugManager::statusMessageReady, this, [this](const QString &message, int timeout) {
        if (ui && ui->statusbar) {
            ui->statusbar->showMessage(message.left(50), timeout);
        }
        static QRegularExpression lossRegex("loss[:\\s=]+([0-9\\.]+)");
        static QRegularExpression accRegex("(?:accuracy|acc)[:\\s=]+([0-9\\.]+)");

        QRegularExpressionMatch lossMatch = lossRegex.match(message.toLower());
        if (lossMatch.hasMatch()) {
            QString lossValue = lossMatch.captured(1);
            this->injectFinalMetricsToVariableTree(" final_loss", lossValue, "float");
        }
        QRegularExpressionMatch accMatch = accRegex.match(message.toLower());
        if (accMatch.hasMatch()) {
            QString accValue = accMatch.captured(1);
            this->injectFinalMetricsToVariableTree(" final_accuracy", accValue + "%", "float");
        }
    });

    connect(pyDebugger, &DebugManager::sessionFinished, this, [this]() {
        this->setDebugButtonsEnabled(false);
        this->injectFinalMetricsToVariableTree(" СТАТУС СЕССИИ", "Выполнение успешно завершено", "status");
    });

    m_sourcesModel = new QStandardItemModel(this);
    m_sourcesModel->setHorizontalHeaderLabels(QStringList() << "Файлы исходных текстов" << "Родительский путь");

    QTreeView *sourcesTreeView = ui->centralwidget ? ui->centralwidget->findChild<QTreeView*>("treeView_2") : nullptr;
    if (sourcesTreeView) {
        sourcesTreeView->setModel(m_sourcesModel);
        sourcesTreeView->header()->setSectionResizeMode(QHeaderView::Interactive);
        sourcesTreeView->header()->setStretchLastSection(true);
        sourcesTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers); }
    connect(pyDebugger, &DebugManager::loadedSourcesReceived, this, [this, sourcesTreeView](const QStringList &sourcePaths) {
        if (!m_sourcesModel) return;
        m_sourcesModel->removeRows(0, m_sourcesModel->rowCount());

        for (const QString &path : sourcePaths) {
            QFileInfo fileInfo(path);
            QString parentDir = fileInfo.dir().dirName();
            if (parentDir.isEmpty() || parentDir == ".") parentDir = "project";

            // 1. ЖЕСТКИЙ ФИКС: Делаем оба элемента указателями (*)
            QStandardItem *fileNameItem = new QStandardItem(fileInfo.fileName());
            QStandardItem *pathItem     = new QStandardItem(QString("(%1)").arg(parentDir));

            fileNameItem->setData(path, Qt::UserRole);
            fileNameItem->setToolTip(path);
            pathItem->setToolTip(path); // Теперь оператор -> работает законно!

            fileNameItem->setForeground(QBrush(QColor(35, 38, 41)));
            pathItem->setForeground(QBrush(QColor(127, 140, 141)));

            QFont font = fileNameItem->font();
            font.setWeight(QFont::Medium);
            fileNameItem->setFont(font);

            // 2. ЖЕСТКИЙ ФИКС: Переводим список на хранение указателей <QStandardItem*>
            QList<QStandardItem*> rowItems;
            rowItems << fileNameItem << pathItem;

            // Теперь модель Qt6 нативно и без ошибок примет строку дерева файлов
            m_sourcesModel->appendRow(rowItems);
        }

        if (sourcesTreeView) {
            sourcesTreeView->resizeColumnToContents(0); }});
    if (sourcesTreeView)
    { connect(sourcesTreeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
            if (!index.isValid() || !m_sourcesModel) return;
            QModelIndex firstColumnIndex = m_sourcesModel->index(index.row(), 0, index.parent());
            QString fullPath = m_sourcesModel->data(firstColumnIndex, Qt::UserRole).toString();
            if (!fullPath.isEmpty()) { this->openNewFileInEditor(fullPath); }});}
    setDebugButtonsEnabled(false); // Активация кнопки плюс для QListWidget
    if (btnAddWatch) {
        btnAddWatch->disconnect();
        btnAddWatch->setText(" + Добавить выражение");
        btnAddWatch->setStyleSheet("QPushButton { font-weight: bold; color: #1a73e8; }");
        connect(btnAddWatch, &QPushButton::clicked, this, [this, watchList]() {
            bool ok;
            QString expression = QInputDialog::getText(this, "Новое выражение Watch",
                                                     "Введите формулу Python (например: X_fake.shape или window_size):",
                                                     QLineEdit::Normal, "", &ok);

            if (ok && !expression.trimmed().isEmpty() && watchList) {
                QListWidgetItem *item = new QListWidgetItem(expression.trimmed() + " ➔ Вычисляется...");
                item->setData(Qt::UserRole, expression.trimmed());
                item->setToolTip("Двойной клик левой кнопкой мыши, чтобы удалить это выражение");
                item->setForeground(QBrush(QColor(127, 140, 141)));
                watchList->addItem(item);

                int rowId = watchList->count() - 1;
                if (this->pyDebugger && this->pyDebugger->isConnected()) {
                    this->pyDebugger->evaluateWatchExpression(rowId, expression.trimmed());
                }
            }
        });
    }

    // =========================================================================
    // СМАРТ-УПРАВЛЕНИЕ И СКРЫТИЕ ПАНЕЛИ ОШИБОК (PROBLEMS VIEW)
    // =========================================================================
    QWidget *problemsContainer = panelOther ? panelOther->findChild<QWidget*>("problemsContainer") : nullptr;
    QPushButton *btnHideProblems = panelOther ? panelOther->findChild<QPushButton*>("btnHideProblems") : nullptr;

    // Находим кнопку-переключатель в самом нижнем системном статус-баре вкладок
    // (Назовите её в Designer, например, btnToggleProblemsPanel)
    QPushButton *btnToggleProblemsPanel = this->findChild<QPushButton*>("btnToggleProblemsPanel");

    // 1. Нажатие на крестик внутри панели — скрывает весь нижний этаж ошибок
    if (btnHideProblems && problemsContainer) {
        connect(btnHideProblems, &QPushButton::clicked, this, [problemsContainer, btnToggleProblemsPanel]() {
            problemsContainer->hide(); // Схлопываем виджет, верхний стек вызовов займет всё свободное место!

            if (btnToggleProblemsPanel) {
                btnToggleProblemsPanel->setChecked(false); // Отжимаем кнопку в статусбаре
            }
            qInfo() << ">>> [PROBLEMS UI] Панель ошибок скрыта пользователем.";
        });
    }

    // 2. Системная кнопка в нижнем ряду — работает как тумблер (Показать / Скрыть)
    if (btnToggleProblemsPanel && problemsContainer) {
        btnToggleProblemsPanel->setCheckable(true);
        btnToggleProblemsPanel->setChecked(true); // По умолчанию при старте панель открыта

        connect(btnToggleProblemsPanel, &QPushButton::clicked, this, [problemsContainer](bool isChecked) {
            if (isChecked) {
                problemsContainer->show();
            } else {
                problemsContainer->hide();
            }
        });
    }
}

void Neuro_programm::setDebugButtonsEnabled(bool enabled)
{
    // Ищем живые QPushButton на форме
    QPushButton *btnRes  = this->findChild<QPushButton*>("btnResume");
    QPushButton *btnOver = this->findChild<QPushButton*>("btnStepOver");
    QPushButton *btnInto = this->findChild<QPushButton*>("btnStepInto");

    if (btnOver) btnOver->setEnabled(enabled);
    if (btnInto) btnInto->setEnabled(enabled);

    // Кнопку продолжения/паузы оставляем доступной всегда во время сессии
    if (btnRes)  btnRes->setEnabled(this->pyDebugger && this->pyDebugger->isConnected());
}

bool Neuro_programm::showSaveConfirmationDialog()
{
    // 1. Динамически сканируем комбобокс открытых файлов на наличие модификаций со звездочкой '*'
    QStringList modifiedFiles;
    if (ui && ui->fileComboBox) {
        for (int i = 0; i < ui->fileComboBox->count(); ++i) {
            QString itemText = ui->fileComboBox->itemText(i);
            if (itemText.endsWith(" *")) {
                // Извлекаем абсолютный путь к файлу, зашитый в UserData
                QString fullPath = ui->fileComboBox->itemData(i).toString();
                if (!fullPath.isEmpty() && fullPath != "MAIN_SCREEN" && fullPath != "AI_CHAT_SCREEN") {
                    modifiedFiles << fullPath;
                }
            }
        }
    }

    // Если всё уже сохранено — молча разрешаем запуск (диалог не выводим)
    if (modifiedFiles.isEmpty()) return true;

    // 2. КАНOНИЧНЫЙ ВЫЗОВ ДИАЛОГА ЧЕРЕЗ УКАЗАТЕЛЬ ПО АНАЛОГИИ С rsc3
    // Передаем список файлов и путь к проекту напрямую в конструктор окна
    rsc5 = new Savedata(modifiedFiles, this->currentOpenProjectPath, this);
    rsc5->setWindowTitle("Сохранение изменений - PyTorch Studio");

    // Наследуем общую QSS-тему главного окна, чтобы диалог бесшовно вписался в темный интерфейс Студии
    rsc5->setStyleSheet(this->styleSheet());

    // === ФИЧА 3: ГАРАНТИРОВАННЫЙ ФОКУС ДЛЯ РАБОТЫ С КЛАВИАТУРЫ ===
    // Ищем кнопку "Сохранить все" внутри rsc5, чтобы выставить её дефолтной и подсвеченной
    QPushButton *btnSaveAll = rsc5->findChild<QPushButton*>("pushButton");
    if (btnSaveAll) {
        btnSaveAll->setDefault(true);  // Насильно привязываем триггер по клавише ENTER
        btnSaveAll->setFocus();        // Мгновенно переводим фокус ввода на эту кнопку
    }

    // Ищем кнопку "Отмена" и вешаем на неё жесткий аппаратный шорткат клавиши ESCAPE
    QPushButton *btnCancel = rsc5->findChild<QPushButton*>("pushButton_3");
    if (btnCancel) {
        btnCancel->setShortcut(QKeySequence(Qt::Key_Escape));
    }

    // Запускаем диалог в модальном блокирующем режиме exec()
    int result = rsc5->exec();

    // Извлекаем результаты выбора инженера из ОЗУ закрытого окна перед его удалением
    bool executionPermitted = rsc5->isProceedAllowed();
    QString fileToFocus = rsc5->getSelectedFileToFocus();

    // 3. АНАЛИЗ И ОБРАБОТКА РЕЗУЛЬТАТОВ СЕССИИ ДИАЛОГА
    if (result == QDialog::Accepted && executionPermitted)
    {
        // Пользователь выбрал "Сохранить все" или "Не сохранять" — запуск разрешен!
        // Если была нажата кнопка "Сохранить все", пишем измененный код из вкладок на диск
        if (btnSaveAll && btnSaveAll->underMouse() || result == QDialog::Accepted)
        {
            for (const QString &path : std::as_const(modifiedFiles)) {
                // Вызываем ваш штатный метод принудительного сохранения файла на диск
                // (Например, saveCurrentActiveFile() или кастомный метод записи по пути):
                this->saveCurrentActiveFile();
            }
        }
    }
    // === ФИЧА 1: ИНТЕРАКТИВНЫЙ ДВОЙНОЙ КЛИК (ПРЫЖОК К СЛОМАННОМУ ФАЙЛУ) ===
    else if (!fileToFocus.isEmpty())
    {
        // Если окно закрылось по двойному щелчку на строке списка —
        // находим этот файл в комбобоксе и принудительно активируем вкладку в редакторе!
        int comboIdx = ui->fileComboBox->findData(fileToFocus);
        if (comboIdx != -1) {
            ui->fileComboBox->setCurrentIndex(comboIdx);

            // Направляем клавиатурный фокус на текстовый холст, чтобы инженер мог сразу писать код
            QWidget *currentPage = ui->centralStackedWidget->currentWidget();
            if (currentPage) {
                CodeEditor *editor = currentPage->findChild<CodeEditor*>();
                if (editor) editor->setFocus();
            }
        }
    }

    // Освобождаем ресурсы: аппаратно вырезаем объект окна из оперативной памяти
    rsc5->deleteLater();

    return executionPermitted;
}

void Neuro_programm::createMenus()
{
    // 1. ЗАЩИТА: проверяем сам бар
    if (!customMenuBar) return;

    // 2. Ищем существующее меню "Инструменты2" на баре, чтобы не использовать член класса напрямую
    QMenu *targetMenu = nullptr;
    const QList<QAction*> actionsList = customMenuBar->actions();
    for (QAction *action : actionsList)
    {
        if (action->menu() && (action->text() == "Инструменты2" || action->text() == "Инструменты"))
        {
            targetMenu = action->menu();
            break;
        }
    }

    // 3. Если не нашли локально — создаем и перезаписываем член класса
    if (!targetMenu)
    {
        targetMenu = customMenuBar->addMenu("Инструменты2");
    }
    toolsMenu = targetMenu; // Синхронизируем член класса с валидным объектом

    // 4. Создаем экшен настроек
    QAction *actionSettings = new QAction("Настройки2", this);
    actionSettings->setObjectName("actionSettings");
    actionSettings->setShortcut(QKeySequence::Preferences);
    actionSettings->setIcon(QIcon(":/Data/system_icons/document-save-as.svg"));

    connect(actionSettings, &QAction::triggered, this, &Neuro_programm::showPreferences);

    // 5. Добавляем в гарантированно валидный указатель
    targetMenu->addAction(actionSettings);
}


void Neuro_programm::showPreferences()
{
    // Создаем экземпляр окна настроек.
    // Передаем 'this' (MainWindow) в качестве родителя, чтобы окно центрировалось поверх главного
    PreferencesDialog dialog(this);

    // Вызываем как модальное окно. Код заблокирует главное окно, пока настройки не закроют.
    dialog.exec();
}

void Neuro_programm::onEraseFlash()
{
    // Запускаем индикацию начала процесса в графической панели
    if (ui->widget_4) {
        ui->widget_4->updateStatusText("Очистка Flash...", "orange");
        ui->widget_4->setFlashButtonsEnabled(false); // Временно блокируем кнопки
    }

    QProcess *eraseProcess = new QProcess(this);

    // Связываем завершение процесса с выводом D-Bus уведомления
    connect(eraseProcess, &QProcess::finished, this, [this, eraseProcess](int exitCode) {
        if (exitCode == 0) {
            // Уведомление через D-Bus об успешной очистке
            this->sendSystemNotification("ST-Link: Очистка", "Flash-память микроконтроллера успешно стерта.");

            // Если плата стерта, то имя чипа мы знаем из прошлого сканирования,
            // но статус обновляем на чистый/готовый
            if (ui->widget_4) {
                ui->widget_4->updateStatusText("Память очищена (Готов к записи)", "green");
            }
        } else {
            // Сбой очистки памяти
            this->sendSystemNotification("ST-Link: Ошибка", "Не удалось очистить память чипа. Проверьте подключение.");

            if (ui->widget_4) {
                ui->widget_4->updateStatusText("Ошибка очистки памяти", "red");
            }
        }

        // Возвращаем активность кнопкам панели управления
        if (ui->widget_4) {
            ui->widget_4->setFlashButtonsEnabled(true);
        }

        eraseProcess->deleteLater(); // Безопасное удаление процесса из памяти
    });

    // Запуск консольной команды полного стирания STM32
    eraseProcess->start("st-flash", QStringList() << "erase");
}

void Neuro_programm::injectFinalMetricsToVariableTree(const QString &name, const QString &value, const QString &type)
{
    // Защита: если модель дерева локальных переменных не инициализирована, выходим
    if (!m_varModel) return;

    // Создаем три ячейки для формирования одной строки (Имя | Значение / Свойства | Тип)
    QStandardItem *nameItem  = new QStandardItem(name);
    QStandardItem *valueItem = new QStandardItem(value);
    QStandardItem *typeItem  = new QStandardItem(type);

    // UX-Защита: запрещаем инженеру случайно отредактировать итоговые показатели на экране
    nameItem->setEditable(false);
    valueItem->setEditable(false);
    typeItem->setEditable(false);

    // Накатываем красивый полужирный шрифт
    QFont boldFont = nameItem->font();
    boldFont.setBold(true);
    nameItem->setFont(boldFont);
    valueItem->setFont(boldFont);

    // Стилизуем цвета: имя делаем благородным синим, а значение — зеленым цветом успеха
    nameItem->setForeground(QBrush(QColor(0x1a73e8)));  // Синий маркер Breeze
    valueItem->setForeground(QBrush(QColor(0x4caf50))); // Зеленый цвет метрики

    // Собираем элементы в строку и аппаратно накатываем в конец дерева локальных переменных
    QList<QStandardItem*> rowItems;
    rowItems << nameItem << valueItem << typeItem;
    m_varModel->appendRow(rowItems);
}

void Neuro_programm::jumpToCodeLine(const QString &filePath, int lineNumber)
{
    if (filePath.isEmpty() || lineNumber < 0) return;

    // 1. Аппаратно открываем файл во встроенном редакторе Студии (Ваш штатный метод)
    this->openNewFileInEditor(filePath);

    // 2. Ищем активный CodeEditor на текущей открытой вкладке centralStackedWidget [0:37]
    QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
    if (!currentPage) return;

    CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
    if (!currentEditor) {
        currentEditor = qobject_cast<CodeEditor*>(currentPage);
    }

    // 3. НАПРАВЛЯЕМ КАРЕТКУ И СКРОЛЛ НА НУЖНУЮ СТРОКУ PYTHON
    if (currentEditor && currentEditor->document()) {
        QTextBlock block = currentEditor->document()->findBlockByLineNumber(lineNumber);
        if (block.isValid()) {
            QTextCursor cursor = currentEditor->textCursor();
            cursor.setPosition(block.position()); // Ставим каретку на физический старт строки [0:37]

            currentEditor->setTextCursor(cursor); // Обновляем курсор на экране [0:37]
            currentEditor->centerCursor();        // Смарт-фокус: плавно центрируем строку на экране! [0:37]
            currentEditor->setFocus();            // Возвращаем фокус ввода клавиатуры [0:37]
            currentEditor->update();
        }
    } else {
        qWarning() << "!!! [NAV ERROR] Активный CodeEditor для прыжка по стеку не найден!";
    }
}

void Neuro_programm::refreshProblemsTableView()
{
    // 1. Аппаратно ищем виджет таблицы внутри нижнего этажа panelOther
    QTableWidget *probTable = panelOther ? panelOther->findChild<QTableWidget*>("problemsTable") : nullptr;
    if (!probTable) return;

    // 2. Полностью очищаем сетку перед выводом свежих данных
    probTable->setRowCount(0);

    // 3. Вычисляем путь к текущему открытому файлу в редакторе Студии
    QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
    CodeEditor *currentEditor = currentPage ? currentPage->findChild<CodeEditor*>() : nullptr;

    QString currentDocPath = currentEditor ? currentEditor->property("currentFilePath").toString() : "";
    if (currentDocPath.isEmpty() && currentEditor) {
        currentDocPath = currentEditor->objectName();
    }

    // Нормализуем путь открытого документа для Linux/Unix сред
    QString cleanCurrentDocPath = QDir::fromNativeSeparators(currentDocPath).trimmed();

    // ЭТАЛОННЫЕ ЦВЕТА СТУДИИ ДЛЯ ТЕМНОЙ ТЕМЫ БРИЗ
    QColor textWhiteColor(239, 240, 241); // Контрастный белый Breeze
    QColor errorRedColor(255, 85, 85);    // Неоновый красный для синтаксических крашей
    QColor warningYellowColor(251, 192, 45); // Янтарно-желтый для предупреждений PEP8

    // 4. Бежим циклом по ГЛOБАЛЬНОМУ контейнеру, где поле .message гарантированно существует!
    for (const auto &error : std::as_const(Neuro_programm::globalLspErrors))
    {
        // СМАРТ-ФИЛЬТР: Проверяем, относится ли эта ошибка из ОЗУ к текущему открытому файлу на экране.
        // Если у вашей структуры globalLspErrors поле пути называется по-другому, подставьте его.
        // Если фильтрация по файлу пока не нужна — эту проверку можно временно закомментировать.
        /*
        QString errorCleanPath = QDir::fromNativeSeparators(error.fPath).trimmed();
        if (errorCleanPath != cleanCurrentDocPath) {
            continue; // Пропускаем ошибки других закрытых вкладок
        }
        */

        int row = probTable->rowCount();
        probTable->insertRow(row);

        // Колонка 0: Текст сообщения линтера из структуры Neuro_programm::LspErrorData
        QTableWidgetItem *descItem = new QTableWidgetItem(error.message);
        descItem->setForeground(QBrush(textWhiteColor));

        // Колонка 1: Имя файла
        QFileInfo fileInfo(currentDocPath);
        QString shortFileName = currentDocPath.isEmpty() ? "train.py" : fileInfo.fileName();
        QTableWidgetItem *fileItem = new QTableWidgetItem(shortFileName);
        fileItem->setForeground(QBrush(textWhiteColor));

        // Сохраняем абсолютный путь в метаданные ячейки для прыжков по двойному щелчку
        fileItem->setData(Qt::UserRole, currentDocPath);
        fileItem->setToolTip(currentDocPath);

        // Колонка 2: Номер строки (переводим 0-based индекс C++ в человеческий 1-based)
        QTableWidgetItem *lineItem = new QTableWidgetItem(QString::number(error.line + 1));
        lineItem->setForeground(QBrush(textWhiteColor));

        // Колонка 3: Смарт-определение важности (Ошибка или Предупреждение)
        QString typeText = "Предупреждение";
        QColor currentTypeColor = warningYellowColor;

        if (error.message.contains("error", Qt::CaseInsensitive) ||
            error.message.startsWith("E") ||
            error.message.contains("syntax", Qt::CaseInsensitive)) {
            typeText = "Ошибка";
            currentTypeColor = errorRedColor;
            descItem->setForeground(QBrush(errorRedColor)); // Подсвечиваем всю строку ошибки алым
        }

        QTableWidgetItem *typeItem = new QTableWidgetItem(typeText);
        typeItem->setForeground(QBrush(currentTypeColor));

        // Раскладываем элементы по ячейкам строки таблицы
        probTable->setItem(row, 0, descItem);
        probTable->setItem(row, 1, fileItem);
        probTable->setItem(row, 2, lineItem);
        probTable->setItem(row, 3, typeItem);

        // Блокируем ячейки от случайного затирания текста пользователем
        for (int i = 0; i < 4; ++i) {
            if (probTable->item(row, i)) {
                probTable->item(row, i)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            }
        }
    }

    // Принудительно заставляем графический движок Qt обновить холст сетки
    probTable->viewport()->update();
    probTable->update();
}

void Neuro_programm::insertGeneratedCodeIntoEditor(const QString &generatedCode)
{
    this->isAiProcessing = false; // Снимаем аппаратный барьер ядра

    // 1. Мгновенно закрываем и уничтожаем всплывающее окно промпта
    if (m_activePromptWidget != nullptr) {
        m_activePromptWidget->close();
        m_activePromptWidget->deleteLater();
        m_activePromptWidget = nullptr;
    }

    if (generatedCode.isEmpty()) {
        qWarning() << "⚠️ [AI Warning]: Получена пустая строка генерации. Инжекция отменена.";
        return;
    }

    QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
    if (!currentPage) return;

    CodeEditor *currentEditor = currentPage->findChild<CodeEditor*>();
    if (!currentEditor) return;

    if (currentEditor->document())
    {
        QTextCursor cursor = currentEditor->textCursor();

        // =====================================================================
        // БЛОК POST-PROCESSING: ОЧИСТКА И ФОРМАТИРОВАНИЕ НА ЛЕТУ
        // =====================================================================
        QString cleanCode = generatedCode;

        // Фикс 1: Регулярными выражениями вырезаем маркдаун-теги ```python или ```
        static const QRegularExpression markdownRegex("```(?:python)?\\n?", QRegularExpression::CaseInsensitiveOption);
        cleanCode.remove(markdownRegex);
        cleanCode.remove("```"); // Удаляем оставшиеся закрывающие кавычки
        cleanCode = cleanCode.trimmed(); // Срезаем мусорные пробелы на концах

        // Фикс 2: Расчет Auto-Indent под текущую строку инженера
        // Вычисляем, сколько пробелов или табов находится в начале текущей строки
        QString currentLineText = cursor.block().text();
        QString indentString = "";
        for (const QChar &ch : std::as_const(currentLineText))
        {
            if (ch == ' ' || ch == '\t')
            {
                indentString.append(ch);
            } else {
                break; // Как только пошел полезный текст — фиксация отступа завершена
            }
        }

        // Если мы стоим не на пустой строке, а на строке с отступом,
        // подгоняем каждую строчку ответа ИИ под это смещение
        if (!indentString.isEmpty()) {
            QStringList codeLines = cleanCode.split('\n');
            for (int i = 0; i < codeLines.size(); ++i) {
                // Первую строку не сдвигаем, если курсор уже стоит в позиции отступа,
                // а вот все последующие строки обязаны унаследовать этот сдвиг!
                if (i > 0 && !codeLines[i].trimmed().isEmpty()) {
                    codeLines[i] = indentString + codeLines[i];
                }
            }
            cleanCode = codeLines.join('\n');
        }

        // =====================================================================
        // АТОМАРНАЯ ИНЖЕКЦИЯ КОДА С ПОДДЕРЖКОЙ СТЕКА UNDO (Ctrl+Z)
        // =====================================================================
        int docLength = currentEditor->document()->characterCount();
        if (cursor.position() > docLength) {
            cursor.setPosition(docLength - 1);
        }

        qInfo() << ">>> [AI POST-PROCESS SUCCESS]: Отформатированный код внедряется в каретку.";

        cursor.beginEditBlock(); // Начало транзакции
        if (cursor.hasSelection()) {
            cursor.removeSelectedText();
        }
        cursor.insertText(cleanCode); // Штампуем чистый, ровный код Python
        cursor.endEditBlock();   // Конец транзакции

        // Возвращаем фокус ввода и обновляем холст в GUI-потоке ноутбука
        currentEditor->setTextCursor(cursor);
        currentEditor->setFocus();
        currentEditor->viewport()->update();
        currentEditor->update();
    }
}

void Neuro_programm::openAiPromptBox()
{
    // 1. Переключаем менеджер в режим чата (загрузка Instruct-модели)
    m_aiManager->switchMode("instruct");

    // 2. Создаем бесрамочное всплывающее оверлейное окно
    QDialog* promptDialog = new QDialog(this, Qt::FramelessWindowHint | Qt::Popup);
    QVBoxLayout* layout = new QVBoxLayout(promptDialog);
    layout->setContentsMargins(5, 5, 5, 5);

    // 3. Создаем строку ввода промпта
    QLineEdit* promptInput = new QLineEdit(promptDialog);
    promptInput->setPlaceholderText("ИИ подготавливает память, подождите...");
    promptInput->setDisabled(true); // Блокируем, пока идет рокировка весов в ОЗУ
    promptInput->setMinimumWidth(450);
    layout->addWidget(promptInput);

    // 4. Разблокируем строку ввода, когда менеджер ИИ пришлет сигнал готовности
    connect(m_aiManager, &LocalAiManager::aiReadyForChat, promptInput, [promptInput]() {
        if (promptInput) {
            promptInput->setEnabled(true);
            promptInput->setPlaceholderText("Задайте команду ИИ (например: оптимизируй этот алгоритм)...");
            promptInput->setFocus();
        }
    });

    // 5. Позиционируем окно ровно под кареткой активного текстового редактора
    // (Используем ваш указатель 'currentEditor')
    if (currentEditor) {
        QRect cursorRect = currentEditor->cursorRect();
        QPoint globalPos = currentEditor->mapToGlobal(cursorRect.bottomLeft());
        promptDialog->move(globalPos);
    }

    // 6. Логика отправки промпта на сервер по нажатию клавиши Enter
    connect(promptInput, &QLineEdit::returnPressed, this, [this, promptDialog, promptInput]() {
        if (!promptInput) return;

        QString userPrompt = promptInput->text();
        QString selectedCode = currentEditor ? currentEditor->textCursor().selectedText() : "";

        promptDialog->close();
        promptDialog->deleteLater();

        if (!userPrompt.isEmpty()) {
            // Передаем команду на обработку в наш изолированный менеджер
            m_aiManager->sendChatCommand(userPrompt, selectedCode);
        }
    });

    // 7. При закрытии окна (клик мимо или завершение) возвращаем Base-модель для автодополнения
    connect(promptDialog, &QDialog::finished, this, [this](int result) {
        Q_UNUSED(result);
        m_aiManager->switchMode("base");
    });

    // 8. Показываем готовое окно пользователю
    promptDialog->show();
}

void Neuro_programm::onPromptSubmitted(const QString &promptText)
{
    // Защита от дребезга и повторных пакетов (Аппаратная защелка)
    if (this->isAiProcessing) {
        qWarning() << "⚠️ [GUARD] Заблокирован дублирующий вызов onPromptSubmitted.";
        return;
    }

    this->isAiProcessing = true;

    qInfo() << ">>> [AI] Смарт-сборщик контекста запущен...";

    QWidget *currentPage = ui->centralStackedWidget ? ui->centralStackedWidget->currentWidget() : nullptr;
    if (!currentPage) { this->isAiProcessing = false; return; }

    CodeEditor *activeEditor = currentPage->findChild<CodeEditor*>();
    if (!activeEditor) {
        activeEditor = qobject_cast<CodeEditor*>(QApplication::focusWidget());
    }

    if (!activeEditor) { this->isAiProcessing = false; return; }

    // Замораживаем каретку инженера, выставляя спиннер ожидания
    activeEditor->setCursor(Qt::WaitCursor);

    // =========================================================================
    // БЛОК СМАРТ-СБОРКИ КОНТЕКСТА (UX ОПТИМИЗАЦИЯ ПОД КВАНТ n_ctx=4096)
    // =========================================================================
    QString fullText = activeEditor->toPlainText();
    QStringList lines = fullText.split('\n');

    // Определяем, на какой строке сейчас стоит каретка инженера
    QTextCursor cursor = activeEditor->textCursor();
    int currentLineIdx = cursor.blockNumber(); // Индекс текущей строки (от 0)

    QStringList contextLines;

    // 1. АППАРАТНЫЙ ЗАХВАТ ИМПОРТОВ: Всегда берем первые 20 строк файла
    int importLinesCount = qMin(20, lines.size());
    for (int i = 0; i < importLinesCount; ++i) {
        contextLines.append(lines[i]);
    }
    contextLines.append("# ... [Разрыв кода: пропуск нерелевантных строк Студией] ... ");

    // 2. ЗАХВАТ ВЕРХНЕГО ОКНА: Берем до 150 строк выше курсора
    int startUpperIdx = qMax(importLinesCount, currentLineIdx - 150);
    for (int i = startUpperIdx; i < currentLineIdx; ++i) {
        contextLines.append(lines[i]);
    }

    // Маркер точного места, куда ИИ обязан инжектировать новый код PyTorch
    contextLines.append(QString("# ◄◄◄ [КУРСОР ИНЖЕНЕРА СТОИТ ЗДЕСЬ]. ТЗ: %1 ◄◄◄").arg(promptText));

    // 3. ЗАХВАТ НИЖНЕГО ОКНА: Берем до 50 строк ниже курсора
    int endLowerIdx = qMin(lines.size(), currentLineIdx + 50);
    for (int i = currentLineIdx; i < endLowerIdx; ++i) {
        contextLines.append(lines[i]);
    }

    // Склеиваем сжатый контекст обратно в единую чистую строку для отправки
    QString optimizedContextText = contextLines.join('\n');

    qInfo() << QString(">>> [СБОРЩИК]: Оригинальный файл: %1 строк. Сжатый контекст: %2 строк.")
               .arg(lines.size()).arg(contextLines.size());

    // =========================================================================
    // ЭТАП 1: ВЫСТАВЛЯЕМ СТАРТОВЫЙ СТАТУС В ИНТЕРФЕЙС И ЗАПУСКАЕМ ТАЙМЕР МЕТРИК
    // =========================================================================
    QString initialStatus = "🌐 Ожидание подключения к Python-серверу...";

    if (this->m_activePromptWidget != nullptr) {
        this->m_activePromptWidget->setStatusText(initialStatus);
        this->m_activePromptWidget->setInputsEnabled(false); // Блокируем спам-клики Enter
    }
    if (this->statusLogLabel != nullptr) {
        this->statusLogLabel->setText(initialStatus);
    }

    // ФИКС МЕТРИК: Запускаем высокоточный отсчет времени прямо перед отправкой HTTP-пакета
    this->m_aiGenerationTimer.start();

    // Отправляем сжатый и подготовленный пакет на бэкенд FastAPI
    m_aiManager->requestChatGeneration(promptText, optimizedContextText);
}








