# --- ИСХОДНЫЕ ДАННЫЕ ОТ ПОЛЬЗОВАТЕЛЯ (Имитация UI) ---
# Комбобокс 1: Выбранная нейросеть (из вашего JSON)
current_net = "resnet18_unet"  # Варианты: "resnet18_regressor", "resnet18_unet", "mobilenet_lraspp"

# Комбобокс 2: Выбранный режим отображения
current_view = "variant_3_report"  # Варианты: "variant_1_masks", "variant_2_isolated", "variant_3_report"


# --- ВЫПОЛНЕНИЕ ПАЙПЛАЙНА ---

# 1. Запускаем нужную нейросеть и получаем геометрию/температуры
orig_img, temp_matrix, masks = run_inference("test_motor.jpg", current_net, models_dict=None)

# 2. Рендерим итоговую картинку на основе выбора второго комбобокса
final_ui_image = generate_visualization(current_view, orig_img, temp_matrix, masks)

# 3. Отображаем или сохраняем результат
cv2.imwrite("final_output.jpg", final_ui_image)
print(f"Отрендерен результат для сети '{current_net}' в режиме '{current_view}'")
