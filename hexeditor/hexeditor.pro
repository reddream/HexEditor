#-------------------------------------------------
#
# HexEditor
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = hexeditor
TEMPLATE = app


DEFINES += QT_DEPRECATED_WARNINGS

DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/objs/
MOC_DIR  = $$PWD/Moc/
DEFINES += QHEXEDIT_EXPORTS

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    hexeditor/qhexedit.cpp \
    hexeditor/chunks.cpp \
    hexeditor/commands.cpp \

HEADERS += \
        mainwindow.h \
    hexeditor/qhexedit.h \
    hexeditor/chunks.h \
    hexeditor/commands.h \


FORMS += \
        mainwindow.ui \

RESOURCES += \
    res.qrc

