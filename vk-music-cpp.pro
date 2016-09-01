QT += core gui network widgets

TARGET = vk-music-cpp
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    vkauthenticator.cpp

HEADERS  += mainwindow.h \
    vkauthenticator.h

FORMS    += mainwindow.ui

