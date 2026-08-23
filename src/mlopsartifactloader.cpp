#include "mlopsartifactloader.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

// Конструктор с инициализацией базовых безопасных значений
MLOpsArtifactLoader::MLOpsArtifactLoader()
    : scalerType("StandardScaler")
    , windowSize(256)
    , samplingRateHz(1000)
    , currentMode("unknown")
{
}

bool MLOpsArtifactLoader::loadSessionMeta(const QString &artifactDirPath)
{
    // 1. Чтение структуры и параметров нарезки окон (features_config.json)
    QFile configFile(artifactDirPath + "/features_config.json");
    if (configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        QJsonObject obj = doc.object();

        windowSize = obj.value("window_size").toInt(256);
        samplingRateHz = obj.value("sampling_rate_hz").toInt(1000);

        QJsonArray arr = obj.value("features_order").toArray();
        featuresOrder.clear();
        for (const QJsonValue &val : arr) {
            featuresOrder.append(val.toString());
        }
        configFile.close();
    }

    // 2. Чтение коэффициентов скейлера (transform_meta.json)
    QFile transformFile(artifactDirPath + "/transform_meta.json");
    if (!transformFile.open(QIODevice::ReadOnly)) {
        qWarning() << "⚠️ [MLOps] Критическая ошибка: Файл transform_meta.json не найден!";
        return false;
    }

    QJsonDocument transDoc = QJsonDocument::fromJson(transformFile.readAll());
    QJsonObject transObj = transDoc.object();
    transformFile.close();

    // Верификация и распределение параметров по модальностям
    if (transObj.contains("sensor_branch")) {
        currentMode = transObj.contains("image_branch") ? "hybrid" : "sensors";
        QJsonObject sensorBranch = transObj.value("sensor_branch").toObject();

        scalerType = sensorBranch.value("scaler_type").toString("StandardScaler");
        QJsonArray meansArr = sensorBranch.value("means").toArray();
        QJsonArray stdsArr = sensorBranch.value("stds").toArray();

        sensorMeans.clear();
        sensorStds.clear();

        // Резервируем память в векторе для оптимизации аллокаций
        sensorMeans.reserve(meansArr.size());
        sensorStds.reserve(stdsArr.size());

        for (int i = 0; i < meansArr.size(); ++i) {
            sensorMeans.push_back(meansArr.at(i).toDouble());
            sensorStds.push_back(stdsArr.at(i).toDouble());
        }
        qDebug() << "🟢 [MLOps] Метаданные ПАК загружены успешно. Режим:" << currentMode;
    }
    else if (transObj.contains("image_branch")) {
        currentMode = "thermograms";
        qDebug() << "🟢 [MLOps] Загружен режим анализа теплограмм.";
    }

    return true;
}

torch::Tensor MLOpsArtifactLoader::prepareSensorTensor(const std::vector<std::vector<double>> &rawWindowData)
{
    int numFeatures = static_cast<int>(featuresOrder.size());

    // Выделяем непрерывный блок памяти под тензор [1, Window_Size, Features]
    auto tensor = torch::zeros({1, windowSize, numFeatures}, torch::kFloat32);
    auto accessor = tensor.accessor<float, 3>();

    // Пошаговая нормализация временных рядов под шаг обучения Python
    for (int step = 0; step < windowSize; ++step) {
        for (int f = 0; f < numFeatures; ++f) {
            double rawValue = rawWindowData[step][f];
            float normalizedValue = 0.0f;

            if (scalerType == "StandardScaler") {
                // (X - mean) / std
                normalizedValue = static_cast<float>((rawValue - sensorMeans[f]) / sensorStds[f]);
            }
            else if (scalerType == "MinMaxScaler") {
                // (X - min) * scale
                normalizedValue = static_cast<float>((rawValue - sensorMeans[f]) * sensorStds[f]);
            }

            accessor[0][step][f] = normalizedValue;
        }
    }
    return tensor;
}
