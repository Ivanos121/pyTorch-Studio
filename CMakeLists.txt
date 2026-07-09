cmake_minimum_required(VERSION 3.16)

# Название вашего проекта и исполняемого файла
project(pytorch-studio VERSION 0.2 LANGUAGES CXX)

# Включаем автоматическую генерацию мета-объектов Qt (MOC, UIC, RCC)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск необходимых модулей Qt (извлекаем модули на основе ваших инклудов)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Charts Network WebEngineWidgets Concurrent WebChannel)

set(KF6_VERSION "6.26.0" CACHE STRING "Force KF6 Version" FORCE)
set(KF6Parts_VERSION "6.26.0" CACHE STRING "Force KF6 Parts Version" FORCE)

find_package(KF6Parts REQUIRED)

# Список всех исходных файлов (.cpp) вашего проекта
set(PROJECT_SOURCES
    src/about_program.cpp
    src/advancedclosedialog.cpp
    src/aiprojectmodel.cpp
    src/breezeflatstyle.cpp
    src/codeeditor.cpp
    src/help_window.cpp
    src/main.cpp
    src/minimaparea.cpp
    src/neuro_programm.cpp
    src/panel_other.cpp
    src/projectrootproxymodel.cpp
    src/replwidget.cpp
    src/search.cpp
    src/settings.cpp
    src/start_progect.cpp
    src/statusbuttonstyle.cpp
    src/terminalwidget.cpp
    src/editorplaceholder.cpp
    src/stickyscrollarea.cpp
    src/qhtmldelegate.cpp
    src/elidedlabel.cpp

    src/FolderBlockData.h
    src/about_program.h
    src/advancedclosedialog.h
    src/aiprojectmodel.h
    src/breezeflatstyle.h
    src/codeeditor.h
    src/help_window.h
    src/minimaparea.h
    src/neuro_programm.h
    src/panel_other.h
    src/projectrootproxymodel.h
    src/replwidget.h
    src/search.h
    src/settings.h
    src/start_progect.h
    src/statusbuttonstyle.h
    src/terminalwidget.h
    src/editorplaceholder.h
    src/stickyscrollarea.h
    src/qhtmldelegate.h
    src/elidedlabel.h
)

# Список всех файлов интерфейса (.ui)
set(PROJECT_FORMS
    src/about_program.ui
    src/help_window.ui
    src/neuro_programm.ui
    src/panel_other.ui
    src/search.ui
    src/settings.ui
    src/start_progect.ui
)

# Список файлов ресурсов (.qrc), где лежат ваши иконки и картинки
set(PROJECT_RESOURCES
    src/resources.qrc # Замените на реальное имя вашего .qrc файла
)

# Создаем исполняемый файл приложения Linux
add_executable(pytorch-studio
    ${PROJECT_SOURCES}
    ${PROJECT_FORMS}
    ${PROJECT_RESOURCES}
    src/minimaparea.h src/minimaparea.cpp
    src/editorplaceholder.h src/editorplaceholder.cpp
    src/stickyscrollarea.h src/stickyscrollarea.cpp
    src/qhtmldelegate.h src/qhtmldelegate.cpp
    src/elidedlabel.h src/elidedlabel.cpp
    src/ai_panel.h src/ai_panel.cpp src/ai_panel.ui
    src/pythonenvmanager.h src/pythonenvmanager.cpp
    src/projectbuilderworker.h src/projectbuilderworker.cpp
)



# Указываем CMake искать заголовочные файлы в корне проекта
# Это позволит компилятору находить <panel_other.h> и другие локальные файлы
target_include_directories(pytorch-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Линковка системных библиотек и модулей Qt к вашему приложению
target_link_libraries(pytorch-studio PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::DBus     # Для системных Freedesktop-уведомлений D-Bus
    Qt${QT_VERSION_MAJOR}::Charts   # Для графиков Loss/Accuracy
    Qt${QT_VERSION_MAJOR}::Network  # Для API-запросов к Ollama ИИ
    Qt${QT_VERSION_MAJOR}::WebEngineWidgets  # Для API-запросов к Ollama ИИ
    Qt${QT_VERSION_MAJOR}::Concurrent  # Для API-запросов к Ollama ИИ
    Qt${QT_VERSION_MAJOR}::WebChannel  # Для API-запросов к Ollama ИИ
    KF6::Parts
)

target_link_libraries(pytorch-studio PRIVATE
    KF6::Parts
)

# Установка имени WM_CLASS для Wayland (чтобы работала иконка приложения)
set_target_properties(pytorch-studio PROPERTIES
    OUTPUT_NAME "pytorch-studio"
)

# Автоматически генерируем номер сборки по текущей дате (ГГММДД)
string(TIMESTAMP CURRENT_DATE "%y%m%d")
set(DYNAMIC_BUILD_NUMBER "1.0.${CURRENT_DATE}")

# Передаем сгенерированное значение в компилятор
target_compile_definitions(pytorch-studio PRIVATE
    APP_BUILD_NUMBER="${DYNAMIC_BUILD_NUMBER}"
    APP_VERSION_MAJOR=1
    APP_VERSION_MINOR=0
    APP_VERSION_PATCH=0
)
