#include "statusbuttonstyle.h"
#include <QPushButton> // ОБЯЗАТЕЛЬНО добавьте этот инклуд в самый верх файла!

// ВОТ ЭТА СЕКЦИЯ ОБЯЗАТЕЛЬНО ДОЛЖНА БЫТЬ ТУТ:
StatusButtonStyle::StatusButtonStyle(QStyle *baseStyle)
    : QProxyStyle(baseStyle)
{
}

void StatusButtonStyle::drawControl(ControlElement element,
                                    const QStyleOption *option,
                                    QPainter *painter,
                                    const QWidget *widget) const
{
    if (element == CE_PushButton) {
        if (const QStyleOptionButton *btnOpt = qstyleoption_cast<const QStyleOptionButton *>(option)) {

            painter->save();

            // ХИТРОСТЬ: Проверяем реальное состояние "нажата" напрямую у виджета,
            // так как операционная система затирает флаг State_On в настройках btnOpt
            bool isBtnChecked = false;
            if (widget) {
                if (const QPushButton *btn = qobject_cast<const QPushButton*>(widget)) {
                    isBtnChecked = btn->isChecked();
                }
            }

            QColor bgColor;
            // 1. Если кнопка реально зафиксирована в программе (Первый щелчок)
            if (isBtnChecked) {
                bgColor = QColor("#121212"); // Максимально темный
            }
            // 2. Если на кнопку просто наведен курсор мыши
            else if (btnOpt->state & QStyle::State_MouseOver) {
                bgColor = QColor("#212121"); // Полутемный (в половину темного)
            }
            // 3. Обычное базовое состояние кнопки (Отжата)
            else {
                bgColor = QColor("#2b2b2b"); // Светлый под цвет статусбара
            }

            // Рисуем фон
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(bgColor);
            painter->drawRect(btnOpt->rect);

            // Определяем цвет текста кнопки
            QPalette palette = btnOpt->palette;
            if (isBtnChecked) {
                palette.setColor(QPalette::ButtonText, QColor("#007acc")); // Синеватый/яркий при активации
            } else {
                palette.setColor(QPalette::ButtonText, QColor("#ffffff")); // Белый в обычном режиме
            }

            QStyleOptionButton textOpt = *btnOpt;
            textOpt.palette = palette;

            // Сдвигаем текст на 1 пиксель вниз для эффекта физического утапливания
            if (isBtnChecked) {
                textOpt.rect.translate(0, 1);
            }

            painter->restore();

            // Отрисовка родного текста поверх нашего фона
            QProxyStyle::drawControl(CE_PushButtonLabel, &textOpt, painter, widget);
            return;
        }
    }

    QProxyStyle::drawControl(element, option, painter, widget);
}
