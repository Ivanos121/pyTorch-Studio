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
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets DBus Charts Network WebEngineWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets DBus Charts Network WebEngineWidgets)

# Список всех исходных файлов (.cpp) вашего проекта
set(PROJECT_SOURCES
    about_program.cpp
    advancedclosedialog.cpp
    aiprojectmodel.cpp
    breezeflatstyle.cpp
    codeeditor.cpp
    help_window.cpp
    main.cpp
    neuro_programm.cpp
    panel_other.cpp
    projectrootproxymodel.cpp
    replwidget.cpp
    search.cpp
    settings.cpp
    start_progect.cpp
    statusbuttonstyle.cpp
    terminalwidget.cpp

    FolderBlockData.h
    about_program.h
    advancedclosedialog.h
    aiprojectmodel.h
    breezeflatstyle.h
    codeeditor.h
    help_window.h
    neuro_programm.h
    panel_other.h
    projectrootproxymodel.h
    replwidget.h
    search.h
    settings.h
    start_progect.h
    statusbuttonstyle.h
    terminalwidget.h
)

# Список всех файлов интерфейса (.ui)
set(PROJECT_FORMS
    about_program.ui
    help_window.ui
    neuro_programm.ui
    panel_other.ui
    search.ui
    settings.ui
    start_progect.ui
)

# Список файлов ресурсов (.qrc), где лежат ваши иконки и картинки
set(PROJECT_RESOURCES
    resources.qrc # Замените на реальное имя вашего .qrc файла
)

# Создаем исполняемый файл приложения Linux
add_executable(pytorch-studio
    ${PROJECT_SOURCES}
    ${PROJECT_FORMS}
    ${PROJECT_RESOURCES}
)

# Указываем CMake искать заголовочные файлы в корне проекта
# Это позволит компилятору находить <panel_other.h> и другие локальные файлы
target_include_directories(pytorch-studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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
    APP_BUILD_NUMBER="1.0.260624"  # Наш номер сборки из прошлого шага
)