#include "terminalwidget.h"
#include <QTextCursor>
#include <QRegularExpression>

TerminalWidget::TerminalWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    this->setReadOnly(true);
    // Задаем моноширинный шрифт и темный консольный фон
    QFont monoFont("Monospace", 10);
    monoFont.setStyleHint(QFont::TypeWriter);
    this->setFont(monoFont);

    this->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; border: none;");
    m_defaultFormat = this->currentCharFormat();
}

void TerminalWidget::appendTerminalData(const QByteArray &data)
{
    // Преобразуем сырые байты в Linux-текст UTF-8
    QString text = QString::fromUtf8(data);
    parseAnsiStream(text);
}

void TerminalWidget::parseAnsiStream(const QString &text)
{
    QTextCursor cursor = this->textCursor();
    QTextCharFormat currentFormat = m_defaultFormat;

    // Регулярное выражение для поиска ANSI escape-последовательностей (например, \033[31;1m)
    QRegularExpression ansiRegex("\x1B\\[([0-9;]*)[mK]");

    int lastPos = 0;
    QRegularExpressionMatchIterator it = ansiRegex.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int matchPos = match.capturedStart();

        // 1. Извлекаем чистый текст ДО управляющего кода
        QString plainChunk = text.mid(lastPos, matchPos - lastPos);
        if (!plainChunk.isEmpty()) {
            handleCarriageReturn(plainChunk);
        }

        // 2. Парсим сам ANSI код и меняем цвет карандаша
        QString codeStr = match.captured(1);
        if (codeStr.isEmpty() || codeStr == "0") {
            currentFormat = m_defaultFormat; // Сброс на дефолт
        } else {
            QStringList codes = codeStr.split(';');
            for (const QString &c : codes) {
                applyAnsiCode(c.toInt(), currentFormat);
            }
        }
        this->setCurrentCharFormat(currentFormat);

        lastPos = match.capturedEnd();
    }

    // Дописываем оставшийся хвост текста
    QString remaining = text.mid(lastPos);
    if (!remaining.isEmpty()) {
        handleCarriageReturn(remaining);
    }
}

void TerminalWidget::applyAnsiCode(int code, QTextCharFormat &format)
{
    switch (code) {
    case 1:  format.setFontWeight(QFont::Bold); break;
    case 4:  format.setFontUnderline(true); break;
    // Цвета текста (Foreground)
    case 31: format.setForeground(QColor("#ff5555")); break; // Красный (Ошибки)
    case 32: format.setForeground(QColor("#50fa7b")); break; // Зеленый (Успех)
    case 33: format.setForeground(QColor("#f1fa8c")); break; // Желтый (Предупреждения pip)
    case 34: format.setForeground(QColor("#bd93f9")); break; // Синий
    case 36: format.setForeground(QColor("#8be9fd")); break; // Бирюзовый
    // Цвета фона (Background), если понадобятся
    case 41: format.setBackground(QColor("#ff5555")); break;
    default: break;
    }
}

void TerminalWidget::handleCarriageReturn(const QString &text)
{
    QTextCursor cursor = this->textCursor();

    // Если внутри блока текста есть \r (возврат каретки без перевода строки)
    if (text.contains('\r')) {
        QStringList parts = text.split('\r');

        for (int i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                // Имитируем физический возврат каретки: выделяем текущую строку и стираем её
                cursor.movePosition(QTextCursor::EndOfBlock);
                cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            cursor.insertText(parts[i]);
        }
    } else {
        // Обычный вывод (включая символы \n)
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(text);
    }

    // Автоматический скролл терминала вниз вслед за выводом tqdm
    this->setTextCursor(cursor);
    this->ensureCursorVisible();
}

