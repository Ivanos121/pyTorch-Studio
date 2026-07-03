#include "stickyscrollarea.h"
#include "codeeditor.h"
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QTextLayout>
#include <QScrollBar>

StickyScrollArea::StickyScrollArea(CodeEditor *editor)
    : QWidget(editor), m_editor(editor)
{
    m_lineHeight = m_editor->fontMetrics().height();
    setFixedHeight(0); // Изначально скрыт, пока нет блоков

    // Устанавливаем фильтр событий на редактор, чтобы ловить скролл и изменение текста
    m_editor->installEventFilter(this);
    m_editor->viewport()->installEventFilter(this);

    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int){
        this->calculateStickyBlocks();
    });
}

bool StickyScrollArea::eventFilter(QObject *obj, QEvent *event)
{
    // Перехватываем сигналы отрисовки/скролла редактора для NOAA обновления липких строк
    if (obj == m_editor || obj == m_editor->viewport()) {
        if (event->type() == QEvent::Wheel ||
            event->type() == QEvent::KeyPress ||
            event->type() == QEvent::ContextMenu) // <-- Исправлено здесь
        {
            // Небольшая задержка, чтобы QPlainTextEdit успел обновить свой firstVisibleBlock
            metaObject()->invokeMethod(this, &StickyScrollArea::calculateStickyBlocks, Qt::QueuedConnection);
        }
    }
    return QWidget::eventFilter(obj, event);
}


void StickyScrollArea::updateGeometrySize()
{
    QRect cr = m_editor->contentsRect();

    // Используем методы ширины панелей из вашего codeeditor.h
    int lineNumWidth = m_editor->lineNumberAreaWidth();
    int foldWidth = m_editor->foldingAreaWidth();

    int leftOffset = cr.left() + lineNumWidth + foldWidth;
    int rightMargin = 70; // Ваша фиксированная ширина миникарты из resizeEvent

    setGeometry(QRect(leftOffset, cr.top(), cr.width() - leftOffset - rightMargin, height()));
}

void StickyScrollArea::calculateStickyBlocks()
{
    m_stickyBlocks.clear();
    m_lineHeight = m_editor->fontMetrics().height();

    QTextBlock firstVisible = m_editor->getFirstVisibleBlock();
    if (!firstVisible.isValid()) {
        setFixedHeight(0);
        return;
    }

    int currentIndentLevel = 100000;
    QTextBlock block = firstVisible.previous();

    // Поднимаемся вверх от первого видимого блока
    while (block.isValid()) {
        QString text = block.text();
        QString trimmed = text.trimmed();

        // Проверяем, является ли строка началом блока Python
        if (trimmed.startsWith("def ") || trimmed.startsWith("class ")) {

            // Считаем отступ этой строки (количество пробелов)
            int spaces = 0;
            for (int i = 0; i < text.length(); ++i) {
                if (text[i] == ' ') spaces++;
                else if (text[i] == '\t') spaces += 4;
                else break;
            }

            // Если отступ меньше текущего — это наш родительский блок!
            if (spaces < currentIndentLevel) {
                m_stickyBlocks.prepend(block);
                currentIndentLevel = spaces;

                if (currentIndentLevel == 0) break; // Глобальный уровень, выше идти некуда
            }
        }
        block = block.previous();
    }

    // Корректируем высоту панели
    int targetHeight = m_stickyBlocks.size() * m_lineHeight;
    if (height() != targetHeight) {
        setFixedHeight(targetHeight);
        updateGeometrySize();
    }
    update();
}


void StickyScrollArea::paintEvent(QPaintEvent *event)
{
    if (m_stickyBlocks.isEmpty()) return;

    QPainter painter(this);
    m_lineHeight = m_editor->fontMetrics().height();

    // --- 1. ВЫЧИСЛЕНИЕ ЭФФЕКТА ВЫТАЛКИВАНИЯ (PUSH EFFECT) ---
    int pushYOffset = 0; // Смещение вверх всей панели в пикселях

    // Нам интересен блок, который идет сразу за последним прилипшим
    QTextBlock lastStickyBlock = m_stickyBlocks.last();
    QTextBlock nextBlock = lastStickyBlock.next();

    if (nextBlock.isValid()) {
        // Используем новые безопасные геттеры вашего CodeEditor
        int nextBlockTop = m_editor->getBlockTop(nextBlock) + m_editor->getVerticalOffset();

        // Если следующий блок уже поджимает нашу Sticky-панель снизу
        int panelHeight = m_stickyBlocks.size() * m_lineHeight;
        if (nextBlockTop < panelHeight && nextBlockTop > panelHeight - m_lineHeight) {
            // Рассчитываем плавный сдвиг вверх
            pushYOffset = nextBlockTop - panelHeight;
        }
    }

    // --- 2. ОТРИСОВКА ФОНА ---
    QColor bgColor = m_editor->palette().color(QPalette::Base).darker(103);
    painter.fillRect(event->rect(), bgColor);

    // --- 3. НАСТРОЙКА ПЕРА ДЛЯ СИНТАКСИЧЕСКИХ ЛИНИЙ (INDENT GUIDES) ---
    QPen indentPen;
    indentPen.setColor(QColor("#b0b4bc")); // Насыщенный серый Breeze Light из вашего paintEvent
    indentPen.setWidth(1);
    indentPen.setStyle(Qt::SolidLine);

    int y = pushYOffset; // Начальная координата отрисовки с учетом выталкивания
    int xOffset = m_editor->getHorizontalOffset(); // Наш геттер смещения
    int lineNumberAreaWidth = m_editor->lineNumberAreaWidth();

    // --- 4. ОСНОВНОЙ ЦИКЛ ОТРИСОВКИ СТРОК И ЛИНИЙ ---
    for (const QTextBlock &block : m_stickyBlocks) {
        QTextLayout *layoutObj = block.layout();
        if (layoutObj && layoutObj->lineCount() > 0) {
            QTextLine textLine = layoutObj->lineAt(0);
            QString text = block.text();

            // А. Отрисовка вертикальных линий отступов (Indent Guides)
            int leadingSpaces = 0;
            for (char ch : text.toStdString()) {
                if (ch == ' ') leadingSpaces++;
                else if (ch == '\t') leadingSpaces += 4;
                else break;
            }
            int currentLevel = leadingSpaces / 4;

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, false); // Идеальная резкость линий в 1px
            painter.setPen(indentPen);

            // Рисуем линии родительских блоков внутри прилипшей строки
            for (int i = 1; i < currentLevel; ++i) {
                int targetCharIndex = (i - 1) * 4;
                qreal startTextX = textLine.cursorToX(targetCharIndex);

                // Вычисляем физический X. Так как координаты m_stickyScrollWidget уже
                // смещены относительно номеров строк, вычитаем их ширину для выравнивания текста
                int lineX = xOffset + static_cast<int>(startTextX) - lineNumberAreaWidth;
                lineX += 1; // Оптический микро-сдвиг

                // Рисуем отрезок строго в границах текущей прилипшей строки
                painter.drawLine(lineX, y, lineX, y + m_lineHeight);
            }
            painter.restore();

            // Б. Отрисовка подсвеченного текста блока (Breeze форматы синтаксиса)
            painter.save();
            painter.translate(xOffset - lineNumberAreaWidth, y);
            QList<QTextLayout::FormatRange> blockFormats = layoutObj->formats();
            layoutObj->draw(&painter, QPointF(0, 0), blockFormats);
            painter.restore();
        }
        y += m_lineHeight;
    }

    // --- 5. ФИНАЛЬНАЯ НИЖНЯЯ ГРАНИЦА ПАНЕЛИ ---
    painter.setPen(QColor("#b0b4bc"));
    // Линия тоже смещается вверх при выталкивании, чтобы переход был бесшовным
    int finalLineY = height() - 1 + pushYOffset;
    painter.drawLine(0, finalLineY, width(), finalLineY);
}

void StickyScrollArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_stickyBlocks.isEmpty()) {
        int clickedIndex = event->position().y() / m_lineHeight;
        if (clickedIndex >= 0 && clickedIndex < m_stickyBlocks.size()) {
            QTextBlock targetBlock = m_stickyBlocks[clickedIndex];

            // Перемещаем курсор к физическому объявлению функции/класса при клике
            QTextCursor cursor(targetBlock);
            m_editor->setTextCursor(cursor);
            m_editor->ensureCursorVisible();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
