#include "configmanager.h"
#include <QMutexLocker>

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

ConfigManager::ConfigManager() {
    // Настройки сохраняются в формате кроссплатформенного .ini файла.
    // На Linux: ~/.config/PyTorchStudioOrg/PyTorchStudio.ini
    // На Windows: AppData/Roaming/PyTorchStudioOrg/PyTorchStudio.ini
    m_settings = new QSettings(
        QSettings::IniFormat,
        QSettings::UserScope,
        QString("PyTorchStudioOrg"),
        QString("PyTorchStudio")
        );
}

ConfigManager::~ConfigManager() {
    delete m_settings;
}

QVariant ConfigManager::getValue(const QString &key, const QVariant &defaultValue) const {
    QMutexLocker locker(&m_mutex);
    return m_settings->value(key, defaultValue);
}

void ConfigManager::setValue(const QString &key, const QVariant &value) {
    QMutexLocker locker(&m_mutex);

    // Перезаписываем файл на диске только если значение реально изменилось
    if (m_settings->value(key) != value) {
        m_settings->setValue(key, value);
        m_settings->sync(); // Принудительно сбрасываем данные из кэша памяти на диск

        // Оповещаем систему об изменении настройки
        emit configChanged(key, value);
    }
}
