#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QSettings>

// Структура для декларативного хранения настроек колонок из json-файла
struct SessionColumn {
    QString key;
    QString header;
    int width;
    bool visible;
};

/**
 * @class SessionTableWidget
 * @brief Журнал сессий (4-я страница Студии), строящийся на базе QTableWidget.
 *
 * Класс динамически собирает свою структуру, заголовки и размеры из файла
 * конфигурации Config/session_table_config.json.
 */
class SessionTableWidget : public QWidget {
    Q_OBJECT

public:
    explicit SessionTableWidget(QWidget *parent = nullptr);
    ~SessionTableWidget() override = default;

    /**
     * @brief Задать рабочий путь к ИИ-проекту z1 и принудительно обновить список строк.
     * @param projectPath Абсолютный путь к каталогу проекта
     */
    void setProjectPath(const QString &projectPath);

    /**
     * @brief Программное чтение конфига, сканирование папки metrics/ и заполнение сетки.
     */
    void refreshTable();

private:
    /**
     * @brief Начальная инициализация геометрии и стилей QTableWidget.
     */
    void setupUi();

private:
    QTableWidget        *m_tableWidget;        // Главный внутренний виджет сетки ячеек
    QString              m_currentProjectPath; // Координаты открытого проекта z1
    QList<SessionColumn> m_columns;            // Конфиг структуры колонок, прочитанный из JSON
};
