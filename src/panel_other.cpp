#include "panel_other.h"
#include "replwidget.h"
#include "ui_panel_other.h"

#include <QDir>
#include <QRegularExpression>
#include <QTextCursor>
#include <QStyleFactory>
#include <QScrollBar>
#include <QTextCursor>
#include <iostream>
#include <QSettings>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

panel_other::panel_other(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::panel_other)
{
    ui->setupUi(this);

    // Настраиваем моноширинные шрифты для всех консолей интерактива
    QFont monoFont("Courier New", 10);
    ui->terminalHistoryEdit->setFont(monoFont);
    ui->debugHistoryEdit->setFont(monoFont);

    // Инициализируем член класса, а не локальную переменную
    replBackend = new REPLWidget(ui->historyEdit, ui->inputEdit, this);
    replBackend->startPython();

    process = nullptr;

    // 2. Инициализируем Системный Терминал (Вкладка 3)
    initSystemTerminal();

    // 3. Инициализируем Консоль Отладки (Вкладка 2)
    initDebugConsole();

    // Добавьте этот код строго в конец конструктора panel_other::panel_other
    ui->consoleOutput->verticalScrollBar()->setStyle(QStyleFactory::create("Fusion"));
    ui->pipToolBar->setVisible(false);
    ui->installProgress->setVisible(false);

    connect(ui->btnClose2, &QPushButton::clicked, this, &panel_other::btnClosePanel);

    // 2. Накатываем пуленепробиваемый плоский StyleSheet
    // Замените финальный блок setStyleSheet для скроллбара в конструкторе panel_other::panel_other на этот код:

    ui->consoleOutput->setStyleSheet(R"(
        /* --- НАСТРОЙКА ВСЕГО ТЕКСТОВОГО ПОЛЯ --- */
        QTextEdit {
            background-color: #ffffff !important;
            color: #1a1a1a !important;
            font-family: 'Monospace', 'Courier New', 'Liberation Mono' !important;
            font-size: 14px !important;
            border: 1px solid #b0b0b0 !important;
            padding: 10px !important;
        }

        /* --- 1. ДОРОЖКА СКРОЛЛБАРА BREEZE --- */
        QTextEdit QScrollBar:vertical {
            background-color: #f5f5f5 !important;  /* Светлая подложка */
            width: 10px !important;                 /* В стиле Breeze скроллбар более изящный - 10px */
            margin: 0px 0px 0px 0px !important;
            border: none !important;
            border-left: 1px solid #e0e0e0 !important; /* Едва заметный разделитель */
        }

        /* --- 2. ПОЛЗУНОК В СТИЛЕ BREEZE (АДАПТИВНАЯ КАПСУЛА) --- */
        QTextEdit QScrollBar::handle:vertical {
            background-color: #3daee9 !important;  /* Нативный синий цвет Breeze */

            /* Минимальная высота, чтобы не превращался в точку */
            min-height: 40px !important;

            /* МАГИЯ BREEZE №1: Скругление углов 3px делает ползунок капсулой */
            border-radius: 3px !important;

            /* МАГИЯ BREEZE №2: Отступы по краям (2px слева и справа), */
            /* благодаря чему капсула аккуратно "парит" по центру дорожки */
            margin-left: 2px !important;
            margin-right: 2px !important;
            margin-top: 0px !important;
            margin-bottom: 0px !important;
            border: none !important;
        }

        /* ЭФФЕКТ ХОВЕРА В СТИЛЕ KDE PLASMA: */
        /* Когда пользователь наводит мышь на ползунок, он расширяется на всю ширину и темнеет */
        QTextEdit QScrollBar::handle:vertical:hover {
            background-color: #2a93cc !important;
            margin-left: 0px !important;  /* Расширяется до краев */
            margin-right: 0px !important;
            border-radius: 0px !important; /* Становится строгим на момент захвата */
        }

        /* Состояние при зажатии мышью */
        QTextEdit QScrollBar::handle:vertical:pressed {
            background-color: #1b75a6 !important;
            margin-left: 0px !important;
            margin-right: 0px !important;
            border-radius: 0px !important;
        }

        /* --- 3. СБРОС СИСТЕМНЫХ МАСОК ПЛАТФОРМЫ --- */
        QTextEdit QScrollBar::add-page:vertical,
        QTextEdit QScrollBar::sub-page:vertical {
            background: transparent !important;
            border: none !important;
        }

        /* --- 4. СКРЫТИЕ СТРЕЛОЧЕК (В стиле Breeze они отсутствуют) --- */
        QTextEdit QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px !important;
            width: 0px !important;
            background: transparent !important;
            border: none !important;
        }
    )");

    connect(ui->btnCloseToolBar, &QPushButton::clicked, this, [this]()
    {
        ui->pipToolBar->setVisible(false);
        emit pipPanelClosed(); // Испускаем сигнал наружу для статусбара главного окна
    });

    // --- ПОДКЛЮЧЕНИЕ СИГНАЛОВ СТРОКИ ВВОДА К НОВОМУ МЕТОДУ ---
    connect(ui->btnSend, &QPushButton::clicked, this, [this]() {
        executeCustomPipCommand(ui->inputCommand->text().trimmed());
    });

    // Отправка по нажатию Enter на клавиатуре
    connect(ui->inputCommand, &QLineEdit::returnPressed, this, [this]() {
        executeCustomPipCommand(ui->inputCommand->text().trimmed());
    });

    // 1. Указываем точное количество колонок
    ui->pipTableWidget->setColumnCount(2);

    // 2. Передаем список названий для заголовков (через QStringList)
    QStringList headers;
    headers << "Имя пакета" << "Версия";
    ui->pipTableWidget->setHorizontalHeaderLabels(headers);

    ui->pipTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

panel_other::~panel_other()
{
    delete ui;
}

void panel_other::startVenvInstallation(const QString &projectPath, const QString &archType)
{
    // КРИТИЧНО: Запоминаем путь к текущему проекту в переменную класса!
    // Без этого кастомный pip не поймет, в какую папку на диске Arch Linux доставлять пакеты
    currentProjectPath = projectPath;

    ui->stackedWidget->setCurrentIndex(0);
    ui->consoleOutput->clear();
    ui->consoleOutput->appendPlainText("======================================================================");
    ui->consoleOutput->appendPlainText(">>> [BASH INTERFACE] Инициализация окружения разработки ИИ...");
    ui->consoleOutput->appendPlainText(QString(">>> [BASH INTERFACE] Путь к проекту: %1").arg(projectPath));
    ui->consoleOutput->appendPlainText(QString(">>> [BASH INTERFACE] Архитектура PyTorch: %1").arg(archType));
    ui->consoleOutput->appendPlainText("======================================================================");
    // Блокируем ввод кастомных команд, пока идет первичная тяжелая установка PyTorch
    ui->pipToolBar->setEnabled(false);

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray rawOutput = process->readAllStandardOutput();
        QString outputStr = QString::fromUtf8(rawOutput);
        static QString lineBuffer = "";
        lineBuffer += outputStr;
        static QRegularExpression progressRegex("Progress\\s+(\\d+)\\s+of\\s+(\\d+)");

        if (lineBuffer.contains('\n') || lineBuffer.contains('\r')) {
            QStringList lines = lineBuffer.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                QRegularExpressionMatch match = progressRegex.match(line);
                if (match.hasMatch()) {
                    double downloaded = match.captured(1).toDouble();
                    double total = match.captured(2).toDouble();
                    if (total > 0) {
                        int percent = static_cast<int>((downloaded / total) * 100);
                        ui->installProgress->setValue(percent);
                    }
                } else {
                    ui->consoleOutput->appendPlainText(line);
                }
            }
            lineBuffer.clear();
        }
    });

    connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
            ui->installProgress->setValue(100);
            ui->consoleOutput->appendPlainText("\n>>> [SUCCESS] Базовое окружение venv успешно развернуто!");
        } else {
            ui->consoleOutput->appendPlainText("\n>>> [CRITICAL ERROR] Не удалось завершить настройку окружения.");
        }

        // Базовая тяжелая установка закончена — активируем панель ввода пакетов для пользователя!
        ui->pipToolBar->setEnabled(true);
    });

    QString bashCommands = QString(
                               "cd '%1' && "
                               "python -m venv venv && "
                               "mkdir -p .pip_tmp && "
                               "PYTHONUNBUFFERED=1 ./venv/bin/python -m pip install --no-cache-dir --upgrade pip && "
                               "TMPDIR='./.pip_tmp' TEMP='./.pip_tmp' TMP='./.pip_tmp' "
                               "PYTHONUNBUFFERED=1 ./venv/bin/pip install --no-cache-dir --progress-bar raw -r requirements.txt && "
                               "rm -rf .pip_tmp"
                               ).arg(projectPath);

    QStringList arguments;
    arguments << "-c" << bashCommands;
    process->start("bash", arguments);
}

// // =============================================================================
// // РЕАЛИЗАЦИЯ НОВОГО МЕТОДА: ДОУСТАНОВКА ПАКЕТОВ ПО ЗАПРОСУ ПОЛЬЗОВАТЕЛЯ
// // =============================================================================
// void panel_other::executeCustomPipCommand(const QString &packageName)
// {
//     if (packageName.isEmpty()) return;

//     // Защита: не даем запустить новую установку, если QProcess еще занят предыдущей
//     if (process && process->state() != QProcess::NotRunning) {
//         ui->consoleOutput->append("\n>>> [SYSTEM] Пожалуйста, дождитесь завершения текущей операции pip...");
//         return;
//     }

//     // Пишем команду в эмулятор белого терминала, имитируя классическую консоль Linux
//     ui->consoleOutput->append(QString("\n$ pip install %1").arg(packageName));
//     ui->installProgress->setValue(0);

//     // Временно блокируем панель ввода, чтобы пользователь не спамил кнопкой в процессе загрузки
//     ui->pipToolBar->setEnabled(false);

//     // Перевыделяем процесс, чтобы полностью очистить старые сигналы регулярных выражений PyTorch
//     if (process) {
//         process->kill();
//         process->deleteLater();
//     }

//     process = new QProcess(this);
//     process->setProcessChannelMode(QProcess::MergedChannels);

//     // Коннект на чтение логов доустановки пакета в реальном времени
//     connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
//         QByteArray raw = process->readAllStandardOutput();
//         QString out = QString::fromUtf8(raw);

//         // Выводим чистые строчки pip (Downloading, Installing collected packages...)
//         ui->consoleOutput->insertPlainText(out);
//         ui->consoleOutput->moveCursor(QTextCursor::End);
//     });

//     // Коннект на завершение установки пакета
//     connect(process, &QProcess::finished, this, [this, packageName](int exitCode, QProcess::ExitStatus status) {
//         if (status == QProcess::NormalExit && exitCode == 0) {
//             ui->consoleOutput->append(QString("\n>>> [SUCCESS] Пакет '%1' успешно добавлен в venv проекта!").arg(packageName));
//             ui->installProgress->setValue(100);
//             ui->inputCommand->clear(); // Очищаем строку ввода при успехе
//         } else {
//             ui->consoleOutput->append(QString("\n>>> [ERROR] Ошибка установки пакета '%1'. Проверьте интернет или имя пакета.").arg(packageName));
//         }

//         // Возвращаем доступ к панели управления
//         ui->pipToolBar->setEnabled(true);
//         ui->inputCommand->setFocus();
//     });

//     // Формируем безопасный скрипт: заходим в корень и вызываем локальный pip текущего проекта.
//     // Флаг --progress-bar off отключает буферизацию процентов для мгновенного вывода лога одиночных библиотек
//     QString customScript = QString("cd '%1' && ./venv/bin/pip install --no-cache-dir --progress-bar off %2")
//                                .arg(currentProjectPath).arg(packageName);

//     QStringList args;
//     args << "-c" << customScript;
//     process->start("bash", args);
// }

// Обычные методы переключения индексов stackedWidget
void panel_other::setTerminalPageActive() { ui->stackedWidget->setCurrentIndex(0); }
void panel_other::setSearchPageActive()   { ui->stackedWidget->setCurrentIndex(1); }
void panel_other::setLogsPageActive()     { ui->stackedWidget->setCurrentIndex(2); }
void panel_other::togglePipPanel(bool visible) {
    ui->stackedWidget->setCurrentIndex(0);
    ui->pipToolBar->setVisible(visible);
    if (visible) ui->inputCommand->setFocus();
}

// void panel_other::setTerminalPageActive()
// {
//     // Жестко переключает stackedWidget нижней панели на первую страницу (индекс 0)
//     ui->stackedWidget->setCurrentIndex(0);
// }

// void panel_other::setSearchPageActive()
// {
//     ui->stackedWidget->setCurrentIndex(1);
// }

// void panel_other::setLogsPageActive()
// {
//     ui->stackedWidget->setCurrentIndex(2);
// }

// void panel_other::togglePipPanel(bool visible)
// {
//     // Принудительно включаем страницу терминала, чтобы пользователь видел открывшуюся панель pip
//     ui->stackedWidget->setCurrentIndex(0);
//     ui->pipToolBar->setVisible(visible);
//     if (visible) {
//         ui->inputCommand->setFocus();
//     }
// }

void panel_other::executeCustomPipCommand(const QString &packageName)
{
    if (packageName.isEmpty()) return;

    // 1. БЕЗОПАСНАЯ ПРОВЕРКА СОСТОЯНИЯ:
    // Если процесс уже существует и он физически СЕЙЧАС РАБОТАЕТ — выходим
    if (process != nullptr && process->state() != QProcess::NotRunning) {
        ui->consoleOutput->appendPlainText("\n>>> [SYSTEM] Пожалуйста, дождитесь завершения текущей операции pip...");
        return;
    }

    // 2. Печатаем имитацию ввода в наш белый терминал
    ui->consoleOutput->appendPlainText(QString("\n$ pip install %1").arg(packageName));

    if (ui->installProgress) {
        ui->installProgress->setValue(0);
    }

    // Блокируем верхнюю панель ввода, чтобы защитить от спама кнопкой
    ui->pipToolBar->setEnabled(false);

    // 3. ИСПРАВЛЕНИЕ БАГА КРАША: Вместо опасного удаления процесса через deleteLater(),
    // мы просто СБРАСЫВАЕМ (очищаем) старые коннекты readyRead/finished у ТЕКУЩЕГО процесса.
    // Если процесса еще нет в памяти (первый запуск) — безопасно создаем его.
    if (process == nullptr) {
        process = new QProcess(this);
    } else {
        process->disconnect(); // Намертво счищаем старые лямбды, чтобы они не дублировались
    }

    process->setProcessChannelMode(QProcess::MergedChannels);

    // Коннект на чтение логов доустановки пакета в реальном времени
    connect(process, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray raw = process->readAllStandardOutput();
        QString out = QString::fromUtf8(raw);
        ui->consoleOutput->insertPlainText(out);
        ui->consoleOutput->moveCursor(QTextCursor::End);
    });

    // Коннект на завершение установки пакета
    connect(process, &QProcess::finished, this, [this, packageName](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
            ui->consoleOutput->appendPlainText(QString("\n>>> [SUCCESS] Пакет '%1' успешно добавлен в venv проекта!").arg(packageName));
            if (ui->installProgress) ui->installProgress->setValue(100);
            ui->inputCommand->clear(); // Очищаем строку ввода при успехе
        } else {
            ui->consoleOutput->appendPlainText(QString("\n>>> [ERROR] Ошибка установки пакета '%1'. Проверьте интернет или имя пакета.").arg(packageName));
        }

        // Возвращаем доступ к панели управления
        ui->pipToolBar->setEnabled(true);
        ui->inputCommand->setFocus();
    });

    // 4. ЗАПУСК КОМАНДЫ
    QString customScript = QString("cd '%1' && ./venv/bin/pip install --no-cache-dir --progress-bar off %2")
                               .arg(currentProjectPath).arg(packageName);

    QStringList args;
    args << "-c" << customScript;
    process->start("bash", args);
}

void panel_other::appendLiveLogText(const QString &text)
{
    if (text.isEmpty()) return;

    // =========================================================================
    // ТОЧНОЕ ИСПРАВЛЕНИЕ ИМЕНИ ВИДЖЕТА: Выводим текст напрямую в ui->textEdit
    // =========================================================================
    if (ui->consoleOutput)
    {
        ui->consoleOutput->insertPlainText(text);
        ui->consoleOutput->moveCursor(QTextCursor::End); // Автоматический скролл консоли вниз
    }
}

// =============================================================================
// МЕТОД СОХРАНЕНИЯ ТЕКУЩЕГО ПУТИ К ИИ-ПРОЕКТУ В ПАМЯТИ ПАНЕЛИ
// =============================================================================
void panel_other::setCurrentProjectPath(const QString &path)
{
    // Запоминаем абсолютный путь в приватную переменную класса panel_other
    currentProjectPath = path;

    // Выводим отладочный лог во внешний терминал Linux для контроля связей
    std::cout << "📂 [panel_other] Рабочий каталог панели успешно переключен на: "
              << path.toStdString() << std::endl;
    std::cout.flush();
}

void panel_other::btnClosePanel()
{
    this->setVisible(false); // Скрываем саму панель
    emit panelClosed();      // Посылаем сигнал во внешний мир (для Neuro_programm)
}

// void panel_other::setTerminalPageActive()
// {
//     // Замените stackedWidget на реальное objectName вашего стэка в панели логов
//     ui->stackedWidget->setCurrentIndex(0);

//     // ПРИНУДИТЕЛЬНО ПРЯЧЕМ PIP-ТЕНДЕР, если открыт обычный терминал
//     if (ui->pipToolBar) {
//         ui->pipToolBar->setVisible(false);
//     }
// }

void panel_other::setPipPageActive()
{
    // Переключаем на нужный экран с консолью
    ui->stackedWidget->setCurrentIndex(0);

    // ПРИНУДИТЕЛЬНО ВКЛЮЧАЕМ ВЕРХНЮЮ ПАНЕЛЬ ВВОДА ИМЕНИ ПАКЕТА
    if (ui->pipToolBar) {
        ui->pipToolBar->setVisible(true);
    }
}

void panel_other::setInstallProgressVisible(bool visible)
{
    // Внутри своего класса доступ к ui->installProgress открыт на 100%!
    if (ui && ui->installProgress) {
        ui->installProgress->setVisible(visible);
    }
}

void panel_other::setInstallProgressValue(int value)
{
    if (ui && ui->installProgress) {
        ui->installProgress->setValue(value);
    }
}

void panel_other::setInstallProgressRange(int min, int max)
{
    if (ui && ui->installProgress) {
        ui->installProgress->setRange(min, max);
    }
}

void panel_other::forwardCodeToREPL(const QString &code) {
    if (replBackend) {
        replBackend->executeSelection(code);
    }
}

void panel_other::updateProjectVenv(const QString &projectPath) {
    if (projectPath.isEmpty()) return;

    // Собираем путь к venv внутри папки открытого проекта
    QString newVenvPath = projectPath + "/venv";

    // Сохраняем этот путь в настройки как основной рабочий
    QSettings settings;
    settings.setValue("python/venv_path", newVenvPath);

    // Если REPL уже был инициализирован, приказываем ему перезапустить Python по новому пути
    if (replBackend) {
        // Мы можем вызвать startPython() — он считает свежий путь из QSettings
        replBackend->startPython();
    }
}

void panel_other::refreshPipList() {
    // 1. Проверяем, открыт ли проект и сохранен ли venv
    QSettings settings;
    QString venvPath = settings.value("python/venv_path", "").toString();

    if (venvPath.isEmpty()) {
        ui->historyEdit->appendPlainText("⚠️ Ошибка: Проект не открыт или venv не настроен.");
        return;
    }

    QString pipExecutable;

    // 2. Формируем путь к pip
#if defined(Q_OS_WIN)
    pipExecutable = venvPath + "/Scripts/pip.exe";
#else
    pipExecutable = venvPath + "/bin/pip";
#endif

    // 3. Запускаем утилиту pip list с флагом вывода в JSON
    QProcess *pipProcess = new QProcess(this);
    QStringList arguments;
    arguments << "list" << "--format=json";

    pipProcess->start(pipExecutable, arguments);

    // 4. Ждем завершения (так как операция быстрая, можно подождать асинхронно или через wait)
    if (!pipProcess->waitForFinished(3000)) {
        ui->historyEdit->appendPlainText("❌ Ошибка: pip list не ответил за отведенное время.");
        pipProcess->deleteLater();
        return;
    }

    // 5. Читаем сырой JSON-ответ
    QByteArray rawJson = pipProcess->readAllStandardOutput();
    pipProcess->deleteLater();

    // 6. Парсим JSON данные
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        ui->historyEdit->appendPlainText("❌ Ошибка парсинга списка пакетов.");
        return;
    }

    QJsonArray packageArray = doc.array();

    // 7. Очищаем старую таблицу и блокируем обновление для скорости
    ui->pipTableWidget->setRowCount(0);
    ui->pipTableWidget->setSortingEnabled(false); // Отключаем сортировку на время заполнения

    // 8. Заполняем таблицу строками
    for (int i = 0; i < packageArray.size(); ++i) {
        QJsonObject packageObj = packageArray.at(i).toObject();
        QString name = packageObj["name"].toString();
        QString version = packageObj["version"].toString();

        // Создаем новую строку в QTableWidget
        int row = ui->pipTableWidget->rowCount();
        ui->pipTableWidget->insertRow(row);

        // Ячейка имени
        QTableWidgetItem *nameItem = new QTableWidgetItem(name);
        nameItem->setFlags(nameItem->flags() ^ Qt::ItemIsEditable); // Запрещаем редактирование
        ui->pipTableWidget->setItem(row, 0, nameItem);

        // Ячейка версии
        QTableWidgetItem *versionItem = new QTableWidgetItem(version);
        versionItem->setFlags(versionItem->flags() ^ Qt::ItemIsEditable);
        ui->pipTableWidget->setItem(row, 1, versionItem);
    }

    // Возвращаем сортировку обратно, чтобы пользователь мог кликать по заголовкам колонок
    ui->pipTableWidget->setSortingEnabled(true);
}

void panel_other::setActivePage(PageIndex page)
{
    // 1. Переключаем страницу в QStackedWidget
    ui->stackedWidget->setCurrentIndex(static_cast<int>(page));

    // 2. Управляем видимостью дополнительной панели установки пакетов
    if (page == PageTerminal) {
        ui->pipToolBar->setVisible(true); // Показываем панель, когда открыт Терминал
    } else {
        ui->pipToolBar->setVisible(false); // Скрываем на страницах Поиска и Таблицы пакетов
    }

    // 3. Авто-обновление таблицы при открытии третьей страницы
    if (page == PagePipTable) {
        refreshPipList();
    }
}

void panel_other::initSystemTerminal() {
    QSettings settings;
    // Считываем сохраненный путь к venv
    QString venvPath = settings.value("python/venv_path", "").toString();

    // Вычисляем путь к корню проекта (поднимаемся на один уровень выше папки venv)
    QDir projectDir(venvPath);
    projectDir.cdUp();

    // 🔥 ОБЯЗАТЕЛЬНО: Объявляем переменную projectPath типа QString
    QString projectPath = projectDir.absolutePath();

    terminalProcess = new QProcess(this);

    // Настройка окружения
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#if defined(Q_OS_WIN)
    env.insert("PATH", venvPath + "/Scripts;" + env.value("PATH"));
    QString shell = "powershell.exe";
#else
    env.insert("PATH", venvPath + "/bin:" + env.value("PATH"));
    QString shell = "/bin/bash";
#endif
    terminalProcess->setProcessEnvironment(env);

    // ТЕПЕРЬ ОШИБКИ НЕ БУДЕТ: Переменная объявлена выше и видна в этой строке
    terminalProcess->setWorkingDirectory(projectPath);

    connect(terminalProcess, &QProcess::readyReadStandardOutput, this, &panel_other::readTerminalOutput);
    connect(terminalProcess, &QProcess::readyReadStandardError, this, &panel_other::readTerminalOutput);
    connect(ui->terminalInputLineEdit, &QLineEdit::returnPressed, this, &panel_other::sendTerminalCommand);

    terminalProcess->start(shell);
}

void panel_other::readTerminalOutput()
{
    QByteArray output = terminalProcess->readAllStandardOutput();
    QByteArray error = terminalProcess->readAllStandardError();

    if(!output.isEmpty())
    {
        ui->terminalHistoryEdit->appendPlainText(QString::fromUtf8(output));
        // АВТОСКРОЛЛ: Перемещаем текстовый курсор в самый конец и прокручиваем экран
        ui->terminalHistoryEdit->moveCursor(QTextCursor::End);
        ui->terminalHistoryEdit->ensureCursorVisible();
    }
    if(!error.isEmpty())
    {
        ui->terminalHistoryEdit->appendPlainText(QString::fromUtf8(error));
        // АВТОСКРОЛЛ ДЛЯ ОШИБОК
        ui->terminalHistoryEdit->moveCursor(QTextCursor::End);
        ui->terminalHistoryEdit->ensureCursorVisible();
    }
}

void panel_other::sendTerminalCommand()
{
    QString cmd = ui->terminalInputLineEdit->text().trimmed();
    if (cmd.isEmpty()) return;

    ui->terminalHistoryEdit->appendPlainText("$ " + cmd);
    terminalProcess->write((cmd + "\n").toUtf8());
    ui->terminalInputLineEdit->clear();
}

void panel_other::initDebugConsole()
{
    debugSocket = new QTcpSocket(this);
    debugSeqCounter = 0;

    connect(debugSocket, &QTcpSocket::readyRead, this, &panel_other::readDebugSocket);
    connect(ui->debugInputLineEdit, &QLineEdit::returnPressed, this, &panel_other::sendDebugCommand);

    // Подключаемся к порту, на котором крутится запущенный дебаггером Python-скрипт
    debugSocket->connectToHost("127.0.0.1", 5678);
}

void panel_other::sendDebugCommand() {
    QString expression = ui->debugInputLineEdit->text().trimmed();
    if (expression.isEmpty()) return;

    ui->debugHistoryEdit->appendPlainText(">> " + expression);

    // Упаковываем JSON по протоколу DAP Microsoft
    QJsonObject request;
    request["type"] = "request";
    request["seq"] = ++debugSeqCounter;
    request["command"] = "evaluate";

    QJsonObject arguments;
    arguments["expression"] = expression;
    arguments["context"] = "repl";
    request["arguments"] = arguments;

    QJsonDocument doc(request);
    // Компактный JSON + обязательный символ конца пакета \r\n
    QByteArray rawJson = doc.toJson(QJsonDocument::Compact) + "\r\n";

    if (debugSocket && debugSocket->state() == QAbstractSocket::ConnectedState) {
        // 🔥 ПРОВЕРКА ОТПРАВКИ: записываем байты и проверяем результат
        qint64 bytesWritten = debugSocket->write(rawJson);

        if (bytesWritten == -1) {
            ui->debugHistoryEdit->appendPlainText("❌ Системная ошибка сокета: не удалось отправить данные в сеть.");
        } else {
            // Принудительно выталкиваем байты из буфера Qt в сеть прямо сейчас
            debugSocket->flush();
        }
    } else {
        ui->debugHistoryEdit->appendPlainText("❌ Ошибка: Нет физического соединения с отладчиком.");
    }

    ui->debugInputLineEdit->clear();
}

void panel_other::readDebugSocket()
{
    // 1. Создаем статический буфер, который сохраняет данные между вызовами функции
    static QByteArray networkBuffer;

    // Дописываем новые прилетевшие байты в конец накопительного буфера
    networkBuffer.append(debugSocket->readAll());

    // 2. Крутим цикл, пока в буфере есть открывающая фигурная скобка
    while (true) {
        int jsonStartIndex = networkBuffer.indexOf('{');
        if (jsonStartIndex == -1) {
            // Если скобки нет, значит JSON еще не начался (там идут заголовки Content-Length)
            // Очищаем буфер, оставляя только хвост, если он есть, или просто выходим
            if (networkBuffer.contains("Content-Length:") && !networkBuffer.contains("{")) {
                networkBuffer.clear();
            }
            break;
        }

        // Ищем конец JSON-пакета (считаем баланс фигурных скобок)
        int braceCount = 0;
        int jsonEndIndex = -1;

        for (int i = jsonStartIndex; i < networkBuffer.size(); ++i) {
            if (networkBuffer[i] == '{') braceCount++;
            else if (networkBuffer[i] == '}') {
                braceCount--;
                if (braceCount == 0) {
                    jsonEndIndex = i; // Нашли точную границу закрытия JSON-объекта!
                    break;
                }
            }
        }

        // Если закрывающая скобка не найдена, значит пакет прилетел не полностью (разрыв сети)
        // Выходим из цикла и ждем следующего вызова readyRead, когда долетят остальные байты
        if (jsonEndIndex == -1) {
            break;
        }

        // 3. Извлекаем чистый, гарантированно целый JSON-пакет из буфера
        int jsonLength = jsonEndIndex - jsonStartIndex + 1;
        QByteArray jsonPacket = networkBuffer.mid(jsonStartIndex, jsonLength);

        // Удаляем обработанный кусок из начала сетевого буфера
        networkBuffer.remove(0, jsonEndIndex + 1);

        // 4. Парсим полностью целый JSON документ
        QJsonDocument doc = QJsonDocument::fromJson(jsonPacket);
        if (doc.isNull() || !doc.isObject()) {
            continue;
        }

        QJsonObject response = doc.object();
        QString type = response["type"].toString();
        QString command = response["command"].toString();
        QString event = response["event"].toString();

        // А) Ловим ответ на Инициализацию -> Отправляем прикрепление (attach)
        if (command == "initialize" && type == "response") {
            QJsonObject attachRequest;
            attachRequest["type"] = "request";
            attachRequest["seq"] = ++debugSeqCounter;
            attachRequest["command"] = "attach";

            QJsonObject arguments;
            attachRequest["arguments"] = arguments;

            QJsonDocument attachDoc(attachRequest);
            debugSocket->write(attachDoc.toJson(QJsonDocument::Compact) + "\r\n");
            debugSocket->flush();
        }

        // Б) Ловим подтверждение готовности сессии от debugpy
        else if (type == "event" && event == "initialized") {
            QJsonObject configDone;
            configDone["type"] = "request";
            configDone["seq"] = ++debugSeqCounter;
            configDone["command"] = "configurationDone";

            QJsonDocument configDoc(configDone);
            debugSocket->write(configDoc.toJson(QJsonDocument::Compact) + "\r\n");
            debugSocket->flush();

            // ВЫВОДИМ ФИНАЛЬНЫЙ СТАТУС В ИНТЕРФЕЙС
            this->appendDebugLog("✅ Консоль отладки успешно инициализирована и готова!");
            this->appendDebugLog(">> Теперь вы можете вводить команды (например: x.shape)");
        }

        // В) Выводим результат вычисления ваших команд в консоль отладки
        else if (command == "evaluate") {
            if (response["success"].toBool()) {
                QString result = response["body"].toObject()["result"].toString();
                ui->debugHistoryEdit->appendPlainText(result);
            } else {
                QString msg = response["message"].toString();
                ui->debugHistoryEdit->appendPlainText("❌ Ошибка вычисления: " + msg);
            }
            ui->debugHistoryEdit->moveCursor(QTextCursor::End);
            ui->debugHistoryEdit->ensureCursorVisible();
        }
    }
}



void panel_other::connectToDebugger() {
    if (!debugSocket) {
        debugSocket = new QTcpSocket(this);
        debugSeqCounter = 0;
        connect(debugSocket, &QTcpSocket::readyRead, this, &panel_other::readDebugSocket);
    }

    // Сбрасываем старые коннекты
    disconnect(debugSocket, &QTcpSocket::connected, nullptr, nullptr);
    disconnect(debugSocket, &QTcpSocket::errorOccurred, nullptr, nullptr);

    // СЛОТ УСПЕШНОГО ПОДКЛЮЧЕНИЯ
    connect(debugSocket, &QTcpSocket::connected, this, [this]() {
        this->appendDebugLog("✅ Сетевой мост установлен. Инициализация сессии отладки...");

        // Отправляем стартовый пакет DAP
        QJsonObject initRequest;
        initRequest["type"] = "request";
        initRequest["seq"] = ++debugSeqCounter;
        initRequest["command"] = "initialize";

        QJsonObject arguments;
        arguments["clientID"] = "pytorch-studio";
        arguments["adapterID"] = "python";
        arguments["linesStartAt1"] = true;
        arguments["columnsStartAt1"] = true;
        arguments["pathFormat"] = "path";
        initRequest["arguments"] = arguments;

        QJsonDocument doc(initRequest);
        debugSocket->write(doc.toJson(QJsonDocument::Compact) + "\r\n");
        debugSocket->flush();
    });

    // СЛОТ ПЕРЕХВАТА ОШИБОК (Умный опрос)
    connect(debugSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        // Если порт всё еще закрыт (Python импортирует torch)
        if (error == QAbstractSocket::ConnectionRefusedError) {
            // Закрываем сокет, ждем полсекунды и пробуем СНОВА автоматически
            debugSocket->close();
            QTimer::singleShot(500, this, &panel_other::connectToDebugger);
        } else {
            this->appendDebugLog("❌ ОШИБКА ОТЛАДЧИКА: " + debugSocket->errorString());
        }
    });

    // Пытаемся подключиться
    if (debugSocket->state() == QAbstractSocket::UnconnectedState) {
        debugSocket->connectToHost("127.0.0.1", 5678);
    }
}


void panel_other::appendLogText(const QString &text)
{
    // Направляем текст в главную левую консоль общего вывода!
    if (ui->consoleOutput)
    {
        ui->consoleOutput->appendPlainText(text);
        ui->consoleOutput->moveCursor(QTextCursor::End);
        ui->consoleOutput->ensureCursorVisible();
    }
}

void panel_other::appendDebugLog(const QString &text)
{
    if (ui->debugHistoryEdit)
    {
        ui->debugHistoryEdit->appendPlainText(text);

        // Автоскролл для вкладки дебаггера
        ui->debugHistoryEdit->moveCursor(QTextCursor::End);
        ui->debugHistoryEdit->ensureCursorVisible();
    }
}
