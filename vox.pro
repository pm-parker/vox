QT += core gui quick network widgets svg
CONFIG += c++17
TEMPLATE = app
TARGET = duckvox

# The release version, single source of truth: PKGBUILD's pkgver and the
# GitHub tag read this number, the app shows it in Settings → About and in
# `duckvox --version`. Bump it (and pkgrel) together at release time.
VERSION = 2.5.0
# Double-escaped quotes: qmake hands \" to the Makefile, make's shell pass
# strips the backslashes, so the compiler really sees -DAPP_VERSION="2.5.0".
# A single \\" loses the quotes in the shell pass and the macro becomes the
# bare token 2.5.0 — that broke the build once already.
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/theme.cpp \
    src/translator.cpp \
    src/store.cpp \
    src/recorder.cpp \
    src/engine.cpp \
    src/inserter.cpp \
    src/textpipeline.cpp \
    src/controlserver.cpp \
    src/statusreporter.cpp \
    src/trayicon.cpp

HEADERS += \
    src/backend.h \
    src/theme.h \
    src/translator.h \
    src/store.h \
    src/recorder.h \
    src/engine.h \
    src/inserter.h \
    src/textpipeline.h \
    src/controlserver.h \
    src/statusreporter.h \
    src/trayicon.h

RESOURCES += src/resources.qrc
