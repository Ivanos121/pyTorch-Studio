#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QPlainTextEdit>
#include <QTextCharFormat>

class TerminalWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    // Главный метод: вызывайте его, когда QProcess присылает новые данные
    void appendTerminalData(const QByteArray &data);

private:
    void parseAnsiStream(const QString &text);
    void applyAnsiCode(int code, QTextCharFormat &format);
    void handleCarriageReturn(const QString &line);

    QTextCharFormat m_defaultFormat;
};

#endif // TERMINALWIDGET_H

