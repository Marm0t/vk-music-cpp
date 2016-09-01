QT += core gui network widgets

TARGET = vk-music-cpp
VERSION = 0.1.0 # major.minor.patch
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    vkauthenticator.cpp

HEADERS  += mainwindow.h \
    vkauthenticator.h

FORMS    += mainwindow.ui

DISTFILES +=

RESOURCES += \
    resources.qrc

