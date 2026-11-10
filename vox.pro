QT += core gui quick network
CONFIG += c++17
TEMPLATE = app
TARGET = vox

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
    src/statusreporter.cpp

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
    src/statusreporter.h

RESOURCES += src/resources.qrc
