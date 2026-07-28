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
    Concurrent WebChannel WebSockets UiTools SerialPort
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

# Создаем исполняемый файл проекта
add_executable(pytorch-studio ${PROJECT_SOURCES})

# Указываем пути поиска заголовочных файлов
target_include_directories(pytorch-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBUSB_INCLUDE_DIRS}
)

# Линковка статической библиотеки проекта, libusb и KF6
target_link_libraries(pytorch-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libstlink.a
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
