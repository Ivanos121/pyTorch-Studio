// codeeditor.cpp
#include "codeeditor.h"
#include <QJsonDocument>
#include <QTextBlock>
#include "neuro_programm.h"
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
#include <QSettings>

QList<CodeEditor::LspErrorData> CodeEditor::currentLspErrors;

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
    // 1. Ключевые слова Python (Связываем регулярку напрямую с адресом формата класса)
    const QString keywordPatterns[] = {
        "\\bclass\\b", "\\bdef\\b", "\\bimport\\b", "\\bfrom\\b", "\\bas\\b",
        "\\bif\\b", "\\belse\\b", "\\belif\\b", "\\bfor\\b", "\\bwhile\\b",
        "\\bin\\b", "\\breturn\\b", "\\bpass\\b", "\\btry\\b", "\\bexcept\\b",
        "\\bwith\\b", "\\bassert\\b", "\\bbreak\\b", "\\bcontinue\\b",
        "\\blambda\\b", "\\bprint\\b"
    };
    for (const QString &pattern : keywordPatterns) {
        highlightingRulesMap.insert(QRegularExpression(pattern), &keywordFormat);
    }

    // 2. Встроенные константы (True, False, None) (Запись напрямую в QMap)
    highlightingRulesMap.insert(QRegularExpression("\\b(True|False|None)\\b"), &constantFormat);

    // 3. Специфика PyTorch и ИИ (Выделяем классы торча, Модели, Тензоры)
    highlightingRulesMap.insert(QRegularExpression("\\b(torch|nn|optim|utils|data|Tensor|Module|Linear|Conv2d|ReLU|Sequential)\\b"), &pytorchFormat);

    // 4. Функции и методы (Например, __init__, forward, .append)
    highlightingRulesMap.insert(QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()"), &functionFormat);

    // 5. Числа (Breeze Value: Терракотовый/Оранжевый)
    highlightingRulesMap.insert(QRegularExpression("\\b\\d+(\\.\\d+)?\\b"), &numberFormat);

    // 6. Однострочные строки в кавычках (Зелёный чистый)
    highlightingRulesMap.insert(QRegularExpression("\".*?\"|'.*?'"), &stringFormat);

    // 7. Однострочные комментарии #
    highlightingRulesMap.insert(QRegularExpression("#.*"), &commentFormat);

    // 8. Многострочные Docstrings-комментарии """ ... """ (Инициализируем регулярки)
    tripleSingleQuote = QRegularExpression("'''");
    tripleDoubleQuote = QRegularExpression("\"\"\"");

    // ЗАПУСКАЕМ ДИНАМИЧЕСКУЮ ЗАГРУЗКУ ЦВЕТОВ ИЗ РЕЕСТРА QSETTINGS
    loadThemeSettings();
}

#include "codeeditor.h" // ОБЯЗАТЕЛЬНО проверьте наличие этого инклуда в самом верху cpp-файла!

void PythonHighlighter::highlightBlock(const QString &text)
{
    // 1. ВАШ РОДНОЙ ЦИКЛ ОБХОДА МАРКЕРОВ (Ключевые слова, def, class...)
    auto i = highlightingRulesMap.constBegin();
    while (i != highlightingRulesMap.constEnd()) {
        QRegularExpressionMatchIterator matchIterator = i.key().globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), *(i.value()));
        }
        ++i;
    }

    // 2. ВАШ РОДНОЙ ЦИКЛ ОБРАБОТКИ МНОГОСТРОЧНЫХ DOCSTRINGS И КАВЫЧЕК
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

    // =========================================================================
    // 3. НАШ МОДЕРНИЗИРОВАННЫЙ СЛОЙ ОШИБОК LSP (ТЕПЕРЬ СТРОГО В САМОМ ФИНАЛЕ!)
    // =========================================================================
    if (text.trimmed().isEmpty()) return; // Защита от пустых строк

    int currentLineNumber = currentBlock().blockNumber();

    for (const auto &error : std::as_const(Neuro_programm::globalLspErrors)) {
        if (error.line == currentLineNumber) {
            // Выделяем всю строку, где зафиксирован SyntaxError внутри функции
            int startChar = 0;
            int length = text.length();

            QTextCharFormat errorFormat;
            errorFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline); // Волнистая линия
            errorFormat.setUnderlineColor(error.isError ? Qt::red : QColor(255, 165, 0)); // Красный / Оранжевый

            // Накладываем графику в самый последний момент.
            // Теперь Qt нарисует линию ПОВЕРХ любого кода, Docstring или комментария внутри функции!
            setFormat(startChar, length, errorFormat);
        }
    }
} // Самый конец функции highlightBlock


void PythonHighlighter::loadThemeSettings()
{
    QSettings settings("PyTorchStudio", "EditorSettings");

    // Вытягиваем цвета из реестра. Если они отсутствуют, включаются ваши родные дефолтные цвета Breeze
    QColor keyCol(settings.value("Theme/KeywordColor", "#1b6ac7").toString());
    QColor constCol(settings.value("Theme/ConstantColor", "#0057ae").toString());
    QColor torchCol(settings.value("Theme/PytorchColor", "#a31515").toString());
    QColor funcCol(settings.value("Theme/FunctionColor", "#008080").toString());
    QColor numCol(settings.value("Theme/NumberColor", "#b56c00").toString());
    QColor strCol(settings.value("Theme/StringColor", "#047a15").toString());
    QColor commCol(settings.value("Theme/CommentColor", "#898f94").toString());

    // Записываем новые цвета в QTextCharFormat
    keywordFormat.setForeground(keyCol);
    keywordFormat.setFontWeight(QFont::Bold);

    constantFormat.setForeground(constCol);
    constantFormat.setFontWeight(QFont::Bold);

    pytorchFormat.setForeground(torchCol);
    pytorchFormat.setFontWeight(QFont::Bold);

    functionFormat.setForeground(funcCol);
    numberFormat.setForeground(numCol);
    stringFormat.setForeground(strCol);

    commentFormat.setForeground(commCol);
    commentFormat.setFontItalic(true);

    multiLineCommentFormat.setForeground(commCol);
    multiLineCommentFormat.setFontItalic(true);

    // Команда Qt принудительно перерисовать документ новыми цветами темы на экране
    rehighlight();
}

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    // Инициализируем флаги и базовые переменные
    this->isLspFreeze = false; //
    this->lspDocumentVersion = 1; // Стартовая версия документа для сессии

    // --- 1. ЗАПУСК И АППАРАТНОЕ РУКОПОЖАТЬЕ LSP-СЕРВЕРА ---
    lspProcess = new QProcess(this); //
    QStringList arguments;
    arguments << "-m" << "pylsp"; // Запуск python-lsp-server / jedi

    lspProcess->start("python3", arguments); //

    if (lspProcess->waitForStarted(2000)) { //
        qDebug() << "[LSP SUCCESS] Процесс запущен. PID:" << lspProcess->processId();

        // А. Отправляем обязательный пакет INITIALIZE (Запрос сессии)
        QJsonObject rootInit;
        rootInit["jsonrpc"] = "2.0";
        rootInit["id"] = 1;
        rootInit["method"] = "initialize";

        QJsonObject paramsInit;
        paramsInit["processId"] = QCoreApplication::applicationPid();
        paramsInit["rootUri"] = "file:///";

        // Объявляем серверу, что наш редактор поддерживает полную синхронизацию текста (Full Sync = 1)
        QJsonObject capabilities;
        QJsonObject textDocument;
        textDocument["synchronization"] = QJsonObject{
            {"dynamicRegistration", false},
            {"willSave", false},
            {"willSaveWaitUntil", false},
            {"didSave", true}
        };
        capabilities["textDocument"] = textDocument;
        paramsInit["capabilities"] = capabilities;
        rootInit["params"] = paramsInit;

        QJsonDocument docInit(rootInit);
        QByteArray rawJsonInit = docInit.toJson(QJsonDocument::Compact);
        QByteArray packetInit;
        packetInit.append(QString("Content-Length: %1\r\n\r\n").arg(rawJsonInit.length()).toUtf8());
        packetInit.append(rawJsonInit);

        lspProcess->write(packetInit);
        lspProcess->waitForBytesWritten(100);

        // Б. Ожидаем ответ и отправляем подтверждение INITIALIZED
        if (lspProcess->waitForReadyRead(2000)) {
            //QByteArray response = lspProcess->readAllStandardOutput(); // Очищаем буфер ответа приветствия
            lspProcess->readAllStandardOutput();
            QJsonObject rootInitialized;
            rootInitialized["jsonrpc"] = "2.0";
            rootInitialized["method"] = "initialized";
            rootInitialized["params"] = QJsonObject();

            QJsonDocument docInitialized(rootInitialized);
            QByteArray rawJsonInitialized = docInitialized.toJson(QJsonDocument::Compact);
            QByteArray packetInitialized;
            packetInitialized.append(QString("Content-Length: %1\r\n\r\n").arg(rawJsonInitialized.length()).toUtf8());
            packetInitialized.append(rawJsonInitialized);

            lspProcess->write(packetInitialized);
            lspProcess->waitForBytesWritten(100);
            qDebug() << "[LSP SUCCESS] Полный цикл рукопожатия (Initialize -> Initialized) завершен.";
        }
    } else {
        qDebug() << "[LSP ERROR] Не удалось запустить Python LSP сервер!"; //
    }

    // --- 2. НАСТРОЙКА СИСТЕМЫ ТАЙМЕРОВ И СИНХРОНИЗАЦИИ ---
    // --- Внутри конструктора CodeEditor в файле codeeditor.cpp ---

    // Наш таймер дебаунса (остается без изменений)
    lspDelayTimer = new QTimer(this);
    lspDelayTimer->setSingleShot(true);
    connect(lspDelayTimer, &QTimer::timeout, this, [this]() {
        this->sendLspDidChange();
    });

    // КРИТИЧЕСКИЙ АРХИТЕКТУРНЫЙ ФИКС ДЛЯ QT:
    // Вместо капризного textChanged самого виджета, подключаемся НАПРЯМУЮ к документу!
    // Сигнал contentsChanged() генерируется ядром Qt всегда, пробивая любые блокировки хайлайтеров!
    if (this->document()) {
        connect(this->document(), &QTextDocument::contentsChanged, this, [this]() {
            if (lspProcess && lspProcess->state() == QProcess::Running) {
                // Запускаем или перезапускаем отсчет 300 миллисекунд
                lspDelayTimer->start(300);
            }
        });
    }

    // УДАЛЕН старый дублирующий коннект connect(this, &CodeEditor::textChanged...),
    // который отправлял ложные пакеты "version" = 1 и сбрасывал кэш Jedi!

    // --- 3. ИНИЦИАЛИЗАЦИЯ ПАНЕЛЕЙ И РОДНЫХ СИГНАЛОВ QT ТЕКСТОВОГО ПОЛЯ ---
    lineNumberArea = new LineNumberArea(this); //
    m_foldingArea = new FoldingArea(this); //

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth); //
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea); //
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine); //

    // Потокобезопасный асинхронный коннект чтения готовой диагностики от сервера
    connect(lspProcess, &QProcess::readyReadStandardOutput, this, &CodeEditor::onLspReadyRead); //

    // Обновление геометрии фолдинга и отступов
    connect(this, &CodeEditor::textChanged, this, &CodeEditor::updateFoldingData); //

    connect(this, &CodeEditor::updateRequest, this, [this](const QRect &rect, int dy) { //
        if (dy) m_foldingArea->scroll(0, dy); //
        else m_foldingArea->update(0, rect.y(), m_foldingArea->width(), rect.height()); //
    });

    // Принудительная инициализация отрисовщика и хайлайтера при старте
    updateLineNumberAreaWidth(0); //
    highlightCurrentLine(); //

    // Создаем объект подсветки синтаксиса, передавая ему указатель на документ
    m_highlighter = new PythonHighlighter(this->document()); //
}

CodeEditor::~CodeEditor()
{
    // Безопасное завершение процесса фонового сервера Jedi
    if (lspProcess && lspProcess->state() != QProcess::NotRunning) {
        lspProcess->kill();               // Намертво завершаем процесс в ОС Linux
        lspProcess->waitForFinished(1000); // Блокируем поток максимум на 1 сек, ожидая выхода
    }
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
    painter.fillRect(event->rect(), QColor(240, 241, 242)); // Серый фон Breeze

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);

            if (textCursor().blockNumber() == blockNumber) {
                painter.setPen(QColor(35, 38, 41));
                QFont boldFont = painter.font();
                boldFont.setBold(true);
                painter.setFont(boldFont);
            } else {
                painter.setPen(QColor(127, 140, 141));
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

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(228, 242, 252));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

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
    painter.fillRect(event->rect(), QColor(240, 240, 240));

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
                painter.setPen(QColor(35, 38, 41));
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

void CodeEditor::foldingAreaMousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    QTextBlock targetBlock;
    QTextBlock block = firstVisibleBlock();
    int blockTop = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int blockBottom = blockTop + (int) blockBoundingRect(block).height();

    // Находим, по какому именно блоку текста кликнул пользователь
    while (block.isValid()) {
        if (event->position().y() >= blockTop && event->position().y() < blockBottom) {
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
                    painter.setPen(QColor(199, 199, 199));
                    painter.setBrush(QColor(239, 240, 241));
                    painter.drawRoundedRect(badgeRect, 3, 3);

                    painter.setPen(QColor(35, 38, 41));
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
        if (e->position().y() >= blockTop && e->position().y() < blockBottom)
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

void CodeEditor::onLspReadyRead()
{
    if (!lspProcess) return;

    // Накапливаем сырые данные от процесса LSP сервера в статический буфер
    static QByteArray buffer;
    buffer.append(lspProcess->readAllStandardOutput());

    // Ищем разделитель заголовков и тела JSON-RPC пакета
    int separatorIndex = buffer.indexOf("\r\n\r\n");

    while (separatorIndex != -1) {
        QByteArray headers = buffer.left(separatorIndex);

        // Парсим заголовок Content-Length, чтобы знать точный размер JSON
        int contentLength = 0;
        QList<QByteArray> headerLines = headers.split('\n');
    for (const QByteArray &line : std::as_const(headerLines)) {
            if (line.trimmed().startsWith("Content-Length:")) {
                contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
        }

        // Ищем первую открывающую скобку по всему доступному буферу
        int jsonStart = buffer.indexOf('{');
        if (jsonStart == -1 || jsonStart < separatorIndex) {
            jsonStart = buffer.indexOf('{', separatorIndex);
        }

        if (jsonStart == -1) {
            buffer.remove(0, separatorIndex + 4); // Очищаем поврежденные заголовки
            break;
        }

        // Если Content-Length не прислали, вычисляем размер до конца JSON-структуры
        if (contentLength <= 0) {
            int nextHeaderIndex = buffer.indexOf("Content-Type:", jsonStart);
            if (nextHeaderIndex != -1) {
                contentLength = nextHeaderIndex - jsonStart;
            } else {
                contentLength = buffer.length() - jsonStart;
            }
        }

        // Защита от фрагментации данных в потоке Linux
        if (jsonStart + contentLength > buffer.length()) {
            break; // Пакет долетел не полностью, выходим ждать продолжения
        }

        // Вырезаем чистый изолированный JSON-пакет и удаляем его из буфера
        QByteArray jsonData = buffer.mid(jsonStart, contentLength).trimmed();
        buffer.remove(0, jsonStart + contentLength);

        // Передаем очищенный массив байт в парсер документов Qt
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject root = doc.object();

            // ПЕРЕХВАТЫВАЕМ ВХОДЯЩУЮ ДИАГНОСТИКУ ОШИБОК ОТ СЕРВЕРА JEDI
            if (root.value("method").toString() == "textDocument/publishDiagnostics") {
                QJsonObject params = root.value("params").toObject();
                QJsonArray diagnostics = params.value("diagnostics").toArray();

                // Очищаем накопительный графический буфер для этой итерации
                m_lspSelectionsBuffer.clear();

                QTextDocument *docObj = this->document();
                if (docObj) {
                    // Обходим массив ошибок, присланных сервером
                    for (int i = 0; i < diagnostics.size(); ++i) {
                        QJsonObject diagObj = diagnostics[i].toObject();
                        int severity = diagObj.value("severity").toInt(); // 1 - ошибка, 2 - варнинг

                        QJsonObject range = diagObj.value("range").toObject();
                        QJsonObject start = range.value("start").toObject();
                        QJsonObject end = range.value("end").toObject();

                        int startLine = start.value("line").toInt();
                        int startChar = start.value("character").toInt();
                        int endLine = end.value("line").toInt();
                        int endChar = end.value("character").toInt();

                        // Извлекаем текстовые блоки строк напрямую из документа Qt
                        QTextBlock startBlock = docObj->findBlockByLineNumber(startLine);
                        QTextBlock endBlock = docObj->findBlockByLineNumber(endLine);

                        if (startBlock.isValid() && endBlock.isValid()) {
                            // Вычисляем абсолютные позиции символов в памяти текстового поля
                            int absoluteStartPos = startBlock.position() + startChar;
                            int absoluteEndPos = endBlock.position() + endChar;

                            // ФИКС ДЛЯ КОНЦА СТРОКИ: Если ошибка указывает на пустой конец строки,
                            // принудительно сдвигаем выделение на 1 символ назад, чтобы линия легла под буквы
                            if (absoluteStartPos >= absoluteEndPos || startChar >= startBlock.length() - 1) {
                                if (absoluteStartPos > startBlock.position()) {
                                    absoluteStartPos--;
                                } else {
                                    absoluteEndPos++;
                                }
                            }

                            // Формируем структуру дополнительного графического выделения Qt
                            QTextEdit::ExtraSelection selection;
                            selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline); // Волнистая линия
                            selection.format.setUnderlineColor(severity == 1 ? Qt::red : QColor(255, 165, 0)); // Красный / Оранжевый

                            // Привязываем формат к абсолютным координатам через QTextCursor
                            selection.cursor = QTextCursor(docObj);
                            selection.cursor.setPosition(absoluteStartPos);
                            selection.cursor.setPosition(absoluteEndPos, QTextCursor::KeepAnchor);

                            // Складываем готовую линию в буфер отрисовки
                            m_lspSelectionsBuffer.append(selection);
                        }
                    }
                }

                // ПОТОКОБЕЗОПАСНЫЙ ВЫЗОВ ОТРИСОВКИ:
                // Отправляем сформированный буфер линий напрямую в очередь графического потока GUI.
                // Это полностью исключает любые блокировки и мерцания со стороны Qt!
                QMetaObject::invokeMethod(this, "applySelectionsFromLsp", Qt::QueuedConnection);
            }
        }

        // Ищем разделитель для следующего пакета в оставшемся буфере (для цикла while)
        separatorIndex = buffer.indexOf("\r\n\r\n");
    }
}

void CodeEditor::clearErrorHighlights()
{
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);

    QTextCharFormat clearFormat;
    clearFormat.setUnderlineStyle(QTextCharFormat::NoUnderline); // Стираем все волнистые линии

    // Блокируем сигналы изменения текста, чтобы вызов mergeCharFormat не вызвал бесконечную лавину didChange пакетов!
    this->blockSignals(true);
    cursor.mergeCharFormat(clearFormat);
    this->blockSignals(false);
}

void CodeEditor::highlightError(int startLine, int startChar, int endLine, int endChar, bool isError)
{
    QTextDocument *doc = document();
    if (!doc) return;

    QTextBlock startBlock = doc->findBlockByLineNumber(startLine);
    QTextBlock endBlock = doc->findBlockByLineNumber(endLine);

    if (!startBlock.isValid() || !endBlock.isValid()) return;

    int absoluteStartPos = startBlock.position() + startChar;
    int absoluteEndPos = endBlock.position() + endChar;

    // КРИТИЧЕСКИЙ ФИКС ДЛЯ КОНЦА СТРОКИ:
    // Если сервер ругается на конец строки (startChar указывает на \n или пустой символ),
    // мы сдвигаем выделение на 1 символ назад. Линия гарантированно ляжет ПОД последнюю букву оператора (например под True)!
    if (absoluteStartPos >= absoluteEndPos || startChar >= startBlock.length() - 1) {
        if (absoluteStartPos > startBlock.position()) {
            absoluteStartPos--; // Захватываем последний видимый символ строки
        } else {
            absoluteEndPos++;
        }
    }

    QTextCursor cursor(doc);
    cursor.setPosition(absoluteStartPos);
    cursor.setPosition(absoluteEndPos, QTextCursor::KeepAnchor);

    QTextCharFormat format;
    format.setUnderlineStyle(QTextCharFormat::WaveUnderline); // Жесткая волнистая линия Qt
    format.setUnderlineColor(isError ? Qt::red : QColor(255, 165, 0)); // Красный или Оранжевый

    // Закрашиваем буквы напрямую в документе
    this->blockSignals(true);
    cursor.mergeCharFormat(format);
    this->blockSignals(false);
}

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "neuro_programm.h" // ОБЯЗАТЕЛЬНО проверьте этот инклуд!

void CodeEditor::sendLspDidChange()
{
    // Проверяем, что путь к файлу передан и не пуст
    if (this->currentFilePath.isEmpty()) return;

    // Инкрементируем версию документа (+1), чтобы Jedi обновлял кэш в ОЗУ
    this->lspDocumentVersion++;

    // СБОРКА ПАРАМЕТРОВ ПО СТАНДАРТУ ПОЛНОЙ СИНХРОНИЗАЦИИ (FULL SYNC)
    QJsonObject params;

    QJsonObject textDocument;
    textDocument["uri"] = "file://" + this->currentFilePath;
    textDocument["version"] = this->lspDocumentVersion; // Передаем растущую версию!
    params["textDocument"] = textDocument;

    // Маскировка под Full Sync: убираем range и rangeLength, чтобы Jedi обновил весь текст в памяти
    QJsonArray contentChanges;
    QJsonObject changeObj;
    changeObj["text"] = this->toPlainText(); // Актуальный текст из редактора
    contentChanges.append(changeObj);
    params["contentChanges"] = contentChanges;

    // ВЫЗЫВАЕМ ЕДИНЫЙ РАБОЧИЙ ТРАНСПОРТ ГЛАВНОГО ОКНА!
    // Мы не пишем в lspProcess->write напрямую из вкладки.
    // Мы отдаем параметры в центральную функцию, которая уже успешно умеет общаться с Jedi.
    if (Neuro_programm::self) {
        Neuro_programm::self->sendLspRequest("textDocument/didChange", params);

        qDebug() << "[LSP CLIENT] Пакет изменения текста отправлен через глобальный транспорт. Версия:"
                 << this->lspDocumentVersion;
    }
}

void CodeEditor::applySelectionsFromLsp()
{
    // Этот метод выполняется строго внутри главного GUI-потока.
    // Передаем накопленный буфер выделений напрямую в движок отображения Qt!
    this->setExtraSelections(m_lspSelectionsBuffer);

    if (this->viewport()) {
        this->viewport()->update();
    }
}

void CodeEditor::sendLspDidOpen()
{
    if (!lspProcess || lspProcess->state() != QProcess::Running || currentFilePath.isEmpty()) return;

    QJsonObject root;
    root["jsonrpc"] = "2.0";
    root["method"] = "textDocument/didOpen";

    QJsonObject params;
    QJsonObject textDocument;
    textDocument["uri"] = "file://" + currentFilePath;
    textDocument["languageId"] = "python"; // Указываем серверу язык разметки
    textDocument["version"] = 1;           // Начальная версия документа всегда 1
    textDocument["text"] = this->toPlainText(); // Передаем стартовый текст

    params["textDocument"] = textDocument;
    root["params"] = params;

    QJsonDocument doc(root);
    QByteArray rawJson = doc.toJson(QJsonDocument::Compact);
    QByteArray lspPacket;
    lspPacket.append(QString("Content-Length: %1\r\n\r\n").arg(rawJson.length()).toUtf8());
    lspPacket.append(rawJson);

    lspProcess->write(lspPacket);
    lspProcess->waitForBytesWritten(50);

    // Сбрасываем локальный счетчик версий для didChange, чтобы он рос с двойки: 2, 3, 4...
    this->lspDocumentVersion = 1;
    qDebug() << "[LSP] Сессия для файла успешно открыта через didOpen! URI:" << textDocument["uri"];
}
