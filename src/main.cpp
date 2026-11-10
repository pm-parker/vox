#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "backend.h"
#include "engine.h"
#include "inserter.h"
#include "recorder.h"
#include "store.h"
#include "theme.h"
#include "translator.h"

// One window, one job. Omarchy's own apps wire the same collaborators:
// Store (settings + history), Engine (SenseVoice), Recorder (parec),
// Inserter (wtype/wl-copy), Theme (live Omarchy accent) and Translator
// (live Omarchy language). QML renders Backend's state machine.
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("omarchy");
    QGuiApplication::setApplicationName("vox");

    const QString dataDir = QGuiApplication::applicationDirPath() + "/data";
    auto *store = new Store({}, &app);
    auto *engine = new Engine({}, &app); // default model dir under ~/.local/share/vox
    auto *recorder = new Recorder({}, &app);
    auto *inserter = new Inserter(&app);
    auto *backend = new Backend(store, engine, recorder, inserter, &app);
    auto *theme = new Theme(dataDir + "/current/theme", &app);
    auto *translator = new Translator(dataDir + "/current/language", ":/i18n", &app);
    qInfo() << "vox: collaborators wired";

    qmlRegisterSingletonInstance("Vox", 1, 0, "Backend", backend);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Settings", store);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Speech", engine);
    qmlRegisterSingletonInstance("Vox", 1, 0, "Theme", theme);
    qmlRegisterSingletonInstance("Vox", 1, 0, "I18n", translator);

    QQmlApplicationEngine qml;
    // Loud QML failures: creation warnings print to stderr before exit(1).
    QObject::connect(&qml, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &w : warnings)
                             qWarning().noquote() << w.toString();
                     });
    // Live retranslation when Omarchy's language changes (Translator watches
    // the current-language file).
    QObject::connect(translator, &Translator::changed, &qml, &QQmlApplicationEngine::retranslate);
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QGuiApplication::exit(1); }, Qt::QueuedConnection);
    // Plain qmake + RESOURCES has no qmldir, so load the qrc URL directly
    // instead of loadFromModule (that path needs qt_add_qml_module).
    qml.load(QUrl("qrc:/vox/ui/Main.qml"));
    qInfo() << "vox: qml.load returned, rootObjects =" << qml.rootObjects().size();

    if (qml.rootObjects().isEmpty())
        return 1;

    return QGuiApplication::exec();
}
