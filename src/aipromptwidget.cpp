#include "aipromptwidget.h"

AiPromptWidget::AiPromptWidget(QWidget *parent)
    : QWidget(parent)
{
    // Настраиваем внешний вид всплывающего окна (компактный и аккуратный)
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("AiPromptWidget { background-color: #eff0f1; border: 1px solid #b0b0b0; border-radius: 6px; }");
    setFixedSize(520, 100); // Немного увеличили ширину с 450 до 520, чтобы три элемента красиво встали в ряд

    // Создаем элементы управления
    m_promptEdit = new QLineEdit(this);
    m_promptEdit->setPlaceholderText("Введите техническое задание для ИИ...");
    m_promptEdit->setStyleSheet("QLineEdit { border: 1px solid #b0b0b0; padding: 6px; background: #ffffff; border-radius: 4px; }");

    m_sendButton = new QPushButton("Отправить", this);
    m_sendButton->setStyleSheet("QPushButton { background-color: #3498db; color: white; font-weight: bold; border: none; padding: 6px 12px; border-radius: 4px; }"
                                "QPushButton:hover { background-color: #2980b9; }"
                                "QPushButton:disabled { background-color: #bdc3c7; }");

    // ФИЧА: Кнопка «Отмена» для мгновенного прерывания тяжелого инференса на CPU ноутбука
    m_cancelButton = new QPushButton("Отмена", this);
    m_cancelButton->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border: none; padding: 6px 12px; border-radius: 4px; }"
                                  "QPushButton:hover { background-color: #c0392b; }");

    // Метка для этапов работы модели
    m_statusLabel = new QLabel("🤖 Ожидание ввода...", this);
    m_statusLabel->setStyleSheet("color: #7f8c8d; font-size: 11px; font-weight: bold;");

    // Горизонтальный слой для поля ввода, кнопки отправки и кнопки отмены
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(m_promptEdit);
    inputLayout->addWidget(m_sendButton);
    inputLayout->addWidget(m_cancelButton); // Уложили третью кнопку в правый край ряда

    // Главный вертикальный макет окна
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 10);
    mainLayout->setSpacing(6);
    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(m_statusLabel); // Укладываем метку статуса в самый низ

    // Логика отправки данных по нажатию на Enter или кнопку "Отправить"
    auto submitAction = [this]() {
        QString text = m_promptEdit->text().trimmed();
        if (!text.isEmpty()) {
            emit promptSubmitted(text);
        }
    };

    connect(m_promptEdit, &QLineEdit::returnPressed, this, submitAction);
    connect(m_sendButton, &QPushButton::clicked, this, submitAction);

    // Логика кнопки "Отмена" — выстреливает сигналом в главное окно и сама уничтожает виджет из ОЗУ
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        emit cancelRequested(); // Этот сигнал перехватит neuro_programm.cpp и разорвет сокет uvicorn
        this->close();
        this->deleteLater();
    });

    m_promptEdit->setFocus();
}


// Изменение текста текущего этапа на лету
void AiPromptWidget::setStatusText(const QString &text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
        // Если ИИ начал думать, подсвечиваем строку фирменным синим цветом Breeze
        if (text.contains("🧠") || text.contains("✍️")) {
            m_statusLabel->setStyleSheet("color: #3498db; font-size: 11px; font-weight: bold;");
        }
    }
}

// Замораживание интерфейса, чтобы исключить спам-клики во время инференса
void AiPromptWidget::setInputsEnabled(bool enabled)
{
    if (m_promptEdit) m_promptEdit->setEnabled(enabled);
    if (m_sendButton) m_sendButton->setEnabled(enabled);
}
