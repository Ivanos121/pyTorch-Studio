QT += widgets dbus network charts

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    codeeditor.cpp \
    main.cpp \
    neuro_programm.cpp \
    panel_other.cpp \
    start_progect.cpp

HEADERS += \
    FolderBlockData.h \
    codeeditor.h \
    neuro_programm.h \
    panel_other.h \
    start_progect.h

FORMS += \
    neuro_programm.ui \
    panel_other.ui \
    start_progect.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
