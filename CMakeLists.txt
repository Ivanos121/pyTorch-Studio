cmake_minimum_required(VERSION 3.16)

project(pytorch-studio VERSION 0.2 LANGUAGES CXX)

# Включаем автоматическую генерацию мета-объектов Qt (MOC, UIC, RCC)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск необходимых модулей Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets DBus Charts Network WebEngineWidgets
    Concurrent WebChannel WebSockets UiTools SerialPort Test PrintSupport
)

# Поиск компонентов KDE (KF6)
find_package(KF6Parts REQUIRED)

# НАЙТИ LIBUSB (Обязательно для Linux)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Единый список ВСЕХ исходных файлов (.cpp, .h, .ui, .qrc)
set(PROJECT_SOURCES
    # Главные файлы и базовые компоненты
    src/main.cpp
    src/codeeditor.cpp
    src/codeeditor.h
    src/neuro_programm.cpp
    src/neuro_programm.h
    src/about_program.cpp
    src/about_program.h
    src/help_window.cpp
    src/help_window.h
    src/settings.cpp
    src/settings.h
    src/search.cpp
    src/search.h
    src/start_progect.cpp
    src/start_progect.h
    src/documentmanager.cpp
    src/documentmanager.h

    # Стили и вспомогательные виджеты
    src/breezeflatstyle.cpp
    src/breezeflatstyle.h
    src/statusbuttonstyle.cpp
    src/statusbuttonstyle.h
    src/minimaparea.cpp
    src/minimaparea.h
    src/editorplaceholder.cpp
    src/editorplaceholder.h
    src/stickyscrollarea.cpp
    src/stickyscrollarea.h
    src/qhtmldelegate.cpp
    src/qhtmldelegate.h
    src/elidedlabel.cpp
    src/elidedlabel.h
    src/replwidget.cpp
    src/replwidget.h
    src/terminalwidget.cpp
    src/terminalwidget.h
    src/advancedclosedialog.cpp
    src/advancedclosedialog.h
    src/qcustomplot.cpp
    src/qcustomplot.h
    src/mlopsartifactloader.cpp
    src/mlopsartifactloader.h

    # Менеджеры данных, процессов и ИИ
    src/projectmanager.cpp
    src/projectmanager.h
    src/configmanager.cpp
    src/configmanager.h
    src/preferencesdialog.cpp
    src/preferencesdialog.h
    src/localaimanager.cpp
    src/localaimanager.h
    src/aiprojectmodel.cpp
    src/aiprojectmodel.h
    src/projectrootproxymodel.cpp
    src/projectrootproxymodel.h
    src/panel_other.cpp
    src/panel_other.h
    src/ai_panel.cpp
    src/ai_panel.h
    src/pythonenvmanager.cpp
    src/pythonenvmanager.h
    src/projectbuilderworker.cpp
    src/projectbuilderworker.h
    src/pipmanagerpage.cpp
    src/pipmanagerpage.h
    src/jupytermanager.cpp
    src/jupytermanager.h
    src/jupyterclient.cpp
    src/jupyterclient.h
    src/tensorboardmanager.cpp
    src/tensorboardmanager.h
    src/huggingfacemanager.cpp
    src/huggingfacemanager.h

    # Отладчик и прошивка (USB / STM)
    src/debugmanager.cpp
    src/debugmanager.h
    src/debug_panel.cpp
    src/debug_panel.h
    src/prog_stm.cpp
    src/prog_stm.h
    src/stacktablehandler.cpp
    src/stacktablehandler.h
    src/variablestablehandler.cpp
    src/variablestablehandler.h
    src/savedata.cpp
    src/savedata.h
    src/usbreceiver.cpp
    src/usbreceiver.h
    src/prog_stm_work.cpp
    src/prog_stm_work.h
    src/aipromptwidget.cpp
    src/aipromptwidget.h

    # Чистые заголовочные файлы структур данных
    src/FolderBlockData.h

    # Файлы интерфейсов (.ui)
    src/about_program.ui
    src/help_window.ui
    src/neuro_programm.ui
    src/panel_other.ui
    src/search.ui
    src/settings.ui
    src/start_progect.ui
    src/ai_panel.ui
    src/debug_panel.ui
    src/prog_stm.ui
    src/savedata.ui
    src/prog_stm_work.ui

    # Файлы ресурсов (.qrc)
    src/resources.qrc
)

# Передаем абсолютный путь к исходникам текущего компьютера в компилятор C++
add_compile_definitions(PROJECT_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Создаем исполняемый файл проекта
add_executable(pytorch-studio ${PROJECT_SOURCES}
    src/dynamicformbuilder.h src/dynamicformbuilder.cpp
    src/sessiontablewidget.h src/sessiontablewidget.cpp
    src/sessiondetailswidget.h src/sessiondetailswidget.cpp src/sessiondetailswidget.ui
    src/trainconfigwizard.h src/trainconfigwizard.cpp
    src/qcustomplot.cpp src/qcustomplot.h
    src/mlopsartifactloader.h src/mlopsartifactloader.cpp
)

# Указываем пути поиска заголовочных файлов
target_include_directories(pytorch-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBUSB_INCLUDE_DIRS}
)

# Линковка статической библиотеки проекта, libusb и KF6
target_link_libraries(pytorch-studio PRIVATE
    ${LIBUSB_LIBRARIES}
    KF6::Parts
)

# Линковка модулей Qt6
target_link_libraries(pytorch-studio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Charts
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::Concurrent
    Qt6::WebChannel
    Qt6::WebSockets
    Qt6::UiTools
    Qt6::SerialPort
    Qt6::Test
    Qt6::PrintSupport
    ${TORCH_LIBRARIES}
)

# Настройки свойств исполняемого файла
set_target_properties(pytorch-studio PROPERTIES
    OUTPUT_NAME "pytorch-studio"
)

# Автоматически генерируем номер сборки по текущей дате (ГГММДД)
string(TIMESTAMP CURRENT_DATE "%y%m%d")
set(DYNAMIC_BUILD_NUMBER "1.0.${CURRENT_DATE}")

# Передаем сгенерированное значение в компилятор C++
target_compile_definitions(pytorch-studio PRIVATE
    APP_BUILD_NUMBER="${DYNAMIC_BUILD_NUMBER}"
    APP_VERSION_MAJOR=1
    APP_VERSION_MINOR=0
    APP_VERSION_PATCH=0
)

# =========================================================================
# ЖЕСТКИЙ ХАРДКОД ПУТЕЙ К TORCH (ДЛЯ VENV PYTHON)
# =========================================================================

# 1. Явно указываем CMake, где искать файл TorchConfig.cmake
# Внимание: проверьте точную версию папки python (3.11, 3.12 или 3.13) в вашем venv!
set(Torch_DIR "/home/elf/venv/lib/python3.14/site-packages/torch/share/cmake/Torch")

# Подключаем сам пакет
find_package(Torch REQUIRED)

# 2. Жестко прописываем путь к заголовочным файлам (где лежит torch/script.h)
set(HARDCODED_TORCH_INCLUDES "/home/elf/venv/lib/python3.11/site-packages/torch/include")

# 3. Передаем эти пути компилятору (добавьте к вашему существующему target_include_directories)
target_include_directories(pytorch-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBUSB_INCLUDE_DIRS}
    ${HARDCODED_TORCH_INCLUDES}
    ${HARDCODED_TORCH_INCLUDES}/torch/csrc/api/include
)

# 4. Флаг совместимости ABI (критически важен для Arch Linux, чтобы не было ошибок линковки)
target_compile_definitions(pytorch-studio PRIVATE _GLIBCXX_USE_CXX11_ABI=1)

# 5. Принудительно связываем бинарные библиотеки
target_link_libraries(pytorch-studio PRIVATE
    ${LIBUSB_LIBRARIES}
    Qt6::Widgets
    ${TORCH_LIBRARIES}
)
