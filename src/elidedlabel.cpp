#include "elidedlabel.h"
#include <QPainter>
#include <QFontMetrics>

ElidedLabel::ElidedLabel(QWidget *parent) : QLabel(parent) {
    // Говорим системе компоновки, что мы можем сжиматься по горизонтали сколько угодно
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Задаем минимальную базовую ширину, меньше которой лейбл не сожмется (например, 150px)
    setMinimumWidth(150);
}

void ElidedLabel::setFullText(const QString &text) {
    m_fullText = text;
    update(); // Запускаем перерисовку
}

void ElidedLabel::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    QFontMetrics metrics(font());

    // Вычисляем доступную ширину с учетом внутренних отступов виджета
    int availableWidth = width() - contentsMargins().left() - contentsMargins().right();

    // Самая главная магия Qt: автоматически усекаем текст под доступную ширину
    QString elidedText = metrics.elidedText(m_fullText, Qt::ElideRight, availableWidth);

    // Рисуем усеченный (или полный, если он поместился) текст
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(rect(), alignment(), elidedText);
}
