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
}

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
    lspProcess = new QProcess(this);

    QStringList arguments;
    // ОБЯЗАТЕЛЬНО возвращаем этот флаг, чтобы запустить сервер, а не пустой интерпретатор!
    arguments << "-m" << "pylsp";

    // Указываем путь к python именно ВНУТРИ вашего venv
    QString pythonExecutable = "/home/elf/projects/z1/venv/bin/python";

    // Запускаем с флагом QIODevice::Unbuffered для отключения буферизации пайпов
    lspProcess->start(pythonExecutable, arguments, QIODevice::ReadWrite | QIODevice::Unbuffered);

    connect(lspProcess, &QProcess::errorOccurred, this, [](QProcess::ProcessError error) {
        fprintf(stderr, "\n!!! [LSP КРИТИЧЕСКИЙ СБОЙ] Процесс Jedi выдал ошибку: %d !!!\n", error);
        fflush(stderr);
    });

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
    // connect(lspDelayTimer, &QTimer::timeout, this, [this]() {
    //     this->sendLspDidChange();
    // });

    connect(lspDelayTimer, &QTimer::timeout, this, &CodeEditor::sendLspDidChange);

    // При любом изменении текста перезапускаем таймер заново
    connect(this, &CodeEditor::textChanged, this, [this]() {
        lspDelayTimer->start();
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
   // connect(lspProcess, &QProcess::readyReadStandardOutput, this, &CodeEditor::onLspReadyRead); //

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
    // Очищаем локальные буферы и таймеры конкретно этой вкладки
    if (lspDelayTimer) {
        lspDelayTimer->stop();
        delete lspDelayTimer;
        lspDelayTimer = nullptr;
    }

    // --- УДАЛЯЕМ УБИЙСТВО ГЛОБАЛЬНОГО ПРОЦЕССА! ---
    // Вкладка закрывается мирно, процесс lspProcess НЕ трогаем.
    // Им теперь централизованно управляет главное окно Neuro_programm.
    lspProcess = nullptr;

    // Очищаем бэкапы файлов, если они создавались этой вкладкой
    temporaryOpenFilesBackup.clear();
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

    // 1. Рисуем стандартную голубую/серую полосу под текущей строкой (ваш оригинальный код)
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(228, 242, 252)); // цвет текущей строки
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // 2. САМЫЙ КРИТИЧЕСКИЙ ФИКС: Принудительно склеиваем полосу строки с ошибками Jedi!
    // Благодаря этому, когда Qt обновляет строку под курсором, красные линии НЕ ИСЧЕЗНУТ.
    extraSelections.append(m_currentLspSelections);

    // 3. Отдаем объединенный буфер в систему отображения Qt
    this->setExtraSelections(extraSelections);
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    // =========================================================================
    // СЛУЧАЙ 1: ОКНО ПОДСКАЗОК УЖЕ ОТКРЫТО — ПЕРЕХВАТЫВАЕМ КЛАВИШИ УПРАВЛЕНИЯ
    // =========================================================================
    if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget)
    {
        if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget)
        {
            switch (e->key())
            {
            case Qt::Key_Down: {
                // Перемещаем выделение в списке на одну строку вниз
                int currentRow = m_listWidget->currentRow();
                if (currentRow < m_listWidget->count() - 1) {
                    m_listWidget->setCurrentRow(currentRow + 1);
                }
                e->accept(); // Поглощаем событие, чтобы текстовый курсор не двигался вниз!
                return;
            }
            case Qt::Key_Up: {
                // Перемещаем выделение в списке на одну строку вверх
                int currentRow = m_listWidget->currentRow();
                if (currentRow > 0) {
                    m_listWidget->setCurrentRow(currentRow - 1);
                }
                e->accept(); // Поглощаем событие, чтобы текстовый курсор не двигался вверх!
                return;
            }
            case Qt::Key_PageDown: {
                // Прокручиваем список на страницу вниз
                int nextRow = qMin(m_listWidget->count() - 1, m_listWidget->currentRow() + 5);
                m_listWidget->setCurrentRow(nextRow);
                e->accept();
                return;
            }
            case Qt::Key_PageUp: {
                // Прокручиваем список на страницу вверх
                int prevRow = qMax(0, m_listWidget->currentRow() - 5);
                m_listWidget->setCurrentRow(prevRow);
                e->accept();
                return;
            }
            default:
                break;
            }
        }


        if (e->modifiers() == Qt::AltModifier && (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return))
        {
            e->accept();

            QJsonObject params;
            QJsonObject textDocument;
            textDocument["uri"] = "file://" + this->currentFilePath;
            params["textDocument"] = textDocument;

            // Передаем текущие координаты курсора
            QTextCursor cursor = this->textCursor();
            QJsonObject position;
            position["line"] = cursor.blockNumber(); // В Qt строки с 0, Jedi это понимает
            position["character"] = cursor.columnNumber();

            QJsonObject range;
            range["start"] = position;
            range["end"] = position;
            params["range"] = range;

            // Контекст: передаем список ошибок, если они есть (опционально для Jedi)
            QJsonObject context;
            QJsonArray diagnostics;
            context["diagnostics"] = diagnostics;
            params["context"] = context;

            // Отправляем асинхронный запрос через центральный транспорт
            if (Neuro_programm::self) {
                // Используем фиксированный ID для этого запроса (например, 999)
                Neuro_programm::self->sendLspRequest("textDocument/codeAction", params, 999);
            }
            return;
        }

        // 2. Закрытие окна по Esc
        if (e->key() == Qt::Key_Escape) {
            m_popupWindow->hide();
            m_popupWindow->deleteLater();
            m_popupWindow = nullptr;
            e->accept(); // Говорим Qt, что событие обработано, дальше идти нельзя
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
            if (m_popupWindow) {
                m_popupWindow->hide();
                m_popupWindow->deleteLater();
                m_popupWindow = nullptr;
                m_listWidget = nullptr; // Обязательно очищаем дочерний виджет!
            }
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
            } else
                if (m_popupWindow) {
                m_popupWindow->hide();
                m_popupWindow->deleteLater();
                m_popupWindow = nullptr;
                m_listWidget = nullptr; // Очищаем, чтобы верхний if больше не срабатывал
            }
        }
        return;
    }

    // =========================================================================
    // СЛУЧАЙ 2: ОКНО ЗАКРЫТО — ОТДАЕМ КЛАВИШИ СИСТЕМЕ И ЛОВИМ ВВОД СИМВОЛА ТОЧКИ
    // =========================================================================
    QPlainTextEdit::keyPressEvent(e); // Сначала печатаем точку на экране

    if (e->text() == ".")
    {
        // 1. МГНОВЕННО отправляемdidChange для точки, не дожидаясь таймера дебаунса в 300мс!
        // Это критично, чтобы Jedi узнал о существовании точки ДО того, как мы попросим подсказки.
        QJsonObject changeParams;
        QJsonObject textDocument;
        textDocument["uri"] = "file://" + this->currentFilePath;
        textDocument["version"] = ++this->lspDocumentVersion;
        changeParams["textDocument"] = textDocument;

        QJsonArray contentChanges;
        QJsonObject changeObj;
        changeObj["text"] = this->toPlainText(); // Отдаем актуальный текст с точкой
        contentChanges.append(changeObj);
        changeParams["contentChanges"] = contentChanges;

        if (Neuro_programm::self) {
            Neuro_programm::self->sendLspRequest("textDocument/didChange", changeParams);
        }

        // 2. СРАЗУ ЖЕ отправляем запрос на получение подсказок автодополнения (Completion)
        QJsonObject compParams;
        QJsonObject compDoc;
        compDoc["uri"] = "file://" + this->currentFilePath;
        compParams["textDocument"] = compDoc;

        QTextCursor cursor = this->textCursor();
        QJsonObject position;
        position["line"] = cursor.blockNumber();      // Текущая строка (с 0)
        position["character"] = cursor.columnNumber(); // Позиция СРАЗУ ПОСЛЕ точки
        compParams["position"] = position;

        if (Neuro_programm::self) {
            // =====================================================================
            // КРИТИЧЕСКИЙ ФИКС: Передаем число 100 в качестве третьего аргумента!
            // Это заставит sendLspRequest добавить обязательное поле "id": 100 в JSON,
            // и Python-валидатор cattrs больше не упадет с ошибкой KeyError: 'id'!
            // =====================================================================
            Neuro_programm::self->sendLspRequest("textDocument/completion", compParams, 100);
        }
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
    // 1. Дописываем ВСЕ новые сырые данные из процесса в конец нашего буфера
    m_lspBuffer.append(lspProcess->readAllStandardOutput());

    // 2. Крутим цикл, пока в буфере есть целые пакеты
    while (true)
    {
        // Ищем начало заголовка размера пакета
        int contentLengthIndex = m_lspBuffer.indexOf("Content-Length:");
        if (contentLengthIndex == -1) break; // Заголовка еще нет, ждем данные

        // Ищем маркер конца заголовков (двойной перенос строки)
        int jsonStartIndex = m_lspBuffer.indexOf("\r\n\r\n", contentLengthIndex);
        if (jsonStartIndex == -1) break; // Заголовок пришел не полностью, ждем данные
        jsonStartIndex += 4; // Смещаем указатель на фактическое начало JSON текста

        // Вытаскиваем число размера пакета из строки "Content-Length: 123"
        int headerLengthOffset = contentLengthIndex + 15;
        QByteArray lengthString = m_lspBuffer.mid(headerLengthOffset, (jsonStartIndex - 4) - headerLengthOffset).trimmed();
        int expectedJsonLength = lengthString.toInt();

        // Проверяем: накопилось ли в буфере достаточно байт для целого JSON?
        if (m_lspBuffer.size() < jsonStartIndex + expectedJsonLength) {
            break; // Пакет еще «долетает» по сети/пайпу, выходим и ждем следующего вызова
        }

        // 3. ПАКЕТ СОБРАН! Вырезаем чистый JSON-текст из общего буфера
        QByteArray cleanJsonData = m_lspBuffer.mid(jsonStartIndex, expectedJsonLength);

        // Удаляем обработанный кусок из начала буфера, чтобы освободить место для следующих пакетов
        m_lspBuffer.remove(0, jsonStartIndex + expectedJsonLength);

        // 4. ПАРСИМ ТОЛЬКО ЧИСТЫЙ И ЦЕЛЫЙ JSON
        QJsonDocument doc = QJsonDocument::fromJson(cleanJsonData);
        if (doc.isNull()) continue;

        QJsonObject root = doc.object();

        // Проверяем метод
        if (root.value("method").toString() == "textDocument/publishDiagnostics")
        {
            QJsonObject params = root.value("params").toObject();
            QJsonArray diagnostics = params.value("diagnostics").toArray();

            QList<QTextEdit::ExtraSelection> newSelections;

            for (int i = 0; i < diagnostics.size(); ++i)
            {
                QJsonObject diagObj = diagnostics[i].toObject();
                QJsonObject range = diagObj.value("range").toObject();
                QJsonObject start = range.value("start").toObject();
                QJsonObject end = range.value("end").toObject();

                int startLine = start.value("line").toInt();
                int startChar = start.value("character").toInt();
                int endChar = end.value("character").toInt();

                QTextEdit::ExtraSelection selection;

                // Настройка внешнего вида линии (красная волнистая)
                selection.format.setUnderlineColor(Qt::red);
                selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);

                QTextCursor cursor(this->document());
                QTextBlock block = this->document()->findBlockByLineNumber(startLine);

                if (block.isValid()) {
                    int startPos = block.position() + startChar;
                    int endPos = block.position() + (endChar <= startChar ? startChar + 1 : endChar);

                    cursor.setPosition(startPos);
                    cursor.setPosition(endPos, QTextCursor::KeepAnchor);
                    selection.cursor = cursor;

                    newSelections.append(selection);
                }
            }

            // Передаем готовый список выделений в UI поток
            // Используем Qt::QueuedConnection для безопасности потоков
            QMetaObject::invokeMethod(this, "applySelectionsFromLsp",
                                      Qt::QueuedConnection,
                                      Q_ARG(QList<QTextEdit::ExtraSelection>, newSelections));
        }
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

// void CodeEditor::sendLspDidChange()
// {
//     // Проверяем, что путь к файлу передан и не пуст
//     if (this->currentFilePath.isEmpty()) return;

//     // Инкрементируем версию документа (+1), чтобы Jedi обновлял кэш в ОЗУ
//     this->lspDocumentVersion++;

//     // СБОРКА ПАРАМЕТРОВ ПО СТАНДАРТУ ПОЛНОЙ СИНХРОНИЗАЦИИ (FULL SYNC)
//     QJsonObject params;

//     QJsonObject textDocument;
//     textDocument["uri"] = "file://" + this->currentFilePath;
//     textDocument["version"] = this->lspDocumentVersion; // Передаем растущую версию!
//     params["textDocument"] = textDocument;

//     // Маскировка под Full Sync: убираем range и rangeLength, чтобы Jedi обновил весь текст в памяти
//     QJsonArray contentChanges;
//     QJsonObject changeObj;
//     changeObj["text"] = this->toPlainText(); // Актуальный текст из редактора
//     contentChanges.append(changeObj);
//     params["contentChanges"] = contentChanges;

//     // ВЫЗЫВАЕМ ЕДИНЫЙ РАБОЧИЙ ТРАНСПОРТ ГЛАВНОГО ОКНА!
//     // Мы не пишем в lspProcess->write напрямую из вкладки.
//     // Мы отдаем параметры в центральную функцию, которая уже успешно умеет общаться с Jedi.
//     if (Neuro_programm::self) {
//         Neuro_programm::self->sendLspRequest("textDocument/didChange", params);

//         qDebug() << "[LSP CLIENT] Пакет изменения текста отправлен через глобальный транспорт. Версия:"
//                  << this->lspDocumentVersion;
//     }

// }

#include <QTimer>

void CodeEditor::sendLspDidChange()
{
    // Проверяем, что путь к файлу передан и не пуст
    if (this->currentFilePath.isEmpty()) return;

    // ОПТИМИЗАЦИЯ: Дебаунсинг (Задержка ввода)
    // Чтобы не спамить Jedi при каждой зажатой клавише, используем встроенный таймер.
    // Если таймер уже запущен, мы его перезапускаем (сбрасываем задержку).
    // Для этого добавьте QTimer* m_lspDelayTimer в private секцию вашего codeeditor.h
    // и инициализируйте его в конструкторе: m_lspDelayTimer = new QTimer(this); m_lspDelayTimer->setSingleShot(true);

    // Если вы еще не создали таймер в .h файле, можно использовать статический (быстрое решение):
    static QTimer* delayTimer = nullptr;
    if (!delayTimer) {
        delayTimer = new QTimer(this);
        delayTimer->setSingleShot(true);
    }

    // Переподключаем сигнал таймера, чтобы он сработал один раз после паузы в наборе текста
    disconnect(delayTimer, &QTimer::timeout, nullptr, nullptr);
    connect(delayTimer, &QTimer::timeout, this, [this]() {

        if (!Neuro_programm::self) return;

        // Инкрементируем версию документа (+1), чтобы Jedi обновлял кэш в ОЗУ
        this->lspDocumentVersion++;

        // СБОРКА ПАРАМЕТРОВ ПО СТАНДАРТУ ПОЛНОЙ СИНХРОНИЗАЦИИ (FULL SYNC)
        QJsonObject params;

        QJsonObject textDocument;
        // КРИТИЧНО ДЛЯ LINUX: убедитесь, что путь абсолютный, чтобы получилось file:/// (три слэша)
        textDocument["uri"] = "file://" + this->currentFilePath;
        textDocument["version"] = this->lspDocumentVersion;
        params["textDocument"] = textDocument;

        // Передаем весь текст из памяти на лету
        QJsonArray contentChanges;
        QJsonObject changeObj;
        changeObj["text"] = this->toPlainText();
        contentChanges.append(changeObj);
        params["contentChanges"] = contentChanges;

        // ВЫЗЫВАЕМ ЕДИНЫЙ РАБОЧИЙ ТРАНСПОРТ ГЛАВНОГО ОКНА
        Neuro_programm::self->sendLspRequest("textDocument/didChange", params);

        // КРИТИЧНО ДЛЯ ВЫПЛЕСКА ДАННЫХ ИЗ БУФЕРА LINUX:
        // Так как lspProcess лежит внутри главного окна Neuro_programm,
        // мы принудительно заставляем его очистить пайпы отправки.
        // (Убедитесь, что у вас есть доступ к lspProcess из Neuro_programm)
        if (Neuro_programm::self->lspProcess) {
            Neuro_programm::self->lspProcess->waitForBytesWritten(10);
        }

        qDebug() << "[LSP CLIENT] Пакет изменения текста отправлен после паузы. Версия:"
                 << this->lspDocumentVersion;
    });

    // Запускаем задержку в 350 миллисекунд.
    // Пакет уйдет в Jedi только тогда, когда пользователь остановит печать на треть секунды.
    delayTimer->start(350);
}


#include <QStatusBar>

void CodeEditor::applySelectionsFromLsp(const QList<QTextEdit::ExtraSelection> &selections)
{
    // Блокируем сигналы, чтобы избежать петли обновлений
    this->blockSignals(true);

    // СОХРАНЯЕМ ОШИБКИ В КЛАСС: m_currentLspSelections объявлен в codeeditor.h
    m_currentLspSelections = selections;

    // Вызываем централизованный метод отрисовки, который мы исправим на Шаге 2
    this->highlightCurrentLine();

    this->blockSignals(false);

    // --- ГАРАНТИРОВАННЫЙ ВЫВОД В КОНСОЛЬ LINUX ---
    // Этот код сработает 100%, даже если qDebug() полностью вырезан компилятором
    fprintf(stderr, "\n>>> [LSP КЛИЕНТ] УСПЕШНО ПОЛУЧЕНЫ ОШИБКИ! Количество: %d <<<\n", selections.size());
    fflush(stderr); // Мгновенно выталкиваем короткую строчку на экран

    if (Neuro_programm::self && Neuro_programm::self->statusBar()) {
        if (selections.size() > 0) {
            Neuro_programm::self->statusBar()->showMessage(
                QString("Критическая ошибка синтаксиса! Найдено: %1").arg(selections.size())
                );
            Neuro_programm::self->statusBar()->setStyleSheet("QStatusBar { color: #ff3333; font-weight: bold; }");
        } else {
            Neuro_programm::self->statusBar()->showMessage("Код успешно проверен Jedi. Ошибок нет.", 3000);
            Neuro_programm::self->statusBar()->setStyleSheet("QStatusBar { color: #00ff00; }");
        }
    }
}

void CodeEditor::sendLspDidOpen()
{
    if (this->currentFilePath.isEmpty()) return;
    if (!Neuro_programm::self) return;

    this->lspDocumentVersion = 1; // Стартовая версия файла

    QJsonObject params;
    QJsonObject textDocument;

    // КРИТИЧНО: Ровно три слэша для абсолютных путей в Linux (file:///)
    textDocument["uri"] = "file://" + this->currentFilePath;
    textDocument["languageId"] = "python";
    textDocument["version"] = this->lspDocumentVersion;
    textDocument["text"] = this->toPlainText(); // Передаем стартовый текст из памяти редактора
    params["textDocument"] = textDocument;

    // Отправляем пакет активации документа через наш обновленный sendLspRequest
    Neuro_programm::self->sendLspRequest("textDocument/didOpen", params);

    qDebug() << "[LSP] Документ официально зарегистрирован в памяти Jedi!";
}

#include <QMenu>
#include <QTextBlock>

void CodeEditor::showQuickFixMenu(const QList<QuickFixAction>& fixes)
{
    if (fixes.isEmpty()) {
        if (Neuro_programm::self && Neuro_programm::self->statusBar()) {
            Neuro_programm::self->statusBar()->showMessage("Jedi: Вариантов исправления не найдено", 2000);
        }
        return;
    }

    // Создаем меню быстрых исправлений с родителем-редактором (для безопасности памяти)
    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu { background-color: #232323; color: #ffffff; border: 1px solid #555; padding: 4px; }"
        "QMenu::item { padding: 4px 20px; }"
        "QMenu::item:selected { background-color: #3a3a3a; color: #ff2a2a; }"
        );

    // Наполняем меню действиями
    for (const auto& fix : fixes) {
        QAction* action = menu->addAction(fix.title);

        // Упаковываем C++ структуру фикса прямо в QAction
        QVariant data;
        data.setValue(fix);
        action->setData(data);
    }

    // Показываем меню ровно под курсором ввода текста на экране
    QPoint globalCursorPos = this->mapToGlobal(this->cursorRect().bottomLeft());
    QAction* selectedAction = menu->exec(globalCursorPos);

    // Пользователь кликнул на исправление — применяем его в памяти на лету!
    if (selectedAction) {
        QuickFixAction fix = selectedAction->data().value<QuickFixAction>();

        QTextDocument* doc = this->document();
        QTextCursor editCursor(doc);

        // Перемещаем курсор к началу ошибочного блока
        QTextBlock startBlock = doc->findBlockByLineNumber(fix.startLine);
        int startPos = startBlock.position() + fix.startChar;
        editCursor.setPosition(startPos);

        // Выделяем текст до конца ошибочного блока
        QTextBlock endBlock = doc->findBlockByLineNumber(fix.endLine);
        int endPos = endBlock.position() + fix.endChar;
        editCursor.setPosition(endPos, QTextCursor::KeepAnchor);

        // Начинаем транзакцию редактирования, чтобы работал Ctrl+Z (Undo/Redo)
        editCursor.beginEditBlock();
        editCursor.insertText(fix.newText); // Заменяем старый текст на исправленный!
        editCursor.endEditBlock();
    }

    // Безопасное отложенное удаление меню силами очереди событий Qt
    menu->deleteLater();
}
