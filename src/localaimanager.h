#pragma once
#include <QObject>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class LocalAiManager : public QObject {
    Q_OBJECT
public:
    explicit LocalAiManager(QObject *parent = nullptr);
    ~LocalAiManager();

    // Методы управления жизненным циклом сервера
    void startServer();
    void stopServer();
    void notifyTrainingStart();
    void notifyTrainingStop();

    // Методы работы с ИИ-моделями
    void switchMode(const QString &mode);
    void sendChatCommand(const QString &prompt, const QString &context);
    void requestAutocomplete(const QString &prefix, const QString &suffix);
    void requestChatGeneration(const QString &prompt, const QString &context);
    void abortChatGeneration();

signals:
    // Сигналы, на которые будет подписываться интерфейс Neuro_programm
    void statusChanged(const QString &text, const QString &colorHtml);
    void aiReadyForChat();
    void codeGenerated(const QString &code);
    void autocompleteReceived(const QString &code);
    void modelLoadedAndReady();

private:
    QProcess *m_serverProcess;
    QNetworkAccessManager *m_networkManager;
    QString m_currentMode;
    bool m_modelReady = false;
    QNetworkReply* m_currentChatReply = nullptr;
};
