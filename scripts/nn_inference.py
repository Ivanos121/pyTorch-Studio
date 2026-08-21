def generate_visualization(variant_id, orig_img, temperature_matrix, masks_dict):
    """
    Генерирует финальное изображение на основе выбранного комбобокса Варианта
    """
    h, w, c = orig_img.shape

    # --- ВАРИАНТ 1: Стандартный (Разметка полупрозрачными масками поверх) ---
    if variant_id == "variant_1_masks":
        annotated_img = orig_img.copy()
        colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255)] # Синий, Зеленый, Красный

        for i, (class_name, mask) in enumerate(masks_dict.items()):
            color = colors[i % len(colors)]
            # Создаем цветной слой
            colored_layer = np.zeros_like(orig_img)
            colored_layer[:] = color

            # Накладываем маску с прозрачностью 40%
            mask_3d = np.repeat(mask[:, :, np.newaxis], 3, axis=2)
            annotated_img = np.where(mask_3d > 0.5, cv2.addWeighted(annotated_img, 0.6, colored_layer, 0.4, 0), annotated_img)

            # Добавляем текстовую подпись на узел
            y_indices, x_indices = np.where(mask > 0.5)
            if len(x_indices) > 0:
                cx, cy = int(np.mean(x_indices)), int(np.mean(y_indices))
                cv2.putText(annotated_img, class_name, (cx - 20, cy), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)

        return annotated_img

    # --- ВАРИАНТ 2: Изолированные узлы (Двигатель на черном фоне) ---
    elif variant_id == "variant_2_isolated":
        # Складываем все маски вместе, чтобы получить общий силуэт двигателя
        combined_mask = np.zeros((h, w))
        for mask in masks_dict.values():
            combined_mask = np.maximum(combined_mask, mask)

        mask_3d = np.repeat(combined_mask[:, :, np.newaxis], 3, axis=2)
        black_bg = np.zeros_like(orig_img)

        # Где маска 1 — оставляем ИК-картинку, где 0 — черный фон
        isolated_img = np.where(mask_3d > 0.5, orig_img, black_bg)
        return isolated_img

    # --- ВАРИАНТ 3: Бизнес-отчет (Стрелки + Температуры) ---
    elif variant_id == "variant_3_report":
        report_img = orig_img.copy()

        for i, (class_name, mask) in enumerate(masks_dict.items()):
            # Находим температуры внутри этой маски
            node_temps = temperature_matrix[mask > 0.5]
            if len(node_temps) == 0: continue

            max_temp = np.max(node_temps)
            mean_temp = np.mean(node_temps)
            status = "DANGER" if max_temp > 85 else "OK"

            # Ищем центр узла для стрелки
            y_indices, x_indices = np.where(mask > 0.5)
            cx, cy = int(np.mean(x_indices)), int(np.mean(y_indices))

            # Рисуем точку в центре узла и выносную линию для текста
            text_x, text_y = cx + 50, cy - 40
            cv2.circle(report_img, (cx, cy), 5, (0, 0, 255), -1)
            cv2.line(report_img, (cx, cy), (text_x, text_y), (255, 255, 255), 1)

            # Выводим блок текста с метриками
            text_line1 = f"{class_name.upper()} ({status})"
            text_line2 = f"Max: {max_temp:.1f}C"
            cv2.putText(report_img, text_line1, (text_x, text_y - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
            cv2.putText(report_img, text_line2, (text_x, text_y + 10), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 255), 1)

        return report_img

    return orig_img
