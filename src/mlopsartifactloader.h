#pragma once

#include <QString>
#include <QStringList>
#include <vector>

// =========================================================================
// ЗАЩИТНЫЙ СЭНДБОКС ДЛЯ LIBTORCH (ИСПРАВЛЕНИЕ ОШИБКИ КОНФЛИКТА МАКРОСОВ QT)
// =========================================================================
#ifdef slots
#  define QT_SLOTS_HIDDEN
#  undef slots
#endif

// Насильно отключаем макросы X11, которые часто ломают компиляцию PyTorch на Linux
#ifdef Success
#  undef Success
#endif

// Теперь безопасно подключаем ядро PyTorch
#include <torch/script.h>

// Возвращаем макрос slots обратно для корректной работы сигналов и слотов Qt
#ifdef QT_SLOTS_HIDVEN
#  define slots Q_SLOTS
#  undef QT_SLOTS_HIDDEN
#endif
// =========================================================================


class MLOpsArtifactLoader {
public:
    // Конструктор по умолчанию
    MLOpsArtifactLoader();

    // Параметры калибровки датчиков
    std::vector<double> sensorMeans;
    std::vector<double> sensorStds;
    QString scalerType;

    // Конфигурация временного окна
    QStringList featuresOrder;
    int windowSize;
    int samplingRateHz;

    QString currentMode;

    /**
     * @brief Загрузка калибровочных параметров и метаданных из папки запуска MLOps
     * @param artifactDirPath Абсолютный путь к папке с файлами json
     * @return true в случае успешного парсинга, false при повреждении или отсутствии файлов
     */
    bool loadSessionMeta(const QString &artifactDirPath);

    /**
     * @brief Преобразование сырого окна данных АЦП в стандартизированный LibTorch-тензор
     * @param rawWindowData Двумерный вектор размера [windowSize x numFeatures]
     * @return torch::Tensor готовой размерности [1, windowSize, numFeatures] под требования LSTM/GRU
     */
    torch::Tensor prepareSensorTensor(const std::vector<std::vector<double>> &rawWindowData);
};
