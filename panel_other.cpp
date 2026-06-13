#include "panel_other.h"
#include "ui_panel_other.h"

#include <QDir>
#include <QRegularExpression>
#include <QTextCursor>
#include <QStyleFactory>
#include <QScrollBar>
#include <QTextCursor>
#include <iostream>

Panel_other::Panel_other(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Panel_other)
{
    ui->setupUi(this);

    process = nullptr;

    // Добавьте этот код строго в конец конструктора Panel_other::Panel_other
    ui->consoleOutput->verticalScrollBar()->setStyle(QStyleFactory::create("Fusion"));
    ui->pipToolBar->setVisible(false);
    ui->installProgress->setVisible(false);

    connect(ui->btnClose, &QPushButton::clicked, this, &Panel_other::on_btnClosePanel_clicked);


    // 2. Накатываем пуленепробиваемый плоский StyleSheet
    // Замените финальный блок setStyleSheet для скроллбара в конструкторе Panel_other::Panel_other на этот код:

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

    connect(ui->btnCloseToolBar, &QPushButton::clicked, this, [this]() {
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
}

Panel_other::~Panel_other()
{
    delete ui;
}

void Panel_other::startVenvInstallation(const QString &projectPath, const QString &archType)
{
    // КРИТИЧНО: Запоминаем путь к текущему проекту в переменную класса!
    // Без этого кастомный pip не поймет, в какую папку на диске Arch Linux доставлять пакеты
    currentProjectPath = projectPath;

    ui->stackedWidget->setCurrentIndex(0);
    ui->consoleOutput->clear();
    ui->consoleOutput->append("======================================================================");
    ui->consoleOutput->append(">>> [BASH INTERFACE] Инициализация окружения разработки ИИ...");
    ui->consoleOutput->append(QString(">>> [BASH INTERFACE] Путь к проекту: %1").arg(projectPath));
    ui->consoleOutput->append(QString(">>> [BASH INTERFACE] Архитектура PyTorch: %1").arg(archType));
    ui->consoleOutput->append("======================================================================\n");

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
                    ui->consoleOutput->append(line);
                }
            }
            lineBuffer.clear();
        }
    });

    connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
            ui->installProgress->setValue(100);
            ui->consoleOutput->append("\n>>> [SUCCESS] Базовое окружение venv успешно развернуто!");
        } else {
            ui->consoleOutput->append("\n>>> [CRITICAL ERROR] Не удалось завершить настройку окружения.");
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
// void Panel_other::executeCustomPipCommand(const QString &packageName)
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
void Panel_other::setTerminalPageActive() { ui->stackedWidget->setCurrentIndex(0); }
void Panel_other::setSearchPageActive()   { ui->stackedWidget->setCurrentIndex(1); }
void Panel_other::setLogsPageActive()     { ui->stackedWidget->setCurrentIndex(2); }
void Panel_other::togglePipPanel(bool visible) {
    ui->stackedWidget->setCurrentIndex(0);
    ui->pipToolBar->setVisible(visible);
    if (visible) ui->inputCommand->setFocus();
}

// void Panel_other::setTerminalPageActive()
// {
//     // Жестко переключает stackedWidget нижней панели на первую страницу (индекс 0)
//     ui->stackedWidget->setCurrentIndex(0);
// }

// void Panel_other::setSearchPageActive()
// {
//     ui->stackedWidget->setCurrentIndex(1);
// }

// void Panel_other::setLogsPageActive()
// {
//     ui->stackedWidget->setCurrentIndex(2);
// }

// void Panel_other::togglePipPanel(bool visible)
// {
//     // Принудительно включаем страницу терминала, чтобы пользователь видел открывшуюся панель pip
//     ui->stackedWidget->setCurrentIndex(0);
//     ui->pipToolBar->setVisible(visible);
//     if (visible) {
//         ui->inputCommand->setFocus();
//     }
// }

void Panel_other::executeCustomPipCommand(const QString &packageName)
{
    if (packageName.isEmpty()) return;

    // 1. БЕЗОПАСНАЯ ПРОВЕРКА СОСТОЯНИЯ:
    // Если процесс уже существует и он физически СЕЙЧАС РАБОТАЕТ — выходим
    if (process != nullptr && process->state() != QProcess::NotRunning) {
        ui->consoleOutput->append("\n>>> [SYSTEM] Пожалуйста, дождитесь завершения текущей операции pip...");
        return;
    }

    // 2. Печатаем имитацию ввода в наш белый терминал
    ui->consoleOutput->append(QString("\n$ pip install %1").arg(packageName));

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
            ui->consoleOutput->append(QString("\n>>> [SUCCESS] Пакет '%1' успешно добавлен в venv проекта!").arg(packageName));
            if (ui->installProgress) ui->installProgress->setValue(100);
            ui->inputCommand->clear(); // Очищаем строку ввода при успехе
        } else {
            ui->consoleOutput->append(QString("\n>>> [ERROR] Ошибка установки пакета '%1'. Проверьте интернет или имя пакета.").arg(packageName));
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

void Panel_other::appendLiveLogText(const QString &text)
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
void Panel_other::setCurrentProjectPath(const QString &path)
{
    // Запоминаем абсолютный путь в приватную переменную класса Panel_other
    currentProjectPath = path;

    // Выводим отладочный лог во внешний терминал Linux для контроля связей
    std::cout << "📂 [Panel_other] Рабочий каталог панели успешно переключен на: "
              << path.toStdString() << std::endl;
    std::cout.flush();
}

void Panel_other::on_btnClosePanel_clicked()
{
    this->setVisible(false); // Скрываем саму панель
    emit panelClosed();      // Посылаем сигнал во внешний мир (для Neuro_programm)
}

// void Panel_other::setTerminalPageActive()
// {
//     // Замените stackedWidget на реальное objectName вашего стэка в панели логов
//     ui->stackedWidget->setCurrentIndex(0);

//     // ПРИНУДИТЕЛЬНО ПРЯЧЕМ PIP-ТЕНДЕР, если открыт обычный терминал
//     if (ui->pipToolBar) {
//         ui->pipToolBar->setVisible(false);
//     }
// }

void Panel_other::setPipPageActive()
{
    // Переключаем на нужный экран с консолью
    ui->stackedWidget->setCurrentIndex(0);

    // ПРИНУДИТЕЛЬНО ВКЛЮЧАЕМ ВЕРХНЮЮ ПАНЕЛЬ ВВОДА ИМЕНИ ПАКЕТА
    if (ui->pipToolBar) {
        ui->pipToolBar->setVisible(true);
    }
}

void Panel_other::setInstallProgressVisible(bool visible)
{
    // Внутри своего класса доступ к ui->installProgress открыт на 100%!
    if (ui && ui->installProgress) {
        ui->installProgress->setVisible(visible);
    }
}

void Panel_other::setInstallProgressValue(int value)
{
    if (ui && ui->installProgress) {
        ui->installProgress->setValue(value);
    }
}

void Panel_other::setInstallProgressRange(int min, int max)
{
    if (ui && ui->installProgress) {
        ui->installProgress->setRange(min, max);
    }
}
