#include "textpipeline.h"

#include "store.h"

#include <QChar>
#include <QRegularExpression>

namespace {

bool isCJK(QChar c) {
    const ushort u = c.unicode();
    // Ideographs, kana and Hangul syllables only. Full-width punctuation
    // (FF00–FFEF) must NOT count: it hugs the preceding word, and counting
    // it as CJK used to put a space in “你好，world” before “world”.
    return (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF)
           || (u >= 0xAC00 && u <= 0xD7AF);
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
            // Only between word characters of the two systems (the
            // isLetterOrNumber guards), never doubled, and never around
            // punctuation — ，。！ hug their neighbours in Chinese too.
            if (isCJK(prev) && prev.isLetterOrNumber() && !cjk)
                out += ' ';
        }
        if (!out.isEmpty() && cjk && c.isLetterOrNumber()) {
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

// Spans that punctuation conversion must never touch: URLs, bare domains,
// file names, version numbers, clock times, and digit groups (3.14, 1,000).
// Chinese mode maps every ASCII mark to a full-width twin, which used to
// shred “example.com” into “example。com” and “10:30” into “10：30”. Each
// match is lifted into a \x01<index>\x02 placeholder before the rules run
// and put back afterwards; those control bytes never occur in a transcript,
// so they cannot collide with real text.
const QRegularExpression &protectedSpan() {
    static const QRegularExpression regex(
        "(?:https?://|www\\.)[^\\s，。；！？、]+"                                  // full URLs
        "|[\\w.-]+\\.(?:com|net|org|io|dev|ai|cn|edu|gov|co|app|me|info|xyz)(?![\\w.])" // bare domains
        "|\\b[\\w-]+\\.(?:js|ts|tsx|py|cpp|hpp|h|c|go|rs|md|json|ya?ml|toml|html|css|sh|txt|xml|onnx|wav|tar)\\b" // file names
        "|\\d+(?:\\.\\d+){1,3}"                                                    // versions & decimals
        "|\\d{1,2}(?::\\d{2}){1,2}"                                                // clock times
        "|(?<=\\d),(?=\\d)",                                                       // thousands separators
        QRegularExpression::CaseInsensitiveOption);
    return regex;
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
    if (!chineseStyle) {
        // Half-width output never rewrites ASCII runs, so only the CJK
        // marks need converting — nothing to protect here.
        for (const Rule &rule : rules)
            out.replace(rule.cn, QLatin1Char(rule.en));
        return out;
    }

    // Chinese mode: lift the protected spans out, convert what remains.
    QStringList lifted;
    lifted.reserve(4);
    QString masked;
    masked.reserve(text.size() + 8);
    qsizetype last = 0;
    auto spans = protectedSpan().globalMatch(text);
    while (spans.hasNext()) {
        const auto match = spans.next();
        masked += text.mid(last, match.capturedStart() - last);
        masked += QChar(0x01) + QString::number(lifted.size()) + QChar(0x02);
        lifted.append(match.captured(0));
        last = match.capturedEnd();
    }
    masked += text.mid(last);
    for (const Rule &rule : rules)
        masked.replace(QLatin1Char(rule.en), rule.cn);

    static const QRegularExpression placeholder("\x01(\\d+)\x02");
    QString restored;
    restored.reserve(masked.size());
    qsizetype pos = 0;
    auto placeholders = placeholder.globalMatch(masked);
    while (placeholders.hasNext()) {
        const auto match = placeholders.next();
        restored += masked.mid(pos, match.capturedStart() - pos);
        restored += lifted.at(match.captured(1).toInt());
        pos = match.capturedEnd();
    }
    restored += masked.mid(pos);
    return restored;
}

QString capitalizeSentences(const QString &text) {
    QString out = text;
    bool atStart = true;
    bool afterAsciiMark = false;  // a . ? ! just passed
    bool afterFullMark = false;   // 。？！ just passed
    bool spaceSinceMark = false;
    for (int i = 0; i < out.size(); ++i) {
        const QChar c = out.at(i);
        if (c.isSpace()) {
            spaceSinceMark = spaceSinceMark || afterAsciiMark || afterFullMark;
            continue;
        }
        if (c.isLetter()) {
            // Capitalize at the start of the text, and after a sentence
            // mark — but only when the mark reads as a sentence end:
            // full-width marks never live inside identifiers, while an
            // ASCII dot needs whitespace after it, so "example.com" and
            // "Node.js" keep their case. ANY letter closes the window:
            // "main.js done" must not capitalize "done".
            if ((atStart || afterFullMark || (afterAsciiMark && spaceSinceMark))
                && c.toLatin1() != '\0')
                out[i] = c.toUpper();
            atStart = false;
            afterFullMark = false;
            afterAsciiMark = false;
            spaceSinceMark = false;
        } else if (c == '.' || c == '?' || c == '!') {
            afterAsciiMark = true;
            spaceSinceMark = false;
        } else if (c == QChar(0x3002) || c == QChar(0xFF1F) || c == QChar(0xFF01)) {
            afterFullMark = true;
            spaceSinceMark = false;
        } else {
            // Any other character (digit, quote, CJK word…) closes the
            // sentence window — this is what keeps versions like 2.3.3
            // from capitalizing what follows them.
            afterAsciiMark = false;
            afterFullMark = false;
            spaceSinceMark = false;
        }
        if (!c.isSpace())
            atStart = false;
    }
    return out;
}

QString process(const QString &text, const Store &store) {
    if (text.trimmed().isEmpty())
        return text;
    // Order matters: spacing, punctuation, capitalization first — hot words
    // LAST. A dictionary entry like “节点 -> Node.js” must land after the
    // punctuation pass, or that pass would immediately rewrite the very
    // word the user fixed (“Node.js” -> “Node。js”).
    QString out = fixSpacing(text);
    out = applyPunctStyle(out, store.punctStyle() != "en");
    out = capitalizeSentences(out);
    out = store.applyDictionary(out);
    if (!hasSpeakableContent(out))
        return QString();
    return out;
}

} // namespace TextPipeline
