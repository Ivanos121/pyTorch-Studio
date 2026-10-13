#include "aipromptwidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QShortcut>

AiPromptWidget::AiPromptWidget(QWidget *parent)
    : QWidget(parent)
{
    // 1. Конфигурация всплывающего окна
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedSize(540, 108); // Слегка увеличили высоту под индикатор

    // Тень окна в стиле Plasma
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor(0, 0, 0, 45));
    shadow->setOffset(0, 4);
    this->setGraphicsEffect(shadow);

    setStyleSheet(
        "AiPromptWidget {"
        "   background-color: #fcfcfc;"
        "   border: 1px solid #bcbebf;"
        "   border-radius: 8px;"
        "}"
        );

    // 2. Элементы ввода
    m_promptEdit = new QLineEdit(this);
    m_promptEdit->setPlaceholderText("Введите техническое задание для ИИ...");
    m_promptEdit->setClearButtonEnabled(true);
    m_promptEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: #ffffff;"
        "   color: #232629;"
        "   border: 1px solid #bcbebf;"
        "   border-radius: 4px;"
        "   padding: 6px 10px;"
        "   font-family: 'Segoe UI', Arial, sans-serif;"
        "   font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid #3daee9;"
        "}"
        "QLineEdit:disabled {"
        "   background-color: #f5f6f7;"
        "   color: #7f8c8d;"
        "}"
        );

    m_sendButton = new QPushButton("Отправить", this);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #3daee9;"
        "   color: #ffffff;"
        "   font-weight: 600;"
        "   font-size: 12px;"
        "   border: 1px solid #2980b9;"
        "   padding: 6px 14px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #40b3ee; }"
        "QPushButton:pressed { background-color: #2a9cd8; }"
        "QPushButton:disabled {"
        "   background-color: #d1d5db;"
        "   border-color: #c4c7cb;"
        "   color: #898f94;"
        "}"
        );

    m_cancelButton = new QPushButton("Отмена", this);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #eff0f1;"
        "   color: #232629;"
        "   font-weight: 500;"
        "   font-size: 12px;"
        "   border: 1px solid #bcbebf;"
        "   padding: 6px 12px;"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #e4e5e6; border-color: #3daee9; color: #1b1e20; }"
        "QPushButton:pressed { background-color: #d8dada; }"
        );

    // 3. Тонкая плавающая полоса активности (Indeterminate ProgressBar в стиле Breeze)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(3);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 0); // Режим бесконечной анимации
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "   background-color: #e2e8f0;"
        "   border: none;"
        "   border-radius: 1px;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #3daee9;"
        "   border-radius: 1px;"
        "}"
        );
    m_progressBar->hide(); // Скрыта по умолчанию

    // Метка статуса
    m_statusLabel = new QLabel("Ожидание ввода...", this);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "   color: #64748b;"
        "   font-size: 11px;"
        "   font-family: 'JetBrains Mono', monospace;"
        "   padding-left: 2px;"
        "}"
        );

    // 4. Компоновка макетов
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);
    inputLayout->addWidget(m_promptEdit, 1);
    inputLayout->addWidget(m_sendButton);
    inputLayout->addWidget(m_cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(6);
    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(m_progressBar); // Располагается прямо между вводом и текстом статуса
    mainLayout->addWidget(m_statusLabel);

    // 5. Таймер анимации точек
    m_dotsTimer = new QTimer(this);
    connect(m_dotsTimer, &QTimer::timeout, this, [this]() {
        m_dotsCount = (m_dotsCount + 1) % 4;
        QString dots = QString(".").repeated(m_dotsCount);
        m_statusLabel->setText(m_baseStatusText + dots);
    });

    // Замените submitAction, чтобы он вызывал startGenerationStream, а не старый busyIndicator:
    auto submitAction = [this]() {
        QString text = m_promptEdit->text().trimmed();
        if (!text.isEmpty()) {
            startGenerationStream();
            emit promptSubmitted(text);
        }
    };

    connect(m_promptEdit, &QLineEdit::returnPressed, this, submitAction);
    connect(m_sendButton, &QPushButton::clicked, this, submitAction);

    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        stopBusyIndicator();
        emit cancelRequested();
        this->close();
        this->deleteLater();
    });

    m_promptEdit->setFocus();

    QShortcut *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this]() {
        emit cancelRequested();
        this->close();
        this->deleteLater();
    });
}

void AiPromptWidget::startBusyIndicator(const QString &busyMessage)
{
    setInputsEnabled(false);
    m_baseStatusText = busyMessage;
    m_dotsCount = 0;

    m_statusLabel->setText(busyMessage);
    m_statusLabel->setStyleSheet("color: #0284c7; font-size: 11px; font-weight: bold; font-family: 'JetBrains Mono', monospace;");

    m_progressBar->show();
    m_dotsTimer->start(350); // Плавное мигание каждые 350 мс
}

void AiPromptWidget::stopBusyIndicator(const QString &finalMessage)
{
    if (m_dotsTimer && m_dotsTimer->isActive()) {
        m_dotsTimer->stop();
    }

    if (m_progressBar) {
        m_progressBar->hide();
    }

    setInputsEnabled(true);
    setStatusText(finalMessage);
}

void AiPromptWidget::setStatusText(const QString &text)
{
    if (!m_statusLabel) return;

    m_statusLabel->setText(text);
    if (text.contains("Ошибка") || text.contains("Сбой")) {
        m_statusLabel->setStyleSheet("color: #e74c3c; font-size: 11px; font-weight: bold; font-family: 'JetBrains Mono', monospace;");
    } else if (text.contains("Готово") || text.contains("успешно")) {
        m_statusLabel->setStyleSheet("color: #15803d; font-size: 11px; font-weight: bold; font-family: 'JetBrains Mono', monospace;");
    } else {
        m_statusLabel->setStyleSheet("color: #64748b; font-size: 11px; font-family: 'JetBrains Mono', monospace;");
    }
}

void AiPromptWidget::setInputsEnabled(bool enabled)
{
    if (m_promptEdit) m_promptEdit->setEnabled(enabled);
    if (m_sendButton) m_sendButton->setEnabled(enabled);
}

void AiPromptWidget::startGenerationStream()
{
    setInputsEnabled(false);
    m_tokenCounter = 0;
    m_streamTimer.restart();

    if (m_statusLabel) {
        m_statusLabel->setText("⚡ Соединение с Ollama...");
        m_statusLabel->setStyleSheet("color: #3daee9; font-size: 11px; font-weight: bold; font-family: 'JetBrains Mono', monospace;");
        m_statusLabel->repaint(); // Принудительная перерисовка
    }
}

void AiPromptWidget::updateTokenCount(int totalTokens, double tokensPerSec)
{
    m_tokenCounter = totalTokens;
    if (m_statusLabel) {
        QString text = QString("⚡ Генерация: %1 токенов (%2 т/с)")
                           .arg(m_tokenCounter)
                           .arg(tokensPerSec, 0, 'f', 1);

        m_statusLabel->setText(text);
        m_statusLabel->setStyleSheet("color: #0284c7; font-size: 11px; font-weight: bold; font-family: 'JetBrains Mono', monospace;");
        m_statusLabel->repaint(); // Немедленно обновляем текст на экране
    }
}

void AiPromptWidget::finishGenerationStream(int totalTokens)
{
    setInputsEnabled(true);
    if (m_statusLabel) {
        qint64 elapsedMs = m_streamTimer.elapsed();
        double totalSec = elapsedMs > 0 ? (elapsedMs / 1000.0) : 1.0;
        double avgSpeed = totalTokens / totalSec;

        m_statusLabel->setText(QString("✔ Готово: %1 токенов за %2 с (%3 т/с)")
                                   .arg(totalTokens)
                                   .arg(totalSec, 0, 'f', 1)
                                   .arg(avgSpeed, 0, 'f', 1));
        m_statusLabel->setStyleSheet("color: #15803d; font-size: 11px; font-weight: bold; font-family: 'JetBrains Mono', monospace;");
    }

    // Автоматически закрываем окно через 1.5 секунды после завершения
    QTimer::singleShot(1500, this, [this]() {
        this->close();
        this->deleteLater();
    });
}