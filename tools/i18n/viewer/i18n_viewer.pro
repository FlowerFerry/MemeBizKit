QT += core gui widgets sql

CONFIG += c++17
TARGET = i18n_viewer
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    translationmodel.cpp

HEADERS += \
    mainwindow.h \
    translationmodel.h

# MSVC 中文支持
msvc {
    QMAKE_CXXFLAGS += /utf-8
}
