import numpy as np
import pandas as pd
import os

class FactoryAIR100S4Model:
    def __init__(self, C1=4800, C2=14000, dt=1.0):
        # Паспортные теплоемкости, подобранные под постоянную времени нагрева ~15-20 минут
        self.C1 = C1    # Теплоемкость статора
        self.C2 = C2    # Теплоемкость ротора
        self.dt = dt

        # Строгие константы из Протокола №65 (Двигатель АИР100S4 № 215/2951771)
        self.R_s20 = 1.990       # Строка 28 протокола, Ом
        self.R_r20 = 1.250       # Приведенное сопротивление ротора, Ом
        self.alpha_cu = 0.00393
        self.alpha_al = 0.00403
        self.I_nom = 6.38        # Строка 5 протокола, А

        # Калиброванные тепловые сопротивления на основе заводского плато 82.4 °C
        self.R10 = 0.15          # Статор -> Среда
        self.R12 = 0.45          # Статор <-> Ротор (Зазор)
        self.R20 = 0.25          # Ротор -> Среда

    def simulate(self, I_rms, speed, fan_speed, supply_type, pwm_freq, T_amb, steps):
        T1 = np.full(steps, float(T_amb), dtype=np.float32)
        T2 = np.full(steps, float(T_amb), dtype=np.float32)

        P_cu1_log, P_fe1_log = np.zeros(steps), np.zeros(steps)
        P_cu2_log, P_fe2_log = np.zeros(steps), np.zeros(steps)

        for t in range(1, steps):
            curr_I = I_rms[t-1]
            is_pch = supply_type[t-1]
            f_pwm = pwm_freq[t-1]

            # 1. Пересчет сопротивлений R(T)
            R_s_curr = self.R_s20 * (1 + self.alpha_cu * (T1[t-1] - 20.0))

            # Точный расчет потерь на основе паспортных точек протокола
            # Электрические потери статора (Номинал 295 Вт при 6.38А согласно Строке 21)
            P_cu1 = 3.0 * (curr_I ** 2) * R_s_curr * (295.0 / (3.0 * (self.I_nom**2) * self.R_s20))

            # Магнитные потери в стали статора (Номинал 122 Вт согласно Строке 23)
            # Добавочные потери от ШИМ частотно-регулируемого привода (ПЧ) составляют ~15% от стали
            P_fe1_base = 122.0 * (speed[t-1] / 1419.0)
            P_fe1_pwm = is_pch * (P_fe1_base * 0.40 / max(f_pwm - 3.0, 1.0))
            P_fe1 = P_fe1_base + P_fe1_pwm

            # Потери в роторе (Номинал 174 Вт меди + 32 Вт механики согласно Строкам 22 и 24)
            P_cu2_base = 174.0 * ((curr_I / self.I_nom) ** 2) * (1 + self.alpha_al * (T2[t-1] - 20.0))
            P_cu2_pwm = is_pch * (P_cu2_base * 0.25 / max(f_pwm - 3.0, 1.0))
            P_mech = 32.0 * (speed[t-1] / 1419.0)

            P1_total = P_cu1 + P_fe1
            P2_total = P_cu2_base + P_cu2_pwm + P_mech

            # Логирование структуры мощностей
            P_cu1_log[t-1], P_fe1_log[t-1] = P_cu1, P_fe1
            P_cu2_log[t-1], P_fe2_log[t-1] = P_cu2_base, P_cu2_pwm

            # Двухмассовая система теплового баланса IEEE
            dT1 = (P1_total - (T1[t-1] - T_amb) / self.R10 - (T1[t-1] - T2[t-1]) / self.R12) / self.C1
            dT2 = (P2_total - (T2[t-1] - T_amb) / self.R20 + (T1[t-1] - T2[t-1]) / self.R12) / self.C2

            T1[t] = T1[t-1] + dT1 * self.dt
            T2[t] = T2[t-1] + dT2 * self.dt

        P_cu1_log[-1], P_fe1_log[-1] = P_cu1_log[-2], P_fe1_log[-2]
        P_cu2_log[-1], P_fe2_log[-1] = P_cu2_log[-2], P_fe2_log[-2]

        return T1, T2, P_cu1_log, P_fe1_log, P_cu2_log, P_fe2_log

def generate_csv_dataset(filename="motor_thermal_data.csv", num_sessions=35, seq_len=3600):
    if os.path.exists(filename):
        os.remove(filename)

    model = FactoryAIR100S4Model()
    all_data = []

    print(f"Генерация датасета на основе заводского Протокола №65...")
    for session_id in range(num_sessions):
        T_amb = 20.0
        is_pch_session = 1.0 if np.random.rand() > 0.5 else 0.0
        f_pwm_session = np.random.uniform(4.0, 8.0) if is_pch_session else 4.0

        # Генерируем токи вокруг номинала из протокола (6.38 А)
        I_base = np.random.uniform(5.8, 6.8)
        I_rms = np.full(seq_len, I_base)
        speed = np.full(seq_len, 1419.0)
        fan_speed = np.full(seq_len, 0.0)
        supply_type = np.full(seq_len, is_pch_session)
        pwm_freq = np.full(seq_len, f_pwm_session)

        T1, T2, P_cu1, P_fe1, P_cu2, P_fe2 = model.simulate(
            I_rms, speed, fan_speed, supply_type, pwm_freq, T_amb, seq_len
        )

        session_df = pd.DataFrame({
            'session_id': session_id, 'step': np.arange(seq_len),
            'I_rms': I_rms, 'Speed': speed, 'Fan_Speed': fan_speed,
            'Supply_Type': supply_type, 'PWM_Freq': pwm_freq, 'T_ambient': np.full(seq_len, T_amb),
            'P_cu1': P_cu1, 'P_fe1': P_fe1, 'P_cu2': P_cu2, 'P_fe2': P_fe2,
            'T_stator_target': T1, 'T_rotor_target': T2
        })
        all_data.append(session_df)

    final_df = pd.concat(all_data, ignore_index=True)
    final_df.to_csv(filename, index=False)
    print(f"Эталонный паспортный датасет успешно сохранен: {filename}")

if __name__ == "__main__":
    generate_csv_dataset()
