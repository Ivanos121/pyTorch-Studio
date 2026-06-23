QT += widgets dbus network charts webenginewidgets gui

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    about_program.cpp \
    advancedclosedialog.cpp \
    aiprojectmodel.cpp \
    breezeflatstyle.cpp \
    codeeditor.cpp \
    help_window.cpp \
    main.cpp \
    neuro_programm.cpp \
    panel_other.cpp \
    projectrootproxymodel.cpp \
    replwidget.cpp \
    search.cpp \
    settings.cpp \
    start_progect.cpp \
    statusbuttonstyle.cpp \
    terminalwidget.cpp

HEADERS += \
    FolderBlockData.h \
    about_program.h \
    advancedclosedialog.h \
    aiprojectmodel.h \
    breezeflatstyle.h \
    codeeditor.h \
    help_window.h \
    neuro_programm.h \
    panel_other.h \
    projectrootproxymodel.h \
    replwidget.h \
    search.h \
    settings.h \
    start_progect.h \
    statusbuttonstyle.h \
    terminalwidget.h

FORMS += \
    about_program.ui \
    help_window.ui \
    neuro_programm.ui \
    panel_other.ui \
    search.ui \
    settings.ui \
    start_progect.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

DISTFILES += \
    Data/Icons/pTS.svg

# =========================================================================
# АВТОМАТИЧЕСКАЯ ГЕНЕРАЦИЯ ВЕРСИИ И БИЛДА (БЕЗ ХАРДКОДА В C++)
# =========================================================================
# Маркетинговая версия (меняется вручную только при крупных релизах)
APP_VERSION_MAJOR = 2026
APP_VERSION_MINOR = 1
APP_VERSION_PATCH = LTS

# Номер билда генерируется автоматически по текущей дате Linux (Формат: ГГММДД, например 260621)
# Утилита date выполнится на уровне ядра Linux в момент сборки
APP_BUILD_NUMBER = $$system(date +%y%m%d)

# Экранируем и передаем переменные из qmake напрямую в макросы компилятора C++
DEFINES += APP_VERSION_MAJOR=\\\"$${APP_VERSION_MAJOR}\\\"
DEFINES += APP_VERSION_MINOR=\\\"$${APP_VERSION_MINOR}\\\"
DEFINES += APP_VERSION_PATCH=\\\"$${APP_VERSION_PATCH}\\\"
DEFINES += APP_BUILD_NUMBER=\\\"$${APP_BUILD_NUMBER}\\\"
