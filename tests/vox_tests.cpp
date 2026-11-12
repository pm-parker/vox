#include <QDir>
#include <QFile>
#include <QTemporaryDir>
// DuckVox test suite. Needs the QtTest include path, which .clangd provides
// for clangd (qmake supplies it to the real build).
#include <QtTest>

#include "backend.h"
#include "engine.h"
#include "inserter.h"
#include "recorder.h"
#include "store.h"
#include "textpipeline.h"
#include "theme.h"
#include "translator.h"

using namespace Qt::Literals::StringLiterals;

class VoxTests : public QObject {
    Q_OBJECT

private slots:
    void storeSettingsRoundTrip()
    {
        QTemporaryDir dir;
        {
            Store store(dir.path());
            store.setPunctStyle("en");
            store.setAutoInsert(true);
            store.setAutoStop(false);
        }
        Store reopened(dir.path());
        QCOMPARE(reopened.punctStyle(), "en");
        QVERIFY(reopened.autoInsert());
        QVERIFY(!reopened.autoStop());
    }

    void storeAppliesDictionaryLongestFirst()
    {
        QTemporaryDir dir;
        Store store(dir.path());
        store.addWord("gpt four", "GPT-4");
        store.addWord("four", "4");
        QCOMPARE(store.applyDictionary("say gpt four and four please"), "say GPT-4 and 4 please");
        store.removeWord("gpt four");
        QCOMPARE(store.applyDictionary("say gpt four and four please"), "say gpt 4 and 4 please");
    }

    void storeHistoryCapsAndPersists()
    {
                        QTemporaryDir dir;
                        {
                                    Store store(dir.path());
                                    for (int i = 0; i < 55; ++i)
                                                store.addHistory(QStringLiteral("line %1").arg(i));
                                    QVERIFY(store.history().size() <= 50);
                        }
        Store reopened(dir.path());
        QCOMPARE(reopened.history().size(), 50);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QStringLiteral("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QStringLiteral("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QStringLiteral("#112233"));
        QCOMPARE(theme.accentForeground(), QStringLiteral("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QStringLiteral("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QStringLiteral("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QStringLiteral("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QStringLiteral("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QCOMPARE(Translator::pluralCategory("ar", 11), QStringLiteral("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QStringLiteral("other"));
    }

    // A finished take nobody is waiting for (backend idle) is scratch audio:
    // it must be deleted, not left in ~/.local/share/duckvox/takes forever.
    void backendDiscardsUnclaimedTake()
    {
        QTemporaryDir dir;
        Store store(dir.path());
        Engine engine(dir.path() + "/models");
        Recorder recorder(dir.path() + "/takes");
        Inserter inserter;
        Backend backend(&store, &engine, &recorder, &inserter);

        const QString take = dir.path() + "/takes/take-1.wav";
        write(take, QByteArray(44, '\0'));
        QVERIFY(QFile::exists(take));
        emit recorder.stopped(take, QStringLiteral("manual"));
        QVERIFY(!QFile::exists(take));
        QCOMPARE(backend.state(), QStringLiteral("idle"));
    }

    void backendStartsIdleAndStaysQuietWithoutTakes()
    {
        QTemporaryDir dir;
        Store store(dir.path());
        Engine engine(dir.path() + "/models");
        Recorder recorder(dir.path() + "/takes");
        Inserter inserter;
        Backend backend(&store, &engine, &recorder, &inserter);

        QCOMPARE(backend.state(), QStringLiteral("idle"));
        backend.repeatLast(); // no takes yet — must not crash or invent text
        backend.dismissError();
        backend.copyLast();
        QCOMPARE(backend.state(), QStringLiteral("idle"));
        QVERIFY(backend.errorText().isEmpty());
        QVERIFY(backend.lastText().isEmpty());
    }

    void textPipelineDropsPunctuationOnly()
    {
        QTemporaryDir dir;
        Store store(dir.path());
        store.setPunctStyle(QStringLiteral("cn"));

        // A recogniser run over silence returns a lone mark. It carries no
        // words, so it must not become a transcript.
        QVERIFY(TextPipeline::process(QStringLiteral("。"), store).isEmpty());
        QVERIFY(TextPipeline::process(QStringLiteral("，、。！？"), store).isEmpty());
        QVERIFY(TextPipeline::process(QStringLiteral("  . "), store).isEmpty());

        // Real speech of any kind survives, digits included.
        QCOMPARE(TextPipeline::process(QStringLiteral("你好。"), store), QStringLiteral("你好。"));
        QCOMPARE(TextPipeline::process(QStringLiteral("3.14"), store), QStringLiteral("3.14"));
    // cn style is the crostivoice habit: ASCII sentence marks become
    // full-width; digits keep their decimal/thousands separators.
    // "Yeah." -> "Yeah。" is intended behaviour, not a bug.
    QCOMPARE(TextPipeline::process(QStringLiteral("Yeah."), store), QStringLiteral("Yeah\u3002"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(VoxTests)
#include "vox_tests.moc"
