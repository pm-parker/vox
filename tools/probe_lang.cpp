// Decisive probe: which link in the Translator chain is broken?
// Build (from repo root):
//   g++ -fPIC -std=gnu++17 -I. -I/usr/include/qt6 -I/usr/include/qt6/QtCore \
//     tools/probe_lang.cpp translator.o moc_translator.o qrc_resources.o \
//     -lQt6Core -o /tmp/probe_lang
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include "translator.h"

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString file = QDir::homePath() + "/.config/omarchy/language";
    qDebug() << "language file exists:" << QFile::exists(file);
    qDebug() << "readLanguage:" << Translator::readLanguage(file);
    Translator t({}, ":/duckvox/i18n", &app);
    qDebug() << "t.language():" << t.language();
    qDebug() << "t.isEmpty():" << t.isEmpty();
    qDebug() << "qrc zh-CN.json exists:" << QFile::exists(":/duckvox/i18n/zh-CN.json");
    qDebug() << "translate(Settings):" << t.translate("Main", "Settings");
    qDebug() << "translate(Tap or press Space to dictate):" << t.translate("Main", "Tap or press Space to dictate");
    t.setOverride("zh-CN");
    qDebug() << "after override zh-CN -> language:" << t.language()
             << "translate(Settings):" << t.translate("Main", "Settings");
    return 0;
}
