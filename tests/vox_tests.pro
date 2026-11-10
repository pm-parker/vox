QT += core gui network testlib
CONFIG += c++17 testcase
TARGET = vox_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h ../src/store.h \
    ../src/recorder.h ../src/engine.h ../src/inserter.h ../src/textpipeline.h
SOURCES += vox_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp \
    ../src/store.cpp ../src/recorder.cpp ../src/engine.cpp ../src/inserter.cpp ../src/textpipeline.cpp
