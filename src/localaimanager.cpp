#include "aipromptwidget.h"
#include "localaimanager.h"
#include "neuro_programm.h"
#include "ui_ai_panel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QJsonArray>
#include <QNetworkReply>

LocalAiManager::LocalAiManager(QObject *parent)
    : QObject(parent)
    , m_currentMode("base")
    , m_modelReady(false)
{
    m_serverProcess = new QProcess(this);
    m_networkManager = new QNetworkAccessManager(this);

    // Логирование вывода Python-сервера напрямую в отладку Qt Creator
    m_serverProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_serverProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        qDebug() << "[AI Server System Log]:" << m_serverProcess->readAllStandardOutput().trimmed();
    });
}

LocalAiManager::~LocalAiManager()
{
    this->stopServer();
}

void LocalAiManager::startServer() {
    if (m_serverProcess->state() == QProcess::Running) return;

    emit statusChanged("ИИ: Установка сетевого моста...", "#FFA500");

    QString pythonBin = "/home/elf/venv/bin/python3";
    QString scriptPath = "/home/elf/pyTorch-Studio/scripts/server.py";
    // ИСПРАВЛЕНО: Флаг "-u" намертво отключает буферизацию вывода в Linux
    QStringList arguments;
    arguments << "-u" << scriptPath;

    // Запускаем процесс Python с флагом unbuffered
    m_serverProcess->start(pythonBin, arguments);

    QTimer* pingTimer = new QTimer(this);
    connect(pingTimer, &QTimer::timeout, this, [this, pingTimer]() {
        // УСПЕШНО НАСТРОЕНО: Точный адрес для пинга состояния FastAPI
        QNetworkRequest request(QUrl("http://127.0.0"));
        QNetworkReply* reply = m_networkManager->get(request);

        connect(reply, &QNetworkReply::finished, this, [this, reply, pingTimer]() {
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                bool serverModelLoaded = doc.object().value("model_loaded").toBool();

                // Триггер фоновой загрузки весов при первом успешном пинге открывшегося сокета
                static bool loadTriggered = false;
                if (!loadTriggered) {
                    // УСПЕШНО НАСТРОЕНО: Точный адрес для инициализации весов GGUF
                    m_networkManager->post(QNetworkRequest(QUrl("http://127.0.0")), QByteArray());
                    loadTriggered = true;
                    qDebug() << ">>> [AI Bridge]: Сетевой порт открыт. Команда фонового чтения GGUF отправлена!";
                }

                if (!serverModelLoaded) {
                    emit statusChanged("ИИ: Фоновое чтение весов модели Qwen-Base с диска...", "#FFA500");
                } else {
                    qDebug() << ">>> [AI Bridge]: ИИ-модель полностью загружена в VRAM/ОЗУ!";
                    emit statusChanged("ИИ: Автодополнение активно", "#555555");
                    m_modelReady = true;
                    emit modelLoadedAndReady();
                    pingTimer->stop();
                    pingTimer->deleteLater();
                }
            }
            reply->deleteLater();
        });
    });
    pingTimer->start(1500);
}

void LocalAiManager::stopServer() {
    if (!m_serverProcess) return;
    this->disconnect();

    if (m_serverProcess->state() != QProcess::NotRunning) {
        qDebug() << ">>> [AI Bridge]: Посылаем SIGTERM процессу Python...";
        m_serverProcess->terminate();
        if (!m_serverProcess->waitForFinished(500)) {
            qWarning() << ">>> [AI Bridge]: Python завис. Выжигаем процесс через SIGKILL...";
            m_serverProcess->kill();
            m_serverProcess->waitForFinished(300);
        }
    }
    m_modelReady = false;
}

void LocalAiManager::switchMode(const QString &mode) {
    m_currentMode = mode;
    if (mode == "instruct") {
        emit statusChanged("ИИ: Рокировка ОЗУ (Загрузка чата)...", "#FFA500");
    }

    // ИСПРАВЛЕНО: Строгий URL с портом и спецификатором подстановки %1
    QNetworkRequest request(QUrl(QString("http://127.0.0").arg(mode)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply, mode]() {
        if (reply->error() == QNetworkReply::NoError) {
            if (mode == "instruct") {
                emit statusChanged("ИИ: Чат готов", "green");
                emit aiReadyForChat();
            } else {
                emit statusChanged("ИИ: Автодополнение активно", "#555555");
            }
        } else {
            emit statusChanged("ИИ: Ошибка смены режима", "red");
        }
        reply->deleteLater();
    });
}

void LocalAiManager::sendChatCommand(const QString &prompt, const QString &context) {
    QJsonObject json;
    json["prompt"] = prompt;
    json["context"] = context;

    // ИСПРАВЛЕНО: Полный сетевой путь до метода генерации чата
    QNetworkRequest request(QUrl("http://127.0.0.1:8000/v1/generate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument resDoc = QJsonDocument::fromJson(reply->readAll());
            emit codeGenerated(resDoc.object().value("code").toString());
        }
        reply->deleteLater();
    });
}

void LocalAiManager::notifyTrainingStart() {
    // ИСПРАВЛЕНО: Адрес уведомления о начале тренировки
    m_networkManager->post(QNetworkRequest(QUrl("http://127.0.0")), QByteArray());
    emit statusChanged("ИИ: Приостановлен (Идет обучение)", "red");
}

void LocalAiManager::notifyTrainingStop() {
    // ИСПРАВЛЕНО: Адрес уведомления о завершении тренировки
    m_networkManager->post(QNetworkRequest(QUrl("http://127.0.0")), QByteArray());
    emit statusChanged("ИИ: Автодополнение активно", "#555555");
}

void LocalAiManager::requestAutocomplete(const QString &prefix, const QString &suffix)
{
    QJsonObject json;
    json["prefix"] = prefix.right(1500);
    json["suffix"] = suffix.left(1500);

    QUrl url;
    url.setScheme("http");
    url.setHost("127.0.0.1");
    url.setPort(8000);
    url.setPath("/v1/autocomplete");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "❌ Ошибка ИИ-моста:" << reply->errorString();
        } else {
            QByteArray rawData = reply->readAll();
            QJsonDocument resDoc = QJsonDocument::fromJson(rawData);
            QString autocompleteText = resDoc.object().value("code").toString();
            emit autocompleteReceived(autocompleteText);
        }
        reply->deleteLater();
    });
}

void LocalAiManager::abortChatGeneration()
{
    if (m_currentChatReply && m_currentChatReply->isRunning()) {
        qInfo() << "🛑 [AI NETWORK]: Пользователь нажал Отмену. Жестко обрываю HTTP-канал...";
        m_currentChatReply->abort(); // Принудительный сброс сокета на уровне ядра ОС
        m_currentChatReply = nullptr;
    }
}

// ОБНОВЛЕННЫЙ МЕТОД ЗАПРОСА ЧАТА С РАСЧЕТОМ МЕТРИК СКОРОСТИ И ВРЕМЕНИ:
void LocalAiManager::requestChatGeneration(const QString &prompt, const QString &context)
{
    // Считываем настройки пакетного режима, если панель ИИ доступна в памяти
    int batchSize = 32;
    bool lowRamMode = false;

    // if (Neuro_programm::self != nullptr && Neuro_programm::self->aiPanel != nullptr) {
    //     if (Neuro_programm::self->aiPanel->comboBatchSize != nullptr) {
    //         batchSize = Neuro_programm::self->aiPanel->comboBatchSize->currentText().toInt();
    //     }
    //     if (Neuro_programm::self->aiPanel->ui && Neuro_programm::self->aiPanel->ui->comboDevice_2) {
    //         QString deviceText = Neuro_programm::self->aiPanel->ui->comboDevice_2->currentText().toLower();
    //         if (deviceText.contains("cpu")) {
    //             lowRamMode = true;
    //         }
    //     }
    // }

    // Собираем расширенную Pydantic-структуру для FastAPI
    QJsonObject mainJson;
    mainJson["prompt"] = prompt;
    mainJson["context"] = context.isEmpty() ? "" : context;
    mainJson["batch_size"] = batchSize;
    mainJson["low_ram_mode"] = lowRamMode;

    QUrl url;
    url.setScheme("http");
    url.setHost("127.0.0.1");
    url.setPort(8000);
    url.setPath("/v1/generate");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Жестко затираем старый лог диска перед стартом сессии инференса
    QFile::remove("/tmp/ai_status.txt");

    // Выставляем стартовый Этап 1: Ожидание сети
    QString initialStatus = "🌐 Ожидание подключения к Python-серверу...";
    if (Neuro_programm::self && Neuro_programm::self->m_activePromptWidget) {
        Neuro_programm::self->m_activePromptWidget->setStatusText(initialStatus);
        Neuro_programm::self->m_activePromptWidget->setInputsEnabled(false);
    }
    if (Neuro_programm::self && Neuro_programm::self->statusLogLabel) {
        Neuro_programm::self->statusLogLabel->setText(initialStatus);
    }

    // БЛОК ИНДИКАЦИИ: Асинхронный опрос диска с математическим расчетом производительности CPU
    QTimer* statusUiTimer = new QTimer(this);
    connect(statusUiTimer, &QTimer::timeout, this, [this]() {
        QFile file("/tmp/ai_status.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString currentStatus = QString::fromUtf8(file.readAll()).trimmed();
            file.close();

            if (!currentStatus.isEmpty() && Neuro_programm::self) {
                QString finalMetricsStatus = currentStatus;

                // Если Python перешел к генерации токенов (Этап 3), рассчитываем скорость
                if (currentStatus.contains("токенов")) {
                    int tokenCount = 0;

                    // АДАПТИВНЫЙ ПАРСЕР: Извлекаем любую группу цифр, идущую перед корнем "токен"
                    QRegularExpression rx("(\\d+)\\s*токен");
                    QRegularExpressionMatch match = rx.match(currentStatus);
                    if (match.hasMatch()) {
                        tokenCount = match.captured(1).toInt();
                    }

                    // Считаем прошедшее время вычислений в секундах
                    qint64 elapsedMs = Neuro_programm::self->m_aiGenerationTimer.elapsed();
                    double elapsedSec = elapsedMs / 1000.0;

                    if (elapsedSec > 0.1 && tokenCount > 0) {
                        // Вычисляем скорость: Сгенерировано токенов / Прошло секунд
                        double tokensPerSecond = tokenCount / elapsedSec;

                        // Прогнозируем оставшееся время (ETA). Берем средний PyTorch метод за 256 токенов
                        int expectedTotalTokens = qMax(256, tokenCount + 15);
                        int remainingTokens = qMax(0, expectedTotalTokens - tokenCount);
                        int etaSeconds = (tokensPerSecond > 0.1) ? static_cast<int>(remainingTokens / tokensPerSecond) : 0;

                        // Форматируем строку под промышленный стандарт сред разработки
                        finalMetricsStatus = QString("✍️ Генерирую: %1 ток. [%2 ток/сек] | Осталось: ~%3 сек")
                                                 .arg(tokenCount)
                                                 .arg(tokensPerSecond, 0, 'f', 1) // 1 знак после запятой
                                                 .arg(etaSeconds);
                    }
                }

                // Синхронно штампуем метрики в нижнюю панель и во всплывающий виджет
                if (Neuro_programm::self->statusLogLabel) {
                    Neuro_programm::self->statusLogLabel->setText(finalMetricsStatus);
                }

                AiPromptWidget* promptWidget = Neuro_programm::self->m_activePromptWidget;
                if (!promptWidget) {
                    promptWidget = Neuro_programm::self->findChild<AiPromptWidget*>();
                }

                if (promptWidget != nullptr) {
                    promptWidget->setStatusText(finalMetricsStatus);
                    QCoreApplication::processEvents(); // Жестко пробиваем фриз экрана, заставляя обновить пиксели
                }
            }
        }
    });
    statusUiTimer->start(400); // Интервал опроса RAM-диска /tmp/ — 400 мс

    qInfo() << ">>> [AI NETWORK]: Отправляю запрос на генерацию кода...";

    // Фиксируем сетевой ответ в переменной класса для мгновенной отмены
    m_currentChatReply = m_networkManager->post(request, QJsonDocument(mainJson).toJson());

    connect(m_currentChatReply, &QNetworkReply::finished, this, [this, statusUiTimer]() {
        statusUiTimer->stop();
        statusUiTimer->deleteLater();
        QFile::remove("/tmp/ai_status.txt");

        if (!m_currentChatReply) return;

        // Если отмена была вызвана пользователем, сокет вернет OperationCanceledError
        if (m_currentChatReply->error() != QNetworkReply::NoError && m_currentChatReply->error() != QNetworkReply::OperationCanceledError) {
            qCritical() << "❌ Ошибка генерации ИИ-чата:" << m_currentChatReply->errorString();

            // Защитное закрытие окна промпта при аварии сети
            if (Neuro_programm::self && Neuro_programm::self->m_activePromptWidget) {
                Neuro_programm::self->m_activePromptWidget->close();
                Neuro_programm::self->m_activePromptWidget->deleteLater();
                Neuro_programm::self->m_activePromptWidget = nullptr;
            }
        } else if (m_currentChatReply->error() == QNetworkReply::NoError) {
            QByteArray rawData = m_currentChatReply->readAll();
            QJsonDocument resDoc = QJsonDocument::fromJson(rawData);

            // Безопасно парсим ответ бэкенда через смарт-парсер ключей
            QJsonObject jsonObj = resDoc.object();
            QString generatedCode;
            if (jsonObj.contains("code")) {
                generatedCode = jsonObj.value("code").toString();
            } else if (jsonObj.contains("response")) {
                generatedCode = jsonObj.value("response").toString();
            }

            if (!generatedCode.isEmpty()) {
                emit codeGenerated(generatedCode); // Выстреливает сигналом инжекции в CodeEditor
            }
        }

        m_currentChatReply->deleteLater();
        m_currentChatReply = nullptr; // Полностью очищаем дескриптор сессии из памяти ноутбука
    });
}






