#ifndef REPLWIDGET_H
#define REPLWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QProcess>

namespace Ui {
class Panel_other;
}

class REPLWidget : public QObject {
    Q_OBJECT
public:
    explicit REPLWidget(QPlainTextEdit *history, QLineEdit *input, QObject *parent = nullptr);
    ~REPLWidget();

    // Метод для старта фонового процесса Python
    void startPython();
    void executeSelection(const QString &code);


private slots:
    void handleCommandSend();  // Отправка команды при нажатии Enter
    void readPythonOutput();   // Чтение ответа от Python

private:
    QPlainTextEdit *historyEdit;
    QLineEdit *inputEdit;
    QStringList commandHistory; // Для истории команд (Стрелочки Вверх/Вниз)
    QProcess *pythonProcess;
    int historyIndex;
    // Перехват нажатий клавиш (для истории команд)
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // REPLWIDGET_H

