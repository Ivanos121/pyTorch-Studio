#include "minimaparea.h"
#include "codeeditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>

MinimapArea::MinimapArea(CodeEditor *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_showLens(false)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
}

void MinimapArea::mousePressEvent(QMouseEvent *event)
{
    if (!m_editor || !m_editor->document()) return;
    double clickPercent = static_cast<double>(event->position().y()) / height();
    QScrollBar *sb = m_editor->verticalScrollBar();
    sb->setValue(static_cast<int>(clickPercent * sb->maximum()));
}

void MinimapArea::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_editor || !m_editor->document()) return;

    // Передаем координаты в родительский CodeEditor через публичный метод
    m_editor->handleMouseMoveFromEditor(event->pos());

    if (event->buttons() & Qt::LeftButton) {
        mousePressEvent(event); // Скроллинг при протаскивании зажатой мыши
    }
}

void MinimapArea::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (m_editor) {
        m_editor->handleMouseLeaveFromEditor();
    }
}

void MinimapArea::wheelEvent(QWheelEvent *event)
{
    if (!m_editor) return;
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;
    QScrollBar *sb = m_editor->verticalScrollBar();
    sb->setValue(sb->value() - (numSteps * 3));

    // При прокрутке колесиком обновляем координаты лупы под курсором
    m_editor->handleMouseMoveFromEditor(event->position().toPoint());
    event->accept();
}

void MinimapArea::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    if (!m_editor || !m_editor->document()) return;

    // 1. СИСТЕМНЫЙ ФОН ПАНЕЛИ МИНИКАРТЫ
    painter.fillRect(rect(), m_editor->palette().color(QPalette::Window));

    QScrollBar *scrollBar = m_editor->verticalScrollBar();
    double maxScroll = scrollBar->maximum();
    double currentScroll = scrollBar->value();
    double totalLines = m_editor->document()->blockCount();
    if (totalLines <= 0) return;

    double mapRowHeight = static_cast<double>(height()) / totalLines;

    // 2. ОТРИСОВКА МИНИ-КОДА (ПИКСЕЛЬНЫЕ СТРОКИ)
    painter.save();
    QFont miniFont = painter.font();
    miniFont.setPixelSize(2);
    painter.setFont(miniFont);
    painter.setPen(QColor("#7a7a7a")); // Классический серый цвет кода IDE

    QTextBlock block = m_editor->document()->begin();
    int blockNumber = 0;

    while (block.isValid()) {
        int mapY = static_cast<int>(blockNumber * mapRowHeight);
        if (mapY >= 0 && mapY <= height()) {
            QString text = block.text().trimmed();
            if (!text.isEmpty()) {
                painter.drawText(2, mapY + 2, text.left(25));
            }
        }
        block = block.next();
        blockNumber++;
    }
    painter.restore();

    // 3. ОТРИСОВКА КОМПАКТНОЙ КАРЕТКИ СЛАЙДЕРА (НА ВСЮ ШИРИНУ)
    int sliderHeight = 24;
    int sliderTop = 0;
    if (maxScroll > 0) {
        sliderTop = static_cast<int>((currentScroll / maxScroll) * (height() - sliderHeight));
    }

    QColor sliderColor = m_editor->palette().color(QPalette::Highlight);
    sliderColor.setAlpha(35);
    painter.fillRect(0, sliderTop, width(), sliderHeight, sliderColor);
    sliderColor.setAlpha(150);
    painter.setPen(QPen(sliderColor, 1));
    painter.drawRect(0, sliderTop, width() - 1, sliderHeight - 1);
}
