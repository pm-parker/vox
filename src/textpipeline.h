#pragma once

#include <QString>

class Store;

// The deterministic half of crostivoice's text pipeline, ported to C++:
// hot words, punctuation style, CJK/ASCII spacing, and English sentence
// capitalization. The neural half (punctuation model) lives in the engine;
// these rules always run, on every result, with or without models — the same
// "rule engine fallback, input never interrupted" contract.
namespace TextPipeline {

// Full pass: dictionary -> spacing -> punctuation style -> capitalization.
QString process(const QString &text, const Store &store);

// A space between a CJK run and a Latin/digit run reads correctly in both
// writing systems; crostivoice inserted them automatically.
QString fixSpacing(const QString &text);

// Convert the punctuation marks between the two styles, leaving content
// untouched. Only the "wall" characters map; quotes stay as spoken.
QString applyPunctStyle(const QString &text, bool chineseStyle);

// Capitalize the first ASCII letter after a sentence end. Chinese needs no
// case; mixed text still benefits ("hello. next" -> "Hello. Next").
QString capitalizeSentences(const QString &text);

} // namespace TextPipeline
