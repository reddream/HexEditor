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
INCLUDEPATH +=. hexeditor


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
    searchdialog.cpp \
    optionsdialog.cpp \


HEADERS += \
        mainwindow.h \
    hexeditor/qhexedit.h \
    hexeditor/chunks.h \
    hexeditor/commands.h \
    optionsdialog.h \
    searchdialog.h


TRANSLATIONS += \
    translations/qhexedit_cs.ts \
    translations/qhexedit_de.ts

FORMS += \
#        mainwindow.ui \
        optionsdialog.ui \
        searchdialog.ui \



RESOURCES += \
    res.qrc \
    qhexedit.qrc

