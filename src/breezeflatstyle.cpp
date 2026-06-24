#include "breezeflatstyle.h"

using namespace Qt::Literals;

void BreezeFlatStyle::drawComplexControl(ComplexControl control,
                                         const QStyleOptionComplex *option,
                                         QPainter *painter,
                                         const QWidget *widget) const
{
    // 1. ПЕРЕХВАТЫВАЕМ QCOMBOBOX
    if (control == CC_ComboBox) {
        if (const QStyleOptionComboBox *cmbOpt = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Получаем область кнопки выпадающего списка (drop-down)
            QRect dropDownRect = proxy()->subControlRect(CC_ComboBox, cmbOpt, SC_ComboBoxArrow, widget);

            // Определяем цвет подложки стрелки (в цвет поля, либо серый при ховере)
            QColor btnColor = cmbOpt->palette.color(QPalette::Base);
            if (cmbOpt->state & QStyle::State_MouseOver && dropDownRect.contains(widget->mapFromGlobal(QCursor::pos()))) {
                // Если мышка наведена конкретно на стрелочку — делаем аккуратный плоский блик
                btnColor = (cmbOpt->state & QStyle::State_Enabled) ? QColor(224, 224, 224) : btnColor;
            }

            // Рисуем абсолютно плоский фон для кнопки
            painter->fillRect(dropDownRect, btnColor);

            // Вручную рисуем ровный плоский треугольник Breeze (Стрелка вниз)
            QPolygonF triangle;
            qreal cx = dropDownRect.center().x();
            qreal cy = dropDownRect.center().y();
            triangle << QPointF(cx - 4, cy - 2) << QPointF(cx + 4, cy - 2) << QPointF(cx, cy + 3);

            painter->setPen(Qt::NoPen);
            painter->setBrush(cmbOpt->palette.color(QPalette::Text)); // Цвет стрелки под цвет текста
            painter->drawPolygon(triangle);

            painter->restore();
            return; // Прерываем дефолтный Fusion рендеринг стрелки
        }
    }

    // 2. ПЕРЕХВАТЫВАЕМ QSPINBOX / QDOUBLESPINBOX
    if (control == CC_SpinBox) {
        if (const QStyleOptionSpinBox *spnOpt = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Рисуем базовое текстовое поле ввода
            QProxyStyle::drawComplexControl(CC_SpinBox, spnOpt, painter, widget);

            QRect upRect = proxy()->subControlRect(CC_SpinBox, spnOpt, SC_SpinBoxUp, widget);
            QRect downRect = proxy()->subControlRect(CC_SpinBox, spnOpt, SC_SpinBoxDown, widget);
            QPoint mousePos = widget->mapFromGlobal(QCursor::pos());

            // Закрашиваем области кнопок плоским цветом поля, стирая градиенты Fusion
            painter->fillRect(upRect, spnOpt->palette.color(QPalette::Base));
            painter->fillRect(downRect, spnOpt->palette.color(QPalette::Base));

            // Подсветка кнопок при наведении мыши
            if (spnOpt->state & QStyle::State_MouseOver) {
                if (upRect.contains(mousePos)) painter->fillRect(upRect, QColor(224, 224, 224));
                if (downRect.contains(mousePos)) painter->fillRect(downRect, QColor(224, 224, 224));
            }

            painter->setPen(Qt::NoPen);
            painter->setBrush(spnOpt->palette.color(QPalette::Text));

            // Рисуем плоскую стрелочку ВВЕРХ▲
            QPolygonF upTriangle;
            qreal ux = upRect.center().x(); qreal uy = upRect.center().y();
            upTriangle << QPointF(ux - 3.5, uy + 2) << QPointF(ux + 3.5, uy + 2) << QPointF(ux, uy - 2.5);
            painter->drawPolygon(upTriangle);

            // Рисуем плоскую стрелочку ВНИЗ▼
            QPolygonF downTriangle;
            qreal dx = downRect.center().x(); qreal dy = downRect.center().y();
            downTriangle << QPointF(dx - 3.5, dy - 2) << QPointF(dx + 3.5, dy - 2) << QPointF(dx, dy + 2.5);
            painter->drawPolygon(downTriangle);

            painter->restore();
            return; // Прерываем дефолтный Fusion
        }
    }

    // Для остальных элементов вызываем базу
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}
