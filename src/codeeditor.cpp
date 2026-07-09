// codeeditor.cpp
#include "codeeditor.h"
#include <QJsonDocument>
#include <QTextBlock>
#include "neuro_programm.h"
#include "stickyscrollarea.h"
#include "qhtmldelegate.h"


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
#include <QWheelEvent>
#include "minimaparea.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDateTime>
#include <QtConcurrent>
#include <QFuture>
#include <QMenu>
#include <QTextBlock>
#include <QTimer>
#include <QPushButton>
#include <QShortcut>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
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

    // Наш таймер дебаунса (остается без изменений)
    lspDelayTimer = new QTimer(this);
    lspDelayTimer->setSingleShot(true);

    connect(lspDelayTimer, &QTimer::timeout, this, &CodeEditor::sendLspDidChange);

    // При любом изменении текста перезапускаем таймер заново
    connect(this, &CodeEditor::textChanged, this, [this]() {
        lspDelayTimer->start();
    });

    // Сигнал contentsChanged() генерируется ядром Qt всегда, пробивая любые блокировки хайлайтеров!
    if (this->document()) {
        connect(this->document(), &QTextDocument::contentsChanged, this, [this]() {
            // if (lspProcess && lspProcess->state() == QProcess::Running) {
            //     // Запускаем или перезапускаем отсчет 300 миллисекунд
                 lspDelayTimer->start(300);
            // }
        });
    }

    // --- 3. ИНИЦИАЛИЗАЦИЯ ПАНЕЛЕЙ И РОДНЫХ СИГНАЛОВ QT ТЕКСТОВОГО ПОЛЯ ---
    lineNumberArea = new LineNumberArea(this); //
    m_foldingArea = new FoldingArea(this); //

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth); //
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea); //
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        // Вызываем подсветку строки (она сама внутри соберет и обновит extraSelections)
        this->highlightCurrentLine();

        // Принудительно запускаем перекраску скобок следом
        this->matchBrackets();
    });

    connect(this, &QPlainTextEdit::selectionChanged, this, [this]() {
        this->highlightCurrentLine();
    });
    //connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::matchBrackets);


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

    // Вставить в конструктор вашего текстового редактора CodeEditor:
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &CodeEditor::customContextMenuRequested, this, &CodeEditor::showEditorContextMenu);

    connect(this, &QPlainTextEdit::selectionChanged, this, [this]() {
        // Находим вашу кастомную панель номеров строк (LineNumberArea)
        if (this->lineNumberArea) {
            this->lineNumberArea->update(); // Принудительно вызываем paintEvent панели
        }
    });
    // ЖЕСТКИЙ ФИКС КРАША: Гарантируем компилятору, что изначально указатель пуст и безопасен
    this->minimapArea = nullptr;

    // Инициализируем флаги и базовые переменные... (ваш остальной код конструктора)
    this->isLspFreeze = false;

    // --- ВНУТРИ КОНСТРУКТОРА CodeEditor ---
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        // Страховка: если идет первоначальная загрузка файла, маркеры не ставим
        if (this->property("isLoading").toBool()) return;

        QTextCursor cursor = this->textCursor();
        QTextBlock block = cursor.block();

        if (block.isValid()) {
            FolderBlockData *data = static_cast<FolderBlockData*>(block.userData());
            if (!data) {
                data = new FolderBlockData();
                block.setUserData(data);
            }

            // Если строка еще не была изменена или уже была сохранена — делаем её КРАСНОЙ
            if (data->changeState != FolderBlockData::Modified) {
                data->changeState = FolderBlockData::Modified;

                // Запускаем мгновенную перерисовку панели номеров строк, чтобы маркер появился
                if (lineNumberArea) lineNumberArea->update();
            }
        }
    });

    m_stickyScrollWidget = new StickyScrollArea(this);
    m_stickyScrollWidget->show();

    connect(this, &CodeEditor::updateRequest, this, [this](const QRect &rect, int dy) {
        if (dy && m_stickyScrollWidget) {
            // Вызываем метод пересчета напрямую у виджета
            //QMetaObject::invokeMethod(m_stickyScrollWidget, "calculateStickyBlocks", Qt::DirectConnection);
            m_stickyScrollWidget->calculateStickyBlocks();
        }
    });

    // codeeditor.cpp -> Переписываем инициализацию или метод вывода поп-апа
    if (!m_popupWindow) {
        // Создаем базовый контейнер
        m_popupWindow = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

        // ВАЖНО ДЛЯ СКРУГЛЕНИЯ: Разрешаем прозрачность подложки виджета, чтобы скруглить углы QSS
        m_popupWindow->setAttribute(Qt::WA_TranslucentBackground, true);
        m_popupWindow->setAttribute(Qt::WA_ShowWithoutActivating, true);

        // Главный вертикальный макет поп-апа
        QVBoxLayout *popupLayout = new QVBoxLayout(m_popupWindow);
        popupLayout->setContentsMargins(0, 0, 0, 0);
        popupLayout->setSpacing(0);

        // 1. Создаем сам список подсказок
        m_listWidget = new QListWidget(m_popupWindow);
        m_listWidget->setItemDelegate(new QHtmlDelegate(this));

        // НАМЕРТВО ОТКЛЮЧАЕМ ВСЕ СКРОЛЛБАРЫ (И горизонтальный, и вертикальный!)
        m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        popupLayout->addWidget(m_listWidget);

        // 2. СОЗДАЕМ СИСТЕМНУЮ НИЖНЮЮ ПАНЕЛЬ (Панель подсказок и кнопка Троеточие)
        QWidget *bottomBar = new QWidget(m_popupWindow);
        bottomBar->setObjectName("popupBottomBar");
        bottomBar->setFixedHeight(24); // Жесткую высоту задаем САМОМУ ВИДЖЕТУ панели, а не макету!

        QHBoxLayout *bottomLayout = new QHBoxLayout(bottomBar);
        bottomLayout->setContentsMargins(10, 0, 5, 0); // Плотные UX отступы
        bottomLayout->setSpacing(0);

        // Текст-подсказка шорткатов
        QLabel *tipLabel = new QLabel("Ctrl+Down and Ctrl+Up will move caret", bottomBar);
        tipLabel->setStyleSheet("color: #7f8c8d; font-size: 10px; border: none; background: transparent;");
        bottomLayout->addWidget(tipLabel);
        bottomLayout->addStretch();

        // Та самая КНОПКА-ТРОЕТОЧИЕ для вывода быстрой документации
        QPushButton *btnMore = new QPushButton("⋮", bottomBar);
        btnMore->setFixedSize(18, 18);
        btnMore->setCursor(Qt::PointingHandCursor);
        btnMore->setStyleSheet(
            "QPushButton {"
            "  color: #a5a5a5; font-size: 14px; font-weight: bold; border: none; "
            "  background: transparent; border-radius: 3px;"
            "}"
            "QPushButton:hover { background-color: #3a3d41; color: #ffffff; }"
            );
        bottomLayout->addWidget(btnMore);
        popupLayout->addWidget(bottomBar);

        // Настраиваем EventFilter клавиатуры на список подсказок
        m_listWidget->installEventFilter(this);

// =========================================================================
// ПРАВИЛЬНЫЙ КОННЕКТ КНОПКИ ВНУТРИ СФЕРЫ ВИДИМОСТИ ПЕРЕМЕННОЙ btnMore
// =========================================================================
// =========================================================================
// ПРАВИЛЬНЫЙ КОННЕКТ КНОПКИ ВНУТРИ СФЕРЫ ВИДИМОСТИ ПЕРЕМЕННОЙ btnMore
// =========================================================================
#include <QMenu>
#include <QShortcut> // Добавляем шорткат прямо на холст редактора!

        connect(btnMore, &QPushButton::clicked, this, [this, btnMore]() {
            // ФИКС РОДИТЕЛЯ: Передаем nullptr, чтобы меню могло принимать клики в Linux!
            QMenu *menu = new QMenu(nullptr);
            menu->setAttribute(Qt::WA_DeleteOnClose);

            // Стилизуем меню под темную тему Breeze/JetBrains
            menu->setStyleSheet(
                "QMenu { background-color: #232629; color: #eff0f1; border: 1px solid #3a3d41; padding: 4px; }"
                "QMenu::item { padding: 4px 20px; border-radius: 3px; background-color: transparent; }"
                "QMenu::item:selected { background-color: #313539; color: #ffffff; }"
                );

            // Наполняем пункты
            QAction *actSort = menu->addAction("✓ Sort by Name");
            QAction *actDoc = menu->addAction("Quick Documentation      Ctrl+Q");
            QAction *actDef = menu->addAction("Quick Definition     Ctrl+Shift+I");
            menu->addSeparator();
            QAction *actSettings = menu->addAction("Code Completion Settings");

            actSort->setEnabled(false);
            actDef->setEnabled(true);
            actSettings->setEnabled(false);

            // Добавляем обработчик клика для Quick Definition строго по аналогии с actDoc!
            connect(actDef, &QAction::triggered, this, [this]() {
                if (!m_listWidget) return;
                QListWidgetItem* currentItem = m_listWidget->currentItem();
                if (currentItem) {
                    if (m_popupWindow) m_popupWindow->hide();
                        this->setFocus();

                        QTextCursor cursor = this->textCursor();
                    QString realActivePath = this->objectName().isEmpty() ? this->currentFilePath : this->objectName();
                        if (!realActivePath.isEmpty()) {
                        // Излучаем сигнал запроса дефиниции (переход к объявлению функции/класса)
                        emit definitionRequested(realActivePath, cursor.blockNumber(), cursor.columnNumber());
                    }
                }
            });

            // Сигнал вызова окна документации по клику на пункт меню
            connect(actDoc, &QAction::triggered, this, [this]() {
                if (!m_listWidget) return;

                QListWidgetItem* currentItem = m_listWidget->currentItem();
                if (!currentItem && !m_listWidget->selectedItems().isEmpty()) {
                    currentItem = m_listWidget->selectedItems().first();
                }

                if (currentItem) {
                    // СРАЗУ ГАСИМ ОКНО АВТОДОПОЛНЕНИЯ, ЧТОБЫ ВЕРНУТЬ ФОКУС КАРЕТКЕ РЕДАКТОРА
                    if (m_popupWindow) m_popupWindow->hide();
                    this->setFocus();

                    QString cleanMethodName = currentItem->text();
                    if (cleanMethodName.contains("<font") || cleanMethodName.contains("<span")) {
                        static const QRegularExpression htmlRegex("<[^>]*>");
                        cleanMethodName.remove(htmlRegex);
                    }

                    qDebug() << ">>> [UX DOCUMENTATION] Запрос справки для метода:" << cleanMethodName;

                    QTextCursor cursor = this->textCursor();
                    QString realActivePath = this->objectName().isEmpty() ? this->currentFilePath : this->objectName();

                    if (!realActivePath.isEmpty()) {
                        emit documentationRequested(realActivePath, cursor.blockNumber(), cursor.columnNumber());
                    }
                }
            });

            // Используем нативный навигационный exec(), но позиционируем его абсолютно безопасно
            menu->exec(btnMore->mapToGlobal(QPoint(0, btnMore->height())));
        });
        // =========================================================================

        // Накатываем объединенный QSS-стиль на поп-ап линзу
        m_popupWindow->setStyleSheet(
            "QWidget { background-color: #232629; color: #eff0f1; font-family: 'JetBrains Mono', 'Fira Code', 'Monospace'; font-size: 12px; }"
            "QListWidget { border: 1px solid #3a3d41; border-top-left-radius: 6px; border-top-right-radius: 6px; background-color: #232629; }"
            "QListWidget::item { padding: 4px 8px; border: none; }"
            "QListWidget::item:hover { background-color: #2c2f33; }"
            "QListWidget::item:selected { background-color: #313539; color: #ffffff; }"
            "QWidget#popupBottomBar { background-color: #1e2022; border: 1px solid #3a3d41; border-top: none; border-bottom-left-radius: 6px; border-bottom-right-radius: 6px; }"
            );
    }

    // =========================================================================
    // ЖЕЛЕЗНЫЙ ФИКС ДЛЯ ШОРТКАТА CTRL+Q НА УРОВНЕ ВСЕГО РЕДАКТОРА
    // =========================================================================
    // Создаем шорткат, который принудительно сработает, когда вы нажмете Ctrl+Q в коде!
    QShortcut *shortcutDoc = new QShortcut(QKeySequence("Ctrl+Q"), this);
    connect(shortcutDoc, &QShortcut::activated, this, [this]() {
        if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget) {
            QListWidgetItem* currentItem = m_listWidget->currentItem();
            if (currentItem) {
                if (m_popupWindow) m_popupWindow->hide();
                this->setFocus();

                QString cleanMethodName = currentItem->text().remove(QRegularExpression("<[^>]*>"));
                QTextCursor cursor = this->textCursor();
                QString realActivePath = this->objectName().isEmpty() ? this->currentFilePath : this->objectName();
                if (!realActivePath.isEmpty()) {
                    emit documentationRequested(realActivePath, cursor.blockNumber(), cursor.columnNumber());
                }
            }
        }
    });
}

CodeEditor::~CodeEditor()
{
    // Очищаем поп-ап автодополнения, если он существует
    if (m_popupWindow) {
        m_popupWindow->close();
        delete m_popupWindow;
        m_popupWindow = nullptr;
        m_listWidget = nullptr;
    }

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

int CodeEditor::lineNumberAreaWidth()
{
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
    // ЖЕСТКИЙ ФИКС НАЛОЖЕНИЯ: Слева отступаем под номера и фолдинг, а справа — строго 70px под миникарту!
    setViewportMargins(lineNumberAreaWidth() + foldingAreaWidth(), 0, 70, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy) lineNumberArea->scroll(0, dy);
    else lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    static bool isResizing = false;
    if (isResizing) {
        QPlainTextEdit::resizeEvent(e);
        return;
    }
    isResizing = true;

    // 1. Даем базовому классу Qt пересчитать стандартное текстовое окно
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    int lineNumWidth = lineNumberAreaWidth();
    int foldWidth = foldingAreaWidth();
    int mWidth = 70; // Ширина миникарты

    // 2. Позиционируем левые панели
    if (lineNumberArea) {
        lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumWidth, cr.height()));
    }
    if (m_foldingArea) {
        m_foldingArea->setGeometry(QRect(cr.left() + lineNumWidth, cr.top(), foldWidth, cr.height()));
    }

    // =========================================================================
    // АБСОЛЮТНАЯ ЗАЩИТА ОТ КРАША: Проверяем указатель на nullptr и валидность в ОЗУ
    // =========================================================================
    if (this->minimapArea != nullptr) {
        minimapArea->setGeometry(QRect(cr.right() - mWidth, cr.top(), mWidth, cr.height()));
    }

    // Вместо прямой строки setViewportMargins вызываем метод обновления с фиксом:
    updateLineNumberAreaWidth(0);

    if (m_stickyScrollWidget) {
        m_stickyScrollWidget->updateGeometrySize();
    }

    isResizing = false;
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);

    // 1. Системный фон панели номеров
    QColor systemWindowColor = this->palette().color(QPalette::Window);
    painter.fillRect(event->rect(), systemWindowColor);

    // Получаем точные координаты выделения мыши (Ваша оригинальная логику из PDF)
    QTextCursor cursor = this->textCursor();
    int currentActiveLine = cursor.blockNumber(); // Строка, где мигает каретка

    int startLineNum = this->document()->findBlock(cursor.selectionStart()).blockNumber();
    int endLineNum = this->document()->findBlock(cursor.selectionEnd()).blockNumber();
    bool hasSelection = cursor.hasSelection();

    // Стандартный обход видимых блоков
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    // Извлекаем стандартный системный цвет текста темы
    QColor systemTextColor = this->palette().color(QPalette::WindowText);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {

            QString number = QString::number(blockNumber + 1);

            painter.save(); // Сохраняем состояние painter для каждой цифры отдельно

            // Настраиваем базовый шрифт для вычислений
            QFont font = painter.font();

            // =========================================================================
            // УМНАЯ ИДЕНТИФИКАЦИЯ И РАЗДЕЛЕНИЕ ПОД СВЕТЛУЮ ТЕМУ IDE
            // =========================================================================
            bool isInSelectionRange = hasSelection && (blockNumber >= startLineNum && blockNumber <= endLineNum);
            bool isExactCurrentLine = (blockNumber == currentActiveLine);

            if (isExactCurrentLine)
            {
                // ЭТАЛОННЫЙ ФОКУС: Строка, где стоит курсор (Делаем ЖИРНЕЕ и КРУПНЕЕ)
                font.setBold(true);
                font.setWeight(QFont::Black); // Максимальная, абсолютная жирность в Qt!
                font.setPixelSize(fontMetrics().height() - 1); // Физически увеличиваем кегль цифры
                painter.setFont(font);

                painter.setPen(QColor("#000000")); // Радикальный черный цвет
            }
            else if (isInSelectionRange)
            {
                // МНОГОСТРОЧНЫЙ ВЫДЕЛЕННЫЙ ДИАПАЗОН (Мышка зажала несколько строк)
                font.setBold(true);
                painter.setFont(font);

                // Красим в контрастный темно-серый цвет, чтобы сочетался с синим выделением коде
                painter.setPen(QColor("#111111"));
            }
            else
            {
                // НЕАКТИВНЫЕ СТРОКИ: Делаем их блекло-серыми, чтобы они создавали глубину
                font.setBold(false);
                painter.setFont(font);

                // Слегка приглушаем цвет системного текста (делаем мягким серым)
                QColor mutedColor = systemTextColor;
                mutedColor.setAlpha(130); // 50% прозрачности для фоновых цифр
                painter.setPen(mutedColor);
            }
            // =========================================================================

            // Отрисовываем цифру на панели
            painter.drawText(0, top, lineNumberArea->width() - 8, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, number);

            painter.restore(); // Возвращаем painter в исходное состояние

            // =========================================================================
            // ОРИГИНАЛЬНЫЙ БЛОК: ОТРИСОВКА ВЕРТИКАЛЬНОЙ DIFF-ЛИНИИ
            // =========================================================================
            FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());
            if (foldData && foldData->changeState != FolderBlockData::Unchanged)
            {
                painter.save();
                if (foldData->changeState == FolderBlockData::Modified) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor("#ff3333"));
                } else if (foldData->changeState == FolderBlockData::Saved) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor("#4cf54c"));
                }
                int lineAreaWidth = lineNumberArea->width();
                int markerWidth = 3;
                int markerX = lineAreaWidth - markerWidth;
                painter.drawRect(markerX, top, markerWidth, fontMetrics().height());
                painter.restore();
            }
            // =========================================================================
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        blockNumber++;
    }
}

void CodeEditor::highlightCurrentLine()
{
    // Центральный буфер для ВСЕХ графических выделений в редакторе
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 1. ПОДСВЕТКА ТЕКУЩЕЙ СТРОКИ КОДА (Если файл открыт не в режиме "только чтение")
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        // Настраиваем красивый Breeze цвет подложки строки
        selection.format.setBackground(QColor(228, 242, 252));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);

        // Привязываем подсветку строго к текущему положению каретки курсора
        selection.cursor = textCursor();
        selection.cursor.clearSelection();

        extraSelections.append(selection);
    }

    // 2. ИНТЕГРАЦИЯ ОШИБОК СЕРВЕРА JEDI (LSP)
    extraSelections.append(m_currentLspSelections);

    // =========================================================================
    // 3. ОТРИСОВКА ВИРТУАЛЬНЫХ КУРСОРОВ ДЛЯ МУЛЬТИКУРСОРНОСТИ (С ФИЛЬТРОМ ДВОЕНИЯ)
    // =========================================================================
    int mainCaretPos = textCursor().position(); // Позиция главного системного курсора

    for (const QTextCursor &vCursor : m_virtualCursors) {
        // ЗАЩИТА ОТ ГРАФИЧЕСКОГО ДВОЕНИЯ СИМВОЛА:
        // Если виртуальный курсор наложился на позицию главного — не дублируем его отрисовку
        if (vCursor.position() == mainCaretPos) {
            continue;
        }

        QTextEdit::ExtraSelection sel;
        sel.cursor = vCursor;

        if (!vCursor.hasSelection()) {
            // Выделяем ровно 1 символ справа рамкой цвета текста (симуляция каретки)
            if (!sel.cursor.atEnd()) {
                sel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
            }

            QPen caretPen(palette().color(QPalette::Text), 1);
            sel.format.setProperty(QTextFormat::OutlinePen, caretPen);
        } else {
            // Режим выделенного текста: подсвечиваем полупрозрачным синим цветом Breeze
            sel.format.setBackground(QColor(0, 120, 215, 80));
        }
        extraSelections.append(sel);
    }

    // 4. Отдаем объединенный монолитный буфер графическому движку Qt
    this->setExtraSelections(extraSelections);

    // =========================================================================
    // 5. ИНТЕГРАЦИЯ СКОБОК (БЕЗ КОНФЛИКТОВ С БУФЕРОМ)
    // =========================================================================
    // Метод matchBrackets внутри себя считает текущие extraSelections(),
    // подмешивает туда зеленые скобки и сам вызывает финальный setExtraSelections.
    // Вызываем его в самом конце, чтобы он зацепил и мультикурсоры!
    this->matchBrackets();
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    QString textToInsert = e->text();

    // =========================================================================
    // ЧАСТЬ 1: ГЛОБАЛЬНЫЙ ПЕРЕХВАТ ОДИНОЧНОЙ ТОЧКИ ДЛЯ ВЫЗОВА COMPLETION (ID: 100)
    // =========================================================================
    if (textToInsert == "." && m_virtualCursors.isEmpty())
    {
        // Вычисляем чистый абсолютный путь к активному файлу на диске
        QString realActivePath = this->objectName();
            if (realActivePath.isEmpty() && this->parentWidget()) {
                realActivePath = this->parentWidget()->objectName();
        }
        if (realActivePath.isEmpty()) realActivePath = this->currentFilePath;

            if (Neuro_programm::self && !realActivePath.isEmpty())
        {
            QTextCursor beforeCursor = this->textCursor();
                int lineBeforeDot = beforeCursor.blockNumber();
                int charBeforeDot = beforeCursor.columnNumber();

                // 1. Печатаем саму точку на экране, блокируя лишние рекурсивные вызовы
                this->blockSignals(true);
            QPlainTextEdit::keyPressEvent(e);
                this->blockSignals(false);

            // 2. Синхронизируем документ с сервером LSP (didChange)
            this->lspDocumentVersion++;
                QJsonObject changeParams;
            QJsonObject textDocumentObj;

#include <QDir>
            QString cleanPath = QDir::fromNativeSeparators(realActivePath);
                textDocumentObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
                textDocumentObj["version"] = this->lspDocumentVersion;
                changeParams["textDocument"] = textDocumentObj;

                QJsonObject changeContentObj;
            QString pureText = this->toPlainText();
                pureText.replace(QString::fromUtf8("\xE2\x80\xA9"), "\n"); // Очищаем внутренние переносы Qt
            if (!pureText.endsWith('\n')) pureText += "\n";
                changeContentObj["text"] = pureText;
                QJsonArray contentChangesArray;
            contentChangesArray.append(changeContentObj);
                changeParams["contentChanges"] = contentChangesArray;

                // Извещаем Jedi, что символ "." теперь физически находится в документе
                Neuro_programm::self->sendLspRequest("textDocument/didChange", changeParams);

                // 3. Запрашиваем список методов строго в позиции СРАЗУ ЗА ТОЧКОЙ
                QJsonObject compParams;
            QJsonObject compDocObj;
            compDocObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
                compParams["textDocument"] = compDocObj;

                QJsonObject positionObj;
            positionObj["line"] = lineBeforeDot;
                positionObj["character"] = charBeforeDot + 1; // Координата за точкой
            compParams["position"] = positionObj;

                Neuro_programm::self->sendLspRequest("textDocument/completion", compParams, 100);
                qDebug() << ">>> [LSP] Введена точка. Запрос completion (id:100) отправлен!";

            e->accept();
            return; // Полностью выходим, точка обработана!
        }
    }
    // =========================================================================
    // ЧАСТЬ 2: НАКОПИТЕЛЬНАЯ ФИЛЬТРАЦИЯ ПРЕФИКСОВ (Когда окно автодополнения открыто)
    // =========================================================================
    if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget &&
        (!textToInsert.isEmpty() || e->key() == Qt::Key_Backspace))
    {
        // Принудительно пишем символ в текстовый холст редактора
        e->accept();
        this->blockSignals(true);
        QPlainTextEdit::keyPressEvent(e);
        this->blockSignals(false);

        // Считываем обновленные координаты курсора и ищем крайнюю левую точку
        QTextCursor cursor = this->textCursor();
        QString lineText = cursor.block().text();
            QString leftOfCursor = lineText.left(cursor.columnNumber());
            int lastDotIndex = leftOfCursor.lastIndexOf('.');

            // Если пользователь стер точку Бэкспейсом — полностью закрываем панель
            if (lastDotIndex == -1 && e->key() == Qt::Key_Backspace && !leftOfCursor.contains(QRegularExpression("[a-zA-Z0-9_]"))) {
            m_popupWindow->hide();
                this->setFocus();
                return;
        }

        // Вычисляем накопленный буквенный префикс (например: "p" -> "pr" -> "pri")
        QString currentPrefix = "";
        if (lastDotIndex != -1) {
            currentPrefix = leftOfCursor.mid(lastDotIndex + 1).toLower();
        } else {
            static const QRegularExpression lastWordRegex("[a-zA-Z0-9_]+$");
                QRegularExpressionMatch match = lastWordRegex.match(leftOfCursor);
                if (match.hasMatch()) currentPrefix = match.captured(0).toLower();
        }

        // Если префикс очищен пробелом — гасим поп-ап
        if (textToInsert == " ") {
            m_popupWindow->hide();
                this->setFocus();
                return;
        }

        int firstVisibleRow = -1;
        int visibleCount = 0;

        // Быстро пробегаемся по списку и скрываем неподходящие строки
        m_listWidget->setUpdatesEnabled(false);
            for (int i = 0; i < m_listWidget->count(); ++i) {
                QListWidgetItem *item = m_listWidget->item(i);
                if (!item) continue;
                QString itemText = item->text();

                if (itemText.contains("<font")) {
                    static const QRegularExpression htmlRegex("<[^>]*>");
                    itemText.remove(htmlRegex);
            }

            bool matches = itemText.toLower().startsWith(currentPrefix);

                // На лету перекрашиваем совпавшие буквы в голубой цвет темы Breeze
                if (matches && !currentPrefix.isEmpty()) {
                QString typedPart = itemText.left(currentPrefix.length());
                    QString restPart = itemText.mid(currentPrefix.length());
                    item->setText(QString("<font color='#4cc3ff'><b>%1</b></font><font color='#eff0f1'>%2</font>")
                                      .arg(typedPart, restPart));
            } else {
                item->setText(QString("<font color='#eff0f1'>%1</font>").arg(itemText));
            }

            item->setHidden(!matches);
                if (matches) {
                visibleCount++;
                if (firstVisibleRow == -1) firstVisibleRow = i;
            }
        }
        m_listWidget->setUpdatesEnabled(true);

            // Управляем отображением виджета по результатам сканирования
            if (visibleCount > 0 && firstVisibleRow != -1) {
            m_listWidget->setCurrentRow(firstVisibleRow);
                if (m_listWidget->item(firstVisibleRow)) {
                    m_listWidget->setCurrentItem(m_listWidget->item(firstVisibleRow));
            }
            m_popupWindow->show();
        } else {
            // ЖЕСТКИЙ UX-ФИКС: Если совпадений ноль (ввели лишний символ), окно автоматически закрывается!
            m_popupWindow->hide();
                this->setFocus();
        }

        this->sendLspDidChange(); // Асинхронно уведомляем Jedi о вводе новой буквы
        return;
    }
    // =========================================================================
    // ЧАСТЬ 3: МАКРОСЫ, ГОРЯЧИЕ КЛАВИШИ И ПАССИВНЫЙ ДЕБАУНС НАБОРА СЛОВ
    // =========================================================================

    // Блокировка и фикс символа "Z" (Ваш оригинальный код со страницы 1-2)
    if (e->key() == Qt::Key_Z) {
        this->setProperty("isKeyZPressed", true);
    }
    if (e->key() == Qt::Key_Z && this->property("isKeyZPressed").toBool() && e->modifiers() == Qt::NoModifier) {
            if (!m_virtualCursors.isEmpty() || this->underMouse()) {
                e->accept(); return;
        }
    }

    // Shift + Enter — Выполнение кода в терминале (Страница 2)
    if ((e->modifiers() & Qt::ShiftModifier) && (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)) {
            QTextCursor cursor = textCursor(); QString textToExecute = cursor.selectedText();
            if (textToExecute.isEmpty()) { cursor.select(QTextCursor::LineUnderCursor); textToExecute = cursor.selectedText(); }
            textToExecute.replace(QString::fromUtf8("\xE2\x80\xA9"), "\n");
            emit selectionExecutionRequested(textToExecute);
            e->accept(); return;
    }

    // Ctrl + / — Умное построчное комментирование (Страница 2)
    if (e->modifiers() == Qt::ControlModifier && e->key() == Qt::Key_Slash) {
            e->accept(); QTextCursor cursor = this->textCursor();
            int startPos = cursor.selectionStart(); int endPos = cursor.selectionEnd();
            QTextBlock startBlock = this->document()->findBlock(startPos); QTextBlock endBlock = this->document()->findBlock(endPos);
            int startLine = startBlock.blockNumber(); int endLine = endBlock.blockNumber();
            if (startLine > endLine) std::swap(startLine, endLine);
            if (startLine < endLine && cursor.position() == endBlock.position() && cursor.anchor() != cursor.position()) { endLine--; }
            bool shouldComment = false;
            QTextBlock currentBlock = this->document()->findBlockByLineNumber(startLine);
            for (int i = startLine; i <= endLine; ++i) {
                if (currentBlock.isValid()) { QString lineText = currentBlock.text().trimmed(); if (!lineText.isEmpty() && !lineText.startsWith("#")) { shouldComment = true; break; } currentBlock = currentBlock.next(); }
        }
        cursor.beginEditBlock(); currentBlock = this->document()->findBlockByLineNumber(startLine); QTextCursor writeCursor(this->document());
            for (int i = startLine; i <= endLine; ++i) {
                if (currentBlock.isValid()) { QString rawText = currentBlock.text(); writeCursor.setPosition(currentBlock.position());
                    if (shouldComment) { writeCursor.insertText("# "); }
                    else {
                    if (rawText.startsWith("# ")) { writeCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2); writeCursor.removeSelectedText(); }
                        else if (rawText.startsWith("#")) { writeCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1); writeCursor.removeSelectedText(); }
                }
                currentBlock = currentBlock.next();
            }
        }
        cursor.endEditBlock(); return;
    }

    // Навигация стрелками и закрытие по Escape внутри открытого поп-апа (Страница 3-4)
    if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget) {
            if (e->key() == Qt::Key_Down) {
                int currentRow = m_listWidget->currentRow(); int nextRow = (currentRow < m_listWidget->count() - 1) ? currentRow + 1 : 0;
                m_listWidget->setCurrentRow(nextRow); e->accept(); return;
        }
        if (e->key() == Qt::Key_Up) {
                int currentRow = m_listWidget->currentRow(); int prevRow = (currentRow > 0) ? currentRow - 1 : m_listWidget->count() - 1;
                m_listWidget->setCurrentRow(prevRow); e->accept(); return;
        }
        if (e->key() == Qt::Key_Escape) {
                m_popupWindow->hide(); this->setFocus(); e->accept(); return;
        }
    }

    // Нативный пассивный ввод символа на экране
    QPlainTextEdit::keyPressEvent(e);

        // ИНТЕЛЛЕКТУАЛЬНЫЙ ТРИГГЕР НАБОРА СЛОВ (Автоматический вызов окна без точки)
        if (!textToInsert.isEmpty() && m_virtualCursors.isEmpty())
    {
        QTextCursor cursor = this->textCursor();
            QString lineText = cursor.block().text().left(cursor.columnNumber());
            static const QRegularExpression wordRegex("[a-zA-Z0-9_]+$");
            QRegularExpressionMatch match = wordRegex.match(lineText);

            if (match.hasMatch()) {
            QString currentWord = match.captured(0);

                // Если набрали хотя бы 1 букву (например 'p'), запускаем отсчет таймера к Jedi
                if (currentWord.length() >= 1) {
                QTimer* compDelayTimer = this->findChild<QTimer*>("lspCompletionDelayTimer");
                    if (!compDelayTimer) {
                    compDelayTimer = new QTimer(this);
                        compDelayTimer->setObjectName("lspCompletionDelayTimer");
                        compDelayTimer->setSingleShot(true);
                }
                compDelayTimer->disconnect();

                    int savedLine = cursor.blockNumber();
                    int savedChar = cursor.columnNumber();

                    connect(compDelayTimer, &QTimer::timeout, this, [this, savedLine, savedChar, currentWord]() {
                        QString realActivePath = this->objectName().isEmpty() ? this->currentFilePath : this->objectName();
                            if (Neuro_programm::self && !realActivePath.isEmpty()) {
                            QJsonObject compParams; QJsonObject compDocObj;
                                QString cleanPath = QDir::fromNativeSeparators(realActivePath);
                                compDocObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
                                compParams["textDocument"] = compDocObj;
                                QJsonObject positionObj;
                                positionObj["line"] = savedLine;
                                positionObj["character"] = savedChar;
                                compParams["position"] = positionObj;

                                // Посылаем запрос в фоновый поток сервера
                                Neuro_programm::self->sendLspRequest("textDocument/completion", compParams, 100);
                        }
                    });
                compDelayTimer->start(180); // Ждем 180мс затухания дребезга клавиш перед открытием
            }
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
    // =========================================================================
    // ЛЕНИВАЯ РЕГИСТРАЦИЯ В JEDI: Если сервер готов, а файл еще не открыт — открываем!
    // =========================================================================
    static QSet<QString> registeredFiles; // Храним список уже открытых путей
    QString cleanPath = this->currentFilePath;

    if (Neuro_programm::self && !cleanPath.isEmpty() && !registeredFiles.contains(cleanPath)) {
        // Проверяем по вашей логике из neuro_programm.cpp, запущен ли сервер
        // Если метод sendLspDidOpen() ещё не вызывался для этого файла:
        this->sendLspDidOpen();
        registeredFiles.insert(cleanPath); // Маркируем как успешно зарегистрированный
    }

    // =========================================================================
    // БЕЗОПАСНЫЙ СТАРТ: Проверка на пустой экран (Заставка JetBrains шорткатов)
    // =========================================================================
    if (this->toPlainText().trimmed().isEmpty())
    {
        // Если вы используете встроенный сплэш внутри paintEvent, его код выполняется здесь.
        // Если заставка уже вынесена в EditorPlaceholder, Qt просто идет дальше.
    }

    // 1. ЕДИНСТВЕННЫЙ И ПРАВИЛЬНЫЙ ВЫЗОВ СТАНДАРТНОЙ ОТРЫСОВКИ ТЕКСТА QT
    QPlainTextEdit::paintEvent(e);

    // Создаем основной графический контекст для viewport редактора
    QPainter painter(this->viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Считываем ширину панели номеров строк и параметры шрифта
    int lineNumberAreaWidth = (lineNumberArea != nullptr) ? lineNumberArea->width() : 0;

    // Настройка пера для Indent Guides (Четкая контрастная линия в 1px строго под циклы/условия Python)
    QPen indentPen;
    indentPen.setColor(QColor("#b0b4bc")); // Насыщенный серый цвет Breeze Light
    indentPen.setWidth(1);                 // Толщина строго в 1 пиксель
    indentPen.setStyle(Qt::SolidLine);     // Сплошная линия

    // =========================================================================
    // 2. СВЕДЕННЫЙ ЦИКЛ ОБХОДА: ПЛАШКИ СВЕРТЫВАНИЯ И СТРУКТУРНЫЕ НАПРАВЛЯЮЩИЕ
    // =========================================================================
    QTextBlock block = firstVisibleBlock();
    int blockTop = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int blockBottom = blockTop + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && blockTop <= e->rect().bottom())
    {
        if (block.isVisible() && blockBottom >= e->rect().top())
        {
            QString text = block.text();
            QString trimmedText = text.trimmed();
            FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());

            // -----------------------------------------------------------------
            // А. УМНАЯ ИНТЕЛЛЕКТУАЛЬНАЯ ЛОГИКА INDENT GUIDES (ТОЛЬКО ДЛЯ FOR/IF/WHILE)
            // -----------------------------------------------------------------
            QTextLayout *layoutObj = block.layout();
            if (layoutObj && layoutObj->lineCount() > 0)
            {
                QTextLine textLine = layoutObj->lineAt(0);

                // Считаем количество пробелов/табуляций в начале текущей строки
                int leadingSpaces = 0;
                for (char ch : text.toStdString()) {
                    if (ch == ' ') leadingSpaces++;
                    else if (ch == '\t') leadingSpaces += 4;
                    else break;
                }

                // УМНЫЙ АНАЛИЗ БЛОКОВ: Проверяем, является ли строка управляющей конструкцией Python
                bool isControlStructure = trimmedText.startsWith("for ")   ||
                                          trimmedText.startsWith("while ") ||
                                          trimmedText.startsWith("if ")    ||
                                          trimmedText.startsWith("elif ")  ||
                                          trimmedText.startsWith("else:")  ||
                                          trimmedText.startsWith("try:")   ||
                                          trimmedText.startsWith("except ")||
                                          trimmedText.startsWith("with ");

                bool isFunctionDef = trimmedText.startsWith("def ") || trimmedText.startsWith("class ");

                // Текущий уровень вложенности строки (шаг отступа)
                int currentLevel = leadingSpaces / 4;

                // УМНЫЙ АЛГОРИТМ: Заглядываем вперед для ограничения длины линий по блокам
                QTextBlock nextBlock = block.next();
                int nextLeadingSpaces = 0;
                while (nextBlock.isValid() && nextBlock.text().trimmed().isEmpty()) {
                    nextBlock = nextBlock.next();
                }
                if (nextBlock.isValid()) {
                    QString nextText = nextBlock.text();
                    for (char ch : nextText.toStdString()) {
                        if (ch == ' ') nextLeadingSpaces++;
                        else if (ch == '\t') nextLeadingSpaces += 4;
                        else break;
                    }
                }
                int nextLevel = nextLeadingSpaces / 4;

                // На пустых строках наследуем отступ следующего за ней рабочего кода
                if (trimmedText.isEmpty() && nextBlock.isValid()) {
                    currentLevel = nextLevel;
                }

                // Настройка лимита линий на текущей строке
                int maxGuides = currentLevel;

                if (isFunctionDef && currentLevel == 0) {
                    // А. Если это функция def нулевого уровня, нам не нужна линия у левого края
                    maxGuides = 0;
                }
                else if (!isControlStructure && !trimmedText.isEmpty()) {
                    // Б. Если это обычный код (print, parser, переменная), она НЕ имеет права
                    // рисовать линию своего уровня! Оставляем только линии родительских блоков, открытых выше.
                    maxGuides = qMin(currentLevel, nextLevel);
                    if (maxGuides == currentLevel && nextLevel <= currentLevel) {
                        maxGuides = currentLevel - 1; // Схлопываем линию текущего уровня для обычного кода
                    }
                }
                else if (nextLevel < currentLevel) {
                    // В. Если блок ниже закрылся (отступ уменьшился), линия текущего цикла обрывается
                    maxGuides = nextLevel;
                }

                if (maxGuides < 0) maxGuides = 0;

                painter.save();
                // Отключаем размытие, чтобы вертикальная линия в 1px была идеально резкой
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.setPen(indentPen);

                for (int i = 1; i <= maxGuides; ++i)
                {
                    // Вычисляем индекс символа для i-й линии
                    int targetCharIndex = (i - 1) * 4;

                    // Находим истинное физическое начало первой буквы строки в пикселях.
                    qreal startTextX = textLine.cursorToX(targetCharIndex);
                    int lineX = static_cast<int>(contentOffset().x()) + lineNumberAreaWidth + static_cast<int>(startTextX);

                    // Оптический микро-сдвиг на 1 пиксель вправо для ровного отвеса по левой грани букв f, i, w
                    lineX += 1;

                    // Рисуем вертикальный отрезок строго в границах текущей строки
                    painter.drawLine(lineX, blockTop, lineX, blockBottom);
                }
                painter.restore();
            }

            // -----------------------------------------------------------------
            // Б. ЛОГИКА СВЕРТЫВАНИЯ КОДА (Рисуем плашку {...} )
            // -----------------------------------------------------------------
            if (foldData && foldData->isFoldStart && foldData->isFolded)
            {
                QTextLayout *layoutObj = block.layout();
                if (layoutObj && layoutObj->lineCount() > 0)
                {
                    QTextLine line = layoutObj->lineAt(0);
                    int textWidth = static_cast<int>(line.naturalTextWidth());

                    int badgeX = static_cast<int>(contentOffset().x()) + textWidth + 15;
                    int badgeY = blockTop + 3;
                    int badgeWidth = 35;
                    int badgeHeight = static_cast<int>(blockBoundingRect(block).height()) - 6;
                    QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

                    painter.save();
                    painter.setRenderHint(QPainter::Antialiasing, true); // Для плашек сглаживание нужно
                    painter.setPen(QColor(199, 199, 199));
                    painter.setBrush(QColor(239, 240, 241));
                    painter.drawRoundedRect(badgeRect, 3, 3);

                    painter.setPen(QColor(35, 38, 41));
                    QFont font = painter.font();
                    font.setBold(true);
                    painter.setFont(font);

                    painter.drawText(badgeRect, Qt::AlignCenter, "{...}");
                    painter.restore();
                }
            }
        }

        // Переходим к следующему блоку текста ниже
        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + static_cast<int>(blockBoundingRect(block).height());
    }

    // =========================================================================
    // 3. ИНТЕГРИРОВАННЫЙ ВЫВОД ИНТЕРАКТИВНОЙ ЛУПЫ С ПОДСВЕТКОЙ СИНТАКСИСА
    // =========================================================================
    if (this->property("showMinimapLens").toBool() && this->minimapArea != nullptr)
    {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPoint mousePos = this->property("minimapMousePos").toPoint();
        double totalLines = this->document()->blockCount();

        if (totalLines > 0 && minimapArea->height() > 0)
        {
            double hoverPercent = static_cast<double>(mousePos.y()) / minimapArea->height();
            int targetLineNum = static_cast<int>(hoverPercent * totalLines);
            if (targetLineNum < 0) targetLineNum = 0;
            if (targetLineNum >= totalLines) targetLineNum = totalLines - 1;

            int startLook = qMax(0, targetLineNum - 2);
            int endLook = qMin(static_cast<int>(totalLines) - 1, targetLineNum + 2);

            int lensWidth = 320;
            int lensHeight = 90;
            int lensX = this->viewport()->width() - 70 - lensWidth - 2;
            int lensY = mousePos.y() - (lensHeight / 2);
            if (lensY < 5) lensY = 5;
            if (lensY + lensHeight > this->viewport()->height() - 5)
            {lensY = this->viewport()->height() - lensHeight - 5;
            }
            painter.setBrush(this->palette().color(QPalette::Base));
            painter.setPen(QPen(this->palette().color(QPalette::Mid), 1));
            painter.drawRoundedRect(lensX, lensY, lensWidth, lensHeight, 6, 6);
            QFont lensFont;
            lensFont.setPixelSize(12);
            lensFont.setFamily("Monospace");
            painter.setFont(lensFont);
            int textX = lensX + 12;
            int textY = lensY + 8;
            int lineHeight = fontMetrics().height() - 2;
            for (int l = startLook; l <= endLook; ++l)
            {QTextBlock b = this->document()->findBlockByNumber(l);
                if (b.isValid())
                {QString text = b.text();
                    QString clippedText = text.left(45);
                    if (l == targetLineNum)
                    {painter.setPen(this->palette().color(QPalette::Highlight));
                        painter.drawText(textX, textY, "➔");
                    }
                    int currentTextX = textX + 14;
                    QTextLayout textLayout(clippedText, lensFont);
                    textLayout.beginLayout();
                    QTextLine line = textLayout.createLine();
                    textLayout.endLayout();
                    // 1. ИСПРАВЛЕНО: Добавлены угловые скобки для объявления динамического списка форматов
                    QList<QTextLayout::FormatRange> textFormats;

                    if (m_highlighter && b.layout())
                    {
                        // 2. ИСПРАВЛЕНО: Добавлены угловые скобки для получения списка formats() из layout
                        QList<QTextLayout::FormatRange> blockFormats = b.layout()->formats();

                        for (const auto &range : blockFormats)
                        {
                            if (range.start < 45)
                            {
                                QTextLayout::FormatRange r;
                                r.start = range.start;
                                r.length = qMin(range.length, static_cast<int>(45 - range.start));
                                r.format = range.format;
                                textFormats.append(r);
                            }
                        }
                    }

                    // 3. ИСПРАВЛЕНО: Вынесли painter.save() на отдельную строчку с правильными отступами
                    painter.save();

                    painter.setClipRect(QRect(currentTextX, textY, (lensWidth - 35), lineHeight));
                    textLayout.draw(&painter, QPointF(currentTextX, textY), textFormats);
                    painter.restore();
                }
                textY += lineHeight;
            }
        }
        painter.restore();
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
    // 1. Дописываем новые сырые данные из процесса в конец буфера (Ваш родной код)
    m_lspBuffer.append(lspProcess->readAllStandardOutput());

    // 2. Крутим цикл сборки пакетов
    while (true)
    {
        int contentLengthIndex = m_lspBuffer.indexOf("Content-Length:");
        if (contentLengthIndex == -1) break;

        int jsonStartIndex = m_lspBuffer.indexOf("\r\n\r\n", contentLengthIndex);
        if (jsonStartIndex == -1) break;

        jsonStartIndex += 4;
        int headerLengthOffset = contentLengthIndex + 15;
        QByteArray lengthString = m_lspBuffer.mid(headerLengthOffset, (jsonStartIndex - 4) - headerLengthOffset).trimmed();
        int expectedJsonLength = lengthString.toInt();

        if (m_lspBuffer.size() < jsonStartIndex + expectedJsonLength) {
            break; // Пакет еще долетает, ждем данные
        }

        // ВЫРЕЗАЕМ ЧИСТЫЙ JSON-ПАКЕТ ИЗ БУФЕРА
        QByteArray cleanJsonData = m_lspBuffer.mid(jsonStartIndex, expectedJsonLength);
        m_lspBuffer.remove(0, jsonStartIndex + expectedJsonLength);

        // =========================================================================
        // ЖЕЛЕЗНЫЙ АСИНХРОННЫЙ ФИКС ЗАВИСАНИЯ: Фоновый поток парсинга
        // =========================================================================
        QtConcurrent::run([this, cleanJsonData]() {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(cleanJsonData, &parseError);
            if (parseError.error != QJsonParseError::NoError || doc.isNull()) {
                return; // Ошибка парсинга, выходим из фонового потока
            }

            QJsonObject root = doc.object();

            // =========================================================================
            // ГЛОБАЛЬНЫЙ ФИКС: Извлекаем ID пакета на самом верхнем уровне фонового потока
            // =========================================================================
            int responseId = root.value("id").toInt();
            if (responseId == 0 && root.contains("id")) {
                responseId = root.value("id").toString().toInt();
            }
            // =========================================================================
            // --- СЦЕНАРИЙ 1: Пакет диагностики ошибок (publishDiagnostics) ---
            if (root.value("method").toString() == "textDocument/publishDiagnostics")
            {
                QJsonObject params = root.value("params").toObject();
                QJsonArray diagnostics = params.value("diagnostics").toArray();
                QList<QTextEdit::ExtraSelection> newSelections;

                for (int i = 0; i < diagnostics.size(); ++i) {
                    QJsonObject diagObj = diagnostics[i].toObject();
                    QJsonObject range = diagObj.value("range").toObject();
                    QJsonObject start = range.value("start").toObject();
                    QJsonObject end = range.value("end").toObject();
                    int startLine = start.value("line").toInt();
                    int startChar = start.value("character").toInt();
                    int endChar = end.value("character").toInt();

                    QTextEdit::ExtraSelection selection;
                    selection.format.setUnderlineColor(Qt::red);
                    selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);

                    // Создаем безопасную копию документа для фонового потока
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

                // Безопасно передаем готовый массив линий ошибок назад в графический поток
                QMetaObject::invokeMethod(this, "applySelectionsFromLsp",
                                          Qt::QueuedConnection,
                                          Q_ARG(QList<QTextEdit::ExtraSelection>, newSelections));
            }
            // --- СЦЕНАРИЙ 1.5: ОБРАБОТКА И ВЫВОД ДОКУМЕНТАЦИИ В ОТДЕЛЬНОЕ ОКНО (id: 555) ---
            else if (responseId == 555) {
                if (root.contains("result") && !root.value("result").isNull()) {
                    QJsonObject resultObj = root.value("result").toObject();
                    QString docString = "";

                    if (resultObj.contains("contents")) {
                        QJsonValue contentsVal = resultObj.value("contents");
                        if (contentsVal.isString()) docString = contentsVal.toString();
                        else if (contentsVal.isObject()) docString = contentsVal.toObject().value("value").toString();
                    }

                    if (!docString.isEmpty()) {
                        docString.replace("\n", "<br>");

                        // Безопасно вызываем графический поток главного окна
                        QMetaObject::invokeMethod(this->window(), [this, docString]() {
                            Neuro_programm *mainWin = qobject_cast<Neuro_programm*>(this->window());
                            if (!mainWin) return;

                            // Если окно документации еще не создано в памяти — инициализируем его
                            if (!mainWin->m_docWindow) {
#include <QTextBrowser>
                                mainWin->m_docWindow = new QTextBrowser(nullptr); // Независимое окно
                                mainWin->m_docWindow->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
                                mainWin->m_docWindow->resize(400, 250);

                                // Фирменный стиль Breeze Dark со скруглениями углов в 6px
                                mainWin->m_docWindow->setStyleSheet(
                                    "QTextBrowser {"
                                    "  background-color: #2c2f33; color: #eff0f1; border: 1px solid #3a3d41; "
                                    "  border-radius: 6px; padding: 10px; font-family: 'JetBrains Mono'; font-size: 11px;"
                                    "}"
                                    );
                            }

                            // Выводим HTML-код справки от Jedi
                            mainWin->m_docWindow->setHtml("<b style='color:#4cc3ff;'>Quick Documentation:</b><br><br>" + docString);

                            // Умное позиционирование линзы документации СПРАВА от основного окна поп-апа
                            if (m_popupWindow && m_popupWindow->isVisible()) {
                                QPoint popupPos = m_popupWindow->pos();
                                mainWin->m_docWindow->move(popupPos.x() + m_popupWindow->width() + 5, popupPos.y());
                            } else {
                                mainWin->m_docWindow->move(this->viewport()->mapToGlobal(this->cursorRect().bottomRight()));
                            }

                            mainWin->m_docWindow->show();
                            mainWin->m_docWindow->raise();

                            // Возвращаем фокус ввода в редактор кода
                            this->setFocus();
                        });
                    }
                }
                return;
            }
            // --- СЦЕНАРИЙ 2: Фильтруем пакет автодополнения (id: 100) ---
            else if (responseId == 100) {
                if (!root.contains("result") || root.value("result").isNull()) return;

                QJsonArray itemsArray;
                QJsonValue resultVal = root.value("result");
                if (resultVal.isArray()) {
                    itemsArray = resultVal.toArray();
                } else if (resultVal.isObject()) {
                    QJsonObject resultObj = resultVal.toObject();
                    if (resultObj.contains("items")) {
                        itemsArray = resultObj.value("items").toArray();
                    }
                }

                if (itemsArray.isEmpty()) return;
                QStringList completionSuggestions;

                // Универсальный расчет префикса для torch.Linear или print
                QTextCursor cursor = this->textCursor();
                QString lineText = cursor.block().text().left(cursor.columnNumber());
                int lastDot = lineText.lastIndexOf('.');

                QString prefix = "";
                if (lastDot != -1) {
                    prefix = lineText.mid(lastDot + 1).toLower();
                } else {
                    static const QRegularExpression lastWordRegex("[a-zA-Z0-9_]+$");
                    QRegularExpressionMatch match = lastWordRegex.match(lineText);
                    if (match.hasMatch()) {
                        prefix = match.captured(0).toLower();
                    }
                }

                for (int i = 0; i < itemsArray.size(); ++i) {
                    QJsonObject item = itemsArray[i].toObject();
                    QString label = item.value("label").toString();
                    if (label.isEmpty()) label = item.value("insertText").toString();

                    if (!label.isEmpty()) {
                        if (!prefix.isEmpty() && label.toLower().startsWith(prefix)) {
                            QString typedPart = label.left(prefix.length());
                            QString restPart = label.mid(prefix.length());

                            // Формируем чистые теги <font> без черного цвета
                            QString htmlLabel = QString("<font color='#4cc3ff'><b>%1</b></font>"
                                                        "<font color='#eff0f1'>%2</font>")
                                                    .arg(typedPart, restPart);
                            completionSuggestions.append(htmlLabel);
                        } else {
                            QString htmlLabel = QString("<font color='#eff0f1'>%1</font>").arg(label);
                            completionSuggestions.append(htmlLabel);
                        }
                    }
                }

                completionSuggestions.sort(Qt::CaseInsensitive);

                // Передаем готовый отфильтрованный HTML в GUI-поток
                QMetaObject::invokeMethod(this, "showLspCompletionsInGui",
                                          Qt::QueuedConnection,
                                          Q_ARG(QStringList, completionSuggestions));
            }
        });
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

void CodeEditor::sendLspDidChange()
{
    // Ищем или создаем таймер задержки набора (дебаунс)
    QTimer* activeDelayTimer = this->findChild<QTimer*>("lspActiveDelayTimer");
    if (!activeDelayTimer) {
        activeDelayTimer = new QTimer(this);
        activeDelayTimer->setObjectName("lspActiveDelayTimer");
        activeDelayTimer->setSingleShot(true);
    }

    if (activeDelayTimer != nullptr) {
        activeDelayTimer->disconnect();
    }

    // Подключаем лямбду, которая сработает ПОСЛЕ того, как пользователь перестал нажимать кнопки
    connect(activeDelayTimer, &QTimer::timeout, this, [this]() {
        if (!Neuro_programm::self) return;

        // Извлекаем путь строго из objectName активного виджета
        QString realActivePath = this->objectName();
        if (realActivePath.isEmpty() && this->parentWidget()) {
            realActivePath = this->parentWidget()->objectName();
        }
        if (realActivePath.isEmpty()) realActivePath = this->currentFilePath;
        if (realActivePath.isEmpty()) return;

        this->lspDocumentVersion++;

        QJsonObject textDocument;

// =========================================================================
// ЖЕСТКАЯ НОРМАЛИЗАЦИЯ ПУТИ ДЛЯ LINUX (Защита от Windows разделителей \ в URI)
// =========================================================================
#include <QDir> // Убедитесь, что этот инклуд есть вверху файла codeeditor.cpp
        QString cleanPath = QDir::fromNativeSeparators(realActivePath);
        textDocument["uri"] = QUrl::fromLocalFile(cleanPath).toString();
        textDocument["version"] = this->lspDocumentVersion;

        QJsonObject changeObj;

        // СИНТАКСИЧЕСКИЙ ФИКС СТРОК (Заменяем внутренний мусор Qt на стандартные переносы \n)
        QString pureText = this->toPlainText();
        pureText.replace(QString::fromUtf8("\xE2\x80\xA9"), "\n");

        // Передаем текст в QJsonObject НАПРЯМУЮ, без ручного экранирования слэшей.
        // Qt сам превратит \n в валидный для JSON формат при отправке.
        changeObj["text"] = pureText;

        QJsonArray contentChanges;
        contentChanges.append(changeObj);

        QJsonObject params;
        params["textDocument"] = textDocument;
        params["contentChanges"] = contentChanges;

        // Отправляем чистый сформированный JSON-пакет didChange в сервер
        Neuro_programm::self->sendLspRequest("textDocument/didChange", params);

        QFileInfo debugInfo(cleanPath);
        qDebug() << ">>> [LSP КЛИЕНТ] Пакет изменения текста отправлен после паузы. Версия:"
                 << this->lspDocumentVersion << "(" << debugInfo.fileName() << ") URI:" << textDocument["uri"].toString();
    });

    if (activeDelayTimer) {
        activeDelayTimer->start(350); // Ждем 350мс затухания дребезга клавиш
    }
}


void CodeEditor::applySelectionsFromLsp(const QList<QTextEdit::ExtraSelection> &selections)
{
    // Сохраняем ошибки локально
    m_currentLspSelections = selections;

    if (lineNumberArea) {
        lineNumberArea->update();
    }

    // БЕЗОПАСНАЯ ПОТОКОБЕЗОПАСНАЯ ОТПРАВКА СЧЁТЧИКА В ГЛАВНОЕ ОКНО
    if (Neuro_programm::self)
    {
        int totalErrors = selections.size();

        // Магия Qt: принудительно вызываем метод обновления текста статусбара
        // через очередь событий главного GUI-потока, пробивая любые blockSignals!
        QMetaObject::invokeMethod(Neuro_programm::self, "updateJediStatusTextFromLsp",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, totalErrors));

        // ЖЕЛЕЗНЫЙ ТРИГГЕР: Заставляем нативный статусбар перерисовать слой.
        // Это заставит Qt мгновенно обработать очередь событий и обновить ElidedLabel,
        // не дожидаясь, пока пользователь начнет вводить следующий символ.
        if (Neuro_programm::self->statusBar()) {
            Neuro_programm::self->statusBar()->repaint();
            Neuro_programm::self->statusBar()->update();
        }
    }
}



void CodeEditor::sendLspDidOpen()
{
    // Защита от пустых путей и nullptr синглтона главного окна
    if (this->currentFilePath.isEmpty()) return;
    if (!Neuro_programm::self) return;

    this->lspDocumentVersion = 1; // Начальная версия сессии для открываемого файла всегда 1

    QJsonObject params;
    QJsonObject textDocument;

// 1. ЖЕСТКАЯ НОРМАЛИЗАЦИЯ ПУТИ ДЛЯ LINUX (Защита от Windows разделителей \ в URI)
#include <QDir> // Убедитесь, что этот инклуд есть вверху файла codeeditor.cpp
#include <QPushButton>
    QString cleanPath = QDir::fromNativeSeparators(this->currentFilePath);
    textDocument["uri"] = QUrl::fromLocalFile(cleanPath).toString();

    textDocument["languageId"] = "python";
    textDocument["version"] = this->lspDocumentVersion;

    // 2. СИНТАКСИЧЕСКИЙ ФИКС СТРОК (Заменяем внутренний мусор Qt на стандартные переносы \n)
    QString pureText = this->toPlainText();
    pureText.replace(QString::fromUtf8("\xE2\x80\xA9"), "\n");

    // Передаем текст в QJsonObject НАПРЯМУЮ, без ручной склейки или кастомного экранирования.
    // QJsonDocument сам упакует её в валидный JSON-RPC формат.
    textDocument["text"] = pureText;

    params["textDocument"] = textDocument;

    // Отправляем чистый сформированный JSON-пакетdidOpen в сервер
    Neuro_programm::self->sendLspRequest("textDocument/didOpen", params);

    qDebug() << ">>> [LSP] Вызов didOpen выполнен успешно для файла:" << cleanPath;
}


void CodeEditor::showQuickFixMenu(const QList<QuickFixAction>& fixes)
{
    QList<QuickFixAction> finalFixes = fixes;

    QTextCursor cursor = this->textCursor();
    int currentLine = cursor.blockNumber();

    // =========================================================================
    // БРОНИРОВАННЫЙ ГЕНЕРАТОР: Извлекаем ошибки прямо из EXTRA SELECTIONS ЭКРАНА!
    // Это на 100% убирает гонку данных и рассинхрон с асинхронными массивами.
    // =========================================================================
    bool hasLineError = false;
    QString detectedMessage = "";

    // 1. Сначала ищем текстовое описание ошибки в глобальном массиве (запасной вариант)
    for (const auto& error : Neuro_programm::globalLspErrors) {
        if (error.line == currentLine) {
            hasLineError = true;
            detectedMessage = error.message.toLower();
            break;
        }
    }

    // 2. ГРАФИЧЕСКАЯ ПРОВЕРКА: Проверяем, нарисован ли на текущей строке красный маркер ошибки
    if (!hasLineError) {
        for (const auto& selection : m_currentLspSelections) {
            if (selection.cursor.blockNumber() == currentLine) {
                hasLineError = true;
                // Если текст ошибки не долетел, берем содержимое самой строки для контекста
                detectedMessage = selection.cursor.block().text().toLower();
                break;
            }
        }
    }

    // 3. ПРИНУДИТЕЛЬНО РАЗДЕЛЯЕМ МЕНЮ НА ОСНОВЕ АНАЛИЗА СТРОКИ
    if (hasLineError)
    {
        // Получаем чистый текст текущей строки для точного распознавания имени модуля
        QString lineText = cursor.block().text().trimmed();

        // СЛУЧАЙ А: Если строка начинается с "import " (неиспользуемый импорт)
        if (lineText.startsWith("import ") || lineText.startsWith("from ") ||
            detectedMessage.contains("unused") || detectedMessage.contains("imported but") ||
            detectedMessage.contains("не используется") || detectedMessage.contains("импортирован"))
        {
            // Вытаскиваем имя импортированного модуля (последнее слово в строке)
            QString moduleName = lineText.split(' ').last().remove(';').trimmed();
            if (moduleName.isEmpty() || moduleName.length() > 20) {
                moduleName = "библиотеки";
            }

            QuickFixAction customFix;
            customFix.title = "💡 Удалить неиспользуемый импорт [" + moduleName + "]";
            customFix.newText = ""; // Флаг для полного удаления строки
            customFix.startLine = currentLine;
            customFix.endLine = currentLine;

            finalFixes.prepend(customFix); // Выталкиваем в самый верх меню
        }
        // СЛУЧАЙ Б: Если это грубая опечатка или синтаксический мусор (ff fff, if True без двоеточия)
        else
        {
            QuickFixAction customFix;
            customFix.title = "💡 Очистить строку от синтаксического мусора";
            customFix.newText = "";
            customFix.startLine = currentLine;
            customFix.endLine = currentLine;

            finalFixes.prepend(customFix); // Выталкиваем в самый верх меню
        }
    }

    // =========================================================================
    // ОРИГИНАЛЬНЫЙ КОД ПОСТРОЕНИЯ QMENU И ЗАМЕНЫ ТЕКСТА (ОСТАЕТСЯ БЕЗ ИЗМЕНЕНИЙ)
    // =========================================================================
    if (finalFixes.isEmpty()) {
        if (Neuro_programm::self && Neuro_programm::self->statusBar()) {
            Neuro_programm::self->statusBar()->showMessage("Jedi: Доступных авто-исправлений нет", 3000);
        }
        return;
    }

    QMenu* menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu { background-color: #2a2a2a; color: #ffffff; border: 1px solid #555555; padding: 5px; font-size: 13px; }"
        "QMenu::item { padding: 6px 25px 6px 20px; border-radius: 3px; }"
        "QMenu::item:selected { background-color: #ff2a2a; color: #ffffff; }"
        );

    for (const auto& fix : finalFixes) {
        QString displayTitle = fix.title;
        if (!displayTitle.startsWith("💡")) {
            displayTitle = "⚙️ " + displayTitle; // Тяжелый рефакторинг от Rope помечаем шестеренкой
        }
        QAction* action = menu->addAction(displayTitle);
        QVariant data;
        data.setValue(fix);
        action->setData(data);
    }

    QPoint globalCursorPos = this->mapToGlobal(this->cursorRect().bottomLeft());
    QAction* selectedAction = menu->exec(globalCursorPos);

    if (selectedAction) {
        QuickFixAction fix = selectedAction->data().value<QuickFixAction>();
        QTextCursor editCursor = this->textCursor();

        // Наш кастомный С++ фикс удаления строки
        if (fix.newText.isEmpty() && fix.title.startsWith("💡"))
        {
            editCursor.movePosition(QTextCursor::StartOfBlock);
            editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editCursor.beginEditBlock();
            editCursor.removeSelectedText();
            editCursor.deleteChar(); // Схлопываем пустую строку
            editCursor.endEditBlock();
        }
        // Серверный WorkspaceEdit от Rope
        else if (!fix.newText.isEmpty())
        {
            QTextDocument* doc = this->document();
            QTextCursor lspCursor(doc);
            QTextBlock startBlock = doc->findBlockByLineNumber(fix.startLine);
            int startPos = startBlock.position() + fix.startChar;
            lspCursor.setPosition(startPos);

            QTextBlock endBlock = doc->findBlockByLineNumber(fix.endLine);
            int endPos = endBlock.position() + fix.endChar;
            lspCursor.setPosition(endPos, QTextCursor::KeepAnchor);

            lspCursor.beginEditBlock();
            lspCursor.insertText(fix.newText);
            lspCursor.endEditBlock();
        }
    }
    menu->deleteLater();
}

bool CodeEditor::event(QEvent *event)
{
    // СЛУШАТЕЛЬ ДВИЖЕНИЯ КУРСОРA: Прячем окно справки, если пользователь увёл мышь
    if (event->type() == QEvent::Leave || event->type() == QEvent::MouseMove)
    {
        QWidget* activeHover = this->property("currentHoverWidget").value<QWidget*>();
        if (activeHover) {
            activeHover->close();
            activeHover->deleteLater();
            this->setProperty("currentHoverWidget", QVariant());
        }
    }

    // Ловим событие запроса подсказки (наведение и удержание мыши)
    if (event->type() == QEvent::ToolTip)
    {
        // Если открыто окно автодополнения — не спамим подсказками, разгружаем поток
        if (m_popupWindow && m_popupWindow->isVisible()) {
            event->accept();
            return true;
        }

        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        QTextCursor cursor = this->cursorForPosition(helpEvent->pos()); // Вычисляем символ под мышкой
        int mouseLine = cursor.blockNumber();
        int mouseChar = cursor.columnNumber();

        // =========================================================================
        // ШАГ 1: ПРЯМОЙ ОФИЦИАЛЬНЫЙ ВЫВОД ОШИБОК ИЗ ГЛОБАЛЬНОГО МАССИВА СРЕДЫ
        // =========================================================================
        for (const auto& error : Neuro_programm::globalLspErrors)
        {
            // Проверяем, попала ли мышка на строку и символ, где Jedi зафиксировал ошибку
            if (error.line == mouseLine && mouseChar >= error.startChar && mouseChar <= error.endChar)
            {
                // Форматируем текст ошибки в красивый JetBrains/Breeze html-блок
                QString htmlTooltip = QString(
                                          "<div style='background-color: #232629; color: #eff0f1; padding: 8px; "
                                          "font-family: \"JetBrains Mono\", monospace; font-size: 12px; border: 1px solid #ef5350;'>"
                                          "<b style='color: #ef5350;'>Диагностика PyTorch Studio:</b><br/>"
                                          "<span>%1</span>"
                                          "</div>"
                                          ).arg(error.message.toHtmlEscaped());

                // Нативно и мгновенно выводим окно карточки в глобальных координатах мыши
                QToolTip::showText(helpEvent->globalPos(), htmlTooltip, this); //

                event->accept();
                return true; // Ошибка успешно выведена, выходим из метода!
            }
        }

        // =========================================================================
        // ШАГ 2: ЕСЛИ СИНТАКСИЧЕСКИХ ОШИБОК НЕТ — ШЛЕМ ХОВЕР-ЗАПРОС СВЕДЕНИЙ О МЕТОДЕ
        // =========================================================================
        if (Neuro_programm::self)
        {
            qint64 lastHoverTime = this->property("lastHoverSentTime").toLongLong();
                qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
                if (currentTime - lastHoverTime < 500) {
                    event->accept();
                return true;
            }
            this->setProperty("lastHoverSentTime", currentTime);
                this->setProperty("lastTooltipGlobalPos", helpEvent->globalPos());

                QJsonObject params;
                QJsonObject textDocument;
                QString realActivePath = this->objectName();
                if (realActivePath.isEmpty() && this->parentWidget()) {
                    realActivePath = this->parentWidget()->objectName();
            }
            if (realActivePath.isEmpty()) return QPlainTextEdit::event(event);

                textDocument["uri"] = QUrl::fromLocalFile(realActivePath).toString();
                params["textDocument"] = textDocument;

                QTextCursor wordCursor = cursor;
                wordCursor.select(QTextCursor::WordUnderCursor);
                QJsonObject position;
                position["line"] = mouseLine;

                int safeWordChar = wordCursor.selectionStart() - wordCursor.block().position();
                if (safeWordChar < 0) safeWordChar = mouseChar;
                position["character"] = safeWordChar;
                params["position"] = position;

                Neuro_programm::self->sendLspRequest("textDocument/hover", params, 888);
                event->accept();
            return true;
        }

        // ИСПРАВЛЕНО: Убрали безусловный QToolTip::hideText() отсюда, перенеся его под фильтр
    }

    // Для всех остальных системных событий возвращаем стандартное поведение Qt
    return QPlainTextEdit::event(event);
}


void CodeEditor::registerCompletionWidgets(QWidget* popup, QListWidget* list)
{
    // Принудительно сохраняем живые указатели из GUI-потока внутрь класса вкладки!
    this->m_popupWindow = popup;
    this->m_listWidget = list;

    // Сразу фиксируем стартовую позицию текстового курсора для фильтрации букв
    this->m_startPosition = this->textCursor().position();
}

// =========================================================================
// ВСПУМОГАТЕЛЬНЫЙ МЕТОД: Поиск индекса парной скобки с учетом вложенности
// =========================================================================
int CodeEditor::findMatchingBracket(int pos, QChar openBracket, QChar closeBracket, bool directionRight)
{
    QTextDocument *doc = this->document();
    int totalChars = doc->characterCount();
    int matchCount = 1; // Текущий уровень вложенности

    int step = directionRight ? 1 : -1;
    int currentPos = directionRight ? pos : pos - 2;

    while (currentPos >= 0 && currentPos < totalChars - 1)
    {
        QChar ch = doc->characterAt(currentPos);

        if (ch == openBracket) {
            directionRight ? matchCount++ : matchCount--;
        } else if (ch == closeBracket) {
            directionRight ? matchCount-- : matchCount++;
        }

        // Если счетчик вложенности обнулился — пара успешно найдена!
        if (matchCount == 0) {
            return directionRight ? currentPos : currentPos + 1;
        }

        currentPos += step;
    }
    return -1; // Пара не найдена (сломанный синтаксис)
}

void CodeEditor::matchBrackets()
{
    // Буфер для хранения графических выделений скобок
    QList<QTextEdit::ExtraSelection> bracketSelections;

    // Важно: мы не должны затирать маркеры ошибок синтаксиса от Jedi!
    // Поэтому мы берем текущие выделения, отфильтровываем старые скобки и сохраняем ошибки.
    QList<QTextEdit::ExtraSelection> currentSelections = this->extraSelections();
    for (const auto& selection : currentSelections) {
        // Ошибки синтаксиса у нас имеют WaveUnderline или фоновую заливку,
        // а у скобок формат будет строго точечный (Background/Outline).
        if (selection.format.underlineStyle() == QTextCharFormat::WaveUnderline ||
            selection.format.background().color() == QColor(255, 42, 42, 35))
        {
            bracketSelections.append(selection); // Сохраняем ошибки Jedi
        }
    }

    QTextCursor cursor = this->textCursor();
    int currentPos = cursor.position();
    QTextDocument *doc = this->document();

    // Проверяем символы слева и справа от текстовой каретки
    QChar charRight = (currentPos < doc->characterCount() - 1) ? doc->characterAt(currentPos) : QChar();
    QChar charLeft = (currentPos > 0) ? doc->characterAt(currentPos - 1) : QChar();

    int startBracketPos = -1;
    int endBracketPos = -1;
    QChar openChar, closeChar;
    bool directionRight = true;

    // Шаг 1: Определяем, стоит ли курсор рядом со скобкой
    if (charRight == '(' || charRight == '[' || charRight == '{') {
        startBracketPos = currentPos + 1;
        openChar = charRight;
        closeChar = (charRight == '(') ? ')' : (charRight == '[') ? ']' : '}';
        directionRight = true;
    } else if (charLeft == ')' || charLeft == ']' || charLeft == '}') {
        startBracketPos = currentPos;
        closeChar = charLeft;
        openChar = (charLeft == ')') ? '(' : (charLeft == ']') ? '[' : '{';
        directionRight = false;
    } else if (charLeft == '(' || charLeft == '[' || charLeft == '{') {
        startBracketPos = currentPos;
        openChar = charLeft;
        closeChar = (charLeft == '(') ? ')' : (charLeft == '[') ? ']' : '}';
        directionRight = true;
    } else if (charRight == ')' || charRight == ']' || charRight == '}') {
        startBracketPos = currentPos + 1;
        closeChar = charRight;
        openChar = (charRight == ')') ? '(' : (charRight == ']') ? '[' : '{';
        directionRight = false;
    }

    // Шаг 2: Если курсор наткнулся на скобку, запускаем сканер поиска пары
    if (startBracketPos != -1)
    {
        endBracketPos = findMatchingBracket(startBracketPos, openChar, closeChar, directionRight);

        if (endBracketPos != -1)
        {
            // Настраиваем красивый, контрастный стиль подсветки скобок (KDE Breeze Light стандарт)
            QTextEdit::ExtraSelection s1, s2;

            // Цвет подсветки: мягкий светло-зеленый фон с темно-зелеными буквами
            QColor bracketBg = QColor::fromRgb(255, 249, 196);
            QColor bracketBorder = QColor::fromRgb(251, 192, 45);

            s1.format.setBackground(bracketBg);
            s1.format.setProperty(QTextFormat::OutlinePen, QPen(bracketBorder, 1));
            s1.cursor = this->textCursor();
            s1.cursor.setPosition(startBracketPos - 1);
            s1.cursor.setPosition(startBracketPos, QTextCursor::KeepAnchor);

            s2.format.setBackground(bracketBg);
            s2.format.setProperty(QTextFormat::OutlinePen, QPen(bracketBorder, 1));
            s2.cursor = this->textCursor();
            s2.cursor.setPosition(endBracketPos - 1);
            s2.cursor.setPosition(endBracketPos, QTextCursor::KeepAnchor);

            bracketSelections.append(s1);
            bracketSelections.append(s2);
        }
    }

    // Принудительно отдаем обновленный массив selection графическому движку Qt
    // мы забираем уже существующую полосу строки и ошибки, и просто дописываем скобки в конец!
    //QList<QTextEdit::ExtraSelection> currentSelections = this->extraSelections();
    for (const auto& s : bracketSelections) {
        currentSelections.append(s);
    }

    // Принудительно отдаем обновленный массив графическому движку Qt
    this->setExtraSelections(currentSelections);
}

#include "codeeditor.h"
#include <QMenu>
#include <QAction>
#include <QTextCursor>
#include <QApplication>
#include <QClipboard>

void CodeEditor::showEditorContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
        "QMenu { background-color: #252526; color: #CCCCCC; border: 1px solid #3C3C3C; padding: 4px; }"
        "QMenu::item { padding: 4px 24px 4px 28px; }"
        "QMenu::item:selected { background-color: #094771; color: #FFFFFF; }"
        "QMenu::separator { height: 1px; background-color: #3C3C3C; margin: 4px 0px; }"
        );

    // Получаем доступ к базовым проверкам текста
    QTextCursor cursor = textCursor();
    bool hasSelection = cursor.hasSelection();

    // =========================================================================
    // БЛОК 1: ТИПОВЫЕ ОПЕРАЦИИ С PYTHON (Связка с venv Студии)
    // =========================================================================
    QAction *actRunFile = new QAction("▶ Запустить текущий файл в venv", &contextMenu);
    QAction *actCheckSyntax = new QAction("🔍 Проверить синтаксис (Flake8/Pylint)", &contextMenu);

    // Выделяем запуск жирным шрифтом как главное действие
    actRunFile->setFont(QFont(actRunFile->font().family(), -1, QFont::Bold));
    actRunFile->setShortcut(QKeySequence(Qt::Key_F5));

    contextMenu.addAction(actRunFile);
    contextMenu.addAction(actCheckSyntax);
    contextMenu.addSeparator();

    // =========================================================================
    // БЛОК 2: КОММЕНТИРОВАНИЕ КОДА И АВТООТСТУП (Рефакторинг текста)
    // =========================================================================
    QAction *actToggleComment = new QAction("# Закомментировать / Раскомментировать", &contextMenu);
    QAction *actAutoIndent = new QAction("📐 Исправить автоотступы (Форматировать код)", &contextMenu);

    actToggleComment->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash)); // Традиционный Ctrl + /
    actAutoIndent->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I));

    contextMenu.addAction(actToggleComment);
    contextMenu.addAction(actAutoIndent);
    contextMenu.addSeparator();

    // =========================================================================
    // БЛОК 3: ТИПОВОЕ СОДЕРЖИМОЕ МЕНЮ «ПРАВКА» (Стандартные буферы)
    // =========================================================================
    QAction *actUndo = new QAction("Отменить", &contextMenu);
    QAction *actRedo = new QAction("Повторить", &contextMenu);
    actUndo->setShortcut(QKeySequence::Undo);
    actRedo->setShortcut(QKeySequence::Redo);

    // Блокируем отмену/повтор, если история правок чиста
    actUndo->setEnabled(document()->isUndoAvailable());
    actRedo->setEnabled(document()->isRedoAvailable());

    contextMenu.addAction(actUndo);
    contextMenu.addAction(actRedo);
    contextMenu.addSeparator();

    QAction *actCut = new QAction("Cut (Вырезать)", &contextMenu);
    QAction *actCopy = new QAction("Copy (Копировать)", &contextMenu);
    QAction *actPaste = new QAction("Paste (Вставить)", &contextMenu);

    actCut->setShortcut(QKeySequence::Cut);
    actCopy->setShortcut(QKeySequence::Copy);
    actPaste->setShortcut(QKeySequence::Paste);

    // Блокируем вырезание/копирование, если текст не выделен курсором
    actCut->setEnabled(hasSelection);
    actCopy->setEnabled(hasSelection);
    actPaste->setEnabled(canPaste()); // Проверка Qt, есть ли текст в буфере обмена ОС

    contextMenu.addAction(actCut);
    contextMenu.addAction(actCopy);
    contextMenu.addAction(actPaste);

    contextMenu.addSeparator();
    QAction *actSelectAll = new QAction("Выделить всё", &contextMenu);
    actSelectAll->setShortcut(QKeySequence::SelectAll);
    contextMenu.addAction(actSelectAll);
    contextMenu.addSeparator();

    //блок4

    // Добавляем наш пункт форматирования
    QAction *formatAction = new QAction("Выполнить автоотступ (PEP8)", this);
    formatAction->setIcon(QIcon(":/Data/system_icons/format-text.svg"));

    // Активируем пункт только в том случае, если пользователь выделил кусок кода
    formatAction->setEnabled(this->textCursor().hasSelection());
    contextMenu.addAction(formatAction);

    // =========================================================================
    // ПРИВЯЗКА ЛОГИКИ К СИГНАЛАМ (Лямбда-коннекты)
    // =========================================================================
    connect(actRunFile, &QAction::triggered, this, &CodeEditor::onRunCurrentFileRequested);
    connect(actCheckSyntax, &QAction::triggered, this, &CodeEditor::onCheckSyntaxRequested);
    connect(actToggleComment, &QAction::triggered, this, &CodeEditor::onToggleCommentRequested);
    connect(actAutoIndent, &QAction::triggered, this, &CodeEditor::onAutoIndentRequested);
    connect(formatAction, &QAction::triggered, this, &CodeEditor::formatSelectedPythonCode);


    // Прямая адресация на встроенные слоты QPlainTextEdit для блока Правка
    connect(actUndo, &QAction::triggered, this, &CodeEditor::undo);
    connect(actRedo, &QAction::triggered, this, &CodeEditor::redo);
    connect(actCut, &QAction::triggered, this, &CodeEditor::cut);
    connect(actCopy, &QAction::triggered, this, &CodeEditor::copy);
    connect(actPaste, &QAction::triggered, this, &CodeEditor::paste);
    connect(actSelectAll, &QAction::triggered, this, &CodeEditor::selectAll);

    // Выводим меню на экран
    contextMenu.exec(mapToGlobal(pos));
}

// БЛОК 2: Умное комментирование выделенных строк (Ctrl + /)
void CodeEditor::onToggleCommentRequested()
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        // Если ничего не выделено — комментируем текущую одиночную строку, где стоит курсор
        cursor.select(QTextCursor::LineUnderCursor);
    }

    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();

    // Перемещаем курсор к началу выделения
    QTextCursor editCursor(document());
    editCursor.setPosition(start);
    editCursor.movePosition(QTextCursor::StartOfLine);

    // Открываем транзакцию отмены (чтобы Ctrl+Z отменял коммит сразу всей группы строк)
    editCursor.beginEditBlock();

    while (editCursor.position() < end) {
        editCursor.movePosition(QTextCursor::StartOfLine);
        QString lineText = editCursor.block().text();

        if (lineText.trimmed().startsWith("#")) {
            // Если строка уже закомментирована — снимаем комментарий
            int commentIdx = lineText.indexOf("#");
            editCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, commentIdx);
            editCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
            editCursor.removeSelectedText();
        } else {
            // Если комментария нет — вставляем символ решетки в начало строки
            editCursor.insertText("#");
        }

        // Переходим на следующую строчку
        if (!editCursor.movePosition(QTextCursor::NextBlock)) break;
    }

    editCursor.endEditBlock();
    setTextCursor(cursor); // Возвращаем выделение пользователя обратно
}

// БЛОК 2: Форматирование автоотступов (Замена табуляций на 4 пробела по PEP8)
void CodeEditor::onAutoIndentRequested()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();

    // Полное форматирование документа по стандарту Python PEP8
    QString currentText = toPlainText();
    currentText.replace("\t", "    "); // Заменяем жесткие табы на мягкие 4 пробела
    setPlainText(currentText);

    cursor.endEditBlock();
}

// БЛОК 1: Быстрый вызов Python-кода из редактора
void CodeEditor::onRunCurrentFileRequested()
{
    // Запрашиваем у главного окна через механизм инклюдов или сигналов
    // В данном случае, так как у вас есть переменная пути к открытому файлу:
    if (!this->currentFilePath.isEmpty()) {
        // Излучаем или вызываем метод запуска процесса, разработанный ранее
        // Предполагаем, что у вас настроен вызов глобальной функции Neuro_programm через родителя:
        // mainWin->onExecuteScriptRequested(this->currentFilePath);
    }
}

void CodeEditor::onCheckSyntaxRequested()
{
    // Здесь будет вызываться скрытый QProcess для "flake8 <currentFilePath>"
    // с выводом синтаксических предупреждений прямо на вашу панель panelOther
}

QWidget* CodeEditor::createEditorWithMinimap(QWidget *parent, CodeEditor* &outEditor, MinimapArea* &outMinimap)
{
    // 1. Создаем редактор кода на переданном родителе вкладки
    outEditor = new CodeEditor(parent);

    // 2. Сажаем миникарту ВНУТРЬ редактора кода (Это вернет ее на экран!)
    outMinimap = new MinimapArea(outEditor, outEditor);

    // Записываем карту во внутреннее приватное поле редактора
    outEditor->minimapArea = outMinimap;

    // Скрываем нативный тонкий скроллбар Qt, так как миникарта заменяет его функцию
    outEditor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Принудительно включаем отображение элементов
    outMinimap->show();
    outEditor->show();

    // 3. Настраиваем фильтр событий, чтобы редактор отдавал движения мыши карте
    outMinimap->installEventFilter(outEditor);

    // Связываем скроллбары напрямую для перемещения каретки
    QObject::connect(outEditor->verticalScrollBar(), &QScrollBar::valueChanged, outMinimap, [outMinimap]() {
        outMinimap->update();
    });
    QObject::connect(outEditor, &QPlainTextEdit::updateRequest, outMinimap, [outMinimap](const QRect &r, int dy) {
        Q_UNUSED(r); Q_UNUSED(dy);
        outMinimap->update();
    });

    return outEditor; // Возвращаем сам редактор, он займет всю рабочую область
}

bool CodeEditor::eventFilter(QObject *obj, QEvent *event)
{
    // =========================================================================
    // ЧАСТЬ 1: ИНТЕРАКТИВНЫЙ ВЫБОР ФУНКЦИИ ИЗ СПИСКА КЛИКОМ МЫШИ
    // =========================================================================
    if (m_listWidget && obj == m_listWidget)
    {
        // Ловим физическое нажатие левой кнопки мыши по элементу списка
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                // 1. Находим элемент списка, по которому пришелся клик
                QListWidgetItem *clickedItem = m_listWidget->itemAt(mouseEvent->pos());
                if (clickedItem)
                {
                    m_listWidget->setCurrentItem(clickedItem);
                        QString itemText = clickedItem->text();

                        // Очищаем HTML-теги, возвращая чистое имя метода (например, "print")
                        if (itemText.contains("<font") || itemText.contains("<span")) {
                        static const QRegularExpression htmlRegex("<[^>]*>");
                            itemText.remove(htmlRegex);
                    }

                    // 2. Алгоритм удаления префикса и вставки слова под корень
                    QTextCursor tc = this->textCursor();
                        QString lineText = tc.block().text().left(tc.columnNumber());
                        int lastDot = lineText.lastIndexOf('.');
                        int charsToErase = 0;

                    if (lastDot != -1) {
                        charsToErase = lineText.length() - (lastDot + 1);
                    } else {
                        static const QRegularExpression wordRegex("[a-zA-Z0-9_]+$");
                            QRegularExpressionMatch match = wordRegex.match(lineText);
                            if (match.hasMatch()) charsToErase = match.captured(0).length();
                    }

                    tc.beginEditBlock();
                        if (charsToErase > 0) {
                        tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, charsToErase);
                            tc.removeSelectedText();
                    }
                    tc.insertText(itemText); // Штампуем выбранный метод в код
                    tc.endEditBlock();
                        this->setTextCursor(tc);
                }

                // Закрываем поп-ап и возвращаем клавиатуру редактору
                if (m_popupWindow) m_popupWindow->hide();
                    this->setFocus();
                    return true; // Поглощаем событие клика
            }
        }
    }
    // =========================================================================
    // ЧАСТЬ 2: ПРОКРУТКА КОЛЕСИКОМ И ЛОГИКА ИНТЕРАКТИВНОЙ МИНИКАРТЫ
    // =========================================================================
    if (m_listWidget && obj == m_listWidget)
    {
        // Перехватываем скролл колесика мыши над окном подсказок
        if (event->type() == QEvent::Wheel)
        {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            int numDegrees = wheelEvent->angleDelta().y() / 8;
                int numSteps = numDegrees / 15;

                // Сдвигаем синее выделение строки вверх или вниз
                int currentRow = m_listWidget->currentRow();
                int nextRow = currentRow - numSteps;

                if (nextRow >= 0 && nextRow < m_listWidget->count()) {
                    m_listWidget->setCurrentRow(nextRow);
                    if (m_listWidget->item(nextRow)) m_listWidget->setCurrentItem(m_listWidget->item(nextRow));
            }
            return true; // Блокируем стандартный скрытый скроллбар Qt
        }
    }

    // ЛОГИКА МИНИКАРТЫ: Обработка движений мыши и прорисовка лупы (Ваш родной код)
    if (minimapArea != nullptr && obj == minimapArea)
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                this->handleMouseMoveFromEditor(mouseEvent->pos());
                if (mouseEvent->buttons() & Qt::LeftButton) {
                    double clickPercent = static_cast<double>(mouseEvent->pos().y()) / minimapArea->height();
                    int maxScroll = this->verticalScrollBar()->maximum();
                    this->verticalScrollBar()->setValue(static_cast<int>(clickPercent * maxScroll));
            }
            return true;
        }
        else if (event->type() == QEvent::Leave)
        {
            this->handleMouseLeaveFromEditor();
                return true;
        }
    }
    // =========================================================================
    // ЧАСТЬ 3: БЕЗОПАСНАЯ ЛОГИКА ЧАТА И КАНOНИЧНЫЙ ВЫХОД ИЗ ФИЛЬТРА
    // =========================================================================
    if (obj != nullptr && obj->objectName() == "inputChatText")
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
                // Отправка сообщения по комбинации Ctrl + Enter (Ваш родной код)
                if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                    (keyEvent->modifiers() & Qt::ControlModifier))
            {
                QMetaObject::invokeMethod(this->window(), "sendChatMessageToAI", Qt::QueuedConnection);
                    return true;
            }
        }
    }

    // Для всех базовых событий редактора возвращаем стандартное поведение Qt
    return QPlainTextEdit::eventFilter(obj, event);
}


// =========================================================================
// ФИКС ЛИНКОВЩИКА: ДОБАВЛЯЕМ ФИЗИЧЕСКИЕ МЕТОДЫ УПРАВЛЕНИЯ ЛУПОЙ В CODEEDITOR
// =========================================================================

void CodeEditor::handleMouseMoveFromEditor(const QPoint &pos)
{
    // Записываем параметры в динамические свойства редактора, чтобы их прочитал paintEvent
    this->setProperty("minimapMousePos", pos);
    this->setProperty("showMinimapLens", true);

    // Принудительно вызываем мгновенный paintEvent РЕДАКТОРА для отрисовки большой лупы
    this->update();
}

void CodeEditor::handleMouseLeaveFromEditor()
{
    // Тушим лупу на экране
    this->setProperty("showMinimapLens", false);
    this->update();
}

void CodeEditor::setChangesAsSaved()
{
    // Пробегаемся по абсолютно всем строкам документа сверху вниз
    QTextBlock block = document()->begin();
    while (block.isValid())
    {
        FolderBlockData *data = static_cast<FolderBlockData*>(block.userData());

        // Если строка была изменена (была красной) — переводим её в состояние Saved (зеленая)
        if (data && data->changeState == FolderBlockData::Modified) {
            data->changeState = FolderBlockData::Saved;
        }

        block = block.next();
    }

    // Обновляем панель номеров строк, перекрашивая маркеры в зеленый
    if (lineNumberArea) {
        lineNumberArea->update();
    }
}

void CodeEditor::mousePressEvent(QMouseEvent *e)
{
    // МГНОВЕННЫЙ ФИКС: Если поп-ап горит на экране — прячем его при любом клике по тексту
    if (m_popupWindow && m_popupWindow->isVisible()) {
        m_popupWindow->hide();
    }

    bool isZPressed = this->property("isKeyZPressed").toBool();

    if (isZPressed) {
        QTextCursor clickedCursor = cursorForPosition(e->pos());
        if (!clickedCursor.isNull()) {
            int clickedPos = clickedCursor.position();
            int mainPos = textCursor().position();

            // ЗАЩИТА 1: Если кликнули туда же, где стоит главный системный курсор,
            // игнорируем добавление, чтобы не создавать дубликат каретки!
            if (clickedPos == mainPos) {
                e->accept();
                return;
            }

            // ЗАЩИТА 2: Проверяем, нет ли уже виртуального курсора на этой позиции
            bool isDuplicate = false;
            for (const QTextCursor &vCursor : m_virtualCursors) {
                if (vCursor.position() == clickedPos) {
                    isDuplicate = true;
                    break;
                }
            }

            // Добавляем курсор только если он уникальный
            if (!isDuplicate) {
                m_virtualCursors.append(clickedCursor);
                updateVirtualCursorHighlights();
            }

            e->accept();
            return;
        }
    } else {
        // Обычный клик без зажатой Z полностью сбрасывает режим мультикурсоров
        if (!m_virtualCursors.isEmpty()) {
            m_virtualCursors.clear();
            updateVirtualCursorHighlights();
        }
    }

    // Передаем управление базовому классу только для обычных кликов
    QPlainTextEdit::mousePressEvent(e);
}

void CodeEditor::updateVirtualCursorHighlights()
{
    // Вызываем обновление экстра-выделений
    this->highlightCurrentLine();
}

void CodeEditor::keyReleaseEvent(QKeyEvent *e)
{
    // Сбрасываем флаг, когда Z отпущена
    if (e->key() == Qt::Key_Z) {
        this->setProperty("isKeyZPressed", false);
    }
    QPlainTextEdit::keyReleaseEvent(e);
}

void CodeEditor::showLspCompletionsInGui(const QStringList &completions)
{
    if (completions.isEmpty() || !m_listWidget) return;

    // 1. ВКЛЮЧАЕМ БЛОКИРОВКУ ОБНОВЛЕНИЙ И ОЧИЩАЕМ СТАРЫЙ КЭШ
    m_listWidget->setUpdatesEnabled(false);
    m_listWidget->clear();

    // Замеряем актуальный префикс прямо сейчас на экране (для torch.Linear или print)
    QTextCursor cursor = this->textCursor();
    QString leftOfCursor = cursor.block().text().left(cursor.columnNumber());
    int lastDot = leftOfCursor.lastIndexOf('.');

    QString currentPrefix = "";
    if (lastDot != -1) {
        currentPrefix = leftOfCursor.mid(lastDot + 1).toLower().trimmed();
    } else {
        static const QRegularExpression lastWordRegex("[a-zA-Z0-9_]+$");
        QRegularExpressionMatch match = lastWordRegex.match(leftOfCursor);
        if (match.hasMatch()) currentPrefix = match.captured(0).toLower().trimmed();
    }

    int addedCount = 0;

    // 2. ЗАПОЛНЯЕМ СПИСОК С ЖЕСТКОЙ ПРЕДВАРИТЕЛЬНОЙ ФИЛЬТРАЦИЕЙ И ПОДСВЕТКОЙ
    for (const QString &text : completions) {
        QString cleanText = text;
        // Очищаем сырую строку от потенциального старого HTML
        if (cleanText.contains("<")) {
            static const QRegularExpression htmlRegex("<[^>]*>");
            cleanText.remove(htmlRegex);
        }

        // КРИТИЧЕСКИЙ ФИКС АСИНХРОННОЙ ГОНКИ:
        // Если префикс на экране уже не пустой, мы добавляем в список ТОЛЬКО те команды,
        // которые действительно начинаются с этого префикса! Всё лишнее ("and", "if" при вводе "p") отсекаем.
        if (!currentPrefix.isEmpty() && !cleanText.toLower().startsWith(currentPrefix)) {
            continue; // Бесшумно пропускаем нерелевантную подсказку
        }

        QString finalHtml;
        // Динамически красим введенные буквы в голубой, а хвост - в белый цвет
        if (!currentPrefix.isEmpty() && cleanText.toLower().startsWith(currentPrefix)) {
            QString typedPart = cleanText.left(currentPrefix.length());
            QString restPart = cleanText.mid(currentPrefix.length());
            finalHtml = QString("<font color='#4cc3ff'><b>%1</b></font><font color='#eff0f1'>%2</font>")
                            .arg(typedPart, restPart);
        } else {
            finalHtml = QString("<font color='#eff0f1'>%1</font>").arg(cleanText);
        }

        new QListWidgetItem(finalHtml, m_listWidget);
        addedCount++;
    }

    // Включаем обратно отрисовку списка
    m_listWidget->setUpdatesEnabled(true);

    // СМАРТ-ОТКЛЮЧЕНИЕ: Если из-за жесткого фильтра ни одна команда не подошла —
    // мгновенно гасим окно, чтобы не показывать пустую рамку, и выходим.
    if (addedCount == 0) {
        if (m_popupWindow) m_popupWindow->hide();
        return;
    }

    // 3. СБОРКА И ПОЗИЦИОНИРОВАНИЕ ГЕОМЕТРИИ ЛИНЗЫ
    if (m_popupWindow) {
        // Учитываем viewport редактора (ширину номеров строк и фолдинга), чтобы окно не улетало
        QPoint localPos = this->cursorRect().bottomLeft();
        QPoint cursorPos = this->viewport()->mapToGlobal(localPos);

        // Сдвигаем на 3 пикселя вниз, чтобы не перекрывать текущую букву
        m_popupWindow->move(cursorPos.x(), cursorPos.y() + 3);

        // Настраиваем размер окна под количество подсказок (теперь считаем по реальному addedCount)
        int popupHeight = qMin(200, addedCount * 20 + 5);
        m_popupWindow->resize(250, popupHeight);

        // ЖЕЛЕЗНЫЙ UX ФИКС ФОКУСА: Окно рисуется поверх, но клавиатуру НЕ ворует!
        m_popupWindow->setAttribute(Qt::WA_ShowWithoutActivating, true);
        m_popupWindow->show();
        m_popupWindow->raise();

        // Насильно удерживаем фокус клавиатуры в самом текстовом поле редактора
        this->setFocus();

        // Синхронизируем синее выделение первой строки в отфильтрованном списке
        m_listWidget->setCurrentRow(0);
        if (m_listWidget->item(0)) {
            m_listWidget->setCurrentItem(m_listWidget->item(0));
            m_listWidget->item(0)->setSelected(true);
        }
    }
}

void CodeEditor::drawLspErrorsInGui(const QList<CodeEditor::LspErrorData> &errors)
{
    QList<QTextEdit::ExtraSelection> newSelections;

    for (const auto& error : errors) {
        QTextEdit::ExtraSelection selection;
        selection.format.setUnderlineColor(error.isError ? Qt::red : QColor(255, 165, 0));
        selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);

        QTextCursor cursor(this->document());
        QTextBlock block = this->document()->findBlockByLineNumber(error.line);

        if (block.isValid()) {
            int blockLen = block.length(); // Фактическая длина строки в символах

            int startChar = error.startChar;
            int endChar = error.endChar;

            // ЗАЩИТНЫЙ БАРЬЕР: Корректируем индексы под реальный размер строки, чтобы не было out of range!
            if (startChar >= blockLen) startChar = qMax(0, blockLen - 2);
            if (endChar > blockLen) endChar = blockLen - 1;
            if (endChar <= startChar) endChar = startChar + 1;

            int startPos = block.position() + startChar;
            int endPos = block.position() + endChar;

            cursor.setPosition(startPos);
            cursor.setPosition(endPos, QTextCursor::KeepAnchor);
            selection.cursor = cursor;
            newSelections.append(selection);
        }
    }

    // ИСПРАВЛЕНИЕ: Просто сохраняем новые ошибки в буфер класса БЕЗ блокировки сигналов
    // и БЕЗ вызова highlightCurrentLine(). Ошибки нарисуются штатно при следующем движении каретки!
    m_currentLspSelections = newSelections;

    // Вместо highlightCurrentLine() делаем легкое обновление только для панели номеров строк,
    // чтобы не дергать фокус ввода самого текста:
    if (lineNumberArea) lineNumberArea->update();
}

void CodeEditor::formatSelectedPythonCode()
{
    QTextCursor cursor = this->textCursor();

    // Если ничего не выделено — форматировать нечего
    if (!cursor.hasSelection()) return;

    // Запоминаем границы выделения
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    // Переводим операцию в единый транзакционный блок памяти (для Ctrl+Z)
    cursor.beginEditBlock();

    // Определяем базовый отступ (смотрим, сколько пробелов было у первой выделенной строки)
    cursor.setPosition(startPos);
    cursor.movePosition(QTextCursor::StartOfBlock);
    QString firstLine = cursor.block().text();
    int currentIndent = 0;
    for (const QChar &ch : firstLine) {
        if (ch == ' ') currentIndent++;
        else if (ch == '\t') currentIndent += 4;
        else break;
    }

    // Встаем на начало первой строки выделения
    cursor.setPosition(startPos);
    cursor.movePosition(QTextCursor::StartOfBlock);

    // Проходим циклом по всем строкам, попавшим в выделение
    while (cursor.position() <= endPos && !cursor.atEnd())
    {
        QString lineText = cursor.block().text();
        QString trimmedText = lineText.trimmed();

        // Пропускаем пустые строки, чтобы не забивать их пробелами
        if (!trimmedText.isEmpty())
        {
            // Специфика Python: если строка закрывает блок (например, else:, elif:, except:, finally:),
            // мы принудительно уменьшаем текущий отступ этой строки назад на 4 пробела
            if (trimmedText.startsWith("else:") || trimmedText.startsWith("elif ") ||
                trimmedText.startsWith("except ") || trimmedText.startsWith("finally:"))
            {
                currentIndent = qMax(0, currentIndent - 4);
            }

            // УДАЛЯЕМ старые неправильные пробелы и табы в начале текущей строки
            cursor.movePosition(QTextCursor::StartOfBlock);
            // Захватываем анкором всё расстояние до первого печатного символа
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, lineText.indexOf(trimmedText));
            cursor.removeSelectedText(); // Стираем мусорные пробелы

            // ВСТАВЛЯЕМ идеально ровный новый отступ по пробелам
            cursor.insertText(QString(currentIndent, ' ')); // ТЕПЕРЬ ТУТ ИСПОЛЬЗУЕТСЯ insertText()!

            // Если текущая строка оканчивается на двоеточие ':', значит следующий шаг цикла
            // (следующая строка) должна пойти с шагом вложенности +4 пробела
            if (trimmedText.endsWith(':')) {
                currentIndent += 4;
            }
        }

        // Сдвигаем курсор на следующую текстовую строчку (блок)
        if (!cursor.movePosition(QTextCursor::NextBlock)) {
            break; // Если дошли до самого конца документа — выходим
        }
    }

    // Закрываем блок редактирования
    cursor.endEditBlock();

    // Возвращаем фокус ввода в редактор, чтобы продолжить писать код
    this->setFocus();
}

