#include "settings.h"
#include "ui_settings.h"
#include "neuro_programm.h"

#include <QMessageBox>
#include <QSettings>

Settings::Settings(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Settings)
{
    ui->setupUi(this);

    ui->fontComboBoxEditor->setFontFilters(QFontComboBox::AllFonts);

    // =========================================================================
    // 1. СИНХРОНИЗАЦИЯ КАТЕГОРИЙ И СТРАНИЦ STACKEDWIDGET
    // =========================================================================
    // Наполняем QListWidget категориями параметров настроек
    ui->listWidget->addItem("Интерфейс приложения");
    ui->listWidget->addItem("Текстовый редактор"); // Основная целевая категория
    ui->listWidget->addItem("Встроенный чат ИИ");

    // Выбираем первую строчку по умолчанию, чтобы стек не открывался пустым
    ui->listWidget->setCurrentRow(0);

    // Связываем переключение категорий QListWidget со страницами QStackedWidget
    connect(ui->listWidget, &QListWidget::currentItemChanged, this, &Settings::onCategoryChanged);

    // =========================================================================
    // 2. ИНИЦИАЛИЗАЦИЯ ДВИЖКА ТЕМ И ОКНА ПРЕДПРОСМОТРА (PREVIEW)
    // =========================================================================
    // Загружаем текущие сохраненные параметры шрифтов из QSettings
    loadCurrentSettings();

    // Запускаем автоматическое сканирование и наполнение комбобокса файлами тем
    initThemesComboBox();

    // Создаем отдельный изолированный парсер подсветки для виджета codePreviewText
    previewHighlighter = new PythonHighlighter(ui->codePreviewText->document());

    // ПУЛЕНЕПРОБИВАЕМЫЙ КОННЕКТ СИГНАЛА: Связываем изменение строки в комбобоксе
    // с функцией живого обновления цветов в окне превью на лету
    connect(ui->comboTheme, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        Q_UNUSED(text);
        this->updatePreviewTheme(); // Вызываем перерисовку при каждом изменении строки!
    });

    // Принудительно запускаем первичное обновление, чтобы сразу раскрасить превью при открытии
    updatePreviewTheme();

    // =========================================================================
    // 3. УПРАВЛЕНИЕ КНОПКАМИ ЗАКРЫТИЯ И ПРИМЕНЕНИЯ НАСТРОЕК
    // =========================================================================
    connect(ui->btnClose, &QPushButton::clicked, this, &Settings::btnClose_clicked);

    // Кнопка ОК — сохраняет изменения, отправляет сигнал и закрывает окно
    connect(ui->btnOk, &QPushButton::clicked, this, &Settings::btnOk_clicked);

    // Кнопка Применить — сохраняет изменения, отправляет сигнал и НЕ закрывает окно
    connect(ui->btnApply, &QPushButton::clicked, this, &Settings::btnApply_clicked);
}

Settings::~Settings()
{
    delete ui;
}

void Settings::btnClose_clicked()
{
    close();
}

// #include "neuro_programm.h" // Обязательно проверьте наличие этого инклуда в самом верху settings.cpp

// void Settings::apply_settings()
// {
//     QSettings settings("PyTorchStudio", "EditorSettings");

//     // 1. Забираем параметры шрифтов и имени темы из интерфейса настроек
//     QString selectedTheme = ui->comboTheme->currentText();
//     settings.setValue("Theme/Name", selectedTheme);
//     settings.setValue("Editor/FontFamily", ui->fontComboBoxEditor->currentFont().family());
//     settings.setValue("Editor/FontSize", ui->spinBoxEditorSize->value());

//     // 2. Записываем HEX-палитры в реестр
//     if (selectedTheme.contains("Monokai")) {
//         settings.setValue("Theme/BgColor", "#272822");
//         settings.setValue("Theme/TextColor", "#f8f8f2");
//         settings.setValue("Theme/KeywordColor", "#f92672");
//         settings.setValue("Theme/ConstantColor", "#ae81ff");
//         settings.setValue("Theme/PytorchColor", "#66d9ef");
//         settings.setValue("Theme/FunctionColor", "#a6e22e");
//         settings.setValue("Theme/NumberColor", "#ae81ff");
//         settings.setValue("Theme/StringColor", "#e6db74");
//         settings.setValue("Theme/CommentColor", "#75715e");
//     }
//     else if (selectedTheme.contains("Тёмная") || selectedTheme.contains("Dark")) {
//         settings.setValue("Theme/BgColor", "#2b2b2b");
//         settings.setValue("Theme/TextColor", "#a9b7c6");
//         settings.setValue("Theme/KeywordColor", "#cc7832");
//         settings.setValue("Theme/ConstantColor", "#0057ae");
//         settings.setValue("Theme/PytorchColor", "#a31515");
//         settings.setValue("Theme/FunctionColor", "#008080");
//         settings.setValue("Theme/NumberColor", "#6897bb");
//         settings.setValue("Theme/StringColor", "#6a8759");
//         settings.setValue("Theme/CommentColor", "#808080");
//     }
//     else {
//         settings.setValue("Theme/BgColor", "#ffffff");
//         settings.setValue("Theme/TextColor", "#24292e");
//         settings.setValue("Theme/KeywordColor", "#1b6ac7");
//         settings.setValue("Theme/ConstantColor", "#0057ae");
//         settings.setValue("Theme/PytorchColor", "#a31515");
//         settings.setValue("Theme/FunctionColor", "#008080");
//         settings.setValue("Theme/NumberColor", "#b56c00");
//         settings.setValue("Theme/StringColor", "#047a15");
//         settings.setValue("Theme/CommentColor", "#898f94");
//     }
//     settings.sync(); // Жестко сохраняем на диск в эту же миллисекунду

//     // 3. Эмитируем сигнал для штатного механизма Qt
//     emit settingsChanged();

//     // =========================================================================
//     // ПРЯМОЙ СТОПРОЦЕНТНЫЙ ПРОБИТИЕ БЛОКИРОВОК (ОБХОД СЛОМАННЫХ СИГНАЛОВ)
//     // Из-за того, что на вкладках прописан жесткий локальный stylesheet,
//     // мы берем родительский указатель на главное окно и вызываем метод РУКАМИ.
//     // =========================================================================
//     Neuro_programm *mainWin = qobject_cast<Neuro_programm*>(this->parentWidget());
//     if (!mainWin) {
//         // На случай, если окно настроек открыто без указания parent, ищем по всему приложению:
//         for (QWidget *w : QApplication::topLevelWidgets()) {
//             mainWin = qobject_cast<Neuro_programm*>(w);
//             if (mainWin) break;
//         }
//     }

//     if (mainWin) {
//         qDebug() << "[ИИ АВТОМАТИЗАЦИЯ] Прямой принудительный запуск applyGlobalFonts()";
//         mainWin->applyGlobalFonts(); // Вызываем аппаратное обновление на лету!
//     } else {
//         qDebug() << "[ИИ ОШИБКА] Не удалось найти главное окно приложения в памяти!";
//     }

//     close(); // Закрываем окно параметров
// }

void Settings::onCategoryChanged(QListWidgetItem *current, QListWidgetItem *previous) {
    Q_UNUSED(previous);
    if (!current) return;

    // Синхронизируем индекс списка и страницы стека
    int index = ui->listWidget->row(current);
    ui->stackedWidget->setCurrentIndex(index);
}

void Settings::loadCurrentSettings() {
    QSettings settings("PyTorchStudio", "EditorSettings");

    // Загружаем сохраненный шрифт редактора (по умолчанию Monospace, 12pt)
    QString fontFamily = settings.value("Editor/FontFamily", "Monospace").toString();
    int fontSize = settings.value("Editor/FontSize", 12).toInt();

    // Выставляем значения в виджеты на странице "Текстовый редактор" (допустим, это страница 1)
    ui->fontComboBoxEditor->setCurrentFont(QFont(fontFamily));
    ui->spinBoxEditorSize->setValue(fontSize);
}

// void Settings::onSaveClicked() {
//     QSettings settings("PyTorchStudio", "EditorSettings");

//     // 1. Сохраняем шрифты из прошлых шагов
//     settings.setValue("Editor/FontFamily", ui->fontComboBoxEditor->currentFont().family());
//     settings.setValue("Editor/FontSize", ui->spinBoxEditorSize->value());

//     // 2. Считываем выбранную тему из QComboBox
//     QString selectedTheme = ui->comboTheme->currentText();
//     settings.setValue("Theme/Name", selectedTheme); // Запоминаем имя для UI

//     if (selectedTheme.contains("Тёмная") || selectedTheme.contains("Dark"))
//     {
//         // Записываем палитру Darcula
//         settings.setValue("Theme/BgColor", "#2b2b2b");
//         settings.setValue("Theme/TextColor", "#a9b7c6");
//         settings.setValue("Theme/KeywordColor", "#cc7832");
//         settings.setValue("Theme/StringColor", "#6a8759");
//         settings.setValue("Theme/CommentColor", "#808080");
//         settings.setValue("Theme/NumberColor", "#6897bb");
//         settings.setValue("Theme/BuiltinColor", "#bbb529");
//     }
//     else
//     {
//         // Записываем палитру Breeze (Светлая)
//         settings.setValue("Theme/BgColor", "#ffffff");
//         settings.setValue("Theme/TextColor", "#24292e");
//         settings.setValue("Theme/KeywordColor", "#d73a49");
//         settings.setValue("Theme/StringColor", "#032f62");
//         settings.setValue("Theme/CommentColor", "#6a737d");
//         settings.setValue("Theme/NumberColor", "#005cc5");
//         settings.setValue("Theme/BuiltinColor", "#6f42c1");
//     }

//     accept(); // Закрываем диалог с сохранением
// }



void Settings::updatePreviewTheme()
{
    QString themePath = ui->comboTheme->currentData().toString();
    if (themePath.isEmpty()) return;

    QSettings themeFile(themePath, QSettings::IniFormat);

    // Считываем цвета строго по тем ключам, которые генерируются в .ini
    QString bgColor   = themeFile.value("Theme/BgColor", "#ffffff").toString();
    QString textColor = themeFile.value("Theme/TextColor", "#24292e").toString();

    QString keywordColor = themeFile.value("Theme/KeywordColor", "#1b6ac7").toString();
    QString stringColor  = themeFile.value("Theme/StringColor", "#047a15").toString();
    QString commentColor = themeFile.value("Theme/CommentColor", "#898f94").toString();
    QString numberColor  = themeFile.value("Theme/NumberColor", "#b56c00").toString();
    QString constantColor = themeFile.value("Theme/ConstantColor", "#0057ae").toString();
    QString torchColor   = themeFile.value("Theme/PytorchColor", "#a31515").toString();
    QString funcColor    = themeFile.value("Theme/FunctionColor", "#008080").toString();

    qDebug() << "[ПРЕВЬЮ] Загрузка темы:" << themePath << "Фон:" << bgColor;

    // 1. Аппаратно перекрашиваем фон текстового поля превью
    ui->codePreviewText->setStyleSheet(QString(
                                           "QTextEdit {"
                                           "   background-color: %1 !important;"
                                           "   color: %2 !important;"
                                           "   font-family: 'Source Code Pro', Monospace;"
                                           "   font-size: 12px;"
                                           "}"
                                           ).arg(bgColor, textColor));

    // 2. Перекрашиваем форматы хайлайтера превью
    if (previewHighlighter) {
        previewHighlighter->keywordFormat.setForeground(QColor(keywordColor));
        previewHighlighter->stringFormat.setForeground(QColor(stringColor));
        previewHighlighter->commentFormat.setForeground(QColor(commentColor));
        previewHighlighter->numberFormat.setForeground(QColor(numberColor));
        previewHighlighter->constantFormat.setForeground(QColor(constantColor));
        previewHighlighter->pytorchFormat.setForeground(QColor(torchColor));
        previewHighlighter->functionFormat.setForeground(QColor(funcColor));
        previewHighlighter->multiLineCommentFormat.setForeground(QColor(commentColor));

        // Принудительно заставляем перепарсить текст
        previewHighlighter->rehighlight();
    }
    ui->codePreviewText->update();
}

void Settings::initThemesComboBox()
{
    ui->comboTheme->clear();

    // 1. Определяем путь к папке "themes" рядом с запущенной программой
    QString externalThemesPath = QApplication::applicationDirPath() + "/themes";
    QDir externalDir(externalThemesPath);

    if (!externalDir.exists()) {
        externalDir.mkpath(".");
    }

    // =========================================================================
    // 2. АВТОГЕНЕРАЦИЯ ПАЛИТРЫ №1: BREEZE (Светлая тема)
    // =========================================================================
    QString breezePath = externalDir.absoluteFilePath("light_breeze.ini");
    if (!QFileInfo::exists(breezePath)) {
        QSettings theme(breezePath, QSettings::IniFormat);
        theme.setValue("Meta/Name", "Breeze (Светлая)");
        theme.setValue("Theme/BgColor", "#ffffff");
        theme.setValue("Theme/TextColor", "#24292e");
        theme.setValue("Theme/KeywordColor", "#1b6ac7");
        theme.setValue("Theme/ConstantColor", "#0057ae");
        theme.setValue("Theme/PytorchColor", "#a31515");
        theme.setValue("Theme/FunctionColor", "#008080");
        theme.setValue("Theme/NumberColor", "#b56c00");
        theme.setValue("Theme/StringColor", "#047a15");
        theme.setValue("Theme/CommentColor", "#898f94");
        theme.sync();
    }

    // =========================================================================
    // 3. АВТОГЕНЕРАЦИЯ ПАЛИТРЫ №2: DARCULA (Тёмная тема)
    // =========================================================================
    QString darculaPath = externalDir.absoluteFilePath("dark_darcula.ini");
    if (!QFileInfo::exists(darculaPath)) {
        QSettings theme(darculaPath, QSettings::IniFormat);
        theme.setValue("Meta/Name", "Darcula (Тёмная)");
        theme.setValue("Theme/BgColor", "#2b2b2b");
        theme.setValue("Theme/TextColor", "#a9b7c6");
        theme.setValue("Theme/KeywordColor", "#cc7832");
        theme.setValue("Theme/ConstantColor", "#0057ae");
        theme.setValue("Theme/PytorchColor", "#a31515");
        theme.setValue("Theme/FunctionColor", "#008080");
        theme.setValue("Theme/NumberColor", "#6897bb");
        theme.setValue("Theme/StringColor", "#6a8759");
        theme.setValue("Theme/CommentColor", "#808080");
        theme.sync();
    }

    // =========================================================================
    // 4. АВТОГЕНЕРАЦИЯ ПАЛИТРЫ №3: MONOKAI (Ретро киберпанк)
    // =========================================================================
    QString monokaiPath = externalDir.absoluteFilePath("monokai.ini");
    if (!QFileInfo::exists(monokaiPath)) {
        QSettings theme(monokaiPath, QSettings::IniFormat);
        theme.setValue("Meta/Name", "Monokai (Ретро киберпанк)");
        theme.setValue("Theme/BgColor", "#272822");
        theme.setValue("Theme/TextColor", "#f8f8f2");
        theme.setValue("Theme/KeywordColor", "#f92672");
        theme.setValue("Theme/ConstantColor", "#ae81ff");
        theme.setValue("Theme/PytorchColor", "#66d9ef");
        theme.setValue("Theme/FunctionColor", "#a6e22e");
        theme.setValue("Theme/NumberColor", "#ae81ff");
        theme.setValue("Theme/StringColor", "#e6db74");
        theme.setValue("Theme/CommentColor", "#75715e");
        theme.sync();
    }

    // =========================================================================
    // 5. СКАНИРОВАНИЕ И НАПОЛНЕНИЕ ВЫПАДАЮЩЕГО СПИСКА ИЗ ПАПКИ НА ДИСКЕ
    // =========================================================================
    QStringList filters;
    filters << "*.ini";
    externalDir.setNameFilters(filters);

    for (const QFileInfo &fileInfo : externalDir.entryInfoList(QDir::Files)) {
        QSettings themeFile(fileInfo.absoluteFilePath(), QSettings::IniFormat);
        QString displayName = themeFile.value("Meta/Name", fileInfo.baseName()).toString();

        // Наполняем комбобокс: отображаем имя с иконкой, а в скрытых данных (UserData) храним полный путь на диске
        ui->comboTheme->addItem("🎨 " + displayName, fileInfo.absoluteFilePath());
    }

    qDebug() << "Успешно загружено тем в комбобокс:" << ui->comboTheme->count();
}


// 1. Метод кнопки ПРИМЕНИТЬ (Принять изменения и НЕ закрывать окно)
void Settings::btnApply_clicked()
{
    QSettings settings("PyTorchStudio", "EditorSettings");

    // 1. Забираем параметры шрифтов из интерфейса настроек
    settings.setValue("Editor/FontFamily", ui->fontComboBoxEditor->currentFont().family());
    settings.setValue("Editor/FontSize", ui->spinBoxEditorSize->value());

    // 2. БЕЗОПАСНЫЙ ПЕРЕХВАТ: Берем полный путь к файлу темы и переводим его в нижний регистр
    QString themeFilePath = ui->comboTheme->currentData().toString().toLower();
    QString selectedThemeName = ui->comboTheme->currentText();

    // Сохраняем красивое имя темы для вывода логов
    settings.setValue("Theme/Name", selectedThemeName);

    qDebug() << "[НАСТРОЙКИ] Нажата кнопка Применить. Файл темы:" << themeFilePath;

    // =========================================================================
    // ТОЧНАЯ СИСТЕМНАЯ ПРОВЕРКА ПО ИМЕНИ ФАЙЛА КОНФИГУРАЦИИ
    // =========================================================================
    if (themeFilePath.contains("monokai"))
    {
        // Палитра Monokai
        settings.setValue("Theme/BgColor", "#272822");
        settings.setValue("Theme/TextColor", "#f8f8f2");
        settings.setValue("Theme/KeywordColor", "#f92672");
        settings.setValue("Theme/ConstantColor", "#ae81ff");
        settings.setValue("Theme/PytorchColor", "#66d9ef");
        settings.setValue("Theme/FunctionColor", "#a6e22e");
        settings.setValue("Theme/NumberColor", "#ae81ff");
        settings.setValue("Theme/StringColor", "#e6db74");
        settings.setValue("Theme/CommentColor", "#75715e");
    }
    else if (themeFilePath.contains("dark") || themeFilePath.contains("darcula"))
    {
        // Палитра Darcula / Breeze Dark
        settings.setValue("Theme/BgColor", "#232629"); // Родной фон Breeze Dark
        settings.setValue("Theme/TextColor", "#eff0f1");
        settings.setValue("Theme/KeywordColor", "#cc7832");
        settings.setValue("Theme/ConstantColor", "#27ae60");
        settings.setValue("Theme/PytorchColor", "#3daee9");
        settings.setValue("Theme/FunctionColor", "#2980b9");
        settings.setValue("Theme/NumberColor", "#f67400");
        settings.setValue("Theme/StringColor", "#fdbc4b");
        settings.setValue("Theme/CommentColor", "#7a7c7d");
    }
    else
    {
        // ГАРАНТИРОВАННАЯ СВЕТЛАЯ ТЕМА BREEZE (Если файл содержит "light", "breeze" или любой другой)
        qDebug() << "[НАСТРОЙКИ] Активация светлых HEX-кодов в реестре QSettings";
        settings.setValue("Theme/BgColor", "#ffffff");
        settings.setValue("Theme/TextColor", "#232629"); // Родной Text в Breeze Light
        settings.setValue("Theme/KeywordColor", "#1b6ac7");
        settings.setValue("Theme/ConstantColor", "#0057ae");
        settings.setValue("Theme/PytorchColor", "#a31515");
        settings.setValue("Theme/FunctionColor", "#008080");
        settings.setValue("Theme/NumberColor", "#b56c00");
        settings.setValue("Theme/StringColor", "#047a15");
        settings.setValue("Theme/CommentColor", "#898f94");
    }

    settings.sync(); // Немедленно принудительно записываем данные в реестр ОС

    // Эмитируем сигнал изменений для главного окна
    emit settingsChanged();

    // Прямой принудительный запуск обновления главного окна
    Neuro_programm *mainWin = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        mainWin = qobject_cast<Neuro_programm*>(w);
        if (mainWin) break;
    }
    if (mainWin) {
        mainWin->applyGlobalFonts();
    }

    //close(); // Закрываем диалоговое окно параметров
}

void Settings::btnOk_clicked()
{
    // Просто вызываем метод применения, который мы написали выше
    btnApply_clicked();

    // И закрываем диалоговое окно настроек
    close();
}
