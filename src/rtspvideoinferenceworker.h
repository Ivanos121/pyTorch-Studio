#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QThread>
#include <torch/cuda.h>
#include <opencv2/videoio.hpp>

// Изолированное объявление типов данных для защиты области видимости
namespace cv {
class Mat;
}

// =========================================================================
// КЛАСС-ВОРКЕР ВЫСОКОСКОРОСТНОГО ИИ-ВИДЕОМОНИТОРИНГА ПАК
// =========================================================================
class RtspVideoInferenceWorker : public QObject {
    Q_OBJECT
public:
    explicit RtspVideoInferenceWorker(const QString &streamUrl, const QString &modelPath, QObject *parent = nullptr);
    ~RtspVideoInferenceWorker();

public slots:
    // Главный рабочий цикл нарезки и инференса кадров
    void startVideoProcessing();

    // Потокобезопасная команда на остановку конвейера
    void stopVideoProcessing();

    void toggleRecording(bool start, const QString &savePath = QString());

signals:
    void frameAnalyzed(const QImage &image, float value);
    void errorOccurred(const QString &message);
    void finished();

private:
    QString m_streamUrl;
    QString m_modelPath;
    bool m_running;
    bool m_isRecording = false;       // Флаг: идет ли запись прямо сейчас
    cv::VideoWriter m_videoWriter;
};

Q_DECLARE_METATYPE(RtspVideoInferenceWorker*)
