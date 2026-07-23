#pragma once
#include "stacktablehandler.h"

#include <QPlainTextEdit>
#include <QProcess>
#include <QWidget>

#include <KPluginMetaData>
#include <KPluginFactory>
#include <KParts/ReadOnlyPart>
#include <KParts/Part>

// Предварительное объявление классов KDE, чтобы не тащить тяжелые инклуды в заголовок
namespace KParts { class ReadOnlyPart; }

namespace Ui {
class panel_other;
}

class panel_other : public QWidget
{
    Q_OBJECT
public:
    explicit panel_other(QWidget *parent = nullptr);
    ~panel_other();
    void appendTrainingLog(const QString &text);
    void setSingleTerminalMode(bool onlyLeftTerminal);
    void sendInputToTerminal(const QString &text);
    void setDebugAction(QAction *action);
    void setCallStackData(const QList<StackFrame> &frames);

public slots:
    void onDebugModeTriggered(bool checked);

signals:
    void fileNavigationRequested(const QString &filePath, int lineNumber);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onReplReadyRead();

private:
    Ui::panel_other *ui;

    // Указатели на встроенные части терминалов Konsole
    KParts::ReadOnlyPart *m_terminalPart1;
    QPlainTextEdit *m_replEdit;
    QProcess *m_replProcess;
    int m_splitterMode = 0;
    StackTableHandler *m_stackHandler = nullptr;
};
