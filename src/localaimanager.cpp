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
    , m_modelReady(true)
{
    //m_serverProcess = new QProcess(this);
    m_networkManager = new QNetworkAccessManager(this);

    // Логирование вывода Python-сервера напрямую в отладку Qt Creator
    // m_serverProcess->setProcessChannelMode(QProcess::MergedChannels);
    // connect(m_serverProcess, &QProcess::readyReadStandardOutput, this, [this]() {
    //     qDebug() << "[AI Server System Log]:" << m_serverProcess->readAllStandardOutput().trimmed();
    // });
}

LocalAiManager::~LocalAiManager()
{
    this->stopServer();
}

void LocalAiManager::startServer() {

    emit statusChanged("ИИ: Проверка подключения к Ollama...", "#FFA500");

    // Пингуем Ollama напрямую на стандартном порту 11434
    QNetworkRequest request(QUrl("http://127.0.0.1:11434/api/tags"));
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << ">>> [AI Bridge]: Сервер Ollama активен и готов к работе!";
            emit statusChanged("ИИ: Автодополнение активно", "#555555");
            m_modelReady = true;
            emit modelLoadedAndReady();
        } else {
            qWarning() << ">>> [AI Bridge]: Ollama не запущена! Выполните: systemctl --user start ollama";
            emit statusChanged("ИИ: Ошибка подключения к Ollama", "red");
            m_modelReady = false;
        }
    });
}

void LocalAiManager::stopServer() {
    // if (!m_serverProcess) return;
    // this->disconnect();

    // if (m_serverProcess->state() != QProcess::NotRunning) {
    //     qDebug() << ">>> [AI Bridge]: Посылаем SIGTERM процессу Python...";
    //     m_serverProcess->terminate();
    //     if (!m_serverProcess->waitForFinished(500)) {
    //         qWarning() << ">>> [AI Bridge]: Python завис. Выжигаем процесс через SIGKILL...";
    //         m_serverProcess->kill();
    //         m_serverProcess->waitForFinished(300);
    //     }
    // }
    // m_modelReady = false;

    // Выгружаем модели из VRAM перед закрытием
    //this->unloadAllModelsFromVram();
    m_modelReady = false;
}

void LocalAiManager::switchMode(const QString &mode)
{
    m_currentMode = mode;

    if (mode == "instruct") {
        emit statusChanged("ИИ: Чат готов (qwen-chat-7b)", "green");
        emit aiReadyForChat();
    } else {
        emit statusChanged("ИИ: Автодополнение активно (qwen-code-3b)", "#555555");
    }
}

void LocalAiManager::sendChatCommand(const QString &prompt, const QString &context) {
    QJsonObject json;
    json["model"] = "qwen-chat-7b"; // Вторая модель для общения/консультаций
    json["stream"] = false;

    QJsonArray messages;
    if (!context.trimmed().isEmpty()) {
        QJsonObject ctxMsg;
        ctxMsg["role"] = "user";
        ctxMsg["content"] = QString("Контекст проекта:\n%1").arg(context);
        messages.append(ctxMsg);
    }

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = prompt;
    messages.append(userMsg);

    json["messages"] = messages;

    QUrl url("http://127.0.0.1:11434/v1/chat/completions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject resDoc = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonArray choices = resDoc.value("choices").toArray();
            if (!choices.isEmpty()) {
                QString answer = choices.first().toObject()
                .value("message").toObject()
                    .value("content").toString();
                emit codeGenerated(answer);
            }
        } else {
            qCritical() << "❌ Ошибка отправки сообщения чата:" << reply->errorString();
        }
    });
}

void LocalAiManager::notifyTrainingStart() {
    qInfo() << "🧹 [AI]: Выгружаю модели LLM из VRAM перед стартом тренировки нейросети...";

    // Посылаем Ollama сигнал keep_alive: 0 для мгновенного освобождения GPU
    const QStringList models = {"qwen-code-3b", "qwen-chat-7b"};
    for (const QString &model : models) {
        QUrl url("http://127.0.0.1:11434/api/generate");
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject body;
        body["model"] = model;
        body["keep_alive"] = 0;

        m_networkManager->post(req, QJsonDocument(body).toJson());
    }

    emit statusChanged("ИИ: Приостановлен (Идет обучение)", "red");
}

void LocalAiManager::notifyTrainingStop() {
    emit statusChanged("ИИ: Автодополнение активно", "#555555");
}

void LocalAiManager::requestAutocomplete(const QString &prefix, const QString &suffix)
{
    // 1. Формируем FIM-запрос (Fill-In-the-Middle) для Ollama
    QJsonObject json;
    json["model"] = "qwen-code-3b";       // Ваша модель кода в Ollama
    json["prompt"] = prefix.right(1500);  // Код до курсора
    json["suffix"] = suffix.left(1500);   // Код после курсора
    json["stream"] = false;

    QJsonObject options;
    options["temperature"] = 0.1;         // Минимальная вариативность для точности синтаксиса
    options["num_predict"] = 64;          // Небольшой лимит длины токенов для быстрого автокомплита
    json["options"] = options;

    // 2. Нативный эндпоинт Ollama для генерации и FIM
    QUrl url("http://127.0.0.1:11434/api/generate");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 3. Отправка POST-запроса
    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "❌ Ошибка ИИ-автодополнения:" << reply->errorString();
            return;
        }

        QByteArray rawData = reply->readAll();
        QJsonObject resDoc = QJsonDocument::fromJson(rawData).object();

        // Ollama на эндпоинте /api/generate возвращает результат в поле "response"
        QString autocompleteText = resDoc.value("response").toString();

        if (!autocompleteText.isEmpty()) {
            emit autocompleteReceived(autocompleteText);
        }
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

void LocalAiManager::requestChatGeneration(const QString &prompt, const QString &context)
{
    // 1. Формируем OpenAI-совместимый JSON для Ollama
    QJsonObject mainJson;
    mainJson["model"] = "qwen-code-3b"; // Имя модели генерации кода в Ollama
    mainJson["temperature"] = 0.2;
    mainJson["stream"] = false;

    QJsonArray messages;

    // Системная инструкция: заставляем модель выдавать ТОЛЬКО код
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = "Ты ассистент программиста в среде PyTorch Studio. Напиши чистый Python-код по запросу пользователя. "
                        "Возвращай ТОЛЬКО чистый исполняемый код без Markdown-разметки (без тройных кавычек ```), "
                        "без вводных слов, пояснений и комментариев.";
    messages.append(sysMsg);

    // Добавляем контекст (код вокруг курсора), если он передан
    if (!context.trimmed().isEmpty()) {
        QJsonObject ctxMsg;
        ctxMsg["role"] = "user";
        ctxMsg["content"] = QString("Текущий контекст кода в файле:\n%1").arg(context);
        messages.append(ctxMsg);
    }

    // Запрос пользователя (из поля ввода Ctrl+J)
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = prompt;
    messages.append(userMsg);

    mainJson["messages"] = messages;

    // Жестко затираем старый лог диска перед стартом сессии инференса
    QFile::remove("/tmp/ai_status.txt");

    // Выставляем стартовый статус
    QString initialStatus = "🌐 Генерация кода через Ollama...";
    if (Neuro_programm::self && Neuro_programm::self->m_activePromptWidget) {
        Neuro_programm::self->m_activePromptWidget->setStatusText(initialStatus);
        Neuro_programm::self->m_activePromptWidget->setInputsEnabled(false);
    }
    if (Neuro_programm::self && Neuro_programm::self->statusLogLabel) {
        Neuro_programm::self->statusLogLabel->setText(initialStatus);
    }

    // БЛОК ИНДИКАЦИИ: Асинхронный опрос диска
    QTimer* statusUiTimer = new QTimer(this);
    connect(statusUiTimer, &QTimer::timeout, this, [this]() {
        QFile file("/tmp/ai_status.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString currentStatus = QString::fromUtf8(file.readAll()).trimmed();
            file.close();

            if (!currentStatus.isEmpty() && Neuro_programm::self) {
                QString finalMetricsStatus = currentStatus;

                if (currentStatus.contains("токенов")) {
                    int tokenCount = 0;
                    QRegularExpression rx("(\\d+)\\s*токен");
                    QRegularExpressionMatch match = rx.match(currentStatus);
                    if (match.hasMatch()) {
                        tokenCount = match.captured(1).toInt();
                    }

                    qint64 elapsedMs = Neuro_programm::self->m_aiGenerationTimer.elapsed();
                    double elapsedSec = elapsedMs / 1000.0;

                    if (elapsedSec > 0.1 && tokenCount > 0) {
                        double tokensPerSecond = tokenCount / elapsedSec;
                        int expectedTotalTokens = qMax(256, tokenCount + 15);
                        int remainingTokens = qMax(0, expectedTotalTokens - tokenCount);
                        int etaSeconds = (tokensPerSecond > 0.1) ? static_cast<int>(remainingTokens / tokensPerSecond) : 0;

                        finalMetricsStatus = QString("✍️ Генерирую: %1 ток. [%2 ток/сек] | Осталось: ~%3 сек")
                                                 .arg(tokenCount)
                                                 .arg(tokensPerSecond, 0, 'f', 1)
                                                 .arg(etaSeconds);
                    }
                }

                if (Neuro_programm::self->statusLogLabel) {
                    Neuro_programm::self->statusLogLabel->setText(finalMetricsStatus);
                }

                AiPromptWidget* promptWidget = Neuro_programm::self->m_activePromptWidget;
                if (!promptWidget) {
                    promptWidget = Neuro_programm::self->findChild<AiPromptWidget*>();
                }

                if (promptWidget != nullptr) {
                    promptWidget->setStatusText(finalMetricsStatus);
                    QCoreApplication::processEvents();
                }
            }
        }
    });
    statusUiTimer->start(400);

    // Явно и жестко передаем строку в QUrl и сразу в QNetworkRequest:
    QUrl targetUrl(QStringLiteral("http://127.0.0.1:11434/v1/chat/completions"));
    QNetworkRequest request(targetUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // ДОБАВЬТЕ ЭТУ ОТЛАДОЧНУЮ СТРОКУ, чтобы своими глазами увидеть URL перед отправкой:
    qDebug() << "🎯 [DEBUG URL]:" << request.url().toString() << "isValid:" << request.url().isValid();

    qInfo() << ">>> [AI NETWORK]: Отправляю запрос на генерацию кода в Ollama...";

    m_currentChatReply = m_networkManager->post(request, QJsonDocument(mainJson).toJson());

    connect(m_currentChatReply, &QNetworkReply::finished, this, [this, statusUiTimer]() {
        statusUiTimer->stop();
        statusUiTimer->deleteLater();
        QFile::remove("/tmp/ai_status.txt");

        if (!m_currentChatReply) return;

        if (m_currentChatReply->error() != QNetworkReply::NoError && m_currentChatReply->error() != QNetworkReply::OperationCanceledError) {
            qCritical() << "❌ Ошибка генерации ИИ:" << m_currentChatReply->errorString();

            if (Neuro_programm::self && Neuro_programm::self->m_activePromptWidget) {
                Neuro_programm::self->m_activePromptWidget->close();
                Neuro_programm::self->m_activePromptWidget->deleteLater();
                Neuro_programm::self->m_activePromptWidget = nullptr;
            }
        } else if (m_currentChatReply->error() == QNetworkReply::NoError) {
            QByteArray rawData = m_currentChatReply->readAll();
            QJsonObject resDoc = QJsonDocument::fromJson(rawData).object();

            // 3. Парсинг ответа формата OpenAI: choices[0].message.content
            QString generatedCode;
            QJsonArray choices = resDoc.value("choices").toArray();
            if (!choices.isEmpty()) {
                generatedCode = choices.first().toObject()
                .value("message").toObject()
                    .value("content").toString().trimmed();
            }

            // Очистка от случайных Markdown блоков ```python ... ```
            if (generatedCode.startsWith("```python")) {
                generatedCode.remove(0, 9);
            } else if (generatedCode.startsWith("```")) {
                generatedCode.remove(0, 3);
            }
            if (generatedCode.endsWith("```")) {
                generatedCode.chop(3);
            }
            generatedCode = generatedCode.trimmed();

            if (!generatedCode.isEmpty()) {
                emit codeGenerated(generatedCode); // Вставка сгенерированного кода в редактор
            }

            // Закрываем окно промпта после успешной генерации
            if (Neuro_programm::self && Neuro_programm::self->m_activePromptWidget) {
                Neuro_programm::self->m_activePromptWidget->close();
                Neuro_programm::self->m_activePromptWidget->deleteLater();
                Neuro_programm::self->m_activePromptWidget = nullptr;
            }
        }

        m_currentChatReply->deleteLater();
        m_currentChatReply = nullptr;
    });
}






