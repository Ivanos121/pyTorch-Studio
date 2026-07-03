#include "editorplaceholder.h"

EditorPlaceholder::EditorPlaceholder(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void EditorPlaceholder::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // =========================================================================
    // 1. АВТО-ЦВЕТ ФОНА: Динамически берем базовый цвет окна из текущей темы Qt!
    // Если в конфиге включена светлая тема — фон станет светлым (#eff0f1 / #ffffff),
    // если переключите на темную — он автоматически перерисуется в темный.
    // =========================================================================
    QColor backgroundColor = this->palette().color(QPalette::Base);
    painter.fillRect(rect(), backgroundColor);

    // 2. НАСТРОЙКА ШРИФТОВ С ИСПОЛЬЗОВАНИЕМ СИСТЕМНОГО ЦВЕТА ТЕКСТА
    QColor textColor = this->palette().color(QPalette::Text);

    // Блеклый цвет для названий действий (с прозрачностью 55%)
    QColor hintColor = textColor;
    hintColor.setAlpha(140);

    QFont fontTitle;
    fontTitle.setFamily("Segoe UI");
    fontTitle.setPixelSize(14);

    QFont fontShortcut = fontTitle;
    fontShortcut.setBold(true); // Горячие клавиши выделяем жирным

    // =========================================================================
    // ВАРИАНТЫ ЗАПИСЕЙ, АДАПТИРОВАННЫЕ СТРОГО ПОД ИИ И ОБУЧЕНИЕ НЕЙРОСЕТЕЙ:
    // =========================================================================
    QList<QPair<QString, QString>> shortcuts = {
        {"Запустить обучение модели", "F5 / Запуск Debug"},
        {"ИИ-Ассистент (Генерация слоев PyTorch)", "Ctrl + Enter (в Чате)"},
        {"Быстрый поиск по коду тензоров", "Ctrl + F"},
        {"Комментирование строк кода Python", "Ctrl + /"},
        {"Менеджер пакетов (Установка torch/cuda)", "Панель в статусбаре"},
        {"Перетащите файлы скриптов .py сюда для открытия", ""}
    };

    int centerX = width() / 2;
    int centerY = height() / 2;

    // Вычисляем стартовую точку по вертикали, центрируя весь блок
    int currentY = centerY - (shortcuts.size() * 38 / 2);

    // 3. ПОСТРОЧНЫЙ ВЫВОД ТАБЛИЦЫ ПОДСКАЗОК
    for (const auto &pair : shortcuts)
    {
        if (pair.second.isEmpty()) {
            // Нижняя строка-инструкция (Перетащите файлы...)
            painter.setPen(hintColor);
            painter.setFont(fontTitle);
            painter.drawText(centerX - 250, currentY, 500, 30, Qt::AlignCenter, pair.first);
        }
        else {
            // Вывод названия ИИ-действия (Выравнивание по правому краю, слева от центра)
            painter.setPen(hintColor);
            painter.setFont(fontTitle);
            painter.drawText(centerX - 320, currentY, 300, 30, Qt::AlignRight | Qt::AlignVCenter, pair.first);

            // Вывод горячей клавиши (Выравнивание по левому краю, справа от центра)
            painter.setPen(textColor);
            painter.setFont(fontShortcut);
            painter.drawText(centerX + 20, currentY, 300, 30, Qt::AlignLeft | Qt::AlignVCenter, pair.second);
        }

        currentY += 38; // Шаг смещения на следующую строку вниз
    }
}
