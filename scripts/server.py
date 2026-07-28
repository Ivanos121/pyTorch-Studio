import asyncio
import gc
import os
import time
import json
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import StreamingResponse
from pydantic import BaseModel
from llama_cpp import Llama

# Определение путей к моделям относительно расположения скрипта
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.abspath(os.path.join(CURRENT_DIR, "..", "models"))
MODEL_BASE_PATH = os.path.join(MODELS_DIR, "qwen2.5-coder-3b-q8_0.gguf")
MODEL_INSTRUCT_PATH = os.path.join(MODELS_DIR, "qwen2.5-coder-7b-instruct-q4_k_m.gguf")

# Глобальные переменные состояния оркестратора
llm = None
current_loaded_mode = None  # "base", "instruct", None
is_training_active = False  # Флаг блокировки инференса ради обучения нейросети
last_request_timestamp = 0.0  # Метка времени для трекинга простоя (TTL)
user_intended_mode = "base"  # Желаемый режим со стороны фокуса UI

# Конфигурация таймеров (в секундах)
INACTIVITY_TTL = 300       # Выгрузка через 5 минут простоя для ноутбука
DEBOUNCE_DELAY = 10        # Задержка выгрузки Instruct при рокировке

# Оптимизация под многопоточность процессора ноутбука (nproc - 1)
CPU_THREADS = 3

# Замок для предотвращения гонки потоков при тяжелых вычислениях
execution_lock = asyncio.Lock()

def _safely_free_resources():
    """Внутренний метод полной очистки ОЗУ ноутбука"""
    global llm, current_loaded_mode
    if llm is not None:
        print("[Orchestrator] Освобождение ресурсов оперативной памяти ноутбука...")
        if hasattr(llm, "ctx") and llm.ctx is not None:
            llm.__del__()
        del llm
        llm = None
        current_loaded_mode = None
        gc.collect()

def _physical_load_model(mode: str):
    """Внутренний метод подмены модели в памяти СТРОГО на CPU"""
    global llm, current_loaded_mode
    _safely_free_resources()

    if mode == "base":
        if not os.path.exists(MODEL_BASE_PATH):
            raise FileNotFoundError(f"Файл Base-модели отсутствует: {MODEL_BASE_PATH}")
        print(f"[Orchestrator] Загрузка Base-модели в ОЗУ CPU ({CPU_THREADS} потоков)...")
        llm = Llama(model_path=MODEL_BASE_PATH, n_ctx=2048, n_gpu_layers=0, n_threads=CPU_THREADS, verbose=False)

    elif mode == "instruct":
        if not os.path.exists(MODEL_INSTRUCT_PATH):
            raise FileNotFoundError(f"Файл Instruct-модели отсутствует: {MODEL_INSTRUCT_PATH}")
        print(f"[Orchestrator] Загрузка Instruct-модели в ОЗУ CPU ({CPU_THREADS} потоков)...")
        # ФИЧА 2: Поддержка больших файлов кода (n_ctx=4096)
        llm = Llama(model_path=MODEL_INSTRUCT_PATH, n_ctx=4096, n_gpu_layers=0, n_threads=CPU_THREADS, verbose=False)

    current_loaded_mode = mode
async def background_memory_manager():
    """Фоновый цикл контроля за таймаутами простоя"""
    global llm, current_loaded_mode, last_request_timestamp, user_intended_mode
    while True:
        await asyncio.sleep(5)
        if is_training_active:
            continue
        now = time.time()
        if llm is not None and (now - last_request_timestamp) >= INACTIVITY_TTL:
            print("[Orchestrator] Сработал триггер простоя. Выгружаем веса из ОЗУ.")
            async with execution_lock:
                await asyncio.to_thread(_safely_free_resources)
            continue
        if current_loaded_mode == "instruct" and user_intended_mode == "base":
            if (now - last_request_timestamp) >= DEBOUNCE_DELAY:
                print("[Orchestrator] Задержка дребезга истекла. Возврат к Base для автодополнения.")
                async with execution_lock:
                    await asyncio.to_thread(_physical_load_model, "base")

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Инициализация фоновых потоков при старте сервера"""
    global last_request_timestamp
    last_request_timestamp = time.time()
    manager_task = asyncio.create_task(background_memory_manager())
    print("[Orchestrator] Сетевой мост FastAPI на CPU успешно инициализирован.")
    yield
    manager_task.cancel()
    _safely_free_resources()

app = FastAPI(lifespan=lifespan)

@app.get("/v1/status")
async def get_server_status():
    global llm, current_loaded_mode, is_training_active
    return {
        "status": "online",
        "model_loaded": (llm is not None),
        "active_mode": current_loaded_mode,
        "is_training": is_training_active
    }

@app.post("/v1/load")
async def trigger_initial_load():
    global llm
    if llm is None:
        asyncio.create_task(asyncio.to_thread(_physical_load_model, "base"))
    return {"status": "loading_started"}

@app.post("/v1/switch")
async def switch_mode(mode: str):
    global user_intended_mode, last_request_timestamp, current_loaded_mode
    if mode not in ["base", "instruct"]:
        raise HTTPException(status_code=400, detail="Неверный режим")
    user_intended_mode = mode
    last_request_timestamp = time.time()
    if mode == "instruct" and current_loaded_mode != "instruct":
        async with execution_lock:
            await asyncio.to_thread(_physical_load_model, "instruct")
    return {"status": "success", "active_mode": current_loaded_mode}

@app.post("/v1/train/start")
async def training_start():
    global is_training_active
    async with execution_lock:
        is_training_active = True
        await asyncio.to_thread(_safely_free_resources)
    return {"status": "memory_released_for_training"}

@app.post("/v1/train/stop")
async def training_stop():
    global is_training_active, last_request_timestamp, user_intended_mode
    is_training_active = False
    last_request_timestamp = time.time()
    user_intended_mode = "base"
    return {"status": "ready"}
class AutocompleteBody(BaseModel):
    prefix: str
    suffix: str

@app.post("/v1/autocomplete")
async def autocomplete(data: AutocompleteBody):
    global last_request_timestamp
    if is_training_active:
        raise HTTPException(status_code=503, detail="ИИ отключен: идет обучение нейросети")
    last_request_timestamp = time.time()

    async with execution_lock:
        if current_loaded_mode != "base":
            await asyncio.to_thread(_physical_load_model, "base")
        safe_prefix = data.prefix[-1500:]
        safe_suffix = data.suffix[:1500]
        prompt = f"<|fim_prefix|>{safe_prefix}<|fim_suffix|>{safe_suffix}<|fim_middle|>"
        response = await asyncio.to_thread(
            llm, prompt, max_tokens=32, temperature=0.0,
            stop=["<|fim_prefix|>", "<|fim_suffix|>", "<|fim_middle|>", "<|endoftext|>", "\n\n"]
        )
        last_request_timestamp = time.time()
        return {"code": response["choices"][0]["text"]}

class ChatBody(BaseModel):
    prompt: str
    context: str

@app.post("/v1/generate")
async def generate(data: ChatBody, request: Request):
    global last_request_timestamp, current_loaded_mode
    if is_training_active:
        raise HTTPException(status_code=503, detail="ИИ отключен: идет обучение нейросети")
    last_request_timestamp = time.time()

    async with execution_lock:
        if current_loaded_mode != "instruct":
            with open("/tmp/ai_status.txt", "w", encoding="utf-8") as f:
                f.write("🔄 Рокировка ОЗУ: Загрузка Instruct-модели для чата...")
                f.flush()
                os.fsync(f.fileno())
            await asyncio.to_thread(_physical_load_model, "instruct")

        system_instruction = "You are an expert developer. Output ONLY pure code inside code blocks or text answers."
        full_prompt = f"<|im_start|>system\n{system_instruction}\n<|im_end|>\n<|im_start|>user\nContext code:\n{data.context}\n\nTask: {data.prompt}\n<|im_end|>\n<|im_start|>assistant\n"

        try:
            # =========================================================================
            # ЭТАП 2: ПРОЦЕССОР НАЧАЛ ТЯЖЕЛОЕ ВЫЧИСЛЕНИЕ МАТРИЦЫ ВНИМАНИЯ КОНТЕКСТА (Prefill)
            # =========================================================================
            with open("/tmp/ai_status.txt", "w", encoding="utf-8") as f:
                f.write("🧠 Процессор анализирует контекст файла кода (Prefill)...")
                f.flush()
                os.fsync(f.fileno())

            print("[Orchestrator] Вычисление промпта на ядрах CPU ноутбука...")

            # Запуск итератора llama_cpp в режиме stream=True для подсчета токенов
            iterator = llm(full_prompt, max_tokens=512, temperature=0.3, stop=["<|im_end|>", "<|endoftext|>"], stream=True)

            generated_code = ""
            token_count = 0

            for chunk in iterator:
                # ФИЧА: ОПРЕДЕЛЕНИЕ КНОПКИ ОТМЕНА ИЗ C++
                if await request.is_disconnected():
                    print("[Orchestrator] 🛑 Обнаружен принудительный обрыв сокета из Qt! Немедленно тушу инференс CPU...")
                    _safely_free_resources()
                    break

                # СТРОГИЙ UX ФИКС ОТСТУПОВ: Весь код ниже теперь выровнен по иерархии цикла for
                choices = chunk.get("choices", [])
                if not choices:
                    continue

                first_choice = choices[0] # ИСПРАВЛЕНО: Извлекаем первый словарь из списка choices
                token_text = first_choice.get("text", "")

                generated_code += token_text
                token_count += 1

                # =========================================================================
                # ЭТАП 3: НАЧАЛОСЬ ЖИВОЕ ПЕРЕЧИСЛЕНИЕ СГЕНЕРИРОВАННЫХ ТОКЕНОВ НА ЭКРАНЕ
                # =========================================================================
                if token_count % 3 == 0:
                    with open("/tmp/ai_status.txt", "w", encoding="utf-8") as f:
                        f.write(f"✍️ ИИ генерирует код: {token_count} токенов...")
                        f.flush()
                        os.fsync(f.fileno())

                await asyncio.sleep(0.001)

            # Полностью зачищаем файл перед выдачей готового пакета обратно в C++
            with open("/tmp/ai_status.txt", "w", encoding="utf-8") as f:
                f.write("Генерация успешно завершена!")
                f.flush()
                os.fsync(f.fileno())

            last_request_timestamp = time.time()
            return {"code": generated_code.strip()}

        except Exception as e:
            with open("/tmp/ai_status.txt", "w", encoding="utf-8") as f:
                f.write(f"❌ Ошибка инференса: {e}")
                f.flush()
                os.fsync(f.fileno())
            raise HTTPException(status_code=500, detail=f"Внутренний сбой llama_cpp: {e}")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
