#include "about_program.h"
#include "ui_about_program.h"

#include <QProcess>
#include <QSettings>
#include <QDir>
#include <QSysInfo>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>

About_program::About_program(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::About_program)
{
    ui->setupUi(this);
    //setFixedSize(540, 440);
    this->setMinimumWidth(450);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    // Принудительно заставляем макет пересчитать высоту под весь добавленный текст
    if (this->layout()) {
        this->layout()->setSizeConstraint(QLayout::SetMinimumSize);
    }

    // 1. Создаем объект изображения и указываем путь в ресурсах
    QPixmap logoPixmap(":/Data/Icons/pTS.svg");

    // 2. Важно для Linux: качественно масштабируем картинку под размер 64x64,
    // чтобы на High-DPI (4K) мониторах она не выглядела размытой (SmoothTransformation)
    QPixmap scaledLogo = logoPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 3. Устанавливаем изображение в ваш QLabel
    ui->logoLabel->setPixmap(scaledLogo);

    // 4. Дополнительно: выравниваем иконку по центру внутри виджета
    ui->logoLabel->setAlignment(Qt::AlignCenter);

    // Разрешаем QLabel обрабатывать клики по ссылкам
    ui->aboutInfoText->setWordWrap(true);

    ui->aboutInfoText->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    ui->aboutInfoText->setOpenExternalLinks(false);

    // Связываем сигнал клика по ссылке со слотом (используем безопасную лямбду с контекстом 'this')
    connect(ui->aboutInfoText, &QLabel::linkActivated, this, [this](const QString &link) {

            QDesktopServices::openUrl(QUrl("https://gnu.org"));

    });


    // 1. Читаем кэшированные данные из вашего IDE.conf
    QString confPath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";
    QSettings settings(confPath, QSettings::IniFormat);

    // Если кэша еще нет (первый запуск), выводим статус "Нажмите обновить в настройках"
    QString aiStackValue = settings.value("cache/ai_stack", "Идет определение (перезапустите окно)...").toString();
    QString ollamaValue  = settings.value("cache/ollama_version", "Идет определение...").toString();

    // 2. Собираем системные параметры Linux
    QString osName   = QSysInfo::prettyProductName();
    QString kernel   = QSysInfo::kernelVersion();
    QString display  = QGuiApplication::platformName().toUpper();
    QString qtVer    = qVersion();

    QString versionStr = QApplication::applicationVersion(); // Выдаст: "2026.1-LTS"
    QString buildStr   = APP_BUILD_NUMBER;

    QString topHtml = QString(R"(
    <div style='font-family: sans-serif; line-height: 150%; color: #000000;'>
        <h2 style='margin: 0 0 6px 0; font-size: 16pt; color: #000000;'>PyTorch Studio</h2>
        <p style='margin: 0 0 10px 0; font-size: 10pt; color: #333333;'>
            Версия: %1 (Сборка %2)<br/>
            <span style='color: #000000; font-size: 9pt;'>© 2026 PyTorch Studio Team.</span>
        </p>

        <!-- Новый text с интеграцией ссылки на фразу 'определенными условиями' -->
        <p style='margin: 10px 0 0 0; font-size: 10pt; color: #232629;'>
            PyTorch Studio является свободным программным обеспечением, и вы можете распространять
            его под <a href='open_gpl_license' style='color: #0055aa; text-decoration: none; font-weight: bold;'>определенными условиями</a>.
            Для некоторых компонентов могут применяться различные условия.
        </p>
    </div>
)")
                          .arg(versionStr) // Сюда подставится "2026.1-LTS" из макросов .pro файла
                          .arg(buildStr);                  // Сюда автоматически запишется текущая дата компиляции (например, "260621")


    ui->aboutInfoText->setText(topHtml);

        // Стиль для верхнего контейнера (просто прозрачный текст логов)
    ui->aboutInfoText->setStyleSheet(
        "QTextEdit {"
        "   background-color: transparent;"
        "   border: none;"
        "}"
        );

    // Настройка размера: заставляем контейнер сжиматься под размер текста
    ui->sysDiagnosticsText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 3. Формируем чистый HTML-текст для QLabel без лишней жирности в значениях
    // ... (Считывание QSettings venvRootPath и т.д. остается без изменений) ...

    QString bottomHtml = QString(
                             "<div style='font-family: sans-serif; font-size: 10pt; color: #000000;'>"
                             "   <b style='font-size: 11pt; color: #000000;'>Системная информация для диагностики:</b>"
                             "   <table border='0' cellpadding='4' cellspacing='0' style='width: 100%; margin-top: 15px; font-size: 10pt; color: #000000;'>"
                             "       <tr><td style='color: #000000; width: 140px; font-weight: bold;'>OS:</td><td style='color: #000000;'>%1</td></tr>"
                             "       <tr><td style='color: #000000; font-weight: bold;'>Architecture:</td><td style='color: #000000;'>%2</td></tr>"
                             "       <tr><td style='color: #000000; font-weight: bold;'>Kernel Version:</td><td style='color: #000000;'>%3</td></tr>"
                             "       <tr><td style='color: #000000; font-weight: bold;'>Display Server:</td><td style='color: #000000;'>%4</td></tr>"
                             "       <tr><td style='color: #000000; font-weight: bold;'>Qt Version:</td><td style='color: #000000;'>%5</td></tr>"
                             "       <tr><td style='color: #000000; font-weight: bold;'>AI Stack:</td><td>%6</td></tr>" // ИСПРАВЛЕНО: Убран синий стиль, цвет придет из кэша
                             "       <tr><td style='color: #000000; font-weight: bold;'>Ollama:</td><td style='color: #000000;'>%7</td></tr>"
                             "   </table>"
                             "</div>"
                             )
                             .arg(osName)
                             .arg(QSysInfo::currentCpuArchitecture())
                             .arg(kernel)
                             .arg(display)
                             .arg(qtVer)
                             .arg(aiStackValue) // %6 (подставит готовую черную строку из файла конфигурации)
                             .arg(ollamaValue);  // %7

    ui->sysDiagnosticsText->setText(bottomHtml);

    // Разрешаем тексту переноситься на новые строки, если окно сожмут
    ui->sysDiagnosticsText->setWordWrap(true);

    // Заставляем виджет занимать строго минимально необходимую высоту (убирает пустоту)
    ui->sysDiagnosticsText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Задаем чистый черный цвет и убираем внутренние рамки
    ui->sysDiagnosticsText->setStyleSheet(
        "QLabel {"
        "   background-color: transparent;" // Фон совпадает с окном
        "   border: none;"                  // Убираем серую рамку-карточку
        "   color: #000000;"                // Принудительный черный цвет текста
        "   padding: 0px;"                  // Сбрасываем внутренние отступы
        "}"
        );

    connect(ui->pushButton, &QPushButton::clicked, this, &About_program::close_window);
    connect(ui->pushButton_2, &QPushButton::clicked, this, &About_program::onCopyButtonClicked);

    this->setMinimumWidth(550); // Удерживаем красивую ширину под длинные строки
    this->adjustSize();

    startAsyncAiStackCheck();
}

About_program::~About_program()
{
    delete ui;
}

void About_program::close_window()
{
    close();
}

// Обязательно пишем About_program:: перед именем функции!
QString About_program::gatherOllamaVersion() const {
    QProcess process;
    process.start("ollama", QStringList() << "--version");

    if (process.waitForFinished(300)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            return output.replace("ollama version is ", "");
        }
    }
    return tr("Не запущена / Не установлена");
}

#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

QString About_program::gatherAiStackInfo() const {
    QProcess process;

    // 1. Формируем точный путь
    QString confPath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";

    // Проверяем, существует ли файл физически на диске Arch Linux
    if (!QFile::exists(confPath)) {
        qDebug() << "КРИТИЧЕСКАЯ ОШИБКА: Файл конфигурации не найден по пути:" << confPath;
        return "Файл конфигурации IDE.conf не найден";
    }

    // 2. Открываем с ОБЯЗАТЕЛЬНЫМ указанием IniFormat
    QSettings settings(confPath, QSettings::IniFormat);

    // ВЫВОДИМ В КОНСОЛЬ ВСЕ НАЙДЕННЫЕ ГРУППЫ ДЛЯ ПРОВЕРКИ
    qDebug() << "Все группы в файле:" << settings.childGroups(); // Должно вывести: ("General", "interface", "python")

    // 3. Читаем значение
    QString venvRootPath = settings.value("python/venv_path", "").toString().trimmed();

    qDebug() << "Считанный путь venv из конфига:" << venvRootPath;

    // Если все еще пусто, выводим ошибку детекта
    if (venvRootPath.isEmpty()) {
        return QString("Путь пустой. Проверьте ключ в файле: %1").arg(confPath);
    }

    // 4. Достраиваем путь к интерпретатору
    QString pythonExecutable = QDir(venvRootPath).absoluteFilePath("bin/python");

    if (!QFile::exists(pythonExecutable)) {
        return QString("Папка venv есть, но бинарник %1 отсутствует!").arg(pythonExecutable);
    }

    // 5. Запуск скрипта проверки PyTorch
    QStringList arguments;
    arguments << "-c"
              << "import sys, torch; "
                 "py_v = f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}'; "
                 "torch_v = torch.__version__; "
                 "cuda_v = torch.version.cuda if torch.cuda.is_available() else 'No CUDA'; "
                 "print(f'Python {py_v} / PyTorch {torch_v} + CUDA {cuda_v}')";

    // --- ДОБАВЛЯЕМ ЭТИ ДВЕ СТРОКИ ДЛЯ ИСПРАВЛЕНИЯ ПАДЕНИЯ ---
    // Наследуем все переменные окружения вашей системы (включая пути CUDA и драйверов)
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    // Запускаем интерпретатор
    process.start(pythonExecutable, arguments);

    // Внимание: импорт torch в venv на диске может занимать дольше времени.
    // Увеличим тайм-аут до 1500 мс (1.5 секунды), чтобы скрипт гарантированно успел отработать
    if (process.waitForFinished(1500)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        QString errorOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();

        if (!output.isEmpty() && !output.contains("Error")) {
            return output; // МГНОВЕННЫЙ УСПЕХ!
        }

        // Если Python выдал конкретную ошибку (например, сломан venv или не установлен torch)
        if (!errorOutput.isEmpty()) {
            return tr("Ошибка Python: %1").arg(errorOutput.left(100)); // Показываем первые 100 символов краша
        }
    }

    return tr("Таймаут выполнения скрипта Python в окружении venv.");
}

void About_program::startAsyncAiStackCheck()
{
    // 1. Читаем конфиг, как и раньше
    QString confPath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";
    QSettings settings(confPath, QSettings::IniFormat);
    QString venvRootPath = settings.value("python/venv_path", "").toString();
    QString pythonExecutable = "python3";

    if (!venvRootPath.isEmpty()) {
        pythonExecutable = QDir(venvRootPath).absoluteFilePath("bin/python");
        if (!QFile::exists(pythonExecutable)) pythonExecutable = "python3";
    }

    // 2. Создаем процесс в куче
    aiStackProcess = new QProcess(this);

    QStringList arguments;
    arguments << "-c"
              << "import sys, torch; "
                 "py_v = f'{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}'; "
                 "torch_v = torch.__version__; "
                 "cuda_v = torch.version.cuda if torch.cuda.is_available() else 'No CUDA'; "
                 "print(f'Python {py_v} / PyTorch {torch_v} + CUDA {cuda_v}')";

    aiStackProcess->setProcessEnvironment(QProcessEnvironment::systemEnvironment());

    // 3. СВЯЗЫВАЕМ СИГНАЛ ЗАВЕРШЕНИЯ (Безопасно с контекстом 'this')
    connect(aiStackProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &About_program::onAiStackCheckFinished);

    // 4. Запускаем асинхронно! Функция завершится мгновенно, окно откроется сразу.
    aiStackProcess->start(pythonExecutable, arguments);
}

void About_program::onAiStackCheckFinished() {
    if (!aiStackProcess) return;

    QString output = QString::fromUtf8(aiStackProcess->readAllStandardOutput()).trimmed();
    QString errorOutput = QString::fromUtf8(aiStackProcess->readAllStandardError()).trimmed();

    QString finalAiText;

    if (!output.isEmpty() && !output.contains("Error")) {
        finalAiText = QString("<span style='color: #0055aa;'>%1</span>").arg(output);
    } else if (!errorOutput.isEmpty()) {
        finalAiText = QString("<span style='color: #cc0000;'>Ошибка Python: %1</span>").arg(errorOutput.left(60));
    } else {
        finalAiText = QString("<span style='color: #cc0000;'>Не удалось запустить интерпретатор</span>");
    }

    // Пересобираем и обновляем текст в QLabel на лету
    // Чтобы не переписывать весь HTML, мы можем просто сделать замену подстроки в текущем тексте QLabel
    QString currentText = ui->sysDiagnosticsText->text();

    // Пересохраняем обновленный HTML с реальными версиями
    // Для этого при первой сборке HTML в конструкторе задайте тексту Инициализации маркер, например id='ai-status'
    // Но проще пересобрать весь HTML, вынеся сборку bottomHtml в отдельный метод, либо просто обновить QLabel:

    // Самый простой способ обновить только строчку — перезапустить генерацию HTML.
    // Для этого сохраните переменные окружения ОС в классе, но если не хотите раздувать код,
    // можно просто вырезать старый статус через регулярку или хранить шаблон.

    // Давайте сделаем самый надежный и простой перезапуск текста:
    // Мы просто заменяем временную фразу "Инициализация и опрос PyTorch..." на реальный результат:
    currentText.replace("Инициализация и опрос PyTorch...", finalAiText);
    ui->sysDiagnosticsText->setText(currentText);

    aiStackProcess->deleteLater();
    aiStackProcess = nullptr;
}

// #include <QSettings>
#include <QClipboard>
// #include <QGuiApplication>
#include <QTimer>
// #include <QSysInfo>
#include <QRegularExpression>
// #include <QDesktopServices>

void About_program::onCopyButtonClicked() {
    // 1. Считываем актуальные данные из вашего файла конфигурации IDE.conf
    QString confPath = QDir::homePath() + "/.config/PyTorchStudio/IDE.conf";
    QSettings settings(confPath, QSettings::IniFormat);

    // Удаляем HTML-теги из кэша, если они там есть, чтобы скопировать чистый текст
    QString aiStack = settings.value("cache/ai_stack", "Не определен").toString();
    aiStack.remove(QRegularExpression("<[^>]*>")); // Чистим от тегов <span>

    QString ollama = settings.value("cache/ollama_version", "Не определена").toString();

    // 2. Формируем чистый текст без синтаксиса Markdown с ровными отступами
    QString textToClipboard = QString(
                                  "=== PyTorch Studio System Diagnostics Log ===\n"
                                  "OS:              %1\n"
                                  "Architecture:    %2\n"
                                  "Kernel Version:  %3\n"
                                  "Display Server:  %4\n"
                                  "Qt Version:      %5\n"
                                  "AI Stack:        %6\n"
                                  "Ollama:          %7\n"
                                  "License:         GNU GPLv3\n"
                                  "============================================="
                                  )
                                  .arg(QSysInfo::prettyProductName())
                                  .arg(QSysInfo::currentCpuArchitecture())
                                  .arg(QSysInfo::kernelVersion())
                                  .arg(QGuiApplication::platformName().toUpper())
                                  .arg(qVersion())
                                  .arg(aiStack)
                                  .arg(ollama);

    // 3. Отправляем текст в системный буфер обмена Linux (X11 / Wayland Clipboard)
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(textToClipboard);
    }

    // 4. Умный UX-эффект обратной связи для пользователя
    ui->pushButton_2->setText(tr("Скопировано! ✓"));
    ui->pushButton_2->setEnabled(false);

    // Через 1.5 секунды возвращаем кнопку в исходное состояние с контекстом безопасности
    QTimer::singleShot(1500, this, [this]() {
        if (ui && ui->pushButton_2) {
            ui->pushButton_2->setText(tr("Копировать инфо"));
            ui->pushButton_2->setEnabled(true);
        }
    });
}


