import numpy as np
import cv2

def run_inference(image_path, architecture_id, models_dict):
    """
    Эмулирует или запускает одну из трех нейросетей из вашего JSON-конфига.
    Возвращает: исходное изображение, матрицу температур и маски узлов.
    """
    # 1. Загрузка картинки
    orig_img = cv2.imread(image_path)
    h, w, _ = orig_img.shape

    # 2. Имитируем парсинг реальных температур (в градусах Цельсия)
    # В реальном коде тут: temperature_matrix = flir_extractor.get_thermal(image_path)
    np.random.seed(42)
    temperature_matrix = np.random.uniform(30.0, 95.0, (h, w))

    masks_dict = {} # Сюда соберем маски узлов: {'stator': mask, 'bearing': mask}

    # --- Сценарий для Сети №1: Базовый регрессор (ResNet-18) ---
    if architecture_id == "resnet18_regressor":
        # Эта сеть выдает ОДНО число (температуру статора) и не умеет делать маски.
        # Чтобы комбобокс не падал, мы генерируем условную маску центральной зоны (статора) алгоритмически
        predicted_stator_temp = 78.4  # результат регрессии

        # Делаем грубую маску центральной области для визуализации
        mask = np.zeros((h, w), dtype=np.uint8)
        cv2.circle(mask, (w // 2, h // 2), min(h, w) // 3, 255, -1)
        masks_dict['stator_zone'] = mask / 255.0

    # --- Сценарий для Сети №2: Мультизадачный ResNet-18 + U-Net ---
    elif architecture_id == "resnet18_unet":
        # Эта сеть делает честную попиксельную сегментацию (выдает маски классов)
        # В реальности: output = models_dict['unet'](image)
        # Имитируем маски статора и подшипника:
        mask_stator = np.zeros((h, w), dtype=np.uint8)
        cv2.rectangle(mask_stator, (w//4, h//4), (3*w//4, 3*h//4), 255, -1)

        mask_bearing = np.zeros((h, w), dtype=np.uint8)
        cv2.circle(mask_bearing, (w//4, h//2), 40, 255, -1)

        masks_dict['stator'] = mask_stator / 255.0
        masks_dict['bearing'] = mask_bearing / 255.0

    # --- Сценарий для Сети №3: Легковесный MobileNetV3 (или YOLO) ---
    elif architecture_id == "mobilenet_lraspp":
        # Работает быстро. Возвращает контуры/маски объектов
        # Имитируем маску клеммной коробки и статора
        mask_stator = np.zeros((h, w), dtype=np.uint8)
        cv2.ellipse(mask_stator, (w//2, h//2), (w//3, h//4), 0, 0, 360, 255, -1)

        mask_box = np.zeros((h, w), dtype=np.uint8)
        cv2.rectangle(mask_box, (w//2 - 50, h//4 - 50), (w//2 + 50, h//4 + 20), 255, -1)

        masks_dict['stator'] = mask_stator / 255.0
        masks_dict['terminal_box'] = mask_box / 255.0

    return orig_img, temperature_matrix, masks_dict
