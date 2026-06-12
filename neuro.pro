QT += widgets dbus network charts webenginewidgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    about_program.cpp \
    breezeflatstyle.cpp \
    codeeditor.cpp \
    help_window.cpp \
    main.cpp \
    neuro_programm.cpp \
    panel_other.cpp \
    settings.cpp \
    start_progect.cpp \
    statusbuttonstyle.cpp

HEADERS += \
    FolderBlockData.h \
    about_program.h \
    breezeflatstyle.h \
    codeeditor.h \
    help_window.h \
    neuro_programm.h \
    panel_other.h \
    settings.h \
    start_progect.h \
    statusbuttonstyle.h

FORMS += \
    about_program.ui \
    help_window.ui \
    neuro_programm.ui \
    panel_other.ui \
    settings.ui \
    start_progect.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc