#include "panel_other.h"
//#include "replwidget.h"
#include "ui_panel_other.h"
#include "neuro_programm.h"
#include "neuro_programm.h"

#include <QTextBlock>
#include <QDir>
#include <QRegularExpression>
#include <QTextCursor>
#include <QStyleFactory>
#include <QScrollBar>
#include <QTextCursor>
//#include <iostream>
#include <QSettings>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QPdfWriter>
#include <QPainter>
 #include <QDir>

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
    // 3. ИСПРАВЛЕННАЯ НАСТРОЙКА И СТИЛИЗАЦИЯ КОНСОЛИ ЛОГОВ И КНОПОК НАВИГАЦИИ
    // =========================================================================
    QString logFontFamily = settings.value(QStringLiteral("Editor/FontFamily"), QStringLiteral("Liberation Mono")).toString();
    int logFontSize = settings.value(QStringLiteral("Editor/FontSize"), 10).toInt();
    QFont logFont(logFontFamily, logFontSize);
    logFont.setFixedPitch(true);

    if (ui->logEdit) {
        ui->logEdit->setFont(logFont);
        ui->logEdit->setReadOnly(true);
        ui->logEdit->setUndoRedoEnabled(false);
        ui->logEdit->setMaximumBlockCount(3000);
        ui->logEdit->setStyleSheet(
            QStringLiteral("QPlainTextEdit { background-color: #000000; color: #00FF00; font-family: 'Courier New', 'Consolas', monospace; font-size: 11pt; border: 1px solid #333333; padding: 10px; }")
            );
    }

    // Кнопка ЛОГИ: Переключает строго на страницу с индексом 2
    if (ui->btnViewLog) {
        connect(ui->btnViewLog, &QPushButton::clicked, this, [this]() {
            if (ui->stackedWidget) {
                ui->stackedWidget->setCurrentIndex(2);
                ui->stackedWidget->show();
            }
        });
    }

    // Кнопка ТЕРМИНАЛ: Переключает строго на страницу с индексом 0 (Bash + REPL)
    if (ui->termView) {
        connect(ui->termView, &QPushButton::clicked, this, [this]() {
            if (ui->stackedWidget) {
                ui->stackedWidget->setCurrentIndex(0);
                ui->stackedWidget->show();
            }
        });
    }

    if (ui->btnClose2) {
        connect(ui->btnClose2, &QPushButton::clicked, this, &panel_other::close);
    }

    // =========================================================================
    // 4. ИСПРАВЛЕННАЯ ЛОГИКА КНОПКИ-ЦИКЛЕРА РЕЖИМОВ (btnCycleConsoles)
    // =========================================================================
    if (ui->btnCycleConsoles) {
        ui->btnCycleConsoles->setText(QStringLiteral("Режим: Терминал + REPL"));
        m_splitterMode = 0; // Изначальный режим

        connect(ui->btnCycleConsoles, &QPushButton::clicked, this, [this]() {
            if (!ui->stackedWidget) return;

            // ХАК-ФИКС: Если пользователь нажимает циклер, находясь на странице логов (индекс 2),
            // мы ПРИНУДИТЕЛЬНО возвращаем его на страницу терминалов (индекс 0)
            if (ui->stackedWidget->currentIndex() == 2) {
                ui->stackedWidget->setCurrentIndex(0);
                // При возврате не переключаем режим сплиттера сразу, даем пользователю увидеть текущее состояние
                return;
            }

            // Стандартное циклическое переключение режимов внутри страницы 0
            m_splitterMode = (m_splitterMode + 1) % 3;
            switch (m_splitterMode) {
            case 0:
                ui->myTerminalWidget->show();
                ui->replContainer->show();
                ui->btnCycleConsoles->setText(QStringLiteral("Режим: Терминал + REPL"));
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
                ui->btnCycleConsoles->setText(QStringLiteral("Режим: Только Терминал"));
                if (m_terminalPart1) m_terminalPart1->widget()->setFocus();
                break;

            case 2:
                ui->myTerminalWidget->hide();
                ui->replContainer->show();
                ui->btnCycleConsoles->setText(QStringLiteral("Режим: Только REPL"));
                if (m_replEdit) m_replEdit->setFocus();
                break;
            }
        });
    }

    // =========================================================================
    // 5. ИСПРАВЛЕННАЯ ОЧИСТКА АКТИВНОЙ КОНСОЛИ (btnClearActive)
    // =========================================================================
    if (ui->btnClearActive) {
        connect(ui->btnClearActive, &QPushButton::clicked, this, [this]() {
            if (!ui->stackedWidget) return;

            int currentIndex = ui->stackedWidget->currentIndex();

            // Если мы на странице терминалов
            if (currentIndex == 0) {
                // Если фокус в REPL или терминал скрыт — чистим REPL
                if ((m_replEdit && m_replEdit->hasFocus()) || ui->myTerminalWidget->isHidden()) {
                    if (m_replEdit) m_replEdit->clear();
                }
                // Иначе шлем Ctrl+L в нативный KonsolePart
                else if (m_terminalPart1 && m_terminalPart1->widget()) {
                    QWidget *targetWidget = m_terminalPart1->widget();
                    QKeyEvent *pressEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_L, Qt::ControlModifier, QStringLiteral("l"));
                    QKeyEvent *releaseEvent = new QKeyEvent(QEvent::KeyRelease, Qt::Key_L, Qt::ControlModifier, QStringLiteral("l"));
                    QApplication::postEvent(targetWidget, pressEvent);
                    QApplication::postEvent(targetWidget, releaseEvent);
                }
            }
            // ИСПРАВЛЕНО: Если мы на странице логов обучения (индекс СТРОГО 2)
            else if (currentIndex == 2) {
                if (ui->logEdit) ui->logEdit->clear();
            }
        });
    }

    // =========================================================================
    // ЖЕСТКИЙ ФИКС СПЛИТТЕРА ПРИ СТАРТЕ (50/50)
    // =========================================================================
    if (ui->splitter) {
        ui->splitter->setStretchFactor(0, 1);
        ui->splitter->setStretchFactor(1, 1);

        QTimer::singleShot(100, this, [this]() {
            if (ui->splitter) {
                int totalWidth = ui->splitter->width();
                QList<int> initialSizes;
                initialSizes << (totalWidth / 2) << (totalWidth / 2);
                ui->splitter->setSizes(initialSizes);
            }
        });
    }

    // 🌟 ИСПРАВЛЕННЫЙ ВАРИАНТ: Привязываем обработчик к ВЕРХНЕЙ таблице дебага!
    if (ui->callStackListWidget)
    {
        m_stackHandler = new StackTableHandler(ui->callStackListWidget, this);
    }

    // Ретранслируем сигнал двойного клика фрейма отладки напрямую в MainWindow
    connect(m_stackHandler, &StackTableHandler::frameSelected, this, &panel_other::errorItemDoubleClicked);


    // Настраиваем колонки problemsTable прямо внутри её родного класса
    ui->problemsTable->setColumnCount(4);
    ui->problemsTable->setHorizontalHeaderLabels(QStringList() << "Код" << "Строка" << "Описание ошибки" << "Файл");

    // Жестко включаем сетку, чтобы строки не накладывались на заголовки
    ui->problemsTable->setShowGrid(true);

    // Включаем автоматическое растягивание колонок под текст в стиле PyCharm
    if (ui->problemsTable->horizontalHeader())
    {
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // "Код" ужимается под E501
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // "Строка" ужимается под 26
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);          // "Описание" занимает весь центр экрана
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // "Файл" ужимается под train.py
    }

    ui->problemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->problemsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->problemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->problemsTable->verticalHeader()->setVisible(false);
    ui->problemsTable->setAlternatingRowColors(true);
    ui->problemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    // Связываем двойной клик по таблице с отправкой сигнала наружу в Neuro_programm
    connect(ui->problemsTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (!item) return;
        int row = item->row();

        QTableWidgetItem *rowItem = ui->problemsTable->item(row, 1);
        QTableWidgetItem *fileItem = ui->problemsTable->item(row, 3);
        if (!rowItem || !fileItem) return;

        int lineNum = rowItem->text().toInt();
        QString fullPath = fileItem->data(Qt::UserRole).toString();

        emit errorItemDoubleClicked(fullPath, lineNum);
    });

    connect(ui->callStackListWidget, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (!item) return;
        int row = item->row();
        QTableWidgetItem *rowItem = ui->callStackListWidget->item(row, 1); // Строка
        QTableWidgetItem *fileItem = ui->callStackListWidget->item(row, 2); // Файл
        if (rowItem && fileItem) {
            // Пробрасываем сигнал навигации в главное окно
            emit errorItemDoubleClicked(fileItem->text(), rowItem->text().toInt());
        }
    });

    QPushButton *btnToggleView = ui->menuSwitcherStack->findChild<QPushButton*>(QStringLiteral("btnToggleView"));

    if (btnToggleView) {
        // Задаем исходное состояние кнопки (текст и иконку)
        btnToggleView->setText(QStringLiteral("Показать график"));
        btnToggleView->setIcon(QIcon(QStringLiteral(":/Data/system_icons/office-chart.svg"))); // Укажите ваш путь к иконке графика

        // Гарантированно сносим старые коннекты, чтобы кнопка не кликалась дважды
        QObject::disconnect(btnToggleView, &QPushButton::clicked, nullptr, nullptr);

        // Подключаем реактивное переключение страниц по клику
        connect(btnToggleView, &QPushButton::clicked, this, [this, btnToggleView]() {
            // Безопасная проверка, что ui и кнопка существуют в памяти
            if (!this->ui || !this->ui->stackedWidget || !btnToggleView) return;

            // Если сейчас открыт текстовый лог (Index 2) — включаем страницу нативного графика (Index 3)
            if (this->ui->stackedWidget->currentIndex() == 2) {
                this->ui->stackedWidget->setCurrentIndex(3); // Включаем QCustomPlot

                btnToggleView->setText(QStringLiteral("Показать лог"));
                btnToggleView->setIcon(QIcon(QStringLiteral(":/Data/system_icons/document-edit.svg")));
            }
            // Если открыт график — возвращаем текстовый лог обратно на экран
            else {
                this->ui->stackedWidget->setCurrentIndex(2); // Возврат на logEdit

                btnToggleView->setText(QStringLiteral("Показать график"));
                btnToggleView->setIcon(QIcon(QStringLiteral(":/Data/system_icons/office-chart.svg")));
            }
        });

    }

    // Ищем кнопку очистки лога на новой панели инструментов (Index 3)
    QPushButton *btnClearLog = ui->menuSwitcherStack->findChild<QPushButton*>(QStringLiteral("btnClearLog"));

    if (btnClearLog) {
        // Гарантированно сносим старые коннекты, чтобы не плодить вызовы в ОЗУ
        QObject::disconnect(btnClearLog, &QPushButton::clicked, nullptr, nullptr);

        // Подключаем очистку текстового поля по клику
        connect(btnClearLog, &QPushButton::clicked, this, [this]() {
            // Проверяем, что ui существует в памяти
            if (!this->ui) return;

            // Находим logEdit на индексе 2 вашего stackedWidget
            QPlainTextEdit *logEdit = this->ui->stackedWidget->widget(2) ?
                                          this->ui->stackedWidget->widget(2)->findChild<QPlainTextEdit*>() : nullptr;

            // Если logEdit доступен напрямую как поле ui (например, this->ui->logEdit),
            // то код ниже можно упростить до: this->ui->logEdit->clear();
            if (logEdit) {
                logEdit->clear(); // Жестко очищаем текстовое поле от логов
                qDebug() << " [UI] Лог обучения успешно очищен оператором.";
            } else if (this->ui->stackedWidget->currentIndex() == 2) {
                // Если logEdit сам является виджетом страницы 2
                QPlainTextEdit *directLogEdit = qobject_cast<QPlainTextEdit*>(this->ui->stackedWidget->widget(2));
                if (directLogEdit) directLogEdit->clear();
            }
        });
    }

    if (ui->btnSaveLog) {
        QObject::disconnect(ui->btnSaveLog, &QPushButton::clicked, nullptr, nullptr);
        connect(ui->btnSaveLog, &QPushButton::clicked, this, [this]() {
            if (!this->ui || !this->ui->stackedWidget) return;

            // 1. Ищем logEdit на 2-й странице stackedWidget
            QPlainTextEdit *logEdit = nullptr;
            if (this->ui->stackedWidget->widget(2)) {
                logEdit = this->ui->stackedWidget->widget(2)->findChild<QPlainTextEdit*>();
                if (!logEdit) logEdit = qobject_cast<QPlainTextEdit*>(this->ui->stackedWidget->widget(2));
            }

            // =========================================================================
            // [СВЯЗЬ С ГЛАВНЫМ ОКНОМ]: Динамический поиск NeuroProgramm по дереву Qt
            // =========================================================================
            Neuro_programm *mainWin = nullptr;
            QObject *currentParent = this->parent();
            while (currentParent) {
                mainWin = qobject_cast<Neuro_programm*>(currentParent);
                if (mainWin) break;
                currentParent = currentParent->parent();
            }

            // Защита от сохранения пустого лога через DBus без вызова диалоговых окон
            if (!logEdit || logEdit->toPlainText().isEmpty()) {
                if (mainWin) {
                    mainWin->sendSystemNotification(QStringLiteral("Экспорт отчета"),
                                                    QStringLiteral("Предупреждение: Невозможно сохранить пустой лог!"));
                }
                return;
            }

            // Извлекаем путь к проекту, либо берем домашнюю папку как запасной вариант
            QString projectPath = mainWin ? mainWin->currentOpenProjectPath : QDir::homePath();

            // 2. Открываем системный диалог выбора пути
            QString defaultName = QStringLiteral("%1/training_report").arg(projectPath);
            QString filePath = QFileDialog::getSaveFileName(this,
                                                            QStringLiteral("Экспортировать отчет конвейера MLOps"),
                                                            defaultName,
                                                            QStringLiteral("Документы PDF (*.pdf);;Файлы журналов (*.log);;Текстовые документы (*.txt)"));

            if (filePath.isEmpty()) return; // Оператор отменил операцию

            // =========================================================================
            // ВЕТВЛЕНИЕ 1: ГЕНЕРАЦИЯ PDF-ДОКУМЕНТА СТАНДАРТА А4
            // =========================================================================
            if (filePath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
                QPdfWriter pdfWriter(filePath);
                pdfWriter.setPageSize(QPageSize(QPageSize::A4));
                pdfWriter.setPageOrientation(QPageLayout::Portrait);
                pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15));

                logEdit->print(&pdfWriter);

                // Вызов DBus-уведомления ОС Linux через главное окно
                if (mainWin) {
                    mainWin->sendSystemNotification(QStringLiteral("Экспорт PDF"),
                                                    QStringLiteral("Печатный ГОСТ-отчет сессии обучения успешно сформирован!"));
                }
            }
            // =========================================================================
            // ВЕТВЛЕНИЕ 2: СТАНДАРТНАЯ ЗАПИСЬ ТЕКСТОВЫХ ФАЙЛОВ (.LOG / .TXT)
            // =========================================================================
            else {
                if (!filePath.endsWith(QStringLiteral(".log")) && !filePath.endsWith(QStringLiteral(".txt"))) {
                    filePath += QStringLiteral(".log");
                }

                QFile file(filePath);
                if (file.open(QFile::WriteOnly | QFile::Text)) {
                    QTextStream out(&file);
                    out.setEncoding(QStringConverter::Utf8); // Защита ГОСТ-символов
                    out << logEdit->toPlainText();
                    file.close();

                    // Вызов DBus-уведомления ОС Linux через главное окно
                    if (mainWin) {
                        mainWin->sendSystemNotification(QStringLiteral("Экспорт лога"),
                                                        QStringLiteral("Текстовый журнал успешно записан на диск Linux!"));
                    }
                } else {
                    // Ошибка записи на диск транслируется также строго через DBus-сервис
                    if (mainWin) {
                        mainWin->sendSystemNotification(QStringLiteral("Ошибка файла"),
                                                        QStringLiteral("Критическая ошибка: Не удалось создать файл на диске!"));
                    }
                }
            }
        });
    }

    // 1. Ищем виджет QCustomPlot на новой 3-й странице вашего stackedWidget
    // (this->ui->stackedWidget используется, так как внутри panel_other.cpp у вас есть доступ к собственному UI)
    QCustomPlot *lossPlot = nullptr;
    if (this->ui && this->ui->stackedWidget && this->ui->stackedWidget->widget(3)) {
        lossPlot = this->ui->stackedWidget->widget(3)->findChild<QCustomPlot*>(QStringLiteral("lossPlot"));

        // Если QCustomPlot сам является корневым виджетом 3-й страницы, делаем каст:
        if (!lossPlot) {
            lossPlot = qobject_cast<QCustomPlot*>(this->ui->stackedWidget->widget(3));
        }
    }

    // 2. ФИЗИЧЕСКАЯ НАСТРОЙКА И СТИЛИЗАЦИЯ КОРДИАТНОЙ СЕТКИ
    if (lossPlot) {
        // Полностью очищаем старые кривые перед запуском сессии
        lossPlot->clearGraphs();

        // А. График 0: Линия Train Loss (Ошибка на обучающей выборке ПАК)
        lossPlot->addGraph();
        lossPlot->graph(0)->setName(QStringLiteral("Train Loss"));
        lossPlot->graph(0)->setPen(QPen(QColor("#0055ff"), 2.5)); // Синий цвет, толщина 2.5px
        // Добавляем маркеры-точки на каждом шаге эпохи для наглядности
        lossPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,
                                                            QPen(QColor("#0033cc"), 1), QBrush(QColor("#0055ff")), 6));

        // Б. График 1: Линия Validation Loss (Ошибка на валидационной выборке ПАК)
        lossPlot->addGraph();
        lossPlot->graph(1)->setName(QStringLiteral("Validation Loss"));
        lossPlot->graph(1)->setPen(QPen(QColor("#ff3333"), 2.5)); // Красный цвет, толщина 2.5px
        lossPlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,
                                                            QPen(QColor("#cc0000"), 1), QBrush(QColor("#ff3333")), 6));

        // В. Оформление холста под монохромный дизайн современных IDE (Белый фон, тонкая сетка)
        lossPlot->setBackground(QBrush(QColor("#ffffff")));
        lossPlot->xAxis->setBasePen(QPen(QColor("#888888"), 1));
        lossPlot->yAxis->setBasePen(QPen(QColor("#888888"), 1));
        lossPlot->xAxis->setTickPen(QPen(QColor("#888888"), 1));
        lossPlot->yAxis->setTickPen(QPen(QColor("#888888"), 1));

        // Настраиваем пунктирные светлые линии сетки (DotLine)
        lossPlot->xAxis->grid()->setPen(QPen(QColor("#e2e8f0"), 1, Qt::DotLine));
        lossPlot->yAxis->grid()->setPen(QPen(QColor("#e2e8f0"), 1, Qt::DotLine));

        // Названия осей по ГОСТ стандарту MLOps
        lossPlot->xAxis->setLabel(QStringLiteral("Эпохи обучения"));
        lossPlot->yAxis->setLabel(QStringLiteral("Ошибка (Loss)"));

        // Применяем профессиональный моноширинный шрифт JetBrains Mono
        QFont plotFont(QStringLiteral("JetBrains Mono"), 10);
        lossPlot->xAxis->setLabelFont(plotFont);
        lossPlot->yAxis->setLabelFont(plotFont);
        lossPlot->xAxis->setTickLabelFont(plotFont);
        lossPlot->yAxis->setTickLabelFont(plotFont);

        // Включаем интерактивность: оператор может перемещать график мышкой и зумить колесиком
        lossPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

        // Настройка плавающей легенды (индикаторы линеек в углу экрана)
        lossPlot->legend->setVisible(true);
        lossPlot->legend->setFont(plotFont);
        lossPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 220))); // Полупрозрачный белый фон
        lossPlot->legend->setBorderPen(QPen(QColor("#cbd5e1"), 1));     // Светло-серая рамка легенды

        // Устанавливаем базовый стартовый диапазон осей координат (10 эпох, LOSS до 50)
        lossPlot->xAxis->setRange(1, 10);
        lossPlot->yAxis->setRange(0, 50);

        // Отрисовываем пустой подготовленный холст в Qt-интерфейсе
        lossPlot->replot();
        qDebug() << ">>> [UI] Виджет QCustomPlot успешно инициализирован на Index 3.";
    }

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

void panel_other::updateErrorTable(const QString &filePath, const QStringList &errorLines)
{
    QString fileName = QFileInfo(filePath).fileName();

    // 1. Сначала удаляем из таблицы старые ошибки ТОЛЬКО этого файла
    for (int i = ui->problemsTable->rowCount() - 1; i >= 0; --i) {
        QTableWidgetItem *fileItem = ui->problemsTable->item(i, 3);
        if (fileItem && fileItem->data(Qt::UserRole).toString() == filePath) {
            ui->problemsTable->removeRow(i);
        }
    }

    // 2. Заполняем таблицу новыми ошибками, если они есть
    for (const QString &errorLine : errorLines) {
        QStringList parts = errorLine.split('|');
        if (parts.size() < 3) continue;

        QString errorCode = parts[0].trimmed();
        QString errorRow  = parts[1].trimmed();
        QString errorText = parts[2].trimmed();

        int currentRow = ui->problemsTable->rowCount();
        ui->problemsTable->insertRow(currentRow);

        QTableWidgetItem *codeItem = new QTableWidgetItem(errorCode);
        QTableWidgetItem *rowItem  = new QTableWidgetItem(errorRow);
        QTableWidgetItem *descItem = new QTableWidgetItem(errorText);
        QTableWidgetItem *fileItem = new QTableWidgetItem(fileName);

        // Прячем абсолютный путь для навигации
        fileItem->setData(Qt::UserRole, filePath);

        // Раскраска под стиль PEP8 (E501 и синтаксис)
        if (errorCode.startsWith("E") || errorCode.startsWith("F")) {
            codeItem->setForeground(QBrush(QColor(0xef5350)));
        } else if (errorCode.startsWith("W")) {
            codeItem->setForeground(QBrush(QColor(0xffa726)));
        }

        ui->problemsTable->setItem(currentRow, 0, codeItem);
        ui->problemsTable->setItem(currentRow, 1, rowItem);
        ui->problemsTable->setItem(currentRow, 2, descItem);
        ui->problemsTable->setItem(currentRow, 3, fileItem);
    }
}

void panel_other::clearErrorsForFile(const QString &filePath)
{
    // Очищаем все строки таблицы, чтобы убрать старые зависшие ячейки
    ui->problemsTable->setRowCount(0);
}

void panel_other::addErrorRow(const QString &filePath, const QString &code, const QString &line, const QString &description)
{
    int currentRow = ui->problemsTable->rowCount();
    ui->problemsTable->insertRow(currentRow);

    // 1. Ищем главное окно, чтобы вытащить свойство папки проекта
    // Идем вверх по иерархии родителей, пока не найдем объект главного окна
    QWidget *mainWindowWidget = this->parentWidget();
    while (mainWindowWidget && !mainWindowWidget->inherits("QMainWindow")) {
        mainWindowWidget = mainWindowWidget->parentWidget();
    }

    QString projectRoot = mainWindowWidget ? mainWindowWidget->property("currentOpenProjectPath").toString() : "";
    QString relativePathDisplay;

    if (!projectRoot.isEmpty()) {
        QDir rootDir(projectRoot);
        // Вычисляем красивый относительный путь (например: scripts/train.py)
        relativePathDisplay = rootDir.relativeFilePath(filePath);
    }

    // РЕЗЕРВНЫЙ СЦЕНАРИЙ (Если relativeFilePath вернул пустую строку или сломался)
    if (relativePathDisplay.isEmpty() || relativePathDisplay.startsWith("..") || relativePathDisplay == filePath) {
        QFileInfo fileInfo(filePath);
        // Вручную собираем строку вида "scripts/train.py" на основе структуры папок
        relativePathDisplay = fileInfo.dir().dirName() + "/" + fileInfo.fileName();
    }

    // 2. Создаем элементы ячеек
    QTableWidgetItem *codeItem = new QTableWidgetItem(code);
    QTableWidgetItem *rowItem  = new QTableWidgetItem(line);
    QTableWidgetItem *descItem = new QTableWidgetItem(description);
    QTableWidgetItem *fileItem = new QTableWidgetItem(relativePathDisplay);

    // Надежно прячем полный путь к файлу в UserRole (необходимо для двойного клика курсором!)
    fileItem->setData(Qt::UserRole, filePath);

    // Стилизация кодов под PEP8
    if (code.startsWith("E501")) {
        codeItem->setForeground(QBrush(QColor(0xffa726)));
    } else if (code.startsWith("E") || code.startsWith("F")) {
        codeItem->setForeground(QBrush(QColor(0xef5350)));
    }

    // 3. Раскладываем элементы строго по колонкам новой строки
    ui->problemsTable->setItem(currentRow, 0, codeItem); // Код (E501)
    ui->problemsTable->setItem(currentRow, 1, rowItem);  // Строка (147)
    ui->problemsTable->setItem(currentRow, 2, descItem); // Описание ошибки
    ui->problemsTable->setItem(currentRow, 3, fileItem); // Относительный путь (scripts/train.py)

    // Принудительно растягиваем колонку описания
    if (ui->problemsTable->horizontalHeader()) {
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        ui->problemsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    }
}

void panel_other::activateMode(DisplayMode mode, QSplitter *mainVerticalSplitter)
{
    Q_UNUSED(mainVerticalSplitter);

    // -------------------------------------------------------------------------
    // 🔥 АБСОЛЮТНЫЙ ФИЛЬТР ЛОЖНЫХ ЗАПУСКОВ (Защита от фоновых сигналов)
    // -------------------------------------------------------------------------
    // Проверяем, нажата ли кнопка Терминала физически.
    // Если метод вызван фоновым сигналом/таймером, а кнопка "отжата" — немедленно выходим!
    if (this->parentWidget()) {
        QPushButton *btnTerm = this->parentWidget()->findChild<QPushButton*>("btnTerminal");
        if (btnTerm && !btnTerm->isChecked()) {
            this->setVisible(false);
            this->hide();
            return; // ЖЕСТКИЙ СИЛОВОЙ ВОЗВРАТ — БЛОКИРУЕМ ОТКРЫТИЕ!
        }
    }

    if (!ui) return;

    // ШАГ 1: Показываем панель на экране (выполнится ТОЛЬКО если кнопка зажата)
    this->show();
    this->setVisible(true);

    // Установка корректной высоты и компоновки (Ваш рабочий сеточный код)
    int targetHeight = 390;
    this->setMinimumHeight(targetHeight);
    this->setFixedHeight(targetHeight);
    this->setContentsMargins(10, 0, 10, 28); // Зазор над статусбаром

    if (ui->splitter) {
        ui->splitter->setContentsMargins(0, 0, 0, 0);
        ui->splitter->refresh();
    }

    // ШАГ 2: Управление внутренним контентом в зависимости от нажатой кнопки
    switch (mode) {
    case TerminalMode:
        if (ui->problemsContainer) ui->problemsContainer->hide();
        if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(0);
        break;
    case DebugMode:
        if (ui->problemsContainer) ui->problemsContainer->show();
        if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(2);
        break;
    case TrainingMode:
        if (ui->problemsContainer) ui->problemsContainer->hide();
        if (ui->stackedWidget) ui->stackedWidget->setCurrentIndex(2);
        if (ui->btnViewLog) ui->btnViewLog->click();
        break;
    }

    if (this->parentWidget() && this->parentWidget()->layout()) {
        this->parentWidget()->layout()->activate();
    }
}














