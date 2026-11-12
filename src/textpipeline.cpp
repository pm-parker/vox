#include "textpipeline.h"

#include "store.h"

#include <QChar>
#include <QRegularExpression>

namespace {

bool isCJK(QChar c) {
    const ushort u = c.unicode();
    return (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF) || (u >= 0xFF00 && u <= 0xFFEF);
}

// A transcript made of nothing but punctuation carries no information: the
// recogniser returns a lone mark (e.g. "。") for silence and noise. Such a
// take must not enter the history, and above all must not be typed into the
// window the user is editing — the old gate only rejected a completely empty
// string, so "。" was happily inserted.
bool hasSpeakableContent(const QString &text) {
    for (const QChar c : text) {
        if (c.isLetterOrNumber())
            return true;
    }
    return false;
}

} // namespace

namespace TextPipeline {

QString fixSpacing(const QString &text) {
    QString out;
    out.reserve(text.size() + 8);
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const bool cjk = isCJK(c);
        const bool asciiWord = c.isLetterOrNumber() || c == '\'';
        if (!out.isEmpty() && asciiWord && c != ' ') {
            const QChar prev = out.at(out.size() - 1);
            const bool prevCjk = isCJK(prev);
            // Only between different systems, never doubled, and not before
            // punctuation that hugs the previous word.
            if (prevCjk && !cjk)
                out += ' ';
        }
        if (!out.isEmpty() && cjk) {
            const QChar prev = out.at(out.size() - 1);
            if (prev.isLetterOrNumber() && !isCJK(prev))
                out += ' ';
        }
        out += c;
    }
    // CJK text carries no trailing/leading or doubled spaces.
    static const QRegularExpression doubled(" {2,}");
    QString trimmed = out;
    trimmed.replace(doubled, " ");
    while (trimmed.startsWith(' '))
        trimmed.remove(0, 1);
    while (trimmed.endsWith(' '))
        trimmed.chop(1);
    return trimmed;
}

QString applyPunctStyle(const QString &text, bool chineseStyle) {
    QString out = text;
    struct Rule {
        QChar cn;
        char en;
    };
    static const Rule rules[] = {
        {QChar(0xFF0C), ','},   // ，
        {QChar(0x3002), '.'},   // 。
        {QChar(0xFF1F), '?'},   // ？
        {QChar(0xFF01), '!'},   // ！
        {QChar(0xFF1A), ':'},   // ：
        {QChar(0xFF1B), ';'},   // ；
    };
    if (chineseStyle) {
        // A dot or comma between digits is a decimal point or a thousands
        // separator, not a sentence mark: the recogniser already
        // inverse-normalised "3.14" and "1,000", and turning them into "3。14"
        // would undo exactly that. Hide them from the conversion and put
        // them back afterwards.
        static const QRegularExpression decimalPoint("(?<=\\d)\\.(?=\\d)");
        static const QRegularExpression thousandsSep("(?<=\\d),(?=\\d)");
        out.replace(decimalPoint, QLatin1String("\x01"));
        out.replace(thousandsSep, QLatin1String("\x02"));
    }
    for (const Rule &rule : rules) {
        if (chineseStyle)
            out.replace(QLatin1Char(rule.en), rule.cn);
        else
            out.replace(rule.cn, QLatin1Char(rule.en));
    }
    if (chineseStyle) {
        out.replace(QLatin1Char('\x01'), QLatin1Char('.'));
        out.replace(QLatin1Char('\x02'), QLatin1Char(','));
    }
    return out;
}

QString capitalizeSentences(const QString &text) {
    QString out = text;
    bool atStart = true;
    for (int i = 0; i < out.size(); ++i) {
        const QChar c = out.at(i);
        if (atStart && c.isLetter()) {
            // Only case ASCII letters; accented or script letters pass through.
            if (c.toLatin1() != '\0')
                out[i] = c.toUpper();
            atStart = false;
        } else if (c == '.' || c == '?' || c == '!' || c == QChar(0x3002) || c == QChar(0xFF1F) || c == QChar(0xFF01)) {
            atStart = true;
        } else if (!c.isSpace()) {
            atStart = false;
        }
    }
    return out;
}

QString process(const QString &text, const Store &store) {
    if (text.trimmed().isEmpty())
        return text;
    QString out = store.applyDictionary(text);
    out = fixSpacing(out);
    out = applyPunctStyle(out, store.punctStyle() != "en");
    out = capitalizeSentences(out);
    if (!hasSpeakableContent(out))
        return QString();
    return out;
}

} // namespace TextPipeline
