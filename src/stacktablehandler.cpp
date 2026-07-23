#include "stacktablehandler.h"

StackTableHandler::StackTableHandler(QTableWidget *table, QObject *parent)
    : QObject(parent), m_table(table)
{
    if (!m_table) return;

    // Сконфигурируем внешний вид таблицы
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({"Уровень", "Функция", "Файл", "Строка", "Адрес"});

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);

    // Распределяем ширину колонок
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(ColLevel, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColFunction, QHeaderView::Stretch);
    header->setSectionResizeMode(ColFile, QHeaderView::Stretch);
    header->setSectionResizeMode(ColLine, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColAddress, QHeaderView::ResizeToContents);

    // Подключаем внутреннее событие клика к нашему слоту
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &StackTableHandler::onCellDoubleClicked);
}

void StackTableHandler::clear() {
    if (m_table) m_table->setRowCount(0);
}

void StackTableHandler::updateTable(const QList<StackFrame> &frames) {
    if (!m_table) return;

    m_table->setRowCount(0);

    for (const auto &frame : frames) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        QTableWidgetItem *itemLevel   = new QTableWidgetItem(QString::number(frame.level));
        QTableWidgetItem *itemFunc    = new QTableWidgetItem(frame.function);
        QTableWidgetItem *itemFile    = new QTableWidgetItem(frame.file);
        QTableWidgetItem *itemLine    = new QTableWidgetItem(QString::number(frame.line));
        QTableWidgetItem *itemAddress = new QTableWidgetItem(frame.address);

        itemLevel->setTextAlignment(Qt::AlignCenter);
        itemLine->setTextAlignment(Qt::AlignCenter);
        itemAddress->setTextAlignment(Qt::AlignCenter);

        // Прячем полные метаданные внутри ячейки пути файла
        itemFile->setData(Qt::UserRole, frame.file);
        itemFile->setData(Qt::UserRole + 1, frame.line);

        m_table->setItem(row, ColLevel, itemLevel);
        m_table->setItem(row, ColFunction, itemFunc);
        m_table->setItem(row, ColFile, itemFile);
        m_table->setItem(row, ColLine, itemLine);
        m_table->setItem(row, ColAddress, itemAddress);
    }
}

void StackTableHandler::onCellDoubleClicked(int row, int column) {
    Q_UNUSED(column);
    if (!m_table) return;

    QTableWidgetItem *fileItem = m_table->item(row, ColFile);
    if (!fileItem) return;

    QString filePath = fileItem->data(Qt::UserRole).toString();
    int lineNumber = fileItem->data(Qt::UserRole + 1).toInt();

    // Пробрасываем сигнал наружу для MainWindow
    emit frameSelected(filePath, lineNumber);
}
