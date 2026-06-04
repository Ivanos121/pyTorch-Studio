// codeeditor.cpp
#include "codeeditor.h"
#include <QTextBlock>
#include "neuro_programm.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeyEvent>
#include <QTimer>
#include <iostream>
#include <QComboBox>
#include <QStackedWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor), codeEditor(editor)
{

}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    codeEditor->lineNumberAreaPaintEvent(event);
}

PythonHighlighter::PythonHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    // 1. Ключевые слова Python (Цвет Breeze Control Flow: Насыщенный синий/голубой)
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor("#1b6ac7"));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString keywordPatterns[] = {
        "\\bclass\\b", "\\bdef\\b", "\\bimport\\b", "\\bfrom\\b", "\\bas\\b",
        "\\bif\\b", "\\belse\\b", "\\belif\\b", "\\bfor\\b", "\\bwhile\\b",
        "\\bin\\b", "\\breturn\\b", "\\bpass\\b", "\\btry\\b", "\\bexcept\\b",
        "\\bwith\\b", "\\bassert\\b", "\\bbreak\\b", "\\bcontinue\\b",
        "\\blambda\\b", "\\bprint\\b"
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // 2. Встроенные константы (True, False, None) (Тёмно-голубой)
    QTextCharFormat constantFormat;
    constantFormat.setForeground(QColor("#0057ae"));
    constantFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\b(True|False|None)\\b");
    rule.format = constantFormat;
    highlightingRules.append(rule);

    // 3. Специфика PyTorch и ИИ (Выделяем классы торча, Модели, Тензоры) (Малиновый/Фиолетовый)
    QTextCharFormat pytorchFormat;
    pytorchFormat.setForeground(QColor("#a31515")); // Breeze Модели/Типы
    pytorchFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\b(torch|nn|optim|utils|data|Tensor|Module|Linear|Conv2d|ReLU|Sequential)\\b");
    rule.format = pytorchFormat;
    highlightingRules.append(rule);

    // 4. Функции и методы (Например, __init__, forward, .append) (Тёмно-зелёный/Морской)
    QTextCharFormat functionFormat;
    functionFormat.setForeground(QColor("#008080"));
    rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    // 5. Числа (Breeze Value: Терракотовый/Оранжевый)
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor("#b56c00"));
    rule.pattern = QRegularExpression("\\b\\d+(\\.\\d+)?\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    // 6. Однострочные строки в кавычках (Зелёный чистый)
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor("#047a15"));
    rule.pattern = QRegularExpression("\".*?\"|'.*?'");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // 7. Однострочные комментарии # (Светло-серый/Курсив)
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor("#898f94"));
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("#.*");
    rule.format = commentFormat;
    highlightingRules.append(rule);

    // 8. Многострочные Docstrings-комментарии """ ... """ (Серый)
    multiLineCommentFormat.setForeground(QColor("#898f94"));
    multiLineCommentFormat.setFontItalic(true);
    tripleSingleQuote = QRegularExpression("'''");
    tripleDoubleQuote = QRegularExpression("\"\"\"");
}

void PythonHighlighter::highlightBlock(const QString &text)
{
    // Применяем стандартные правила (ключевые слова, строки, торч)
    for (const HighlightingRule &rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Обработка многострочных блоков комментариев Python (""" docstring """)
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1) {
        QRegularExpressionMatch match = tripleDoubleQuote.match(text);
        startIndex = match.capturedStart();
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch match = tripleDoubleQuote.match(text, startIndex + 3);
        int endIndex = match.capturedStart();
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + 3;
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = tripleDoubleQuote.match(text, startIndex + commentLength).capturedStart();
    }
}

// =============================================================================
// РЕАЛИЗАЦИЯ ОСНОВНОГО КЛАССА РЕДАКТОРА CODEEDITOR
// =============================================================================
CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    // Инициализируем флаг предохранителя в исходное ложное состояние
    this->isLspFreeze = false;

    // Создаем изолированный указатель на текущий экземпляр класса
    CodeEditor *currentEditor = this;

    connect(this, &CodeEditor::textChanged, this, [currentEditor]() {
        // =====================================================================
        // ПРЕДОХРАНИТЕЛЬ: Если взведен флаг отправки точки — уведомление didChange
        // временно блокируется, предотвращая аннулирование ответа сервером Jedi!
        // =====================================================================
        if (currentEditor->isLspFreeze) return;

        // ПУЛЕНЕПРОБИВАЕМЫЙ ВЫЗОВ: Используем глобальный синглтон вместо window()
        Neuro_programm *mainWin = Neuro_programm::self;
        if (!mainWin) return;

        QJsonObject params;
        QJsonObject textDocument;
        textDocument["uri"] = QUrl::fromLocalFile(mainWin->getCurrentOpenFilePath()).toString();
        textDocument["version"] = 1;
        params["textDocument"] = textDocument;

        QJsonArray contentChanges;
        QJsonObject change;
        change["text"] = currentEditor->toPlainText();
        contentChanges.append(change);
        params["contentChanges"] = contentChanges;

        // Отправляем пакет изменений. Наш модифицированный метод sendLspRequest
        // сам отсечет поле "id" для этого уведомления.
        mainWin->sendLspRequest("textDocument/didChange", params);
    });

    // Создаем линейку строк, передавая указатель на текущий редактор
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    m_foldingArea = new FoldingArea(this);

    // Пересчитываем отступы при изменении текста
    connect(this, &CodeEditor::textChanged, this, &CodeEditor::updateFoldingData);

    // При прокрутке обновляем область отрисовки полосы
    connect(this, &CodeEditor::updateRequest, this, [this](const QRect &rect, int dy) {
        if (dy) m_foldingArea->scroll(0, dy);
        else m_foldingArea->update(0, rect.y(), m_foldingArea->width(), rect.height());
    });

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);

    // Принудительно вызываем один раз при старте, чтобы установить отступы для открытого файла
    updateLineNumberAreaWidth(0);
    new PythonHighlighter(this->document());
}


int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        digits++;
    }
    int space = 15 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth() + foldingAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy) lineNumberArea->scroll(0, dy);
    else lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    // 1. Даем базовому классу Qt пересчитать размеры текстового окна
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    int lineNumWidth = lineNumberAreaWidth(); // Ваша существующая функция ширины номеров
    int foldWidth = foldingAreaWidth();       // Наша функция полосы сворачивания (16px)

    // 2. Позиционируем номера строк у самого левого края
    if (lineNumberArea) {
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumWidth, cr.height()));
    }

    // 3. Позиционируем полосу сворачивания строго СЛЕДОМ за номерами строк
    if (m_foldingArea) {
        m_foldingArea->setGeometry(QRect(cr.left() + lineNumWidth, cr.top(), foldWidth, cr.height()));
    }

    // ФИКС: Строка setViewportMargins ОТСЮДА ПОЛНОСТЬЮ УДАЛЕНА!
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event) {
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor("#f0f1f2")); // Серый фон Breeze

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            if (textCursor().blockNumber() == blockNumber) {
                painter.setPen(QColor("#232629"));
                QFont boldFont = painter.font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
            } else {
                painter.setPen(QColor("#7f8c8d"));
                QFont normFont = painter.font();
                normFont.setBold(false);
                painter.setFont(normFont);
            }

            painter.drawText(0, top, lineNumberArea->width() - 5, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        blockNumber++;
    }
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor("#e4f2fc")); // Синяя подсветка Breeze
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

#include <QKeyEvent>
#include <QApplication>
#include <QScrollBar>
#include <QVBoxLayout>

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    // =========================================================================
    // СЛУЧАЙ 1: ОКНО ПОДСКАЗОК УЖЕ ОТКРЫТО — ПЕРЕХВАТЫВАЕМ КЛАВИШИ УПРАВЛЕНИЯ
    // =========================================================================
    if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget)
    {
        // 1. Управление стрелками Up / Down внутри списка
        if (e->key() == Qt::Key_Up) {
            int currentRow = m_listWidget->currentRow();
            for (int i = currentRow - 1; i >= 0; --i) {
                if (!m_listWidget->item(i)->isHidden()) {
                    m_listWidget->setCurrentRow(i);
                    break;
                }
            }
            return; // Запрещаем курсору в текстовом поле прыгать вверх
        }

        if (e->key() == Qt::Key_Down) {
            int currentRow = m_listWidget->currentRow();
            for (int i = currentRow + 1; i < m_listWidget->count(); ++i) {
                if (!m_listWidget->item(i)->isHidden()) {
                    m_listWidget->setCurrentRow(i);
                    break;
                }
            }
            return; // Запрещаем курсору в текстовом поле прыгать вниз
        }

        // 2. Закрытие окна по Esc
        if (e->key() == Qt::Key_Escape) {
            m_popupWindow->close();
            m_popupWindow = nullptr;
            return;
        }

        // 3. Вставка выбранного слова по нажатию Enter / Return / Tab
        if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return || e->key() == Qt::Key_Tab)
        {
            QListWidgetItem *currentItem = m_listWidget->currentItem();
            if (currentItem && !currentItem->isHidden()) {
                QString itemText = currentItem->text();
                QTextCursor tc = this->textCursor();

                // Вычисляем длину уже набранного префикса после точки и стираем его перед вставкой
                int prefixLength = this->textCursor().position() - m_startPosition;
                tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefixLength);

                tc.beginEditBlock();
                tc.insertText(itemText);
                tc.endEditBlock();
                this->setTextCursor(tc);
            }
            m_popupWindow->close();
            m_popupWindow = nullptr;
            return;
        }

        // 4. ДИНАМИЧЕСКАЯ ФИЛЬТРАЦИЯ НА ЛЕТУ ПРИ ВВОДЕ СИМВОЛОВ
        if (!e->text().isEmpty() && e->key() != Qt::Key_Backspace)
        {
            // Сначала отдаем букву стандартному QPlainTextEdit, чтобы она отобразилась в коде
            QPlainTextEdit::keyPressEvent(e);

            // Вычисляем текущий набранный префикс (например, "l" или "lin")
            int prefixLength = this->textCursor().position() - m_startPosition;
            QTextCursor tc = this->textCursor();
            tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefixLength);
            QString currentPrefix = tc.selectedText().toLower();

            int firstVisibleRow = -1;
            // Скрываем или показываем элементы списка на основе введенного текста
            for (int i = 0; i < m_listWidget->count(); ++i) {
                QListWidgetItem *item = m_listWidget->item(i);
                bool matches = item->text().toLower().contains(currentPrefix);
                item->setHidden(!matches);

                if (matches && firstVisibleRow == -1) {
                    firstVisibleRow = i;
                }
            }

            // Автоматически переводим фокус на первую совпавшую подсказку
            if (firstVisibleRow != -1) {
                m_listWidget->setCurrentRow(firstVisibleRow);
            } else {
                m_popupWindow->close();
                m_popupWindow = nullptr;
            }
            return;
        }
    }

    // =========================================================================
    // СЛУЧАЙ 2: ОКНО ЗАКРЫТО — ОТДАЕМ КЛАВИШИ СИСТЕМЕ И ЛОВИМ ВВОД СИМВОЛА ТОЧКИ
    // =========================================================================
    QPlainTextEdit::keyPressEvent(e);

    if (e->text() == ".")
    {
        Neuro_programm *mainWin = Neuro_programm::self;
        if (!mainWin) return;

        QString filePath = mainWin->getCurrentOpenFilePath();
        if (filePath.isEmpty()) return;

        // Фиксируем стартовую позицию сразу ПОСЛЕ точки
        m_startPosition = this->textCursor().position();

        // 1. Синхронизируем текст через didChange
        mainWin->globalLspDocVersion++;
        QJsonObject changeParams;
        QJsonObject textDocumentObj;
        textDocumentObj["uri"] = QUrl::fromLocalFile(filePath).toString();
        textDocumentObj["version"] = mainWin->globalLspDocVersion;
        changeParams["textDocument"] = textDocumentObj;

        QString currentFullText = this->toPlainText();
        currentFullText.remove('\r');
        if (!currentFullText.endsWith('\n')) currentFullText += "\n";

        QJsonArray contentChanges;
        QJsonObject changeNode;
        changeNode["text"] = currentFullText;
        contentChanges.append(changeNode);
        changeParams["contentChanges"] = contentChanges;

        mainWin->sendLspRequest("textDocument/didChange", changeParams);

        // 2. Запрашиваем подсказки у Jedi (textDocument/completion)
        QTextCursor cursor = this->textCursor();
        QJsonObject compParams;
        QJsonObject docIdentifier;
        docIdentifier["uri"] = QUrl::fromLocalFile(filePath).toString();
        compParams["textDocument"] = docIdentifier;

        QJsonObject positionNode;
        positionNode["line"] = cursor.blockNumber();
        positionNode["character"] = cursor.columnNumber();
        compParams["position"] = positionNode;

        mainWin->sendLspRequest("textDocument/completion", compParams);
    }
}

QString CodeEditor::textUnderCursor() const
{
    QTextCursor tc = textCursor();
    // Выделяем слово, на котором сейчас сфокусирован пользователь
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText(); // Возвращаем чистый текст (например: "nn" или "Linear")
}

// codeeditor.cpp (В самый конец файла)

// codeeditor.cpp (Замените метод в самом конце файла)

void CodeEditor::setCompleter(QCompleter *completer)
{
    if (c) {
        c->disconnect(this);
    }

    c = completer;
    if (!c) return;

    // Настраиваем текстовый редактор как целевую платформу для комплитера
    c->setWidget(this);
    c->setCompletionMode(QCompleter::PopupCompletion);
    c->setCaseSensitivity(Qt::CaseInsensitive);

    // =========================================================================
    // КРИТИЧЕСКИЙ ГРАФИЧЕСКИЙ ФИКС ДЛЯ LINUX (KDE PLASMA / GNOME)
    // Насильно заставляем выпадающее окно подсказок быть независимым ToolTip-окном,
    // которое операционная система гарантированно отрисует ПОВЕРХ белого текстового поля!
    // =========================================================================
    if (c->popup()) {
        c->popup()->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        c->popup()->setFocusPolicy(Qt::NoFocus); // Чтобы фокус клавиатуры оставался в train.py

        // Накатываем красивый Breeze-стиль с тенью и рамками
        c->popup()->setStyleSheet(
            "QAbstractItemView {"
            "   background-color: #ffffff !important;"
            "   color: #232629 !important;"
            "   border: 1px solid #bcbebf !important;"
            "   selection-background-color: #2980b9 !important;"
            "   selection-color: #ffffff !important;"
            "   font-family: 'Monospace' !important;"
            "   font-size: 12px !important;"
            "}"
            );
    }
}

// 1. ПЕРЕРАСЧЕТ ОТСТУПОВ PYTHON НА ЛЕТУ
void CodeEditor::updateFoldingData()
{
    QTextBlock block = document()->begin();
    while (block.isValid())
    {
        QString text = block.text();
        // Избегаем конфликта имён: переменная называется foldData
        FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());
        if (!foldData) {
            foldData = new FolderBlockData();
            block.setUserData(foldData);
        }

        // Считаем уровень отступа (количество пробелов)
        int spaces = 0;
        for (int i = 0; i < text.length(); ++i) {
            if (text[i] == ' ') spaces++;
            else if (text[i] == '\t') spaces += 4;
            else break;
        }
        foldData->indentLevel = spaces;

        // Помечаем ключевые слова Python как начало сворачивания
        QString trimmed = text.trimmed();
        if (trimmed.startsWith("def ") || trimmed.startsWith("class ")) {
            foldData->isFoldStart = true;
        } else {
            foldData->isFoldStart = false;
        }

        block = block.next();
    }
}

// 2. ОТРИСОВКА МАРКЕРОВ [+] / [-]
void CodeEditor::foldingAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_foldingArea);
    painter.fillRect(event->rect(), QColor("#f0f0f0")); // Фон Breeze Light

    QTextBlock block = firstVisibleBlock();
    int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int blockBottom = blockTop + (int) blockBoundingRect(block).height();

    while (block.isValid() && blockTop <= event->rect().bottom())
    {
        if (block.isVisible() && blockBottom >= event->rect().top())
        {
            FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());
            if (foldData && foldData->isFoldStart)
            {
                painter.setPen(QColor("#232629")); // Цвет Breeze Dark текста
                QRect iconRect(2, blockTop + 4, 12, 12);

                painter.drawRect(iconRect);
                if (foldData->isFolded) {
                    painter.drawText(iconRect, Qt::AlignCenter, "+");
                } else {
                    painter.drawText(iconRect, Qt::AlignCenter, "-");
                }
            }
        }
        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + (int) blockBoundingRect(block).height();
    }
}

// // 3. ОБРАБОТКА КЛИКА МЫШИ (СВОРАЧИВАНИЕ И РАЗВОРOТ КОДА)
// void CodeEditor::foldingAreaMousePressEvent(QMouseEvent *event)
// {
//     if (event->button() != Qt::LeftButton) return;

//     QTextBlock targetBlock;
//     QTextBlock block = firstVisibleBlock();
//     int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
//     int blockBottom = blockTop + (int) blockBoundingRect(block).height();

//     while (block.isValid()) {
//         if (event->y() >= blockTop && event->y() < blockBottom) {
//             targetBlock = block;
//             break;
//         }
//         block = block.next();
//         blockTop = blockBottom;
//         blockBottom = blockTop + (int) blockBoundingRect(block).height();
//     }

//     if (!targetBlock.isValid()) return;
//     FolderBlockData *startData = static_cast<FolderBlockData*>(targetBlock.userData());
//     if (!startData || !startData->isFoldStart) return;

//     // Переключаем флаг
//     startData->isFolded = !startData->isFolded;

//     QTextBlock nextBlock = targetBlock.next();
//     int startIndent = startData->indentLevel;

//     document()->markContentsDirty(targetBlock.position(), document()->characterCount() - targetBlock.position());

//     while (nextBlock.isValid())
//     {
//         FolderBlockData *nextData = static_cast<FolderBlockData*>(nextBlock.userData());
//         if (!nextData) break;

//         // Конец блока: встретили строку с меньшим или равным отступом
//         if (!nextBlock.text().trimmed().isEmpty() && nextData->indentLevel <= startIndent) {
//             break;
//         }

//         // Сворачиваем или разворачиваем строки
//         if (startData->isFolded) {
//             nextBlock.setVisible(false);
//         } else {
//             nextBlock.setVisible(true);
//         }

//         nextBlock = nextBlock.next();
//     }

//     update();
//     m_foldingArea->update();
// }


// void CodeEditor::foldingAreaPaintEvent(QPaintEvent *event)
// {
//     QPainter painter(m_foldingArea);
//     painter.fillRect(event->rect(), QColor("#f0f0f0")); // Светлый фон в стиле Breeze

//     QTextBlock block = firstVisibleBlock();
//     int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
//     int blockBottom = blockTop + (int) blockBoundingRect(block).height();

//     while (block.isValid() && blockTop <= event->rect().bottom())
//     {
//         if (block.isVisible() && blockBottom >= event->rect().top())
//         {
//             FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());
//             if (foldData && foldData->isFoldStart)
//             {
//                 painter.setPen(QColor("#232629")); // Цвет Breeze
//                 QRect iconRect(2, blockTop + 4, 12, 12);

//                 // Рисуем плюсик или минус
//                 painter.drawRect(iconRect);
//                 if (foldData->isFolded) {
//                     painter.drawText(iconRect, Qt::AlignCenter, "+");
//                 } else {
//                     painter.drawText(iconRect, Qt::AlignCenter, "-");
//                 }
//             }
//         }
//         block = block.next();
//         blockTop = blockBottom;
//         blockBottom = blockTop + (int) blockBoundingRect(block).height();
//     }
// }

void CodeEditor::foldingAreaMousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    QTextBlock targetBlock;
    QTextBlock block = firstVisibleBlock();
    int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int blockBottom = blockTop + (int) blockBoundingRect(block).height();

    // Находим, по какому именно блоку текста кликнул пользователь
    while (block.isValid()) {
        if (event->y() >= blockTop && event->y() < blockBottom) {
            targetBlock = block;
            break;
        }
        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + (int) blockBoundingRect(block).height();
    }

    if (!targetBlock.isValid()) return;
    FolderBlockData *startData = static_cast<FolderBlockData*>(targetBlock.userData());
    if (!startData || !startData->isFoldStart) return;

    // Меняем флаг свертывания на противоположный
    startData->isFolded = !startData->isFolded;

    // Запускаем процесс скрытия/показа вложенных строк
    QTextBlock nextBlock = targetBlock.next();
    int startIndent = startData->indentLevel;

    document()->markContentsDirty(targetBlock.position(), document()->characterCount() - targetBlock.position());

    while (nextBlock.isValid())
    {
        FolderBlockData *nextData = static_cast<FolderBlockData*>(nextBlock.userData());
        if (!nextData) break;

        // Если встретили строку с таким же или меньшим отступом (конец функции) — останавливаемся
        if (nextBlock.text().trimmed().isEmpty() == false && nextData->indentLevel <= startIndent) {
            break;
        }

        // Прячем или показываем блок текста
        if (startData->isFolded) {
            nextBlock.setVisible(false); // Сворачиваем код
        } else {
            nextBlock.setVisible(true);  // Разворачиваем код
        }

        nextBlock = nextBlock.next();
    }

    // Принудительно заставляем Qt пересчитать геометрию документа и обновить скроллбары редактора
    update();
    m_foldingArea->update();
}

void CodeEditor::paintEvent(QPaintEvent *e)
{
    // 1. Сначала вызываем стандартную отрисовку Qt, чтобы нарисовался весь видимый код
    QPlainTextEdit::paintEvent(e);

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // 2. Пробегаемся по всем видимым на экране блокам текста
    QTextBlock block = firstVisibleBlock();
    int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int blockBottom = blockTop + (int) blockBoundingRect(block).height();

    while (block.isValid() && blockTop <= e->rect().bottom())
    {
        if (block.isVisible() && blockBottom >= e->rect().top())
        {
            FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());

            // Если этот блок — начало функции, и пользователь ЕГО СВЕРНУЛ:
            if (foldData && foldData->isFoldStart && foldData->isFolded)
            {
                // Вычисляем, где заканчивается текст строки (например, после скобки или двоеточия)
                QTextLayout *layout = block.layout();
                if (layout && layout->lineCount() > 0)
                {
                    QTextLine line = layout->lineAt(0);
                    // Узнаем физическую ширину текста строки в пикселях
                    int textWidth = (int)line.naturalTextWidth();

                    // Рассчитываем координаты для плашки троеточия
                    // Смещаем её вправо на textWidth + небольшой отступ (15px)
                    int badgeX = contentOffset().x() + textWidth + 15;
                    int badgeY = blockTop + 3;
                    int badgeWidth = 35; // <--- УВЕЛИЧИЛИ ШИРИНУ ПОД СКОБКИ
                    int badgeHeight = (int)blockBoundingRect(block).height() - 6;

                    QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

                    // Рисуем плашку в стиле KDE Breeze Light
                    painter.setPen(QColor("#c7c7c7"));
                    painter.setBrush(QColor("#eff0f1"));
                    painter.drawRoundedRect(badgeRect, 3, 3);

                    painter.setPen(QColor("#232629"));
                    QFont font = painter.font();
                    font.setBold(true);
                    painter.setFont(font);

                    // === МЕНЯЕМ ТЕКСТ НА ФИГУРНЫЕ СКОБКИ ===
                    painter.drawText(badgeRect, Qt::AlignCenter, "{...}");

                }
            }
        }

        // Переходим к следующему блоку текста
        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + (int) blockBoundingRect(block).height();
    }
}

void CodeEditor::mouseDoubleClickEvent(QMouseEvent *e)
{
    // Проверяем клик только левой кнопкой мыши
    if (e->button() != Qt::LeftButton) {
        QPlainTextEdit::mouseDoubleClickEvent(e);
        return;
    }

    // 1. Пробегаемся по блокам, чтобы найти, на какую строку пришелся двойной клик
    QTextBlock block = firstVisibleBlock();
    int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int blockBottom = blockTop + (int) blockBoundingRect(block).height();

    while (block.isValid())
    {
        // Если координата Y мыши попала в границы текущей строки
        if (e->y() >= blockTop && e->y() < blockBottom)
        {
            FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());

            // Нас интересуют только СВЕРНУТЫЕ в данный момент функции
            if (foldData && foldData->isFoldStart && foldData->isFolded)
            {
                QTextLayout *layout = block.layout();
                if (layout && layout->lineCount() > 0)
                {
                    QTextLine line = layout->lineAt(0);
                    int textWidth = (int)line.naturalTextWidth();

                    // Воспроизводим ТУ ЖЕ самую геометрию плашки, что и в paintEvent
                    int badgeX = contentOffset().x() + textWidth + 15;
                    int badgeY = blockTop + 3;
                    int badgeWidth = 35;
                    int badgeHeight = (int)blockBoundingRect(block).height() - 6;

                    QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

                    // === ПРОВЕРКА ПОПАДАНИЯ МЫШИ В ПЛАШКУ {...} ===
                    if (badgeRect.contains(e->pos()))
                    {
                        // 1. Разворачиваем флаг структуры
                        foldData->isFolded = false;

                        // 2. Пробегаемся по вложенным строкам и возвращаем им видимость
                        QTextBlock nextBlock = block.next();
                        int startIndent = foldData->indentLevel;

                        // Помечаем документ как измененный для перерасчета скроллбара
                        document()->markContentsDirty(block.position(), document()->characterCount() - block.position());

                        while (nextBlock.isValid())
                        {
                            FolderBlockData *nextData = static_cast<FolderBlockData*>(nextBlock.userData());
                            if (!nextData) break;

                            // Конец функции (вернулись на прежний уровень отступа)
                            if (!nextBlock.text().trimmed().isEmpty() && nextData->indentLevel <= startIndent) {
                                break;
                            }

                            nextBlock.setVisible(true); // Разворачиваем строку кода обратно!
                            nextBlock = nextBlock.next();
                        }

                        // 3. Принудительно обновляем интерфейс редактора и боковой панели
                        update();
                        if (m_foldingArea) m_foldingArea->update();

                        return; // Событие полностью обработано, выходим, не передавая клик в Qt
                    }
                }
            }
            break; // Строку нашли, дальше цикл крутить нет смысла
        }

        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + (int) blockBoundingRect(block).height();
    }

    // Если двойной клик произошел просто по тексту (мимо плашки),
    // отдаем его стандартному Qt-обработчику (например, для выделения слова целиком)
    QPlainTextEdit::mouseDoubleClickEvent(e);
}
