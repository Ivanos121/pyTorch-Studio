import torch
import torch.nn as nn

class ThermalGRU(nn.Module):
    """
    Истинная Physics-Informed GRU модель.
    Нейросеть предсказывает динамические тепловые сопротивления,
    а интегратор рассчитывает строгие экспоненты баланса масс IEEE.
    """
    def __init__(self, input_size=5, hidden_size=64, num_layers=1):
        super(ThermalGRU, self).__init__()
        self.hidden_size = hidden_size
        self.num_layers = num_layers

        self.gru = nn.GRU(input_size, hidden_size, num_layers, batch_first=True)

        # Голова ИИ для предсказания тепловых сопротивлений цепи
        self.fc_outputs = nn.Linear(hidden_size, 3) # [R10_mod, R20_mod, R12_mod]

        # Заводские константы АИР100S4 (согласовано с генератором данных)
        self.C1 = 4800.0
        self.C2 = 14000.0
        self.R_s20 = 1.990
        self.alpha_cu = 0.00393
        self.alpha_al = 0.00403
        self.I_nom = 6.38

    def forward(self, x_stream, initial_temps):
        batch_size, seq_len, _ = x_stream.size()
        device = x_stream.device

        h = torch.zeros(self.num_layers, batch_size, self.hidden_size).to(device)

        outputs = []
        prev_temp = initial_temps # [Batch, 2] -> (T_stator, T_rotor)

        for t in range(seq_len):
            x_t = x_stream[:, t, :].unsqueeze(1)
            gru_out, h = self.gru(x_t, h)
            gru_out = gru_out.squeeze(1)

            # ИИ вычисляет поправки к теплоотдаче
            r_predictions = torch.sigmoid(self.fc_outputs(gru_out))

            # Восстановление реальных масштабов физических сопротивлений
            R10_eff = 0.05 + r_predictions[:, 0] * 0.25
            R20_eff = 0.10 + r_predictions[:, 1] * 0.35
            R12_eff = 0.20 + r_predictions[:, 2] * 0.60

            # Извлекаем физические датчики из x_stream (разнормализация для уравнений)
            curr_I = x_stream[:, t, 0] * 12.0
            curr_speed = x_stream[:, t, 1] * 1550.0
            t_amb = x_stream[:, t, 2] * 40.0
            is_pch = x_stream[:, t, 3] # Флаг ПЧ (0 или 1)
            f_pwm = x_stream[:, t, 4] * 8.0

            # --- СТРОГАЯ ЭЛЕКТРОТЕХНИЧЕСКАЯ ФИЗИКА ПОТЕРЬ ---
            R_s_curr = self.R_s20 * (1.0 + self.alpha_cu * (prev_temp[:, 0] - 20.0))
            P_cu1 = 3.0 * (curr_I ** 2) * R_s_curr * (295.0 / (3.0 * (self.I_nom**2) * self.R_s20))

            P_fe1_base = 122.0 * (curr_speed / 1419.0)
            P_fe1_pwm = is_pch * (P_fe1_base * 0.40 / torch.clamp(f_pwm - 3.0, min=1.0))
            P1_total = P_cu1 + P_fe1_base + P_fe1_pwm

            P_cu2_base = 174.0 * ((curr_I / self.I_nom) ** 2) * (1.0 + self.alpha_al * (prev_temp[:, 1] - 20.0))
            P_cu2_pwm = is_pch * (P_cu2_base * 0.25 / torch.clamp(f_pwm - 3.0, min=1.0))
            P_mech = 32.0 * (curr_speed / 1419.0)
            P2_total = P_cu2_base + P_cu2_pwm + P_mech

            # --- ИНТЕГРАТОР ТЕПЛОВОГО БАЛАНСА ---
            dT1 = (P1_total - (prev_temp[:, 0] - t_amb) / R10_eff - (prev_temp[:, 0] - prev_temp[:, 1]) / R12_eff) / self.C1
            dT2 = (P2_total - (prev_temp[:, 1] - t_amb) / R20_eff + (prev_temp[:, 0] - prev_temp[:, 1]) / R12_eff) / self.C2

            t_stator_new = prev_temp[:, 0] + dT1
            t_rotor_new = prev_temp[:, 1] + dT2

            current_temp = torch.stack([t_stator_new, t_rotor_new], dim=1)
            outputs.append(current_temp.unsqueeze(1))

            prev_temp = current_temp

        return torch.cat(outputs, dim=1)
