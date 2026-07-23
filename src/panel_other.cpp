#include "panel_other.h"
#include "replwidget.h"
#include "ui_panel_other.h"
#include "neuro_programm.h"

#include <QTextBlock>
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
#include <QAction>

panel_other::panel_other(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::panel_other),
    m_terminalPart1(nullptr),
    m_replEdit(nullptr),
    m_replProcess(nullptr)
{
    ui->setupUi(this);

    if (ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(0);
    }

    QSettings settings(QDir::homePath() + "/.config/PyTorchStudio/EditorSettings.conf", QSettings::IniFormat);

    // Ищем фабрику плагина KonsolePart для KF6 (нужна только для левого терминала)
    auto factory = KPluginFactory::loadFactory(KPluginMetaData(QStringLiteral("kf6/parts/konsolepart")));
    if (!factory) {
        qWarning() << "Критическая ошибка: kf6/parts/konsolepart не найден!";
    }

    // =========================================================================
    // 1. ЛЕВЫЙ ТЕРМИНАЛ В СПЛИТТЕРЕ (Bash)
    // =========================================================================
    if (ui->myTerminalWidget && factory) {
        QLayout *lay1 = ui->myTerminalWidget->layout();
        if (!lay1) {
            lay1 = new QVBoxLayout(ui->myTerminalWidget);
            lay1->setContentsMargins(0,0,0,0);
            ui->myTerminalWidget->setLayout(lay1);
        }
        m_terminalPart1 = factory.plugin->create<KParts::ReadOnlyPart>(this);
        if (m_terminalPart1) {
            m_terminalPart1->setProperty("shell", QStringLiteral("/bin/bash"));
            m_terminalPart1->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_terminalPart1->widget()->setMinimumSize(0, 0);
            lay1->addWidget(m_terminalPart1->widget());
            m_terminalPart1->widget()->show();
        }
    }

    // =========================================================================
    // 2. ПРАВЫЙ ТЕРМИНАЛ: ПОЛНОЦЕННЫЙ ИНТЕРАКТИВНЫЙ REPL
    // =========================================================================
    if (ui->replContainer) {
        QLayout *lay2 = ui->replContainer->layout();
        if (!lay2) {
            lay2 = new QVBoxLayout(ui->replContainer);
            lay2->setContentsMargins(0,0,0,0);
            ui->replContainer->setLayout(lay2);
        }

        m_replEdit = new QPlainTextEdit(this);
        m_replEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_replEdit->setReadOnly(false); // Включаем возможность ввода
        m_replEdit->installEventFilter(this); // Подключаем перехват клавиш

        // Настройка шрифтов под стиль IDE
        QString logFontFamily = settings.value("Editor/FontFamily", "Liberation Mono").toString();
        int logFontSize = settings.value("Editor/FontSize", 10).toInt();
        QFont replFont(logFontFamily, logFontSize);
        replFont.setFixedPitch(true);
        m_replEdit->setFont(replFont);

        // Стилизация под Breeze Dark
        // Стилизация: идеально черный фон, зеленый текст и аккуратный скроллбар
        m_replEdit->setStyleSheet(
            "QPlainTextEdit { "
            " background-color: #000000; " // 1. Абсолютно черный цвет фона
            " color: #00FF00; "            // 2. Яркий зеленый цвет текста
            " border: none; "
            " padding: 10px; "
            "}"
            // Делаем полосу прокрутки тоже темной, чтобы она не выбивалась из дизайна
            "QScrollBar:vertical { "
            " background-color: #1a1a1a; "
            " width: 12px; "
            " margin: 0px; "
            "}"
            "QScrollBar::handle:vertical { "
            " background-color: #333333; "
            " min-height: 20px; "
            " border-radius: 4px; "
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
            " height: 0px; "
            "}"
            );

        lay2->addWidget(m_replEdit);
        m_replEdit->show();

        // Запуск фонового процесса IPython
        // =========================================================================
        // МОДИФИКАЦИЯ: ДИНАМИЧЕСКИЙ ЗАПУСК REPL ИЗ VENV С КЛАССИЧЕСКИМ ПРОМПТОМ ">>> "
        // =========================================================================
        // =========================================================================
        // ПЕРЕКЛЮЧЕНИЕ REPL НА PYTHON ИЗ VENV ТЕКУЩЕГО ПРОЕКТА
        // =========================================================================
        m_replProcess = new QProcess(this);
        m_replProcess->setProcessChannelMode(QProcess::MergedChannels); // Объединяем stdout и stderr

        // Связываем вывод процесса с нашим виджетом
        connect(m_replProcess, &QProcess::readyReadStandardOutput, this, &panel_other::onReplReadyRead);

        // По умолчанию используем системный python3, если проект еще не открыт
        QString pythonExecutable = QStringLiteral("python3");

        // Ищем указатель на главное окно Neuro_programm
        Neuro_programm *mainWindow = qobject_cast<Neuro_programm*>(this->parentWidget());
        if (!mainWindow) {
            mainWindow = qobject_cast<Neuro_programm*>(this->window());
        }

        // Если нашли главное окно, берем путь к проекту и переключаемся на локальный venv
        if (mainWindow) {
            QString projectPath = mainWindow->currentOpenProjectPath; // Теперь это легально благодаря friend class!
            if (!projectPath.isEmpty()) {
                // Формируем путь к бинарнику python внутри venv проекта
                QString localVenvPython = projectPath + "/venv/bin/python";
                if (QFile::exists(localVenvPython)) {
                    pythonExecutable = localVenvPython;
                    qInfo() << "[REPL] Успешно переключено на изолированный venv:" << localVenvPython;
                }
            }
        }

        // Запуск родного Python в интерактивном режиме (-i) из venv проекта
        m_replProcess->start(pythonExecutable, QStringList() << "-i");


    }

    this->setObjectName("MyCustomTerminalPanel");
    this->setStyleSheet("QWidget#MyCustomTerminalPanel { border: 1px solid #b0b0b0 !important; }");

    // =========================================================================
    // 3. НАСТРОЙКА И СТИЛИЗАЦИЯ КОНСОЛИ ЛОГОВ И КНОПОК НАВИГАЦИИ
    // =========================================================================
    QString logFontFamily = settings.value("Editor/FontFamily", "Liberation Mono").toString();
    int logFontSize = settings.value("Editor/FontSize", 10).toInt();
    QFont logFont(logFontFamily, logFontSize);
    logFont.setFixedPitch(true);

    ui->logEdit->setFont(logFont);
    ui->logEdit->setReadOnly(true);
    ui->logEdit->setUndoRedoEnabled(false);
    ui->logEdit->setMaximumBlockCount(3000);
    ui->logEdit->setStyleSheet(
        "QPlainTextEdit { background-color: #232629; color: #eff0f1; border: none; padding: 10px; }"
        );

    if (ui->btnViewLog) {
        connect(ui->btnViewLog, &QPushButton::clicked, this, [this]() {
            ui->stackedWidget->setCurrentIndex(1);
            ui->stackedWidget->show();
        });
    }

    if (ui->termView) {
        connect(ui->termView, &QPushButton::clicked, this, [this]() {
            ui->stackedWidget->setCurrentIndex(0);
            ui->stackedWidget->show();
        });
    }

    // if (actDebug) {
    //     connect(ui->termView, &QPushButton::clicked, this, [this]() {
    //         ui->stackedWidget->setCurrentIndex(2);
    //         ui->stackedWidget->show();
    //     });
    // }

    connect(ui->btnClose2, &QPushButton::clicked, this, &panel_other::close);

    // =========================================================================
    // 4. ЛОГИКА КНОПКИ-ЦИКЛЕРА РЕЖИМОВ СПЛИТТЕРА
    // =========================================================================
    if (ui->btnCycleConsoles) {
        ui->btnCycleConsoles->setText("Режим: Терминал + REPL");
        connect(ui->btnCycleConsoles, &QPushButton::clicked, this, [this]() {
            m_splitterMode = (m_splitterMode + 1) % 3;
            switch (m_splitterMode) {
            case 0:
                ui->myTerminalWidget->show();
                ui->replContainer->show();
                ui->btnCycleConsoles->setText("Режим: Терминал + REPL");
                if (ui->splitter) {
                    QList<int> sizes;
                    sizes << ui->splitter->width() / 2 << ui->splitter->width() / 2;
                    ui->splitter->setSizes(sizes);
                }
                if (m_terminalPart1) m_terminalPart1->widget()->setFocus();
                break;
            case 1:
                ui->myTerminalWidget->show();
                ui->replContainer->hide();
                ui->btnCycleConsoles->setText("Режим: Только Терминал");
                if (m_terminalPart1) m_terminalPart1->widget()->setFocus();
                break;
            case 2:
                ui->myTerminalWidget->hide();
                ui->replContainer->show();
                ui->btnCycleConsoles->setText("Режим: Только REPL");
                if (m_replEdit) m_replEdit->setFocus();
                break;
            }
        });
    }

    // =========================================================================
    // 5. ОЧИСТКА АКТИВНОЙ КОНСОЛИ
    // =========================================================================
    if (ui->btnClearActive) {
        connect(ui->btnClearActive, &QPushButton::clicked, this, [this]() {
            int currentIndex = ui->stackedWidget->currentIndex();
            if (currentIndex == 0) {
                if ((m_replEdit && m_replEdit->hasFocus()) || ui->myTerminalWidget->isHidden()) {
                    if (m_replEdit) m_replEdit->clear();
                }
                else if (m_terminalPart1 && m_terminalPart1->widget()) {
                    QWidget *targetWidget = m_terminalPart1->widget();
                    QKeyEvent *pressEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_L, Qt::ControlModifier, "l");
                    QKeyEvent *releaseEvent = new QKeyEvent(QEvent::KeyRelease, Qt::Key_L, Qt::ControlModifier, "l");
                    QApplication::postEvent(targetWidget, pressEvent);
                    QApplication::postEvent(targetWidget, releaseEvent);
                }
            }
            else if (currentIndex == 1) {
                ui->logEdit->clear();
            }
        });
    }

    // =========================================================================
    // ЖЕСТКИЙ ФИКС: ДЕЛИМ НИЖНЮЮ ПАНЕЛЬ РОВНО ПОПОЛАМ (50/50) ПРИ СТАРТЕ
    // =========================================================================
    if (ui->splitter) {
        // Задаем режим, чтобы при растяжении окна пропорции 50/50 сохранялись
        ui->splitter->setStretchFactor(0, 1);
        ui->splitter->setStretchFactor(1, 1);

        // Посылаем одноразовый таймер, чтобы Qt успел рассчитать геометрию окна,
        // после чего делим доступную ширину пополам
        QTimer::singleShot(100, this, [this]() {
            if (ui->splitter) {
                int totalWidth = ui->splitter->width();
                QList<int> initialSizes;
                initialSizes << (totalWidth / 2) << (totalWidth / 2);
                ui->splitter->setSizes(initialSizes);
            }
        });
    }

    ui->logEdit->setStyleSheet(
        "background-color: #000000; "
        "color: #00FF00; "
        "font-family: 'Courier New', 'Consolas', monospace; "
        "font-size: 11pt; "
        "border: 1px solid #333333;"
        );

    // Инициализируем обработчик прямо здесь, передавая ему вашу таблицу
    m_stackHandler = new StackTableHandler(ui->callStackListWidget, this);

    // Связываем сигнал двойного клика из обработчика с сигналом-ретранслятором панели
    connect(m_stackHandler, &StackTableHandler::frameSelected,
            this, &panel_other::fileNavigationRequested);
}

panel_other::~panel_other()
{
    delete ui;
}

void panel_other::setCallStackData(const QList<StackFrame> &frames) {
    // 1. Активируем нужную вкладку/страницу (индекс 2, где лежит таблица)
    if (ui && ui->stackedWidget) {
        ui->stackedWidget->setCurrentIndex(2);
    }

    // 2. Отправляем данные обработчику таблицы
    if (m_stackHandler) {
        m_stackHandler->updateTable(frames);
    }
}

bool panel_other::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_replEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        QTextCursor cursor = m_replEdit->textCursor();

        cursor.movePosition(QTextCursor::End);
        int totalLength = m_replEdit->document()->characterCount();
        cursor.select(QTextCursor::LineUnderCursor);
        QString currentLine = cursor.selectedText();

        int promptEndIdx = currentLine.indexOf(">>> ");
        int promptLen = 4;

        if (promptEndIdx == -1) {
            promptEndIdx = currentLine.indexOf("... ");
            promptLen = 4;
        }

        int minAllowedPos = (promptEndIdx != -1) ? (cursor.block().position() + promptEndIdx + promptLen) : totalLength;

        if (m_replEdit->textCursor().position() < minAllowedPos &&
            keyEvent->key() != Qt::Key_Control && keyEvent->key() != Qt::Key_C) {
            cursor.movePosition(QTextCursor::End);
            m_replEdit->setTextCursor(cursor);
        }

        // 1. Нажатие ENTER — перехватываем и проверяем на команду pip
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            QString command = currentLine;
            if (promptEndIdx != -1) {
                command = currentLine.mid(promptEndIdx + promptLen).trimmed();
            }

            // =========================================================================
            // ХАК: ПЕРЕХВАТ И ПЕРЕНАПРАВЛЕНИЕ КОМАНД PIP
            // =========================================================================
            if (command.startsWith("pip ") || command.startsWith("pip3 ")) {
                m_replEdit->appendPlainText("\n[PIP] Запуск установки пакета внутри venv...");
                m_replEdit->ensureCursorVisible();
                qApp->processEvents(); // Обновляем UI, чтобы пользователь видел текст

                // =========================================================================
                // АБСОЛЮТНО НАДЕЖНЫЙ ПОИСК PIP ЧЕРЕЗ КОНФИГ СРЕДЫ РАЗРАБОТКИ (QSETTINGS)
                // =========================================================================
                QString pipExecutable = QStringLiteral("pip3");

                // Напрямую считываем путь из глобального конфига
                QSettings ideConfig(QDir::homePath() + "/.config/PyTorchStudio/IDE.conf", QSettings::IniFormat);
                QString activeVenv = ideConfig.value("python/global_venv_path", "").toString();

                if (!activeVenv.isEmpty()) {
                    QString localPip = activeVenv + "/bin/pip";

                    if (QFile::exists(localPip)) {
                        pipExecutable = localPip;
                        m_replEdit->appendPlainText(QString("[Изоляция venv] Считан путь из конфига: %1").arg(localPip));
                    } else {
                        m_replEdit->appendPlainText(QString("[Ошибка] Конфиг указывает на несуществующий pip: %1").arg(localPip));
                        return true; // Блокируем падение на систему
                    }
                } else {
                    m_replEdit->appendPlainText("[Критическая Ошибка] В конфигурации IDE.conf отсутствует путь к виртуальному окружению!");
                    return true; // Не даем выполнить команду глобально
                }

                // Вырезаем аргументы (например, install, python-lsp-server[all])
                QStringList args = command.split(' ', Qt::SkipEmptyParts);
                if (!args.isEmpty()) args.removeFirst(); // Удаляем само слово "pip"

                // Запускаем процесс установки
                // Запускаем процесс установки
                QProcess *pipProc = new QProcess(this);
                pipProc->setProcessChannelMode(QProcess::MergedChannels);

                // =========================================================================
                // НАСТРОЙКА ОКРУЖЕНИЯ VENV ЧЕРЕЗ АКТИВНЫЙ ПУТЬ ИЗ QSETTINGS
                // =========================================================================
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                if (!activeVenv.isEmpty()) {
                    // Переопределяем переменную VIRTUAL_ENV на считанный путь venv
                    env.insert("VIRTUAL_ENV", activeVenv);
                    // Снимаем жесткие ограничения пакетного менеджера Arch Linux
                    env.remove("PIP_REQUIRE_VIRTUALENV");
                }
                pipProc->setProcessEnvironment(env);

                pipProc->start(pipExecutable, args);


                // Ждем завершения и выводим лог прямо в консоль
                if (pipProc->waitForFinished(45000)) {
                    QString output = QString::fromUtf8(pipProc->readAllStandardOutput());
                    m_replEdit->appendPlainText(output);
                } else {
                    m_replEdit->appendPlainText("[PIP ОШИБКА] Превышено время ожидания установки.");
                }

                pipProc->deleteLater();
                m_replEdit->appendPlainText(">>> "); // Возвращаем промпт на экран
                m_replEdit->ensureCursorVisible();
                return true; // Блокируем отправку в Python
            }

            // Если это обычный код Python — отправляем стандартно в m_replProcess
            if (m_replProcess && m_replProcess->state() == QProcess::Running) {
                m_replProcess->write((command + "\n").toUtf8());
            }

            m_replEdit->appendPlainText("");
            m_replEdit->ensureCursorVisible();
            if (m_replEdit->verticalScrollBar())
            {
                m_replEdit->verticalScrollBar()->setValue(m_replEdit->verticalScrollBar()->maximum());
            }

            return true;
        }

        if (keyEvent->key() == Qt::Key_Backspace)
        {
            if (m_replEdit->textCursor().position() <= minAllowedPos)
            {
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void panel_other::onReplReadyRead()
{
    if (!m_replProcess || !m_replEdit) return;

    QByteArray output = m_replProcess->readAllStandardOutput();
    QString text = QString::fromUtf8(output);

    // Вставляем текст
    m_replEdit->appendPlainText(text);

    // ЖЕСТКИЙ UX-ФИКС ПРОКРУТКИ: принудительно двигаем скроллбар на максимум вниз
    m_replEdit->ensureCursorVisible();
    if (m_replEdit->verticalScrollBar()) {
        m_replEdit->verticalScrollBar()->setValue(m_replEdit->verticalScrollBar()->maximum());
    }
}

void panel_other::appendTrainingLog(const QString &text)
{
    if (ui && ui->logEdit) {
        // БЫЛО: ui->logEdit->append(text); или insertPlainText(text);
        // СТАЛО: Заставляем текстовое поле парсить неоновые цвета и теги жирности <b>
        ui->logEdit->appendHtml(text);

        // Автоматический скролл консоли вниз
        QScrollBar *sb = ui->logEdit->verticalScrollBar();
        if (sb) sb->setValue(sb->maximum());
    }
}

void panel_other::onDebugModeTriggered(bool checked)
{
    if (checked) {
        // Силово переключаем stackedWidget на ТРЕТЬЮ страницу (индекс 2 - Стек вызовов)
        if (ui && ui->stackedWidget) {
            ui->stackedWidget->setCurrentIndex(2);
        }

        // Задаем правый отступ в 350 пикселей, чтобы не залезать под правую панель дебага
        //this->setContentsMargins(0, 0, 350, 0);
    }
    else {
        // При остановке возвращаем панель на всю ширину экрана
        this->setContentsMargins(0, 0, 0, 0);
    }
}

void panel_other::setDebugAction(QAction *action)
{
    if (!action) return;

    // Связываем сигнал кнопки сайдбара напрямую со слотом внутри этого класса
    connect(action, &QAction::triggered, this, &panel_other::onDebugModeTriggered);
}