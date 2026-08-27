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
}

void RtspVideoInferenceWorker::stopVideoProcessing()
{
    m_running = false;
}

void RtspVideoInferenceWorker::toggleRecording(bool start, const QString &savePath)
{
    // ЖЕСТКИЙ ФИКС БАРЬЕРА ПАМЯТИ: Блокируем мьютекс, чтобы вытолкнуть флаг из кэша процессора в ОЗУ
    m_writerMutex.lock();

    m_isRecording = start ? 1 : 0; // Теперь Ядро №2 гарантированно увидит этот флаг!

    m_writerMutex.unlock(); // Мгновенно отпускаем, GUI-поток не зависнет

    if (start) {
        qDebug() << " >>> [БЕЗОПАСНЫЙ СТАРТ]: Флаг m_isRecording атомарно изменен на TRUE!";
    } else {
        qDebug() << " >>> [БЕЗОПАСНЫЙ СТОП]: Флаг m_isRecording атомарно изменен на FALSE!";
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

    // 2. АДАПТИВНОЕ ПОДКЛЮЧЕНИЕ К КАМЕРЕ
    cv::VideoCapture cap;
    qDebug() << " [ПАК ТЕСТ]: Попытка жесткого захвата /dev/video0 через V4L2 API...";

    // Принудительный бэкенд V4L2 обеспечивает стабильный FPS и захват на Arch Linux
    cap.open(0, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        qDebug() << " [OpenCV V4L2]: Режим CAP_V4L2 не ответил. Пробуем автовыбор CAP_ANY...";
        cap.open(0, cv::CAP_ANY);
    }
    if (!cap.isOpened()) {
        qWarning() << " [КРИТИЧЕСКИЙ СБОЙ ХАРДВЕРА]: Локальная веб-камера /dev/video0 недоступна!";
        emit errorOccurred(QStringLiteral("Локальная веб-камера /dev/video0 недоступна. Проверьте права доступа группы video!"));
        emit finished();
        return;
    }

    // Настройка параметров камеры (Кодек MJPEG отключен, чтобы избежать холостых циклов rawFrame.empty)
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    int realWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int realHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    qDebug() << " [УСПЕХ ВЕБ-КАМЕРЫ]: Аппаратный захват активен. Разрешение:" << realWidth << "x" << realHeight;

    this->setProperty("real_width", realWidth);
    this->setProperty("real_height", realHeight);

    cv::Mat rawFrame;
    int localFrameCounter = 0; // НАДЕЖНЫЙ СЧЕТЧИК КАДРОВ НА УРОВНЕ ФУНКЦИИ

    qDebug() << " [СИСТЕМА]: Вход в бесконечный цикл обработки и записи...";
    // 3. ЦИКЛ ОБРАБОТКИ, ЗАПИСИ И ИНФЕРЕНСА В ОЗУ
    while (m_running) {
        if (!cap.read(rawFrame) || rawFrame.empty()) {
            QThread::msleep(5);
            continue;
        }

        // =========================================================================
        // ИНИЦИАЛИЗАЦИЯ ПЕРЕМЕННЫХ GUI ДО ИХ ИСПОЛЬЗОВАНИЯ В СЕКЦИИ ЗАПИСИ (ФИКС C++)
        // =========================================================================
        cv::Mat guiFrame;
        cv::resize(rawFrame, guiFrame, cv::Size(640, 480));
        cv::cvtColor(guiFrame, guiFrame, cv::COLOR_BGR2RGB);
        QImage img(guiFrame.data, guiFrame.cols, guiFrame.rows, guiFrame.step, QImage::Format_RGB888);
        QImage outImg = img.copy();
        float predictedValue = 36.6f;

        // =========================================================================
        // ПРИНУДИТЕЛЬНОЕ ПРИВЕДЕНИЕ ТИПА МАТРИЦЫ ПОД СТАНДАРТ LINUX
        // =========================================================================
        cv::Mat recordFrame;
        if (rawFrame.channels() == 1) {
            cv::cvtColor(rawFrame, recordFrame, cv::COLOR_GRAY2BGR);
        } else if (rawFrame.channels() == 4) {
            cv::cvtColor(rawFrame, recordFrame, cv::COLOR_BGRA2BGR);
        } else {
            recordFrame = rawFrame;
        }

        // =========================================================================
        // ВЫЧИСЛИТЕЛЬНЫЙ ИНФЕРЕНС (ОБРАБАТЫВАЕТСЯ ТОЛЬКО ЕСЛИ МОДЕЛЬ СТАБИЛЬНА)
        // =========================================================================
        if (modelLoadedSuccessfully) {
            try {
                // Быстрая нормализация ImageNet средствами OpenCV вместо torch::tensor
                cv::Mat blob;
                cv::Size spatial_size(224, 224);
                cv::Scalar mean_val(0.485 * 255, 0.456 * 255, 0.406 * 255);
                cv::dnn::blobFromImage(rawFrame, blob, 1.0 / (255.0 * 0.226), spatial_size, mean_val, true, false, CV_32F);

                torch::NoGradGuard no_grad;
                torch::Tensor inputTensor = torch::from_blob(blob.data, {1, 3, 224, 224}, torch::kFloat).to(device);
                // =========================================================================
                // ИСПРАВЛЕННЫЙ И БЕЗОПАСНЫЙ ИНФЕРЕНС (ФИКС ОШИБКИ СКАЛЯРА)
                // =========================================================================
                torch::Tensor outputTensor = module.forward({inputTensor}).toTensor();
                if (outputTensor.defined() && outputTensor.numel() > 0) {
                    torch::Tensor flatTensor = outputTensor.flatten();

                    // ИСПРАВЛЕНО: Явно берем индекс, чтобы избежать ошибки "2 elements cannot be converted to Scalar"
                    float rawValue = flatTensor[0].item<float>();

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

        // =========================================================================
        // ЕДИНЫЙ ЦЕНТР ЗАПИСИ С ЖЕСТКО ЗАДАННЫМ ПУТЕМ (ПРОВЕРКА НАПРЯМУЮ)
        // =========================================================================
        {
            QMutexLocker locker(&m_writerMutex);

            // А. Старт записи: Кнопка нажата, но файл еще не открыт
            if (m_isRecording && !m_videoWriter.isOpened()) {

                // ЖЕСТКИЙ ПУТЬ К ФАЙЛУ (Полностью исключает баги GUI)
                std::string hardcodedPath = "/home/elf/zcc/z1/data/raw/video/test_hardcoded_record.avi";

                // Создаем папку на диске Linux Arch на всякий случай
                QDir().mkpath(QStringLiteral("/home/elf/zcc/z1/data/raw/video"));

                int codec = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
                int actualWidth = recordFrame.cols;
                int actualHeight = recordFrame.rows;

                qDebug() << " [ЖЕСТКИЙ ТЕСТ]: Фоновый поток ИНИЦИАЛИЗИРУЕТ FFmpeg по пути:" << QString::fromStdString(hardcodedPath);

                bool ok = m_videoWriter.open(hardcodedPath,
                                             cv::CAP_FFMPEG,
                                             codec,
                                             25.0, // Рабочий FPS
                                             cv::Size(actualWidth, actualHeight),
                                             true);
                if (ok) {
                    localFrameCounter = 0;
                    qDebug() << " !!! [ЖЕСТКИЙ ТЕСТ - УСПЕХ]: Файл успешно создан! Геометрия кадра:" << actualWidth << "x" << actualHeight;
                } else {
                    // Пробуем MJPEG кодек, если XVID не установлен в системе
                    codec = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
                    ok = m_videoWriter.open(hardcodedPath, cv::CAP_FFMPEG, codec, 25.0, cv::Size(actualWidth, actualHeight), true);
                    if (ok) {
                        localFrameCounter = 0;
                        qDebug() << " !!! [ЖЕСТКИЙ ТЕСТ - ЗАПАСНОЙ УСПЕХ]: Файл открыт с кодеком MJPEG.";
                    } else {
                        qWarning() << " !!! [ЖЕСТКИЙ ТЕСТ - КРИТИЧЕСКИЙ СБОЙ]: OpenCV и FFmpeg отказали в создании файла!";
                    }
                }
            }

            // Б. Запись кадра: Пишем, если файл успешно открылся
            if (m_isRecording && m_videoWriter.isOpened()) {
                cv::Mat frameToSave;
                if (recordFrame.cols != 640 || recordFrame.rows != 480) {
                    cv::resize(recordFrame, frameToSave, cv::Size(640, 480));
                } else {
                    frameToSave = recordFrame;
                }

                m_videoWriter.write(frameToSave); // Записываем матрицу на диск

                localFrameCounter++;
                if (localFrameCounter % 15 == 0) {
                    qDebug() << " -> [ЖЕСТКАЯ ЗАПИСЬ]: Кадры пишутся успешно! Сохранено:" << localFrameCounter;
                }
            }

            // В. Стоп записи: Кнопка отжата интерфейсом, закрываем файл
            if (!m_isRecording && m_videoWriter.isOpened()) {
                m_videoWriter.release(); // Финализируем структуру видеофайла AVI
                m_videoWriter = cv::VideoWriter(); // Очищаем дескриптор в ОЗУ
                std::system("sync"); // Принудительно сбрасываем кэш диска Linux
                qDebug() << " !!! [ЖЕСТКИЙ ТЕСТ - ФИНАЛИЗАЦИЯ]: Файл успешно сохранен и закрыт.";
            }
        }

        // Вывод готового кадра и данных инференса в GUI Студии
        emit frameAnalyzed(outImg, predictedValue);

        // Стабилизация FPS и аппаратная разгрузка процессора
        QThread::msleep(33);
    }

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
