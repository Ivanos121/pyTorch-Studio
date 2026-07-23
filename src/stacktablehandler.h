#pragma once
#include <QObject>
#include <QTableWidget>
#include <QHeaderView>

// Простая структура для передачи данных одного фрейма стека
struct StackFrame {
    int level;
    QString function;
    QString file;
    int line;
    QString address;
};

class StackTableHandler : public QObject {
    Q_OBJECT
public:
    // Передаем указатель на готовую таблицу из ui->tableWidget
    explicit StackTableHandler(QTableWidget *table, QObject *parent = nullptr);

    // Метод для полной перезаписи таблицы новыми данными
    void updateTable(const QList<StackFrame> &frames);

    // Метод для быстрой очистки
    void clear();

signals:
    // Сигнал сообщает главному окну, какой файл и строку нужно открыть
    void frameSelected(const QString &filePath, int lineNumber);

private slots:
    void onCellDoubleClicked(int row, int column);

private:
    QTableWidget *m_table;

    enum Columns {
        ColLevel = 0,
        ColFunction,
        ColFile,
        ColLine,
        ColAddress,
        ColCount
    };
};
