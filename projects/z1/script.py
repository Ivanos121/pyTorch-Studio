import time
from tqdm import tqdm

print(">>> Запуск тестового цикла обучения ИИ...")

# Оборачиваем любой цикл в tqdm() для вывода красивого ползунка
for epoch in tqdm(range(100), desc="Обучение эпох", unit="epoch"):
    time.sleep(0.05)  # Имитируем тяжелые вычисления нейросети

print("✔ Тест успешно завершен!")
