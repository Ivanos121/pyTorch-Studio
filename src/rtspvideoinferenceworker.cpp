#include "rtspvideoinferenceworker.h"
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

#ifdef slots
#undef slots // Уничтожаем макрос Qt 'slots' для шаблонов Torch
#endif
#include <torch/script.h>
#define slots Q_SLOTS // Возвращаем макрос обратно для Qt
#include <opencv2/opencv.hpp>

RtspVideoInferenceWorker::RtspVideoInferenceWorker(const QString &streamUrl, const QString &modelPath, QObject *parent)
    : QObject(parent)
    , m_streamUrl(streamUrl)
    , m_modelPath(modelPath)
    , m_running(false)
{
    m_isRecording = 0; // Атомарная инициализация флага записи в значении FALSE
}

RtspVideoInferenceWorker::~RtspVideoInferenceWorker()
{
    m_running = false;

    // Безопасно очищаем атомарный указатель пути, если он остался в памяти
    QString* oldPath = m_atomicSavePath.exchange(nullptr);
    if (oldPath) {
        delete oldPath;
    }

    qDebug() << " [ДЕСТРУКТОР]: Воркер успешно уничтожен, память очищена.";
}

void RtspVideoInferenceWorker::stopVideoProcessing()
{
    m_running = false;
}

void RtspVideoInferenceWorker::toggleRecording(bool start, const QString &savePath)
{
    m_writerMutex.lock();
    m_isRecording = start ? 1 : 0;
    m_writerMutex.unlock();

    if (start) {
        // Принудительно корректируем расширение
        QString cleanPath = savePath;
        cleanPath.replace(QStringLiteral(".mp4"), QStringLiteral(".avi"));
        cleanPath.replace(QStringLiteral(".MP4"), QStringLiteral(".avi"));

        // Атомарно обмениваем указатели в памяти
        QString* oldPath = m_atomicSavePath.exchange(new QString(cleanPath));
        if (oldPath) {
            delete oldPath; // Чистим память, если там лежал старый путь
        }
        qDebug() << " >>> [БЕЗОПАСНЫЙ СТАРТ]: Флаг = TRUE. Путь атомарно передан в ОЗУ:" << cleanPath;
    } else {
        // При остановке зануляем атомарный указатель
        QString* oldPath = m_atomicSavePath.exchange(nullptr);
        if (oldPath) {
            delete oldPath;
        }
        qDebug() << " >>> [БЕЗОПАСНЫЙ СТОП]: Флаг = FALSE. Запрос на финализацию...";
    }
}

void RtspVideoInferenceWorker::startVideoProcessing()
{
    m_running = true;
    qDebug() << " [ФОНОВЫЙ ПОТОК V4L2]: Запуск конвейера захвата кадров и инференса...";

    // 1. АППАРАТНАЯ ИНИЦИАЛИЗАЦИЯ И ЗАГРУЗКА ВЕСОВ СЕТИ
    torch::jit::script::Module module;
    torch::Device device(torch::kCPU);
    bool modelLoadedSuccessfully = false;

#ifdef TORCH_CUDA_AVAILABLE
    if (torch::cuda::is_available()) {
        device = torch::Device(torch::kCUDA);
        qDebug() << " [LibTorch MLOps]: Обнаружено аппаратное ускорение CUDA. Перевод инференса на GPU.";
    } else {
        qDebug() << " [LibTorch MLOps]: Видеокарта CUDA простаивает или занята. Работаем на CPU.";
    }
#else
    qDebug() << " [LibTorch MLOps]: Сборка LibTorch поддерживает только CPU. Работаем на процессоре.";
#endif

    try {
        module = torch::jit::load(m_modelPath.toStdString(), device);
        module.eval();
        modelLoadedSuccessfully = true;
        qDebug() << " [LibTorch MLOps]: Видео-модель успешно развернута на целевом устройстве:" << QString::fromStdString(device.str());
    } catch (const std::exception &e) {
        qWarning() << " [LibTorch СБОЙ]: Не удалось загрузить веса, но мы запускаем видеопоток без ИИ:" << e.what();
        modelLoadedSuccessfully = false;
    }

    // =========================================================================
    // 2. АДАПТИВНОЕ ПОДКЛЮЧЕНИЕ К КАМЕРЕ (ОБНОВЛЕННЫЙ СТАРТ)
    // =========================================================================
    cv::VideoCapture cap;

    // Считываем стартовый ID из метасистемы (0 - вебкамера, 2 - тепловизор)
    int currentCameraId = this->property("requested_camera_id").isValid() ? this->property("requested_camera_id").toInt() : 0;

    qDebug() << " [ХАРДВЕР]: Первичный захват устройства /dev/video" << currentCameraId << "через V4L2 API...";
    cap.open(currentCameraId, cv::CAP_V4L2);

    if (!cap.isOpened()) {
        qDebug() << " [OpenCV V4L2]: Режим CAP_V4L2 не ответил для ID" << currentCameraId << ". Пробуем автовыбор CAP_ANY...";
        cap.open(currentCameraId, cv::CAP_ANY);
    }

    if (!cap.isOpened()) {
        qWarning() << " [КРИТИЧЕСКИЙ СБОЙ ХАРДВЕРА]: Выбранное устройство видеозахвата недоступно!";
        emit errorOccurred(QStringLiteral("Локальное устройство захвата видео недоступно. Проверьте права доступа группы video!"));
        emit finished();
        return;
    }

    // Жестко фиксируем разрешение под архитектуру ИИ-пульта
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    int realWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int realHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    qDebug() << " [УСПЕХ ВЕБ-КАМЕРЫ]: Аппаратный захват активен. Разрешение:" << realWidth << "x" << realHeight;

    this->setProperty("real_width", realWidth);
    this->setProperty("real_height", realHeight);

    cv::Mat rawFrame;
    int localFrameCounter = 0;
    qDebug() << " [СИСТЕМА]: Вход в бесконечный цикл обработки и записи...";
    // =========================================================================
    // 3. ЦИКЛ ОБРАБОТКИ, ЗАПИСИ И ИНФЕРЕНСА В ОЗУ
    // =========================================================================
    while (m_running) {

        // МГНОВЕННЫЙ МОНИТОРИНГ ГОРЯЧЕЙ СМЕНЫ ИСТОЧНИКА ВИДЕОПОТОКА БЕЗ ПЕРЕЗАПУСКА ПРИЛОЖЕНИЯ
        int targetCameraId = this->property("requested_camera_id").isValid() ? this->property("requested_camera_id").toInt() : 0;

        if (targetCameraId != currentCameraId) {
            qDebug() << " [ИИ ПОТОК]: Обнаружен запрос на смену камеры с" << currentCameraId << "на" << targetCameraId;

            // Финализируем видеозапись, если она активна в момент переключения
            {
                QMutexLocker locker(&m_writerMutex);
                if (m_videoWriter.isOpened()) {
                    m_videoWriter.release();
                    m_videoWriter = cv::VideoWriter();
                    std::system("sync");
                    m_isRecording = 0;
                    m_pendingSavePath = "";

                    emit notificationRequested(
                        QStringLiteral("PyTorch Studio: Запись"),
                        QStringLiteral("Текущая сессия сохранения видео автоматически закрыта из-за смены источника.")
                        );
                }
            }

            // ЖЕСТКИЙ ФИКС ГАШЕНИЯ СВЕТОДИОДА ВЕБКАМЕРЫ (СБРОС БУФЕРОВ ЯДРА)
            rawFrame.release();
            rawFrame = cv::Mat(); // Полностью зануляем ссылку на матрицу в памяти

            cap.release(); // Закрываем аппаратный дескриптор V4L2
            cap = cv::VideoCapture(); // Затираем старый контекст OpenCV

            QThread::msleep(300); // Даем время планировщику Arch Linux снять питание с USB-шины

            qDebug() << " [ИИ ПОТОК]: Физическое открытие нового устройства /dev/video" << targetCameraId;
            cap.open(targetCameraId, cv::CAP_V4L2);
            if (!cap.isOpened()) {
                cap.open(targetCameraId, cv::CAP_ANY);
            }

            if (cap.isOpened()) {
                cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
                cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
                cap.set(cv::CAP_PROP_FOURCC,cv::VideoWriter::fourcc('M','J','P','G'));
                currentCameraId = targetCameraId;

                QString deviceName = (currentCameraId == 2) ? QStringLiteral("Тепловизор (ID: 2)") : QStringLiteral("Веб-камера (ID: 0)");
                emit notificationRequested(
                    QStringLiteral("PyTorch Studio: Аппаратный захват"),
                    QStringLiteral("Аппаратный layer V4L2 успешно переинициализирован.\nУстройство: %1").arg(deviceName)
                    );
            } else {
                emit notificationRequested(
                    QStringLiteral("PyTorch Studio: Сбой оборудования"),
                    QStringLiteral("Не удалось открыть устройство /dev/video%1. Сбой переключения.").arg(targetCameraId)
                    );
                // Плавный возврат на старую камеру
                cap.open(currentCameraId, cv::CAP_V4L2);
            }
            continue;
        }

        // Стандартное чтение кадра
        if (!cap.read(rawFrame) || rawFrame.empty()) {
            QThread::msleep(5);
            continue;
        }

        // ИНИЦИАЛИЗАЦИЯ ПЕРЕМЕННЫХ GUI
        cv::Mat guiFrame;
        cv::resize(rawFrame, guiFrame, cv::Size(640, 480));
        cv::cvtColor(guiFrame, guiFrame, cv::COLOR_BGR2RGB);
        QImage img(guiFrame.data, guiFrame.cols, guiFrame.rows, guiFrame.step, QImage::Format_RGB888);
        QImage outImg = img.copy();
        float predictedValue = 36.6f;

        // ПРИНУДИТЕЛЬНОЕ ПРИВЕДЕНИЕ ТИПА МАТРИЦЫ ПОД СТАНДАРТ LINUX
        cv::Mat recordFrame;
        if (rawFrame.channels() == 1) {
            cv::cvtColor(rawFrame, recordFrame, cv::COLOR_GRAY2BGR);
        } else if (rawFrame.channels() == 4) {
            cv::cvtColor(rawFrame, recordFrame, cv::COLOR_BGRA2BGR);
        } else {
            recordFrame = rawFrame;
        }

        // ВЫЧИСЛИТЕЛЬНЫЙ ИНФЕРЕНС
        if (modelLoadedSuccessfully) {
            try {
                cv::Mat blob;
                cv::Size spatial_size(224, 224);
                cv::Scalar mean_val(0.485 * 255, 0.456 * 255, 0.406 * 255);
                cv::dnn::blobFromImage(rawFrame, blob, 1.0 / (255.0 * 0.226), spatial_size, mean_val, true, false, CV_32F);

                torch::NoGradGuard no_grad;
                torch::Tensor inputTensor = torch::from_blob(blob.data, {1, 3, 224, 224}, torch::kFloat).to(device);

                torch::Tensor outputTensor = module.forward({inputTensor}).toTensor();
                if (outputTensor.defined() && outputTensor.numel() > 0) {
                    torch::Tensor flatTensor = outputTensor.flatten();
                    float rawValue = flatTensor.item<float>();

                    static float smoothedTemperature = 36.6f;
                    const float alpha = 0.15f;
                    smoothedTemperature = (alpha * rawValue) + ((1.0f - alpha) * smoothedTemperature);
                    predictedValue = smoothedTemperature;
                }
            }
            catch (const std::exception &e) {
                qWarning() << " [ИИ СБОЙ]: Исключение внутри инференса:" << e.what();
                predictedValue = 36.6f;
            }
            catch (...) {
                predictedValue = 36.6f;
            }
        }
        // ЕДИНЫЙ ЦЕНТР ЗАПИСИ (СПОСОБ 1: АТОМАРНЫЙ ОБМЕН В ОЗУ)
        {
            QMutexLocker locker(&m_writerMutex);
            if (m_isRecording && !m_videoWriter.isOpened()) {
                QString dynamicPath = "";
                QString* sharedPathPtr = m_atomicSavePath.load();
                if (sharedPathPtr && !sharedPathPtr->isEmpty()) {
                    dynamicPath = *sharedPathPtr;
                } else {
                    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
                    dynamicPath = QStringLiteral("/home/elf/zcc/z1/data/raw/video/train_session_%1.avi").arg(timestamp);
                }

                int codec = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
                int actualWidth = recordFrame.cols;
                int actualHeight = recordFrame.rows;

                qDebug() << " [АТОМАРНЫЙ СТАРТ]: Фоновый поток инициализирует FFmpeg по пути:" << dynamicPath;

                bool ok = m_videoWriter.open(dynamicPath.toStdString(), cv::CAP_FFMPEG, codec, 25.0, cv::Size(actualWidth, actualHeight), true);
                if (ok) {
                    localFrameCounter = 0;
                    qDebug() << " !!! [АТОМАРНЫЙ УСПЕХ]: Видеофайл успешно создан!";
                } else {
                    codec = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
                    ok = m_videoWriter.open(dynamicPath.toStdString(), cv::CAP_FFMPEG, codec, 25.0, cv::Size(actualWidth, actualHeight), true);
                    if (ok) {
                        localFrameCounter = 0;
                        qDebug() << " !!! [ЗАПАСНОЙ УСПЕХ]: Видеофайл создан с кодеком MJPEG.";
                    } else {
                        qWarning() << " !!! [КРИТИЧЕСКИЙ СБОЙ]: OpenCV и FFmpeg отказали в создании атомарного файла!";
                    }
                }
            }

            if (m_isRecording && m_videoWriter.isOpened()) {
                cv::Mat frameToSave;
                if (recordFrame.cols != 640 || recordFrame.rows != 480) {
                    cv::resize(recordFrame, frameToSave, cv::Size(640, 480));
                } else {
                    frameToSave = recordFrame;
                }
                m_videoWriter.write(frameToSave);
                localFrameCounter++;
                if (localFrameCounter % 15 == 0) {
                    qDebug() << " -> [ФИЗИЧЕСКАЯ ЗАПИСЬ]: Кадры пишутся успешно! Сохранено:" << localFrameCounter;
                }
            }

            if (!m_isRecording && m_videoWriter.isOpened()) {
                m_videoWriter.release();
                m_videoWriter = cv::VideoWriter();
                std::system("sync");
                QString* oldPath = m_atomicSavePath.exchange(nullptr);
                if (oldPath) delete oldPath;
                qDebug() << " !!! [АТОМАРНАЯ ФИНАЛИЗАЦИЯ]: Файл успешно запечен на жесткий диск.";
            }
        }

        emit frameAnalyzed(outImg, predictedValue);
        QThread::msleep(33);
    } // Конец бесконечного цикла while (m_running)

    // Освобождение ресурсов при аварийном или плановом выходе из бесконечного цикла
    {
        QMutexLocker locker(&m_writerMutex);
        if (m_videoWriter.isOpened()) {
            m_videoWriter.release();
            qDebug() << " [V4L2 ПОТОК]: Файл записи успешно сохранен и закрыт.";
        }
    }
    cap.release();
    qDebug() << " [ФОНОВЫЙ ПОТОК V4L2]: Аппаратные ресурсы вебкамеры успешно освобождены.";
    emit finished();
}

