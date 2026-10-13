#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QElapsedTimer>

class AiPromptWidget : public QWidget {
    Q_OBJECT // МАКРОС ОБЯЗАТЕЛЕН: регистрирует класс в метаобъектной системе Qt

public:
    explicit AiPromptWidget(QWidget *parent = nullptr);
    void setStatusText(const QString &text);
    void setInputsEnabled(bool enabled);
    void startBusyIndicator(const QString &busyMessage = "Генерация кода PyTorch");
    void stopBusyIndicator(const QString &finalMessage = "Готово");
    QLineEdit* lineEdit() const { return m_promptEdit; }
    void startGenerationStream();
    void updateTokenCount(int totalTokens, double tokensPerSec);
    void finishGenerationStream(int totalTokens);

signals:
    // Сигнал, который мы связываем на строке 3726
    void promptSubmitted(const QString &text);
    void cancelRequested();

private:
    QLineEdit    *m_promptEdit{nullptr};
    QPushButton  *m_sendButton{nullptr};
    QPushButton  *m_cancelButton{nullptr};
    QLabel       *m_statusLabel{nullptr};
    QProgressBar *m_progressBar{nullptr};

    // Таймер для анимации статуса (...)
    QTimer       *m_dotsTimer{nullptr};
    QString       m_baseStatusText;
    int           m_dotsCount{0};
    int m_tokenCounter{0};
    QElapsedTimer m_streamTimer;
};
