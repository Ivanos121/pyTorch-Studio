#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QMutex>

class ConfigManager : public QObject {
    Q_OBJECT

public:
    // Потокобезопасный доступ к единственному экземпляру класса
    static ConfigManager& instance();

    // Деструктор
    ~ConfigManager();

    // Запрещаем копирование и присваивание (правило Singleton)
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Получить значение настройки
    QVariant getValue(const QString &key, const QVariant &defaultValue = QVariant()) const;

    // Сохранить значение настройки
    void setValue(const QString &key, const QVariant &value);

signals:
    // Сигнал для живого обновления модулей IDE при изменении параметров
    void configChanged(const QString &key, const QVariant &newValue);

private:
    // Приватный конструктор
    ConfigManager();

    mutable QMutex m_mutex; // Защита данных при обращении из параллельных потоков
    QSettings *m_settings;
};
