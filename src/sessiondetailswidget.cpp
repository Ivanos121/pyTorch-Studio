#include "sessiondetailswidget.h"
#include "ui_sessiondetailswidget.h"
#include <QDebug>
#include <QCoreApplication>
#include <qstyle.h>
#include <QThread>

SessionDetailsWidget::SessionDetailsWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::SessionDetailsWidget)
{
    ui->setupUi(this); // Инициализируем форму из .ui файла

    m_activeWebServiceProcess = nullptr;

    // 1. АППАРАТНЫЙ КОННЕКТ КНОПКИ ВОЗВРАТА К ЖУРНАЛУ СЕССИЙ (ПРОПИСАН ЖЕСТКО)
    if (ui->btnBack) {
        connect(ui->btnBack, &QPushButton::clicked, this, [this]() {
            qDebug() << ">>> [MLOps GUI]: Отправлен сигнал backToJournalRequested наружу в MainWindow.";
            emit backToJournalRequested(); // Передаем команду переключения экранов в главное окно
        });
    }

    // Настраиваем логику и коннект кнопки сворачивания Chromium веб-окна в шапке
    if (ui->btnMinimizeWeb) {
        ui->btnMinimizeWeb->hide();
        connect(ui->btnMinimizeWeb, &QPushButton::clicked, this, [this]() {
            this->setWebMaximizeMode(false);
        });
    }

    // Настраиваем стили текстового браузера под темную тему
    if (ui->m_passportBrowser) {
        ui->m_passportBrowser->setStyleSheet(QStringLiteral(
            "QTextBrowser { background-color: #1e1e1e; color: #d4d4d4; font-family: monospace; border: 1px solid #333; font-size: 13px; }"
            ));
    }
    if (ui->m_graphLabel) {
        // Явно указываем Qt6 использовать оригинальные цвета изображений (Color Rendering)
        ui->m_graphLabel->setAttribute(Qt::WA_NoSystemBackground);
        ui->m_graphLabel->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #151515; border: 1px solid #333; color: initial; }"
            ));
    }

    // РОЖДЕНИЕ ЕДИНСТВЕННОГО УНИВЕРСАЛЬНОГО ВЕБ-ВИДЖЕТА CHROMIUM
    m_webView = new QWebEngineView(this);
    m_webView->setObjectName(QStringLiteral("universalDashboardWebView"));
    m_webView->hide(); // Изначально скрыт

    // Встраиваем Chromium в главный вертикальный Layout формы строго НАД пустым подвалом кнопок
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout && ui->bottomButtonsArea) {
        mainLayout->insertWidget(mainLayout->indexOf(ui->bottomButtonsArea), m_webView, 1);
    } else if (mainLayout) {
        mainLayout->addWidget(m_webView, 1);
    }

    this->clearPanel();
}

SessionDetailsWidget::~SessionDetailsWidget()
{
    if (m_activeWebServiceProcess) {
        m_activeWebServiceProcess->kill();
        m_activeWebServiceProcess->waitForFinished(1000);
    }

    delete ui;
}

void SessionDetailsWidget::setWebMaximizeMode(bool maximized) {
    if (maximized) {
        // --- РЕЖИМ МАКСИМИЗАЦИИ (ВЕБ ИЛИ КАРТИНКА НА ВЕСЬ ЭКРАН) ---
        if (ui->bottomButtonsArea) ui->bottomButtonsArea->hide();
        if (ui->btnBack) ui->btnBack->hide();
        if (ui->btnMinimizeWeb) ui->btnMinimizeWeb->show();

        // Проверяем, загружен ли живой URL веб-службы в Chromium
        QUrl currentUrl = m_webView ? m_webView->url() : QUrl();
        bool isWebActive = (currentUrl.isValid() && currentUrl.toString() != QStringLiteral("about:blank") && !currentUrl.isEmpty());

        if (isWebActive) {
            // СЦЕНАРИЙ А: Инженер открыл ВЕБ (TensorBoard или Streamlit)
            if (ui->reportContentArea) ui->reportContentArea->hide(); // Скрываем паспорт и встроенный в него график
            if (ui->boxGraph) ui->boxGraph->hide();
            if (ui->m_graphLabel) ui->m_graphLabel->hide();

            if (m_webView) m_webView->show(); // Раскрываем браузер на 100% экрана
            qDebug() << ">>> [MLOps ROUTER]: Интерфейс максимизирован под ВЕБ-службу.";
        } else {
            // СЦЕНАРИЙ Б: Инженер открыл локальный PNG-ГРАФИК верификации
            if (m_webView) m_webView->hide();

            if (ui->reportContentArea) ui->reportContentArea->show();
            if (ui->boxPassport) ui->boxPassport->hide(); // Прячем левый паспорт текста
            if (ui->m_passportBrowser) ui->m_passportBrowser->hide();

            if (ui->boxGraph) ui->boxGraph->show();       // Раскрываем правый график во всю ширь
            if (ui->m_graphLabel) ui->m_graphLabel->show();
            qDebug() << ">>> [MLOps ROUTER]: Интерфейс максимизирован под PNG-график.";
        }
    }
    else {
        // --- РЕЖИМ СВОРЫВАНИЯ (ВОЗВРАТ К ИСХОДНОМУ ТЕКСТОВОМУ ПАСПОРТУ) ---

        // 1. УБИВАЕМ СТАНДАРТНЫЙ УПРАВЛЯЕМЫЙ QPROCESS (Для Streamlit с логированием)
        if (m_activeWebServiceProcess) {
            qDebug() << ">>> [MLOps DAEMON]: Силовое завершение управляемого QProcess...";
            m_activeWebServiceProcess->kill();               // Посылаем SIGKILL в ядро Linux
            m_activeWebServiceProcess->waitForFinished(500); // Ждем полсекунды до полной выгрузки
            delete m_activeWebServiceProcess;                // Освобождаем память C++
            m_activeWebServiceProcess = nullptr;             // Обнуляем указатель
        }

        // 2. УБИВАЕМ НЕЗАВИСИМЫЙ ПРОЦЕСС ПО СИСТЕМНОМУ PID (Для TensorBoard)
        if (this->m_activeWebServicePid > 0) {
            qDebug() << ">>> [MLOps DAEMON]: Силовое тушение веб-сервера по PID:" << this->m_activeWebServicePid;
            QProcess::execute(QStringLiteral("kill"), QStringList() << QStringLiteral("-9") << QString::number(this->m_activeWebServicePid));
            this->m_activeWebServicePid = 0;
        }

        // 3. ЗАКРЫВАЕМ ВСТРОЕННЫЙ БРАУЗЕР И ОЧИЩАЕМ ОПЕРАТИВНУЮ ПАМЯТЬ CHROMIUM
        if (m_webView) {
            m_webView->hide();
            m_webView->setUrl(QUrl(QStringLiteral("about:blank")));
        }

        // 4. ГАСИМ ПОЛНОЭКРАННЫЕ ГРАФИКИ ВЕРИФИКАЦИИ
        if (ui->boxGraph) ui->boxGraph->hide();
        if (ui->m_graphLabel) ui->m_graphLabel->hide();

        // 5. ПРИНУДИТЕЛЬНО ВОЗВРАЩАЕМ НА ЭКРАН ТЕКСТОВЫЙ ПАСПОРТ И ПОДВАЛ КНОПОК
        if (ui->reportContentArea) ui->reportContentArea->show();
        if (ui->boxPassport) ui->boxPassport->show();
        if (ui->m_passportBrowser) ui->m_passportBrowser->show();
        if (ui->bottomButtonsArea) ui->bottomButtonsArea->show();

        // 6. КОРРЕКТИРУЕМ УПРАВЛЯЮЩИЕ КНОПКИ В ШАПКЕ СТРАНИЦЫ
        if (ui->btnMinimizeWeb) ui->btnMinimizeWeb->hide();
        if (ui->btnBack) ui->btnBack->show();

        // Синхронизируем и мгновенно перерисовываем пиксели геометрии Qt6
        QCoreApplication::processEvents();
        this->updateGeometry();
        qDebug() << ">>> [MLOps GUI]: Успешный сквозной возврат к текстовому паспорту карточки.";
    }
}

void SessionDetailsWidget::clearPanel()
{
    m_currentSessionId.clear();
    m_currentGraphPath.clear();

    if (ui->m_passportBrowser)
    {
        ui->m_passportBrowser->setHtml(QStringLiteral("<p style='color:#888; text-align:center; padding-top:25%; font-size:13px;'>Выберите сессию из журнала сессий двойным кликом...</p>"));
    }

    // СБРОС В ИСХОДНОЕ СОСТОЯНИЕ ДАШБОРДА (ВИДЕН ТОЛЬКО ПАСПОРТ)
    if (ui->reportContentArea) ui->reportContentArea->show();
    if (ui->boxPassport) ui->boxPassport->show();
    if (ui->m_passportBrowser) ui->m_passportBrowser->show();

    // Все остальные тяжелые медиа-контейнеры полностью гасим
    if (ui->boxGraph) ui->boxGraph->hide();
    if (ui->m_graphLabel) ui->m_graphLabel->hide();
    if (m_webView) m_webView->hide();

    // Корректируем навигацию в шапке
    if (ui->btnMinimizeWeb) ui->btnMinimizeWeb->hide();
    if (ui->btnBack) ui->btnBack->show();
}

void SessionDetailsWidget::loadSession(const QString &projectPath, const QString &sessionId) {
    m_currentSessionId = sessionId;
    this->setWebMaximizeMode(false); // Всегда открываемся в режиме текстового паспорта

    // Фиксируем путь к PNG картинке текущей сессии
    m_currentGraphPath = QDir(projectPath).filePath(QStringLiteral("metrics/") + sessionId + QStringLiteral("/thermal_identification_report.png"));

    // 1. ОТРИСОВКА ВЕРИФИКАЦИОННОГО PNG ГРАФИКА (RGB ИЗОЛЯТОР ОТ СТИЛЕЙ ТЕМЫ)
    if (ui->m_graphLabel) {
        if (QFile::exists(m_currentGraphPath)) {
            QImage img(m_currentGraphPath);
            if (!img.isNull()) {
                // Отключаем влияние родительских CSS-стилей темной темы Студии на картинку
                ui->m_graphLabel->setStyleSheet(QStringLiteral("background-color: white; border: 1px solid #333; color: initial; font: initial;"));
                ui->m_graphLabel->setPalette(QApplication::style()->standardPalette());

                // Загружаем пиксели с флагом сохранения оригинального цветового профиля
                QPixmap pix = QPixmap::fromImage(img, Qt::ColorOnly);
                ui->m_graphLabel->setPixmap(pix.scaled(ui->m_graphLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                qDebug() << ">>> [MLOps GUI]: Цветной график успешно изолирован от фильтра стилей Студии.";
            }
        } else {
            ui->m_graphLabel->setPixmap(QPixmap());
            ui->m_graphLabel->setText(QStringLiteral("График верификации отсутствует."));
        }
    }

    // Сбор путей к json-конфигам (Схема дашборда считывается из глобального корня)
    QString jsonPath = QDir(projectPath).filePath(QStringLiteral("metrics/") + sessionId + QStringLiteral("/session_meta.json"));
    QSettings studioSettings(QStringLiteral("/home/elf/.config/PyTorchStudio/pystudio.conf"), QSettings::IniFormat);
    QString studioRoot = studioSettings.value(QStringLiteral("GlobalEnvironment/studio_src_root"), QStringLiteral("/home/elf/pyTorch-Studio")).toString();
    QString layoutPath = QDir(studioRoot).filePath(QStringLiteral("Config/dashboard_layout.json"));

    QJsonObject dataRoot;
    QJsonArray layoutArray;

    QFile fileData(jsonPath);
    if (fileData.open(QIODevice::ReadOnly)) {
        dataRoot = QJsonDocument::fromJson(fileData.readAll()).object();
        fileData.close();
    }

    QFile fileLayout(layoutPath);
    if (fileLayout.open(QIODevice::ReadOnly)) {
        layoutArray = QJsonDocument::fromJson(fileLayout.readAll()).array();
        fileLayout.close();
    }

    // ---------------------------------------------------------------------
    // 2. ДИНАМИЧЕСКАЯ ГЕНЕРАЦИЯ КНОПОК ПОДВАЛА (ПЕРЕСБОРКА ПОД ТЕКУЩИЙ ПРОЕКТ)
    // ---------------------------------------------------------------------
    if (ui->bottomButtonsArea && ui->bottomButtonsArea->layout()) {
        QLayout *btnLayout = ui->bottomButtonsArea->layout();

        // Сносим старые кнопки предыдущей сессии из памяти ОС Linux
        QLayoutItem *child;
        while ((child = btnLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                child->widget()->hide();
                delete child->widget();
            }
            delete child;
        }

        // АППАРАТНАЯ КНОПКА ГРАФИКА ВЕРИФИКАЦИИ (Всегда идет первой)
        QPushButton *btnShowGraph = new QPushButton(QStringLiteral("📊 Показать график верификации"), ui->bottomButtonsArea);
        btnShowGraph->setCursor(Qt::PointingHandCursor);
        btnShowGraph->setStyleSheet(QStringLiteral(
            "QPushButton { font-weight: bold; height: 38px; background-color: #00cc66; color: white; border-radius: 4px; font-size: 14px; padding: 0 20px; }"
            "QPushButton:hover { opacity: 0.9; }"
            ));

        connect(btnShowGraph, &QPushButton::clicked, this, [this]() {
            if (m_webView) m_webView->hide(); // Гарантируем скрытие Chromium перед картинкой
            this->setWebMaximizeMode(true);

            QCoreApplication::processEvents();
            if (ui->m_graphLabel && QFile::exists(m_currentGraphPath)) {
                QPixmap pix(m_currentGraphPath);
                ui->m_graphLabel->setPalette(QPalette());
                ui->m_graphLabel->setPixmap(pix.scaled(ui->m_graphLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        });
        btnLayout->addWidget(btnShowGraph);
        // ДИНАМИЧЕСКИЕ КНОПКИ ИЗ JSON КОНФИГА (TensorBoard и Streamlit)
        for (const QJsonValue &secVal : std::as_const(layoutArray)) {
            QJsonObject section = secVal.toObject();
            if (section[QStringLiteral("section")].toString() == QStringLiteral("web_services")) {
                QJsonArray services = section[QStringLiteral("services")].toArray();

                for (const QJsonValue &serviceVal : std::as_const(services)) {
                    QJsonObject service = serviceVal.toObject();
                    QString name = service[QStringLiteral("name")].toString();
                    QString label = (name == QStringLiteral("tensorboard")) ? QStringLiteral("📈 Открыть TensorBoard") : QStringLiteral("🧱 Запустить симулятор Streamlit");
                    QString bgStyle = (name == QStringLiteral("tensorboard")) ? QStringLiteral("#ff9900") : QStringLiteral("#00aaff");

                    QPushButton *dynamicBtn = new QPushButton(label, ui->bottomButtonsArea);
                    dynamicBtn->setCursor(Qt::PointingHandCursor);
                    dynamicBtn->setStyleSheet(QString(
                                                  "QPushButton { font-weight: bold; height: 38px; background-color: %1; color: white; border-radius: 4px; font-size: 14px; padding: 0 20px; }"
                                                  "QPushButton:hover { opacity: 0.9; }"
                                                  ).arg(bgStyle));

                    // Кэшируем рабочий путь к текущему проекту инженера (например, /home/elf/zcc/z1)
                    m_currentProjectPath = projectPath;

                    connect(dynamicBtn, &QPushButton::clicked, this, [this, service]() {
                                // 1. АППАРАТНОЕ УНИЧТОЖЕНИЕ СТАРЫХ СЕРВЕРОВ (БЕЗОПАСНАЯ ОЧИСТКА ПОРТОВ)
                                if (m_activeWebServiceProcess) {
                                    m_activeWebServiceProcess->kill();
                                    m_activeWebServiceProcess->waitForFinished(500);
                                    delete m_activeWebServiceProcess;
                                    m_activeWebServiceProcess = nullptr;
                                }
                                if (this->m_activeWebServicePid > 0) {
                                    QProcess::execute(QStringLiteral("kill"), QStringList() << QStringLiteral("-9") << QString::number(this->m_activeWebServicePid));
                                    this->m_activeWebServicePid = 0;
                                }

                                // 2. Считываем настройки из JSON конфигурации
                                QString name = service[QStringLiteral("name")].toString();
                                QString baseUrl = service[QStringLiteral("base_url")].toString();
                                int port = service[QStringLiteral("port")].toInt();
                                QString filterParam = service[QStringLiteral("url_filter_param")].toString();
                                QString binaryPath = service[QStringLiteral("binary_path")].toString();

                                m_activeWebServiceProcess = new QProcess(this);
                                QStringList arguments;
                                QString finalExecutable = binaryPath;
                                QString workingDir = m_currentProjectPath;

                                // --- КОНФИГУРАЦИЯ TENSORBOARD ---
                                if (name == QStringLiteral("tensorboard")) {
                                    QString logDir = QDir(m_currentProjectPath).absoluteFilePath(service[QStringLiteral("log_dir_rel")].toString());
                                    arguments << QStringLiteral("--logdir") << logDir
                                              << QStringLiteral("--port")   << QString::number(port)
                                              << QStringLiteral("--host")   << QStringLiteral("127.0.0.1");
                                }
                                // --- КОНФИГУРАЦИЯ STREAMLIT (ЖЕСТКО ИЗ ТЕКУЩЕГО ПРОЕКТА Z1) ---
                                else if (name == QStringLiteral("streamlit")) {
                                    QString relScriptPath = service[QStringLiteral("script_path_rel")].toString(); // "scripts/app.py"

                                    // СБОРКА АБСОЛЮТНОГО ПУТИ: Склеиваем на основе живого пути к проекту!
                                    QString absoluteScriptPath = QDir(m_currentProjectPath).absoluteFilePath(relScriptPath);
                                    workingDir = QFileInfo(absoluteScriptPath).absolutePath(); // папка /home/elf/zcc/z1/scripts

                                    // Задаем рабочую папку, чтобы скрипт Python не растерял относительные пути внутри себя
                                    m_activeWebServiceProcess->setWorkingDirectory(workingDir);

                                    // Отключаем буферизацию логов пайпами C++
                                    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                                    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
                                    m_activeWebServiceProcess->setProcessEnvironment(env);

                                    // Пишем все стэки ошибок Python строго в дисковый лог активной папки scripts/
                                    QString logFile = QDir(workingDir).absoluteFilePath(QStringLiteral("streamlit_debug.log"));
                                    m_activeWebServiceProcess->setStandardOutputFile(logFile);
                                    m_activeWebServiceProcess->setStandardErrorFile(logFile);

                                    // Хак для надежного старта модулей venv в Arch Linux
                                    finalExecutable = QStringLiteral("/home/elf/venv/bin/python");
                                    arguments << QStringLiteral("-m") << QStringLiteral("streamlit")
                                              << QStringLiteral("run") << absoluteScriptPath
                                              << QStringLiteral("--server.port") << QString::number(port)
                                              << QStringLiteral("--server.headless") << QStringLiteral("true");

                                    qDebug() << ">>> [MLOps DAEMON]: ИСПРАВЛЕННЫЙ запуск Streamlit по пути:" << absoluteScriptPath;
                                }

                                // 3. КОНТРОЛИРУЕМЫЙ ЗАПУСК ПРОЦЕССА В СИСТЕМЕ LINUX
                                m_activeWebServiceProcess->start(finalExecutable, arguments);

                                if (m_activeWebServiceProcess->waitForStarted(3000)) {
                                    // Формируем результирующий URL
                                    QString targetUrl = QString("%1:%2/%3%4").arg(baseUrl).arg(port).arg(filterParam).arg(m_currentSessionId);

                                    if (m_webView) {
                                        m_webView->setUrl(QUrl(targetUrl)); // Шьем URL в Chromium, пока Python поднимает сокет
                                    }

                                    this->setWebMaximizeMode(true); // Переключаем макет интерфейса в полноэкранный веб
                                    QCoreApplication::processEvents();

                                    QThread::msleep(2500); // Даем Python 2.5 секунды полностью занять порт

                                    if (m_webView) m_webView->show();
                                } else {
                                    qWarning() << "⚠️ [MLOps DAEMON CRITICAL]: Не удалось выполнить запуск бинарника:" << finalExecutable;
                                }
                            });

                    btnLayout->addWidget(dynamicBtn);
                }
                break;
            }
        }
    }

    // =========================================================================
    // АНАЛИТИКА: Поиск лучшей сессии в проекте для сравнения
    // =========================================================================
    double currentScore = dataRoot.value(QStringLiteral("metrics")).toObject().value(QStringLiteral("final_val_mae")).toDouble();
    double bestScore = 999999.0;
    QString bestSessionId;

    // Сканируем все папки сессий в поисках лучшего final_val_mae
    QDir metricsDir(QDir(projectPath).filePath(QStringLiteral("metrics")));
    QStringList subDirs = metricsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &dirName : subDirs) {
        QFile f(metricsDir.filePath(dirName + QStringLiteral("/session_meta.json")));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonObject meta = QJsonDocument::fromJson(f.readAll()).object();
            double score = meta.value(QStringLiteral("metrics")).toObject().value(QStringLiteral("final_val_mae")).toDouble();
            if (score > 0 && score < bestScore) {
                bestScore = score;
                bestSessionId = dirName;
            }
            f.close();
        }
    }

    // Собираем строку сравнения
    QString comparisonHtml;
    if (bestSessionId == sessionId) {
        comparisonHtml = QStringLiteral("<div style='margin-bottom:8px; color:#00ff00;'><b>🏆 АБСОЛЮТНЫЙ ЛИДЕР:</b> Эта сессия удерживает рекорд точности в проекте!</div>");
    } else if (bestScore < 999999.0) {
        double delta = currentScore - bestScore;
        comparisonHtml = QString(QStringLiteral(
                                     "<div style='margin-bottom:8px; color:#ff5555;'>"
                                     "<b>⚠️ НЕОПТИМАЛЬНО:</b> Уступает лучшей сессии (<a href='%1' style='color:#00aaff; text-decoration:none;'>%1</a>) на <b>+%2 °C</b>"
                                     "</div>"
                                     )).arg(bestSessionId).arg(QString::number(delta, 'f', 2));
    }

    // ---------------------------------------------------------------------
    // 3. ОТРИСОВКА HTML ПАСПОРТА МЕТРИК СЕССИИ (ОБНОВЛЕНИЕ ЧЕРЕЗ ui->)
    // ---------------------------------------------------------------------

    if (!layoutArray.isEmpty() && !dataRoot.isEmpty()) {
        auto getNestedValue = [](const QJsonObject &obj, const QString &nestedKey) -> QJsonValue {
            QStringList parts = nestedKey.split(QLatin1Char('/'));
            QJsonObject currentObj = obj;
            for (int i = 0; i < parts.size() - 1; ++i) currentObj = currentObj[parts[i]].toObject();
            return currentObj[parts.last()];
        };

        // ИСПРАВЛЕНИЕ 1: Меняем background-color на #252526 (родной матовый цвет IDE) или "transparent" (прозрачный)
        QString html = QStringLiteral("<html><body style='font-family:monospace; font-size:12px; line-height:1.6; color:#d4d4d4; background-color:transparent; margin:10px;'>");

        for (const QJsonValue &secVal : std::as_const(layoutArray)) {
            QJsonObject section = secVal.toObject();
            if (section[QStringLiteral("section")].toString() == QStringLiteral("web_services"))
                continue;

            QString secTitle = section[QStringLiteral("section_title")].toString();
            QJsonArray fields = section[QStringLiteral("fields")].toArray();

            // Красивая рамка заголовка секции
            html += QStringLiteral("<div style='margin-top:15px; margin-bottom:8px; border-bottom:1px solid #3c3c3c; padding-bottom:3px;'><b style='color:#00aaff; font-size:13px;'>") + secTitle + QStringLiteral("</b></div>");

            // Начинаем каждую секцию с блочного тега для изоляции
            html += QStringLiteral("<div style='margin-left:5px;'>");

            for (const QJsonValue &fieldVal : std::as_const(fields)) {
                QJsonObject field = fieldVal.toObject();
                QString source = field[QStringLiteral("source")].toString();
                QString label = field[QStringLiteral("label")].toString();
                QString color = field[QStringLiteral("color")].toString();
                QString suffix = field[QStringLiteral("suffix")].toString();
                bool isBold = field[QStringLiteral("bold")].toBool();
                bool isUpper = field[QStringLiteral("uppercase")].toBool();
                QString type = field[QStringLiteral("type")].toString();

                QJsonValue val = source.contains(QLatin1Char('/')) ? getNestedValue(dataRoot, source) : dataRoot[source];
                QString displayValue;
                if (type == QStringLiteral("date")) {
                    QDateTime dt = QDateTime::fromString(val.toString(), Qt::ISODate);
                    displayValue = dt.isValid() ? dt.toString(QStringLiteral("dd.MM.yyyy hh:mm:ss")) : QStringLiteral("Н/Д");
                }
                else if (val.isDouble()) { displayValue = QString::number(val.toDouble(), 'f', 2); }
                else if (val.isBool()) { displayValue = val.toBool() ? QStringLiteral("Вкл") : QStringLiteral("Выкл"); }
                else { displayValue = val.toString(); }

                if (isUpper) displayValue = displayValue.toUpper();
                displayValue += suffix;

                // ИСПРАВЛЕНИЕ 2: Оборачиваем КАЖДЫЙ параметр в блочный тег <div style='margin-bottom:4px;'>
                // Это на 100% гарантирует строгий перенос строки и выстраивание метрик строго в столбик!
                html += QStringLiteral("<div style='margin-bottom:5px;'> • <span style='color:#aaaaaa;'>") + label + QStringLiteral(":</span> ");

                if (!color.isEmpty()) html += QStringLiteral("<span style='color:") + color + QStringLiteral(";'>");
                if (isBold) html += QStringLiteral("<b>");

                html += displayValue;

                if (isBold) html += QStringLiteral("</b>");
                if (!color.isEmpty()) html += QStringLiteral("</span>");

                html += QStringLiteral("</div>"); // Закрываем блок строки параметра
            }

            html += QStringLiteral("</div>"); // Закрываем блок секции
        }

        html += QStringLiteral("</body></html>");
        if (ui->m_passportBrowser) ui->m_passportBrowser->setHtml(html);
    }
}

void SessionDetailsWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (ui->m_graphLabel && !ui->m_graphLabel->isHidden() && !m_currentGraphPath.isEmpty() && QFile::exists(m_currentGraphPath)) {
        QImage img(m_currentGraphPath);
        if (!img.isNull()) {
            QPixmap pix = QPixmap::fromImage(img, Qt::ColorOnly);
            ui->m_graphLabel->setPixmap(pix.scaled(ui->m_graphLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}




