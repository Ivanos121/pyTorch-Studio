#include "replwidget.h"

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QSettings>
#include <QFile>

REPLWidget::REPLWidget(QPlainTextEdit *history, QLineEdit *input, QObject *parent)
    : QObject(parent)
    , historyEdit(history) // Сохраняем прокинутый указатель истории
    , inputEdit(input)     // Сохраняем прокинутый указатель ввода
    , historyIndex(-1)
{
    // Настраиваем шрифт для готовых виджетов (проверьте их objectName в Дизайнере)
    QFont monoFont("Courier New", 10);
    historyEdit->setFont(monoFont);
    inputEdit->setFont(monoFont);

    // Устанавливаем фильтр событий на поле ввода из формы
    inputEdit->installEventFilter(this);

    // Инициализируем процесс
    pythonProcess = new QProcess(this);

    // Соединяем сигналы
    connect(inputEdit, &QLineEdit::returnPressed, this, &REPLWidget::handleCommandSend);
    connect(pythonProcess, &QProcess::readyReadStandardOutput, this, &REPLWidget::readPythonOutput);
    connect(pythonProcess, &QProcess::readyReadStandardError, this, &REPLWidget::readPythonOutput);
}

void REPLWidget::startPython()
{
    // Останавливаем старый процесс, если он почему-то работал
    if (pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        pythonProcess->waitForFinished(1000);
    }

    QSettings settings;
    QString venvPath = settings.value("python/venv_path", "").toString();

    // Если в настройках чисто (самый первый запуск IDE)
    if (venvPath.isEmpty()) {
        historyEdit->appendPlainText("ℹ️ Добро пожаловать! Создайте проект или откройте существующий, чтобы подключить PyTorch (venv).");
        return; // Не роняем программу и не спамим диалогами, просто ждем открытия проекта
    }

    // Формируем путь к исполняемому файлу
#if defined(Q_OS_WIN)
    QString pythonExecutable = venvPath + "/Scripts/python.exe";
#else
    QString pythonExecutable = venvPath + "/bin/python";
#endif

    // Проверяем физическое наличие venv на диске
    if (!QFile::exists(pythonExecutable)) {
        historyEdit->appendPlainText("⚠️ Окружение vемv не найдено по пути: " + venvPath);
        historyEdit->appendPlainText("Если вы только что создали проект, подождите завершения установки пакетов в Терминале.");
        return;
    }

    // Запуск QProcess
    pythonProcess->setWorkingDirectory(QCoreApplication::applicationDirPath());
    QStringList arguments;
    arguments << "-i" << "-u";
    pythonProcess->start(pythonExecutable, arguments);

    if (!pythonProcess->waitForStarted(1000)) {
        historyEdit->appendPlainText("❌ ОШИБКА: Не удалось поднять подпроцесс Python.");
    } else {
        historyEdit->appendPlainText("🚀 Интерактивный бэкенд PyTorch успешно подключен!");
        historyEdit->appendPlainText("Консоль использует venv: " + venvPath);
    }
}



void REPLWidget::handleCommandSend()
{
    QString command = inputEdit->text().trimmed();
    if (command.isEmpty()) return;

    historyEdit->appendPlainText(">>> " + command);
    commandHistory.append(command);
    historyIndex = commandHistory.size();

    pythonProcess->write((command + "\n").toUtf8());
    inputEdit->clear();
}

void REPLWidget::readPythonOutput() {
    QByteArray output = pythonProcess->readAllStandardOutput();
    QByteArray error = pythonProcess->readAllStandardError();

    // Пропускаем логику автодополнения (IntelliSense)...

    if (!output.isEmpty()) {
        QString text = QString::fromUtf8(output);
        text.remove(">>> "); text.remove("... ");
        if(!text.trimmed().isEmpty()) {
            historyEdit->appendPlainText(text.trimmed());

            // АВТОСКРОЛЛ ДЛЯ REPL
            historyEdit->moveCursor(QTextCursor::End);
            historyEdit->ensureCursorVisible();
        }
    }

    if (!error.isEmpty()) {
        QString errText = QString::fromUtf8(error);
        errText.remove(">>> "); errText.remove("... ");
        if(!errText.trimmed().isEmpty()) {
            historyEdit->appendPlainText(errText.trimmed());

            // АВТОСКРОЛЛ ДЛЯ ОШИБОК REPL
            historyEdit->moveCursor(QTextCursor::End);
            historyEdit->ensureCursorVisible();
        }
    }
}

void REPLWidget::executeSelection(const QString &code) {
    QString trimmedCode = code.trimmed();
    if (trimmedCode.isEmpty()) return;

    historyEdit->appendPlainText(">>> " + trimmedCode);
    commandHistory.append(trimmedCode);
    historyIndex = commandHistory.size();

    if (trimmedCode.contains('\n')) { trimmedCode += "\n\n"; }
    else { trimmedCode += "\n"; }

    pythonProcess->write(trimmedCode.toUtf8());
}

bool REPLWidget::eventFilter(QObject *obj, QEvent *e) {
    if (obj == inputEdit && e->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(e);
        if (keyEvent->key() == Qt::Key_Up) {
            if (!commandHistory.isEmpty() && historyIndex > 0) {
                historyIndex--;
                inputEdit->setText(commandHistory.at(historyIndex));
            }
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Down) {
            if (historyIndex < commandHistory.size() - 1) {
                historyIndex++;
                inputEdit->setText(commandHistory.at(historyIndex));
            } else {
                historyIndex = commandHistory.size();
                inputEdit->clear();
            }
            return true;
        }
    }
    return QObject::eventFilter(obj, e);
}

REPLWidget::~REPLWidget() {
    if (pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
    }
}

