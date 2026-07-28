#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

class AiPromptWidget : public QWidget {
    Q_OBJECT // МАКРОС ОБЯЗАТЕЛЕН: регистрирует класс в метаобъектной системе Qt

public:
    explicit AiPromptWidget(QWidget *parent = nullptr);
    void setStatusText(const QString &text);
    void setInputsEnabled(bool enabled);

signals:
    // Сигнал, который мы связываем на строке 3726
    void promptSubmitted(const QString &text);
    void cancelRequested();

protected:
    //void keyPressEvent(QKeyEvent *event) override;
    //void focusOutEvent(QFocusEvent *event) override;
    //void leaveEvent(QEvent *event) override;

public:
    QLineEdit *m_lineEdit;
    QLineEdit *m_promptEdit;
    QPushButton *m_sendButton;
    QPushButton *m_cancelButton;
    QLabel *m_statusLabel;
};
