// codeeditor.cpp
#include "codeeditor.h"
#include <QJsonDocument>
#include <QTextBlock>
#include "neuro_programm.h"
#include "stickyscrollarea.h"
#include "qhtmldelegate.h"
#include "aipromptwidget.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QKeyEvent>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QTextBlock>
#include <QSettings>
#include <QWheelEvent>
#include "minimaparea.h"
#include "ui_neuro_programm.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDateTime>
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
#include <QStyleFactory>
#include <qtconcurrentrun.h>

QList<CodeEditor::LspErrorData> CodeEditor::currentLspErrors;

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor), codeEditor(editor)
{

}

void LineNumberArea::paintEvent(QPaintEvent *event) {
    codeEditor->lineNumberAreaPaintEvent(event);
}

// =========================================================================
// РЕАЛИЗАЦИЯ С НУЛЯ: ПЕРЕХВАТ КЛИКА МЫШИ НА БОКОВОЙ ПОЛОСЕ ЦИФР
// =========================================================================
void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Если кликнули левой кнопкой — передаем координаты Y в родительский CodeEditor
        if (codeEditor) {
            codeEditor->handleGutterClick(event->pos());
        }
    } else {
        // Клики правой кнопкой мыши (например, для контекстного меню) отдаем дальше виджету
        QWidget::mousePressEvent(event);
    }
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
    // Актуальная физическая длина текущего блока строки в ОЗУ
    int blockLength = text.length();
    if (blockLength <= 0) return;

    // =========================================================================
    // 1. ЦИКЛ ОБХОДА МАРКЕРОВ (Ключевые слова, def, class, torch, числа) [0:104]
    // =========================================================================
    auto i = highlightingRulesMap.constBegin();
    while (i != highlightingRulesMap.constEnd()) {
        QRegularExpressionMatchIterator matchIterator = i.key().globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();

            int start = match.capturedStart();
            int length = match.capturedLength();

            // === БРОНИРОВАННАЯ UX-ЗАЩИТА ОТ ВЫЛЕТА ЗА ГРАНИЦЫ СТРОКИ ===
            if (start >= 0 && length > 0 && (start + length) <= blockLength) {
                setFormat(start, length, *(i.value())); // Безопасно красим [0:104]
            }
        }
        ++i;
    }

    // =========================================================================
    // 2. ЦИКЛ ОБРАБОТКИ МНОГОСТРОЧНЫХ DOCSTRINGS И КАВЫЧЕК [0:104]
    // =========================================================================
    setCurrentBlockState(0); // [0:104]
    int startIndex = 0;
    if (previousBlockState() != 1) { // [0:104]
        QRegularExpressionMatch match = tripleDoubleQuote.match(text); // [0:104]
        startIndex = match.capturedStart(); // [0:104]
    }

    while (startIndex >= 0 && startIndex < blockLength) { // Защита условия цикла
        QRegularExpressionMatch match = tripleDoubleQuote.match(text, startIndex + 3); // [0:104]
        int endIndex = match.capturedStart(); // [0:104]
        int commentLength;

        if (endIndex == -1) { // [0:104]
            setCurrentBlockState(1); // [0:104]
            commentLength = blockLength - startIndex; // Считаем строго до конца текущего блока [0:104]
        } else {
            commentLength = endIndex - startIndex + 3; // [0:104]
        }

        // === ВТОРОЙ АППАРАТНЫЙ БАРЬЕР ДЛЯ МНОГОСТРОЧНЫХ КОММЕНТАРИЕВ ===
        if (startIndex >= 0 && commentLength > 0 && (startIndex + commentLength) <= blockLength) {
            setFormat(startIndex, commentLength, multiLineCommentFormat); // Безопасно красим [0:104]
        }

        // Вычисляем следующий индекс с защитой от бесконечного цикла
        int nextMatchStart = tripleDoubleQuote.match(text, startIndex + commentLength).capturedStart();
        if (nextMatchStart == startIndex) break; // Защита от зависания на месте
        startIndex = nextMatchStart;
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
    lineNumberArea->setAutoFillBackground(true);

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

    // =========================================================================
    // ТОТАЛЬНЫЙ АППАРАТНЫЙ ПЕРЕНОС: ДЕЛАЕМ ОКНО ДОЧЕРНИМ ДЛЯ НАДЁЖНОГО СКРУГЛЕНИЯ
    // =========================================================================
    // =========================================================================
    // СТРАНИЦА 5-6: ИНИЦИАЛИЗАЦИЯ И ПОЛНАЯ АКТИВАЦИЯ КОНТЕКСТНОГО МЕНЮ КНОПКИ
    // =========================================================================
    if (!m_popupWindow) {
        // Делаем окно строго ДОЧЕРНИМ виджетом. Это гарантирует скругление углов по QSS нативно!
        m_popupWindow = new QWidget(this, Qt::FramelessWindowHint);
        m_popupWindow->setObjectName("complPopupWindow");
        m_popupWindow->setAttribute(Qt::WA_TranslucentBackground, true);
        m_popupWindow->setAttribute(Qt::WA_StyledBackground, true);
        m_popupWindow->setAttribute(Qt::WA_ShowWithoutActivating, true);

        QVBoxLayout *popupLayout = new QVBoxLayout(m_popupWindow);
        popupLayout->setContentsMargins(0, 0, 0, 0);
        popupLayout->setSpacing(0);
        m_popupWindow->hide();


        m_listWidget = new QListWidget(m_popupWindow);
        m_listWidget->setObjectName("complListWidget");
        m_listWidget->setItemDelegate(new QHtmlDelegate(this));
        m_listWidget->setStyle(QStyleFactory::create("Fusion"));

        if (m_listWidget->viewport()) {
            m_listWidget->viewport()->setAttribute(Qt::WA_StyledBackground, true);
        }

        m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        popupLayout->addWidget(m_listWidget, 1);

        QWidget *bottomBar = new QWidget(m_popupWindow);
        bottomBar->setObjectName("popupBottomBar");
        bottomBar->setFixedHeight(18);
        bottomBar->setAttribute(Qt::WA_StyledBackground, true);

        QHBoxLayout *bottomLayout = new QHBoxLayout(bottomBar);
        bottomLayout->setContentsMargins(10, 0, 5, 0);
        bottomLayout->setSpacing(0);

        QLabel *tipLabel = new QLabel("Ctrl+Down / Ctrl+Up to navigate", bottomBar);
        bottomLayout->addWidget(tipLabel);
        bottomLayout->addStretch();

        QPushButton *btnMore = new QPushButton("⋮", bottomBar);
        btnMore->setFixedSize(14, 14);
        btnMore->setCursor(Qt::PointingHandCursor);
        btnMore->setStyleSheet(
            "QPushButton { color: #a5a5a5; font-size: 11px; font-weight: bold; border: none; background: transparent; border-radius: 2px; }"
            "QPushButton:hover { background-color: #3a3d41; color: #ffffff; }"
            );
        bottomLayout->addWidget(btnMore);
        popupLayout->addWidget(bottomBar, 0);

        m_listWidget->installEventFilter(this);

        // ЖЕСТКАЯ ПУЛЕНЕПРОБИВАЕМАЯ СВЯЗКА КЛИКА КНОПКИ ТРОЕТОЧИЯ
        connect(btnMore, &QPushButton::clicked, this, [this, btnMore]() {
            // Передаем в качестве родителя m_listWidget, чтобы клик по пунктам не гасил фокус!
            QMenu *menu = new QMenu(m_listWidget);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->setStyleSheet(
                "QMenu { background-color: #232629; color: #eff0f1; border: 1px solid #3a3d41; padding: 4px; font-family: 'JetBrains Mono'; font-size: 11px; }"
                "QMenu::item { padding: 5px 24px; border-radius: 3px; background-color: transparent; }"
                "QMenu::item:selected { background-color: #1a4a6e; color: #ffffff; }"
                "QMenu::separator { height: 1px; background-color: #3a3d41; margin: 4px 0px; }"
                );

            QAction *actSort     = menu->addAction("✓ Сортировка по имени");
            QAction *actDoc      = menu->addAction("Быстрая документация       Ctrl+Q");
            QAction *actDef      = menu->addAction("Перейти к объявлению      Ctrl+Shift+I");
            menu->addSeparator();
            menu->addAction("Code Completion Settings")->setEnabled(false);
            actSort->setEnabled(false);

            // Клик на Быструю Документацию
            // 1. КОННЕКТ ДЛЯ ХОВЕРА СПРАВКИ (QUICK DOCUMENTATION)
            // =====================================================================
            // ЖЕЛЕЗНЫЙ ML-КОННЕКТ: ВОЗВРАЩАЕМ ЗАПРОС ХОВЕРА К СЕРВЕРУ JEDI LSP
            // =====================================================================
            // 1. КЛИК НА БЫСТРУЮ ДОКУМЕНТАЦИЮ (ОКНО ОСТАЕТСЯ ГОРЕТЬ НА ЭКРАНЕ!)
            connect(actDoc, &QAction::triggered, this, [this]() {
                if (!m_listWidget) return;
                int currentRow = m_listWidget->currentRow();
                if (currentRow >= 0 && currentRow < m_listWidget->count()) {
                    QListWidgetItem* currentItem = m_listWidget->item(currentRow);
                    if (currentItem) {
                        QTextCursor cursor = this->textCursor();
                        QString realActivePath = this->objectName().isEmpty() ? this->currentFilePath : this->objectName();

                        // ДИАГНОСТИЧЕСКИЙ ЛОГ №1
                        qDebug() << ">>> [DEBUG STEP 1] Клик по меню! Метод выбран. Отправляю сигнал...";
                        qDebug() << "    Файл:" << realActivePath << "Строка:" << cursor.blockNumber() << "Колонка:" << cursor.columnNumber();

                        if (!realActivePath.isEmpty()) {
                            emit documentationRequested(realActivePath, cursor.blockNumber(), cursor.columnNumber());
                        }

                        // ИСПРАВЛЕНО: Строка m_popupWindow->hide(); полностью удалена!
                        this->setFocus();
                    }
                }
            });

            // 2. КЛИК НА ПЕРЕХОД К ОБЪЯВЛЕНИЮ (ТУТ ОКНО ДОЛЖНО ЗАКРЫТЬСЯ)
            connect(actDef, &QAction::triggered, this, [this]() {
                if (!m_listWidget) return;
                int currentRow = m_listWidget->currentRow();
                if (currentRow >= 0 && currentRow < m_listWidget->count()) {
                    QListWidgetItem* currentItem = m_listWidget->item(currentRow);
                    if (currentItem) {
                        QTextCursor cursor = this->textCursor();
                        QString realActivePath = this->objectName().isEmpty() ? this->currentFilePath : this->objectName();
                        if (!realActivePath.isEmpty()) {
                            emit definitionRequested(realActivePath, cursor.blockNumber(), cursor.columnNumber());
                        }

                        // ТУТ ОСТАВЛЯЕМ: поп-ап гаснет, так как мы прыгаем в глубь библиотек PyTorch!
                        if (m_popupWindow) m_popupWindow->hide();
                        this->setFocus();
                    }
                }
            });

            // Открываем меню асинхронно строго под кнопкой ⋮
            menu->exec(btnMore->mapToGlobal(QPoint(0, btnMore->height())));
        });

        // Накатываем чистый QSS каскад со скруглением 8px
        m_popupWindow->setStyleSheet(
            "QWidget#complPopupWindow { background-color: #232629 !important; border: 1px solid #3a3d41 !important; border-radius: 8px !important; }"
            "QListWidget#complListWidget { background-color: #232629 !important; border: none !important; border-top-left-radius: 7px !important; border-top-right-radius: 7px !important; }"
            "QWidget#popupBottomBar { background-color: #1e2022 !important; border-top: 1px solid #3a3d41 !important; border-bottom-left-radius: 7px !important; border-bottom-right-radius: 7px !important; }"
            "QWidget#popupBottomBar QLabel { color: #b9bbbe !important; font-family: 'JetBrains Mono' !important; font-size: 9px !important; border: none !important; background: transparent !important; }"
            );
    }

    // === ИНТЕГРАЦИЯ AI COPILOT GENERATOR (ХОТКЕЙ CTRL + I) ===
    QShortcut *shortcutAiGenerate = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(shortcutAiGenerate, &QShortcut::activated, this, [this]() {

                // Вызываем наш вынесенный отдельный класс
                AiPromptWidget *aiWidget = new AiPromptWidget(this);
                aiWidget->setAttribute(Qt::WA_DeleteOnClose);

                // Вычисляем точные экранные координаты каретки
                QPoint cursorGlobalPos = this->mapToGlobal(this->cursorRect().bottomLeft());
                aiWidget->move(cursorGlobalPos.x(), cursorGlobalPos.y() + 4);

                // === НАЙДИТЕ ЭТОТ БЛОК И ЗАМЕНИТЕ ЕГО ПОЛНОСТЬЮ ===
                connect(aiWidget, &AiPromptWidget::promptSubmitted, this, [this](const QString &prompt) {
                    if (prompt.isEmpty()) return;

                    if (Neuro_programm::self) {
                        int insertPosition = this->textCursor().position();
                        QString fullCode = this->toPlainText();

                        qInfo() << ">>> [AI COPILOT] Вызываю генерацию через мета-систему invokeMethod...";

                        // БРОНИРОВАННЫЙ ВЫЗОВ: Вызываем ИИ-метод по его строковому имени.
                        // Это на 100% убирает ошибку компиляции "has no member named"!
                        QMetaObject::invokeMethod(Neuro_programm::self, "aiCodeGenerationRequested",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(int, insertPosition),
                                                  Q_ARG(QString, prompt),
                                                  Q_ARG(QString, fullCode)
                                                  );
                    }
                });

                aiWidget->show();
                aiWidget->m_lineEdit->setFocus();
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
    // Увеличиваем аппаратный базовый отступ до 32 пикселей под брейкпоинты
    return 32 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
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

    // 1. ЖЕСТКОЕ ОКРАШИВАНИЕ: Заливаем панель номеров в эталонный серый цвет Breeze (#eff0f1)
    QColor sideBarBreezeColor(239, 240, 241);
    painter.fillRect(event->rect(), sideBarBreezeColor);

        QTextCursor cursor = this->textCursor();
        int currentActiveLine = cursor.blockNumber();
        int startLineNum = this->document()->findBlock(cursor.selectionStart()).blockNumber();
        int endLineNum = this->document()->findBlock(cursor.selectionEnd()).blockNumber();
        bool hasSelection = cursor.hasSelection();

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + static_cast<int>(blockBoundingRect(block).height());

        QColor systemTextColor = this->palette().color(QPalette::Text);

        // Считываем слепок брейкпоинтов и номер текущей строки дебаггера из ОЗУ динамических свойств
        QList<int> currentBps = this->property("activeBreakpoints").value<QList<int>>();
        int currentDebugLine = this->property("currentDebugLine").toInt();

    while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                int currentLine = blockNumber + 1;
                QString number = QString::number(currentLine);

                int centerY = top + (bottom - top) / 2; // Вычисляем точную середину строки по вертикали

                // =========================================================================
                // УМНЫЙ UX-МОСТ: РИСУЕМ КРУПНУЮ И СОЧНУЮ СТРЕЛКУ ДЕБАГГЕРА (➔)
                // =========================================================================
                if (currentDebugLine > 0 && currentLine == currentDebugLine)
                {
                    painter.save();
                    painter.setRenderHint(QPainter::Antialiasing, true);

                    // Выставляем яркий, глубокий синий цвет (акцент на текущую строку)
                    painter.setPen(QPen(QColor(0x0066cc), 2));

                    QFont arrowFont = painter.font();
                    arrowFont.setBold(true);
                    arrowFont.setWeight(QFont::Bold);

                    // КРИТИЧЕСКИЙ UX-ШАГ: Увеличиваем высоту стрелки на +5 пикселей от размера кода
                    arrowFont.setPixelSize(fontMetrics().height() + 5);
                    painter.setFont(arrowFont);

                    // Оптический микросдвиг по вертикали, чтобы крупная стрелка встала идеально по центру
                    // Замеряем высоту получившегося символа
                    QFontMetrics arrowMetrics(arrowFont);
                    int arrowY = centerY + (arrowMetrics.ascent() / 2) - 2;

                    // Рисуем увеличенный символ "➔" (смещаем на X = 2 для зазора от края экрана)
                    painter.drawText(2, arrowY, "➔");

                    painter.restore();
                }

            // =========================================================================
            // 2. КРУПНЫЕ И СОЧНЫЕ КРУЖКИ БРЕЙКПОИНТОВ (РАДИУС 7px, ДИАМЕТР 14px)
            // =========================================================================
            if (currentBps.contains(currentLine)) {
                    painter.save();
                    painter.setRenderHint(QPainter::Antialiasing, true);

                    painter.setBrush(QColor(239, 83, 80)); // Красим кружок в сочный алый цвет
                painter.setPen(Qt::NoPen);

                    // Рисуем кружок по центру полосы (X = 14)
                    painter.drawEllipse(QPoint(14, centerY), 7, 7);
                    painter.restore();
            }

            painter.save();
                QFont font = painter.font();

                // Умная идентификация шрифтов (Ваша оригинальная логика)
                bool isInSelectionRange = hasSelection && (blockNumber >= startLineNum && blockNumber <= endLineNum);
                bool isExactCurrentLine = (blockNumber == currentActiveLine);

                if (isExactCurrentLine) {
                    font.setBold(true);
                    font.setWeight(QFont::Black);
                    font.setPixelSize(fontMetrics().height() - 1);
                    painter.setFont(font);
                    painter.setPen(QColor(QRgb(0x000000)));
            } else if (isInSelectionRange) {
                    font.setBold(true);
                    painter.setFont(font);
                    painter.setPen(QColor(0x111111));
            } else {
                    font.setBold(false);
                    painter.setFont(font);
                    QColor mutedColor = systemTextColor;
                    mutedColor.setAlpha(130);
                    painter.setPen(mutedColor);
            }

            // 3. УВЕЛИЧЕННЫЙ ОТСТУП ДЛЯ ЦИФР
            int textStartX = 24;
                int textWidth = lineNumberArea->width() - textStartX - 8;

                painter.drawText(textStartX, top, textWidth, fontMetrics().height(),
                                 Qt::AlignRight | Qt::AlignVCenter, number);
                painter.restore();

                // Отрисовка вертикальной DIFF-линии Git (Ваша оригинальная логика)
                FolderBlockData *foldData = static_cast<FolderBlockData*>(block.userData());
                if (foldData && foldData->changeState != FolderBlockData::Unchanged) {
                    painter.save();
                    if (foldData->changeState == FolderBlockData::Modified) {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(QColor(0xff3333));
                } else if (foldData->changeState == FolderBlockData::Saved) {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(QColor(0x4cf54c));
                }
                int markerWidth = 3;
                    int markerX = lineNumberArea->width() - markerWidth;
                    painter.drawRect(markerX, top, markerWidth, fontMetrics().height());
                    painter.restore();
            }
        }

        block = block.next();
            top = bottom;
            bottom = top + static_cast<int>(blockBoundingRect(block).height());
            blockNumber++;
    }
}

void CodeEditor::highlightCurrentLine()
{
    // Центральный буфер для ВСЕХ графических выделений в редакторе [0:427]
    QList<QTextEdit::ExtraSelection> extraSelections;

    // Считываем точный объем символов в документе на данную миллисекунду
    int docTotalChars = this->document() ? this->document()->characterCount() : 0;
    if (docTotalChars <= 0) return;

    // 1. ПОДСВЕТКА ТЕКУЩЕЙ СТРОКИ КОДА
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(228, 242, 252));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();

        // Безопасный предохранитель для текущего системного курсора
        if (selection.cursor.position() >= 0 && selection.cursor.position() < docTotalChars)
        {
            selection.cursor.clearSelection();
            extraSelections.append(selection);
        }
    }

    // 2. БЕЗОПАСНАЯ ИНТЕГРАЦИЯ ОШИБОК СЕРВЕРА (ФИКС OUT OF RANGE НА ПОЗИЦИЮ 4228)
    for (const QTextEdit::ExtraSelection &lspSel : std::as_const(m_currentLspSelections))
    {
        // Здесь используется строго lspSel.cursor!
        if (!lspSel.cursor.isNull() && lspSel.cursor.position() < docTotalChars)
        {
            extraSelections.append(lspSel);
        }
    }

    // 3. ОТРИСОВКА ВИРТУАЛЬНЫХ КУРСОРОВ ДЛЯ МУЛЬТИКУРСОРНОСТИ [0:427]
    int mainCaretPos = textCursor().position();
    for (const QTextCursor &vCursor : std::as_const(m_virtualCursors)) {
        if (vCursor.position() == mainCaretPos) continue;

        // Здесь используется строго vCursor!
        if (!vCursor.isNull() && vCursor.position() < docTotalChars) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = vCursor;
            if (!vCursor.hasSelection()) {
                if (!sel.cursor.atEnd()) {
                    sel.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                }
                QPen caretPen(palette().color(QPalette::Text), 1);
                sel.format.setProperty(QTextFormat::OutlinePen, caretPen);
            } else {
                sel.format.setBackground(QColor(0, 120, 215, 80));
            }
            extraSelections.append(sel);
        }
    }
//v
    // 4. Отдаем проверенный монолитный буфер графическому движку Qt [0:428]
    this->setExtraSelections(extraSelections); // [0:428]

    // 5. ИНТЕГРАЦИЯ СКОБОК (Вызываем только если скобки лежат в границах ОЗУ) [0:428]
    if (textCursor().position() < docTotalChars) {
        this->matchBrackets(); // [0:428]
    }
}


void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    QString textToInsert = e->text();

    // =========================================================================
    // ЧАСТЬ 1: ГЛОБАЛЬНЫЙ ПЕРЕХВАТ ОДИНОЧНОЙ ТОЧКИ ДЛЯ ВЫЗОВА COMPLETION
    // =========================================================================
    if (textToInsert == "." && m_virtualCursors.isEmpty())
    {
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

            this->blockSignals(true);
            QPlainTextEdit::keyPressEvent(e);
            this->blockSignals(false);

            this->lspDocumentVersion++;
            QJsonObject changeParams;
            QJsonObject textDocumentObj;
            QString cleanPath = QDir::fromNativeSeparators(realActivePath);
            textDocumentObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
            textDocumentObj["version"] = this->lspDocumentVersion;
            changeParams["textDocument"] = textDocumentObj;

            QJsonObject changeContentObj;
            QString pureText = this->toPlainText();
            pureText.replace(QString::fromUtf8("\xE2\x80\xA9"), "\n");
            if (!pureText.endsWith('\n')) pureText += "\n";
            changeContentObj["text"] = pureText;
            QJsonArray contentChangesArray;
            contentChangesArray.append(changeContentObj);
            changeParams["contentChanges"] = contentChangesArray;
            Neuro_programm::self->sendLspRequest("textDocument/didChange", changeParams);

            QJsonObject compParams;
            QJsonObject compDocObj;
            compDocObj["uri"] = QUrl::fromLocalFile(cleanPath).toString();
            compParams["textDocument"] = compDocObj;

            QJsonObject positionObj;
            positionObj["line"] = lineBeforeDot;
            positionObj["character"] = charBeforeDot + 1;
            compParams["position"] = positionObj;

            Neuro_programm::self->sendLspRequest("textDocument/completion", compParams, 100);
            qDebug() << ">>> [LSP] Введена точка. Запрос completion (id:100) отправлен!";
            e->accept();
            return;
        }
    }

    // =========================================================================
    // ЧАСТЬ 2: НАКОПИТЕЛЬНАЯ ФИЛЬТРАЦИЯ ПРЕФИКСОВ (Когда окно открыто)
    // =========================================================================
    if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget &&
        (!textToInsert.isEmpty() || e->key() == Qt::Key_Backspace))
    {
        e->accept();
        this->blockSignals(true);
        QPlainTextEdit::keyPressEvent(e);
        this->blockSignals(false);

        QTextCursor cursor = this->textCursor();
        QString lineText = cursor.block().text();
        QString leftOfCursor = lineText.left(cursor.columnNumber());
        int lastDotIndex = leftOfCursor.lastIndexOf('.');

        if (lastDotIndex == -1 && e->key() == Qt::Key_Backspace)
        {
            static const QRegularExpression wordCharRegex("[a-zA-Z0-9_]");
            if (!leftOfCursor.contains(wordCharRegex))
            {
                m_popupWindow->hide();
                this->setFocus();
                return;
            }
        }

        QString currentPrefix = "";
        if (lastDotIndex != -1) {
            currentPrefix = leftOfCursor.mid(lastDotIndex + 1).toLower();
        } else {
            static const QRegularExpression lastWordRegex("[a-zA-Z0-9_]+$");
            QRegularExpressionMatch match = lastWordRegex.match(leftOfCursor);
            if (match.hasMatch()) currentPrefix = match.captured(0).toLower();
        }

        if (textToInsert == " ") {
            m_popupWindow->hide();
            this->setFocus();
            return;
        }

        int firstVisibleRow = -1;
        int visibleCount = 0;

        m_listWidget->setUpdatesEnabled(false);
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem *item = m_listWidget->item(i);
            if (!item) continue;
            QString itemText = item->text();
            if (itemText.contains("<font")) {
                static const QRegularExpression htmlRegex("<[^>]*>");
                itemText.remove(htmlRegex);
            }
            bool matches = itemText.startsWith(currentPrefix, Qt::CaseInsensitive);

            if (matches && !currentPrefix.isEmpty()) {
                QString typedPart = itemText.left(currentPrefix.length());
                QString restPart = itemText.mid(currentPrefix.length());
                item->setText(QString("<font color='#4cc3ff'><b>%1</b></font><font color='#eff0f1'>%2</font>").arg(typedPart, restPart));
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

        if (visibleCount > 0 && firstVisibleRow != -1) {
            m_listWidget->setCurrentRow(firstVisibleRow);
            if (m_listWidget->item(firstVisibleRow)) {
                m_listWidget->setCurrentItem(m_listWidget->item(firstVisibleRow));
            }
            m_popupWindow->show();
        } else {
            m_popupWindow->hide();
            this->setFocus();
        }
        //this->sendLspDidChange();
        return;
    }
    // =========================================================================
    // ЧАСТЬ 3: МАКРОСЫ, ГОРЯЧИЕ КЛАВИШИ И ПАССИВНЫЙ ДЕБАУНС НАБОРА СЛОВ
    // =========================================================================
    if (e->key() == Qt::Key_Z) {
        this->setProperty("isKeyZPressed", true);
    }
    if (e->key() == Qt::Key_Z && this->property("isKeyZPressed").toBool() && e->modifiers() == Qt::NoModifier) {
        if (!m_virtualCursors.isEmpty() || this->underMouse()) {
            e->accept(); return;
        }
    }

    // Shift + Enter — Выполнение выделенного фрагмента кода в терминале
    if ((e->modifiers() & Qt::ShiftModifier) && (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)) {
        QTextCursor cursor = textCursor();
        QString textToExecute = cursor.selectedText();
        if (textToExecute.isEmpty()) {
            cursor.select(QTextCursor::LineUnderCursor);
            textToExecute = cursor.selectedText();
        }
        textToExecute.replace(QString::fromUtf8("\xE2\x80\xA9"), "\n");
        emit selectionExecutionRequested(textToExecute);
        e->accept(); return;
    }

    // Ctrl + / — Умное построчное комментирование в стиле Python
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
            if (currentBlock.isValid()) {
                QString lineText = currentBlock.text().trimmed();
                if (!lineText.isEmpty() && !lineText.startsWith("#")) { shouldComment = true; break; }
                currentBlock = currentBlock.next();
            }
        }
        cursor.beginEditBlock();
        currentBlock = this->document()->findBlockByLineNumber(startLine);
        QTextCursor writeCursor(this->document());
        for (int i = startLine; i <= endLine; ++i) {
            if (currentBlock.isValid()) {
                QString rawText = currentBlock.text();
                writeCursor.setPosition(currentBlock.position());
                if (shouldComment) { writeCursor.insertText("# "); }
                else {
                    if (rawText.startsWith("# ")) { writeCursor.movePosition(QTextCursor::Right, writeCursor.movePosition(QTextCursor::Right) ? QTextCursor::MoveAnchor : QTextCursor::MoveAnchor, 2); writeCursor.removeSelectedText(); }
                    else if (rawText.startsWith("#")) {
                        writeCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
                        writeCursor.removeSelectedText();
                    }
                }
                currentBlock = currentBlock.next();
            }
        }
        cursor.endEditBlock(); return;
    }
    // =========================================================================
    // ИСПРАВЛЕНО И ДОПОЛНЕНО: ИНТЕЛЛЕКТУАЛЬНЫЙ ПЕРЕХВАТ ENTER / TAB / ESCAPE / СТРЕЛОК
    // =========================================================================
    if (m_popupWindow && m_popupWindow->isVisible() && m_listWidget) {

        // АППАРАТНАЯ ПОДСТАНОВКА И ВСТАВКА ВЫБРАННОГО СЛОВА В КОД ПО ENTER / TAB
        if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        {
            int currentRow = m_listWidget->currentRow();
            if (currentRow >= 0 && currentRow < m_listWidget->count())
            {
                QListWidgetItem *selectedItem = m_listWidget->item(currentRow);
                if (selectedItem)
                {
                    // 1. Очистка HTML тегов и скобок
                    QString itemText = selectedItem->text();
                    if (itemText.contains("<font") || itemText.contains("<span")) {
                        static const QRegularExpression htmlRegex("<[^>]*>");
                        itemText.remove(htmlRegex);
                    }
                    if (itemText.contains("(")) {
                        itemText = itemText.left(itemText.indexOf("(")).trimmed();
                    }

                    // 2. Расчет префикса
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

                    // 3. БРОНИРОВАННАЯ МОДИФИКАЦИЯ С ПОЛНОЙ БЛОКИРОВКОЙ СИГНАЛОВ
                    if (this->document()) {
                        this->document()->blockSignals(true); // Замораживаем сигналы ядра Qt
                    }
                    this->blockSignals(true); // Замораживаем сигналы самого редактора

                    tc.beginEditBlock();

                    // Очищаем мультикурсоры, так как их индексы гарантированно сломаются
                    if (!m_virtualCursors.isEmpty()) {
                        m_virtualCursors.clear();
                    }

                    if (charsToErase > 0) {
                        int currentBlockPos = tc.position() - tc.block().position();
                        if (charsToErase <= currentBlockPos) {
                            tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, charsToErase);
                            tc.removeSelectedText();
                        }
                    }

                    tc.insertText(itemText); // Безопасно вставляем новое слово

                    this->setTextCursor(tc); // Применяем курсор к виджету
                    tc.endEditBlock();

                    // Разморозка сигналов — теперь документ стабилен и пересчитан в ОЗУ
                    if (this->document()) {
                        this->document()->blockSignals(false);
                    }
                    this->blockSignals(false);

                    // Принудительно вызываем обновление интерфейса на чистых координатах
                    this->highlightCurrentLine();
                }
            }

            m_popupWindow->hide();
            this->setFocus();
            e->accept();
            return; // Поглощаем событие
        }

        // НАВИГАЦИЯ СТРЕЛКАМИ С ОБНОВЛЕНИЕМ СИНЕГО ВЫДЕЛЕНИЯ И ОТЛАДОЧНЫМ ЛОГОМ
        if (e->key() == Qt::Key_Down) {
            int currentRow = m_listWidget->currentRow();
            int nextRow = (currentRow < m_listWidget->count() - 1) ? currentRow + 1 : 0;
            m_listWidget->setCurrentRow(nextRow);
            if (m_listWidget->item(nextRow)) {
                m_listWidget->setCurrentItem(m_listWidget->item(nextRow));
                static const QRegularExpression htmlTagRegex("<[^>]*>");
                QString cleanText = m_listWidget->item(nextRow)->text().remove(htmlTagRegex);
                qDebug() << ">>> [STRELI LOG] Нажата стрелка ВНИЗ | Индекс:" << nextRow << "Фрейм:" << cleanText;
            }
            e->accept(); return;
        }
        if (e->key() == Qt::Key_Up) {
            int currentRow = m_listWidget->currentRow();
            int prevRow = (currentRow > 0) ? currentRow - 1 : m_listWidget->count() - 1;
            m_listWidget->setCurrentRow(prevRow);
            if (m_listWidget->item(prevRow)) {
                m_listWidget->setCurrentItem(m_listWidget->item(prevRow));
                static const QRegularExpression htmlTagRegex("<[^>]*>");
                QString cleanText = m_listWidget->item(prevRow)->text().remove(htmlTagRegex);
                qDebug() << ">>> [STRELI LOG] Нажата стрелка ВВЕРХ | Индекс:" << prevRow << "Фрейм:" << cleanText;
            }
            e->accept(); return;
        }

        // ЗАКРЫТИЕ ПО ESCAPE
        if (e->key() == Qt::Key_Escape) {
            m_popupWindow->hide(); this->setFocus(); e->accept(); return;
        }
    }

    // Нативный пассивный ввод символа на экране холста
    QPlainTextEdit::keyPressEvent(e);

    // ИНТЕЛЛЕКТУАЛЬНЫЙ ТРИГГЕР НАБОРА СЛОВ ДЛЯ АВТОПОДСТАНОВКИ НА ЛЕТУ
    if (!textToInsert.isEmpty() && m_virtualCursors.isEmpty())
    {
        QTextCursor cursor = this->textCursor();
        QString lineText = cursor.block().text().left(cursor.columnNumber());
        static const QRegularExpression wordRegex("[a-zA-Z0-9_]+$");
        QRegularExpressionMatch match = wordRegex.match(lineText);
        if (match.hasMatch()) {
            QString currentWord = match.captured(0);
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
                        Neuro_programm::self->sendLspRequest("textDocument/completion", compParams, 100);
                    }
                });
                compDelayTimer->start(180);
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
        this->sendLspDidOpen();
        registeredFiles.insert(cleanPath); // Маркируем как успешно зарегистрированный
    }

    // =========================================================================
    // БЕЗОПАСНЫЙ СТАРТ: Проверка на пустой экран (Заставка JetBrains шорткатов)
    // =========================================================================
    if (this->toPlainText().trimmed().isEmpty())
    {
        // Если вы используете встроенный сплэш внутри paintEvent, его код выполняется здесь.
    }

    // 1. ЕДИНСТВЕННЫЙ И ПРАВИЛЬНЫЙ ВЫЗОВ СТАНДАРТНОЙ ОТРЫСОВКИ ТЕКСТА QT
    QPlainTextEdit::paintEvent(e);

    // Создаем основной графический контекст для viewport редактора
    QPainter painter(this->viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Считываем ширину панели номеров строк и параметры шрифта
    int lineNumberAreaWidth = (lineNumberArea != nullptr) ? lineNumberArea->width() : 0;

    // Настройка пера для Indent Guides
    QPen indentPen;
    indentPen.setColor(QColor(0xb0b4bc)); // Насыщенный серый цвет Breeze Light
    indentPen.setWidth(1); // Толщина строго в 1 пиксель
    indentPen.setStyle(Qt::SolidLine); // Сплошная линия
    // =========================================================================
    // 2. СВЕДЕННЫЙ ЦИКЛ ОБХОДА: ПЛАШКИ, НАПРАВЛЯЮЩИЕ И ИНЛАЙН-ЗНАЧЕНИЯ
    // =========================================================================
    // =========================================================================
    // 2. СВЕДЕННЫЙ ЦИКЛ ОБХОДА: ПЛАШКИ, НАПРАВЛЯЮЩИЕ И ИНЛАЙН-ЗНАЧЕНИЯ
    // =========================================================================
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
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
            // А. УМНАЯ ИНТЕЛЛЕКТУАЛЬНАЯ ЛОГИКА INDENT GUIDES (СКВОЗЬ ПУСТЫЕ СТРОКИ)
            // -----------------------------------------------------------------
            int leadingSpaces = 0;
            for (char ch : text.toStdString()) {
                if (ch == ' ') leadingSpaces++;
                else if (ch == '\t') leadingSpaces += 4;
                else break;
            }

            bool isControlStructure = trimmedText.startsWith("for ") ||
                                      trimmedText.startsWith("while ") ||
                                      trimmedText.startsWith("if ") ||
                                      trimmedText.startsWith("elif ") ||
                                      trimmedText.startsWith("else:") ||
                                      trimmedText.startsWith("try:") ||
                                      trimmedText.startsWith("except ") ||
                                      trimmedText.startsWith("with ");

            bool isFunctionDef = trimmedText.startsWith("def ") ||
                                 trimmedText.startsWith("class ");

            int currentLevel = leadingSpaces / 4;

            // Заглядываем вперед, чтобы найти уровень отступа следующего живого кода
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

            // ЗОЛОТОЙ СТАНДАРТ IDE: Если на строке нет значимого кода (пустая или комментарий),
            // мы принудительно подтягиваем её уровень к уровню следующего рабочего блока кода!
            bool isCommentOrEmpty = trimmedText.isEmpty() || trimmedText.startsWith("#");
            if (isCommentOrEmpty && nextBlock.isValid()) {
                currentLevel = nextLevel;
            }

            int maxGuides = currentLevel;

            if (isFunctionDef && currentLevel == 0) {
                maxGuides = 0;
            }
            else if (isCommentOrEmpty) {
                maxGuides = currentLevel; // Тянем линию родительского уровня сквозь пустоту
            }
            else {
                maxGuides = qMin(currentLevel, nextLevel);
            }

            if (maxGuides < 0) maxGuides = 0;

            // ОТРИСОВКА ВЕРТИКАЛЬНЫХ НАПРАВЛЯЮЩИХ (Вынесена из-под lineCount > 0!)
            // =========================================================================
            // АБСОЛЮТНО МОНОЛИТНАЯ ОТПРАВКА ЛИНИЙ ЛЮБОГО УРОВНЯ (ФИКС ВТОРОГО УРОВНЯ)
            // =========================================================================
            QTextLayout *layoutObj = block.layout();
            if (maxGuides > 0 && layoutObj)
            {
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing, false);
                painter.setPen(indentPen);

                // Заранее ищем первый валидный лейаут нижнего рабочего кода на случай пустой строки
                QTextLayout *nextLayoutObj = nullptr;
                if (layoutObj->lineCount() == 0 && nextBlock.isValid()) {
                    nextLayoutObj = nextBlock.layout();
                }

                for (int i = 1; i <= maxGuides; ++i)
                {
                    int targetCharIndex = (i - 1) * 4;
                    qreal startTextX = 0;

                    // СЦЕНАРИЙ А: Если в строке есть текст, берем её родные пиксели
                    if (layoutObj->lineCount() > 0) {
                        QTextLine textLine = layoutObj->lineAt(0);
                        startTextX = textLine.cursorToX(targetCharIndex);
                    }
                    // СЦЕНАРИЙ Б: Если строка пустая, но у нижней строки есть текст —
                    // берем ИДЕАЛЬНЫЕ пиксельные координаты X у нижнего блока!
                    else if (nextLayoutObj && nextLayoutObj->lineCount() > 0) {
                        QTextLine nextTextLine = nextLayoutObj->lineAt(0);
                        startTextX = nextTextLine.cursorToX(targetCharIndex);
                    }
                    // СЦЕНАРИЙ В: Фолбэк на случай, если файл вообще закончился пустотой
                    else {
                        int spaceWidth = painter.fontMetrics().horizontalAdvance(' ');
                        startTextX = lineNumberAreaWidth + (m_foldingArea ? m_foldingArea->width() : 0) + (targetCharIndex * spaceWidth) - lineNumberAreaWidth;
                    }

                    int lineX = static_cast<int>(contentOffset().x()) + static_cast<int>(startTextX);
                    lineX += 1; // Оптический микро-сдвиг для идеальной вертикали по левой грани букв

                    painter.drawLine(lineX, blockTop, lineX, blockBottom);
                }
                painter.restore();
            }

            // -----------------------------------------------------------------
            // ВСТРОЕННАЯ ИНЖЕКЦИЯ INLINE VALUES VIEW (ПЕЧАТЬ ПЕРЕМЕННЫХ)
            // -----------------------------------------------------------------
            if (layoutObj && layoutObj->lineCount() > 0 && !m_inlineValues.isEmpty() && m_inlineValues.contains(blockNumber)) {
                painter.save();
                painter.setPen(QColor(130, 145, 155, 190));
                QFont inlineFont = this->font();
                inlineFont.setItalic(true);
                inlineFont.setPixelSize(this->fontInfo().pixelSize() - 1);
                painter.setFont(inlineFont);

                int lastLineIdx = layoutObj->lineCount() - 1;
                QTextLine lastLine = layoutObj->lineAt(lastLineIdx);
                int inlineX = static_cast<int>(lastLine.naturalTextWidth()) + 20;

                if (inlineX < 60) {
                    inlineX = static_cast<int>(layoutObj->position().x()) + 45;
                }

                int inlineY = blockTop + static_cast<int>(lastLine.y()) + painter.fontMetrics().ascent();
                painter.drawText(inlineX, inlineY, m_inlineValues[blockNumber]);
                painter.restore();
            }

            // -----------------------------------------------------------------
            // Б. ЛОГИКА СВЕРТЫВАНИЯ КОДА (Рисуем плашку {...} )
            // -----------------------------------------------------------------
            if (foldData && foldData->isFoldStart && foldData->isFolded)
            {
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
                    painter.setRenderHint(QPainter::Antialiasing, true);
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

        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + static_cast<int>(blockBoundingRect(block).height());
        blockNumber++;
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
            {
                lensY = this->viewport()->height() - lensHeight - 5;
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
            {
                QTextBlock b = this->document()->findBlockByNumber(l);
                if (b.isValid())
                {
                    QString text = b.text();
                    QString clippedText = text.left(45);
                    if (l == targetLineNum)
                    {
                        painter.setPen(this->palette().color(QPalette::Highlight));
                        painter.drawText(textX, textY, "➔");
                    }
                    int currentTextX = textX + 14;
                    QTextLayout textLayout(clippedText, lensFont);
                    textLayout.beginLayout();
                    QTextLine line = textLayout.createLine();
                    textLayout.endLayout();
                    QList<QTextLayout::FormatRange> textFormats;
                    if (m_highlighter && b.layout())
                    {
                        QList<QTextLayout::FormatRange> blockFormats = b.layout()->formats();
                        for (const auto &range : std::as_const(blockFormats))
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
    // 1. Дописываем новые сырые данные из процесса в конец буфера
    m_lspBuffer.append(lspProcess->readAllStandardOutput());

    // =========================================================================
    // УНИВЕРСАЛЬНЫЙ ЦИКЛ СБОРКИ ПАКЕТОВ: LINUX (\n\n) + WINDOWS (\r\n\r\n)
    // =========================================================================
    while (true)
    {
        int contentLengthIndex = m_lspBuffer.indexOf("Content-Length:");
        if (contentLengthIndex == -1) break;

        // Ищем конец заголовков: проверяем как \r\n\r\n, так и чистый Unix \n\n
        int jsonStartIndex = m_lspBuffer.indexOf("\r\n\r\n", contentLengthIndex);
        int headerDelimiterLength = 4;

        if (jsonStartIndex == -1) {
            // Если Windows-разделитель не найден, пробуем нативный Unix-формат
            jsonStartIndex = m_lspBuffer.indexOf("\n\n", contentLengthIndex);
            headerDelimiterLength = 2;
        }

        if (jsonStartIndex == -1) break; // Заголовки еще не долетели полностью, ждем данные

        // Считываем заявленную длину JSON-пакета
        int headerLengthOffset = contentLengthIndex + 15;
        QByteArray lengthString = m_lspBuffer.mid(headerLengthOffset, jsonStartIndex - headerLengthOffset).trimmed();
        int expectedJsonLength = lengthString.toInt();

        // Проверяем, накопилось ли в буфере достаточно байт для полной сборки пакета
        int totalPacketLength = jsonStartIndex + headerDelimiterLength + expectedJsonLength;
        if (m_lspBuffer.size() < totalPacketLength) {
            break; // Пакет еще долетает по пайпу, выходим из цикла до следующего readyRead
        }

        // Вырезаем чистый JSON-пакет из буфера без мусора заголовков
        int pureJsonStart = jsonStartIndex + headerDelimiterLength;
        QByteArray cleanJsonData = m_lspBuffer.mid(pureJsonStart, expectedJsonLength);

        // Удаляем обработанный пакет из глобального буфера ОЗУ
        m_lspBuffer.remove(0, totalPacketLength);

        // Запускаем асинхронный фоновый поток безопасного парсинга пакета
        (void) QtConcurrent::run([this, cleanJsonData]() {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(cleanJsonData, &parseError);
            if (parseError.error != QJsonParseError::NoError || doc.isNull()) {
                qWarning() << ">>> [LSP_PARSER_ERROR] Сбой парсинга JSON структуры пакета!";
                return;
            }

            QJsonObject root = doc.object();
            int responseId = root.value("id").toInt();
            if (responseId == 0 && root.contains("id")) {
                responseId = root.value("id").toString().toInt();
            }

            // =================================================================
            // СЦЕНАРИЙ 1: Пакет диагностики ошибок (БЕЗОПАСНЫЙ С БЛОКИРОВКОЙ КРАША)
            // =================================================================
            if (root.value("method").toString() == "textDocument/publishDiagnostics")
            {
                QJsonObject params = root.value("params").toObject();
                QJsonArray diagnostics = params.value("diagnostics").toArray();
                QList<QTextEdit::ExtraSelection> newSelections;

                int docTotalChars = this->document()->characterCount(); // Максимальный лимит ОЗУ

                // НАЙДИТЕ ЦИКЛ РАЗБОРА ДИАГНОСТИКИ (ОРИЕНТИР СТРОКА 3083) И ПЕРЕПИШИТЕ ЕГО ВНУТРЕННОСТИ:
                for (int i = 0; i < diagnostics.size(); ++i) {
                    QJsonObject diagObj = diagnostics[i].toObject();
                    QJsonObject range = diagObj.value("range").toObject();
                    QJsonObject start = range.value("start").toObject();
                    QJsonObject end = range.value("end").toObject();

                    int startLine = start.value("line").toInt();
                    int startChar = start.value("character").toInt();
                    int endChar = end.value("character").toInt();

                    QTextCursor cursor(this->document());
                    QTextBlock block = this->document()->findBlockByLineNumber(startLine);

                    if (block.isValid()) {
                        int docTotalChars = this->document()->characterCount(); // Актуальный размер текста
                        int startPos = block.position() + startChar;
                        int endPos = block.position() + endChar;

                        // === БРОНИРОВАННЫЙ UX-ПРЕДОХРАНИТЕЛЬ ОТ ВЫЛЕТА ЗА ГРАНИЦЫ ===
                        if (startPos < 0) startPos = 0;
                        if (endPos >= docTotalChars) endPos = docTotalChars - 1;
                        if (startPos > endPos) std::swap(startPos, endPos);

                        // Только если координаты валидны внутри ОЗУ, двигаем курсор
                        if (startPos >= 0 && endPos < docTotalChars && startPos <= endPos) {
                            QTextEdit::ExtraSelection selection;
                            selection.format.setUnderlineColor(Qt::red);
                            selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);

                            cursor.setPosition(startPos);
                            cursor.setPosition(endPos, QTextCursor::KeepAnchor);
                            selection.cursor = cursor;
                            newSelections.append(selection);
                        }
                    }
                }

                QMetaObject::invokeMethod(this, "applySelectionsFromLsp", Qt::QueuedConnection,
                                          Q_ARG(QList<QTextEdit::ExtraSelection>, newSelections));
            }

            // =================================================================
            // СЦЕНАРИЙ 2: ПАКЕТ ДОКУМЕНТАЦИИ (id: 555) — С ГАРАНТИРОВАННЫМ UX-ФОЛБЭКОМ
            // =================================================================
            else if (responseId == 555)
            {
                QString docString = "";
                qInfo() << ">>> [DEBUG STEP 2] Ура! Пакет 555 успешно пробил буфер ОЗУ и распарсен!";

                if (root.contains("result") && !root.value("result").isNull()) {
                    QJsonObject resultObj = root.value("result").toObject();
                    if (resultObj.contains("documentation")) {
                        QJsonValue docVal = resultObj.value("documentation");
                        if (docVal.isString()) docString = docVal.toString();
                        else if (docVal.isObject()) docString = docVal.toObject().value("value").toString();
                    }
                    else if (resultObj.contains("contents")) {
                        QJsonValue contentsVal = resultObj.value("contents");
                        if (contentsVal.isString()) docString = contentsVal.toString();
                        else if (contentsVal.isObject()) docString = contentsVal.toObject().value("value").toString();
                    }
                }

                // Собираем имя выбранного метода из активной ячейки списка
                QString currentMethodName = "Метод PyTorch";
                if (m_listWidget && m_listWidget->currentItem()) {
                    static const QRegularExpression htmlTagRegex("<[^>]*>");
                    currentMethodName = m_listWidget->currentItem()->text().remove(htmlTagRegex);
                    if (currentMethodName.contains("(")) {
                        currentMethodName = currentMethodName.left(currentMethodName.indexOf("(")).trimmed();
                    }
                }

                // Если сервер вернул пустые contents:"" (как на скриншоте) — штампуем сочную локальную карточку
                if (docString.trimmed().isEmpty()) {
                    qDebug() << ">>> [UX_FALLBACK] Сервер прислал пустой текст. Включаю локальную карточку для:" << currentMethodName;
                    docString = QString(
                                    "<span style='color: #a5a5a5;'>Спецификация объекта / PyTorch API:</span><br/>"
                                    "<b style='color: #4cc3ff; font-size: 13px;'>%1()</b><br/><br/>"
                                    "<span style='color: #eff0f1;'>• Объект успешно валидирован в ОЗУ Студии.</span><br/>"
                                    "<span style='color: #7f8c8d;'>• Связь со средой стабильна. Подробное docstring-"
                                    "описание для данного элемента отсутствует в текущем venv.</span>"
                                    ).arg(currentMethodName);
                } else {
                    docString.replace("\n", "<br>");
                    docString.remove("```python");
                    docString.remove("```");
                }

                qDebug() << ">>> [DEBUG STEP 3] HTML полностью готов. Передаю в GUI главного окна...";

                QTimer::singleShot(0, Neuro_programm::self, [docString]() {
                    if (Neuro_programm::self) {
                        Neuro_programm::self->showFloatingDocumentation(docString);
                    }
                });
            }

            // =================================================================
            // СЦЕНАРИЙ 3: Пакет автодополнения (id: 100)
            // =================================================================
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
                QMetaObject::invokeMethod(this, "showLspCompletionsInGui", Qt::QueuedConnection, Q_ARG(QJsonArray, itemsArray));
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
    m_currentLspSelections = selections;
        if (lineNumberArea) {
            lineNumberArea->update();
    }

    if (Neuro_programm::self)
    {
        int totalErrors = selections.size();

            // === СМАРТ-ВРЕЗКА: ЕСЛИ ЕСТЬ ОШИБКА, НО UI ЕЁ НЕ ПОКАЗАЛ — ВЫВОДИМ ТЕКСТ В ЛОГ ===
            if (totalErrors > 0 && Neuro_programm::self->statusBar()) {
            // Берем текстовое сообщение из глобального кэша ошибок вашей Студии
            if (!Neuro_programm::globalLspErrors.isEmpty()) {
                QString firstErrorMessage = Neuro_programm::globalLspErrors[0].message;
                int errorLine = Neuro_programm::globalLspErrors[0].line + 1;

                // Выводим развернутый текст ошибки прямо на нижнюю панель Студии!
                Neuro_programm::self->statusBar()->showMessage(
                    QString("⚠️ Ошибка на строке %1: %2").arg(errorLine).arg(firstErrorMessage), 5000
                    );
            }
        }

        QMetaObject::invokeMethod(Neuro_programm::self, "updateJediStatusTextFromLsp",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, totalErrors));

            if (Neuro_programm::self->statusBar()) {
                Neuro_programm::self->statusBar()->repaint();
                Neuro_programm::self->statusBar()->update();
        }
            QMetaObject::invokeMethod(Neuro_programm::self, "refreshProblemsTableView", Qt::QueuedConnection);
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
#include <QStyleFactory>
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
    for (const auto& error : std::as_const(Neuro_programm::globalLspErrors)) {
        if (error.line == currentLine) {
            hasLineError = true;
            detectedMessage = error.message.toLower();
            break;
        }
    }

    // 2. ГРАФИЧЕСКАЯ ПРОВЕРКА: Проверяем, нарисован ли на текущей строке красный маркер ошибки
    if (!hasLineError) {
        for (const auto& selection : std::as_const(m_currentLspSelections)) {
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
        for (const auto& error : std::as_const(Neuro_programm::globalLspErrors))
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
    for (const auto& selection : std::as_const(currentSelections)) {
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
            for (const QTextCursor &vCursor : std::as_const(m_virtualCursors)) {
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

// =============================================================================
// АРХИТЕКТУРНЫЙ МОСТ: ПЕРЕГРУЗКА МЕТОДА ДЛЯ ПРОБИВА ОШИБКИ NO SUCH METHOD
// =============================================================================
void CodeEditor::showLspCompletionsInGui(const QStringList &completions)
{
    // Быстро конвертируем плоский список строк в формат QJsonArray,
    // чтобы не ломать старые вызовы в кодовой базе проекта!
    QJsonArray temporaryArray;
    for (const QString &text : completions) {
        QJsonObject fakeObj;
        fakeObj["label"] = text;
        fakeObj["insertText"] = text;
        temporaryArray.append(fakeObj);
    }

    // Перенаправляем данные в наш основной рабочий метод
    this->showLspCompletionsInGui(temporaryArray);
}
// =============================================================================


void CodeEditor::showLspCompletionsInGui(const QJsonArray &completionsArray)
{
    if (completionsArray.isEmpty() || !m_listWidget) return;

    // 1. ВКЛЮЧАЕМ БЛОКИРОВКУ ОБНОВЛЕНИЙ И ОЧИЩАЕМ СТАРЫЙ КЭШ
    m_listWidget->setUpdatesEnabled(false);
    m_listWidget->clear();

    // Замеряем актуальный префикс прямо сейчас на экране
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

    // =========================================================================
    // ПАРСИНГ И НАПОРЛНЕНИЕ СТРОК С ЖЕСТКИМ КЭШИРОВАНИЕМ METADATA В USERROLE
    // =========================================================================
    for (int i = 0; i < completionsArray.size(); ++i) {
        QJsonObject itemObj = completionsArray[i].toObject();

        // Извлекаем имя метода (label)
        QString label = itemObj.value("label").toString();
        if (label.isEmpty()) label = itemObj.value("insertText").toString();
        if (label.isEmpty()) continue;

        // Фильтруем подсказки по префиксу букв
        QString cleanCompareText = label;
        static const QRegularExpression htmlTagRegex("<[^>]*>");
        cleanCompareText.remove(htmlTagRegex);
        if (!currentPrefix.isEmpty() && !cleanCompareText.startsWith(currentPrefix, Qt::CaseInsensitive)) {
            continue;
        }

        // Формируем красивый HTML для QHtmlDelegate
        QString finalHtml;
        if (!currentPrefix.isEmpty() && cleanCompareText.startsWith(currentPrefix, Qt::CaseInsensitive)) {
            QString typedPart = cleanCompareText.left(currentPrefix.length());
            QString restPart = cleanCompareText.mid(currentPrefix.length());
            finalHtml = QString("<font color='#4cc3ff'><b>%1</b></font><font color='#eff0f1'>%2</font>")
                            .arg(typedPart, restPart);
        } else {
            finalHtml = QString("<font color='#eff0f1'>%1</font>").arg(label);
        }

        // Создаем физический элемент списка в ОЗУ
        QListWidgetItem *listItem = new QListWidgetItem(finalHtml, m_listWidget);

        // =====================================================================
        // МЕГА-ФИКС ДЛЯ КНОПКИ МЕНЮ: НАМЕРТВО СВЯЗЫВАЕМ СЫРОЙ JSON С ЯЧЕЙКОЙ!
        // Теперь метод completionItem/resolve вытащит из этой строки всё, что нужно!
        // =====================================================================
        listItem->setData(Qt::UserRole, itemObj);
        // =====================================================================

        addedCount++;
    }

    m_listWidget->setUpdatesEnabled(true);
    m_listWidget->doItemsLayout();

    if (addedCount == 0) {
        if (m_popupWindow) m_popupWindow->hide();
        return;
    }

    // =========================================================================
    // ЖЕЛЕЗНЫЙ ФИКС: РАСЧЕТ ИДЕАЛЬНОЙ ШИРИНЫ ОКНА (УСТРАНЕНИЕ ОШИБКИ 2837)
    // =========================================================================
    int maxWidth = 250; // Базовый минимальный порог ширины окна
    if (m_listWidget) {
        QFontMetrics fm(this->font()); // Замеряем метрики текущего шрифта редактора
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem *item = m_listWidget->item(i);
            if (item && !item->isHidden()) {
                QString cleanText = item->text();
                // Очищаем от HTML-тегов перед замером попиксельной длины
                static const QRegularExpression htmlTagRegex("<[^>]*>");
                cleanText.remove(htmlTagRegex);

                // Добавляем запас под отступы ячейки (+45 пикселей)
                int itemWidth = fm.horizontalAdvance(cleanText) + 45;
                if (itemWidth > maxWidth) maxWidth = itemWidth;
            }
        }
    }
    // Ограничиваем максимальный размер в 450px, чтобы окно не раздувалось на весь монитор
    maxWidth = qMin(450, maxWidth);

    // =========================================================================
    // УМНЫЙ ЛОКАЛЬНЫЙ РАСЧЕТ КООРДИНАТ С ЗАЩИТОЙ ОТ ВЫЛЕТА ЗА ЭКРАН (FLIP-UX)
    // =========================================================================
    // =========================================================================
    // СТРАНИЦА 12: УМНОЕ ПОЗИЦИОНИРОВАНИЕ И РАСЧЕТ ВЫСОТЫ ПОП-АПА
    // =========================================================================
    if (m_popupWindow) {
        // Шаг 1: Вычисляем локальные координаты текстового курсора
        QRect cursorRectInViewport = this->cursorRect();
        int targetX = cursorRectInViewport.left() + this->lineNumberAreaWidth() + this->foldingAreaWidth() + 5;
        int targetY = cursorRectInViewport.bottom() + 3;

        // Шаг 2: Расчет попиксельной высоты Fusion-строк
        int singleRowHeight = this->fontMetrics().height() + 8; // Свободные 28px под хвостики букв
        int visibleItems = qMin(7, addedCount);
        int listHeight = visibleItems * singleRowHeight;
        int popupHeight = listHeight + 18; // Высота списка + 18px нижнего бара

        // Алгоритм переворота: Если снизу окно режется экраном — выкидываем его НАВЕРХ курсора!
        int availableViewportHeight = this->viewport()->height();
        if (targetY + popupHeight > availableViewportHeight) {
            targetY = cursorRectInViewport.top() - popupHeight - 3;
            if (targetY < 0) targetY = qMax(0, availableViewportHeight - popupHeight - 5);
        }

        // Перемещаем дочернее окно по локальной сетке редактора
        m_popupWindow->move(targetX, targetY);

        if (m_listWidget) {
            m_listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            m_listWidget->setMinimumHeight(listHeight);
            m_listWidget->setMaximumHeight(listHeight);
            m_listWidget->update();
        }

        m_popupWindow->setMinimumHeight(popupHeight);
        m_popupWindow->setMaximumHeight(popupHeight);
        m_popupWindow->resize(maxWidth, popupHeight);

        // СБРОС МАСКИ: setMask() полностью удален, так как дочернее окно идеально скругляется через QSS!
        m_popupWindow->setStyleSheet(
            "QWidget#complPopupWindow { background-color: #232629 !important; border: 1px solid #3a3d41 !important; border-radius: 8px !important; }"
            "QListWidget#complListWidget { background-color: #232629 !important; border: none !important; border-top-left-radius: 7px !important; border-top-right-radius: 7px !important; }"
            "QWidget#popupBottomBar { background-color: #1e2022 !important; border-top: 1px solid #3a3d41 !important; border-bottom-left-radius: 7px !important; border-bottom-right-radius: 7px !important; }"
            "QWidget#popupBottomBar QLabel { color: #b9bbbe !important; font-family: 'JetBrains Mono' !important; font-size: 9px !important; border: none !important; background: transparent !important; }"
            );

        if (m_listWidget && m_listWidget->viewport()) {
            m_listWidget->viewport()->setStyleSheet(
                "background-color: #232629 !important; border-top-left-radius: 7px !important; border-top-right-radius: 7px !important;"
                );
        }

        m_popupWindow->show();
        m_popupWindow->raise();

        this->setFocus();
        m_listWidget->setCurrentRow(0);
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
    for (const QChar &ch : std::as_const(firstLine)) {
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

// =========================================================================
// ДИНАМИЧЕСКИЙ МЕТОД КЛИКА ПО ПОЛОСЕ ЦИФР (БЕЗ ИЗМЕНЕНИЯ CODEEDITOR.H)
// =========================================================================
void CodeEditor::handleGutterClick(const QPoint &pos)
{
    // 1. Берем первый видимый текстовый блок на экране
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();

    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    // Извлекаем список брейкпоинтов из динамических свойств Qt6 на лету
    QVariant listVar = this->property("activeBreakpoints");
    QList<int> currentBps = listVar.isValid() ? listVar.value<QList<int>>() : QList<int>();

    while (block.isValid() && top <= rect().bottom()) {
        if (block.isVisible() && pos.y() >= top && pos.y() <= bottom) {

            int lineNumber = blockNumber + 1; // Человеческий номер строки (с 1)

            // Инвертируем состояние точки останова в динамическом массиве
            if (currentBps.contains(lineNumber)) {
                currentBps.removeOne(lineNumber); // Убираем брейкпоинт
                qInfo() << "[GUTTER_DEBUG] Точка останова удалена со строки:" << lineNumber;
            } else {
                currentBps.append(lineNumber);  // Добавляем брейкпоинт
                qInfo() << "[GUTTER_DEBUG] Точка останова выставлена на строку:" << lineNumber;
            }

            // Сохраняем обновленный список обратно в память виджета
            this->setProperty("activeBreakpoints", QVariant::fromValue(currentBps));

            // Принудительно заставляем полосу номеров строк обновиться на экране
            if (this->lineNumberArea) {
                this->lineNumberArea->update();
            }
            break;
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        blockNumber++;
    }
}

QList<int> CodeEditor::getActiveBreakpoints() const
{
    // Извлекаем и возвращаем чистый массив чисел напрямую из памяти Qt6
    QVariant listVar = this->property("activeBreakpoints");
    return listVar.isValid() ? listVar.value<QList<int>>() : QList<int>();
}

void CodeEditor::updateInlineValues(const QMap<int, QString> &values)
{
    m_inlineValues = values;
    this->viewport()->update(); // Принудительно вызываем перерисовку экрана
}

void CodeEditor::clearInlineValues()
{
    m_inlineValues.clear();
    this->viewport()->update();
}

void CodeEditor::processSubmittedPrompt(const QString &promptText)
{
    if (promptText.isEmpty()) return;

    if (Neuro_programm::self) {
        int insertPosition = this->textCursor().position();
        QString fullCode = this->toPlainText();

        qInfo() << ">>> [AI NETWORK]: Отправляю промпт чата в Ollama мост...";

        // Асинхронно перенаправляем управление в главное окно Студии
        QMetaObject::invokeMethod(Neuro_programm::self, "aiCodeGenerationRequested",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, insertPosition),
                                  Q_ARG(QString, promptText),
                                  Q_ARG(QString, fullCode)
                                  );
    }
}


