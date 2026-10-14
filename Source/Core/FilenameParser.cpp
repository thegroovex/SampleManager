#include "FilenameParser.h"

namespace sm::filename
{
juce::StringArray tokenize (const juce::String& text)
{
    juce::StringArray out;
    juce::String cur;
    juce::juce_wchar prev = 0;
    auto flush = [&] { if (cur.isNotEmpty()) out.add (cur.toLowerCase()); cur.clear(); };

    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
    {
        auto c = *p;
        auto nextP = p; ++nextP;
        const auto next = nextP.isEmpty() ? (juce::juce_wchar) 0 : *nextP;
        const bool alnum = juce::CharacterFunctions::isLetterOrDigit (c) || c == '#';
        if (! alnum) { flush(); prev = 0; continue; }
        const bool isUpper = juce::CharacterFunctions::isUpperCase (c);
        const bool isDigit = juce::CharacterFunctions::isDigit (c);
        if (prev != 0)
        {
            const bool prevLower = juce::CharacterFunctions::isLowerCase (prev);
            const bool prevDigit = juce::CharacterFunctions::isDigit (prev);
            // camelCase boundary and letter/digit boundary
            // camelCase: "HiHat" splits, "DnB" (upper letter at the end) stays one word
            if ((isUpper && prevLower && juce::CharacterFunctions::isLowerCase (next)) || (isDigit != prevDigit && c != '#' && prev != '#'))
                flush();
        }
        cur += c;
        prev = c;
    }
    flush();
    return out;
}

static double findBpm (const juce::String& raw)
{
    auto s = raw.toLowerCase();
    // 1) explicit "128bpm", "128 bpm", "bpm128", "128_bpm"
    for (int i = 0; i < s.length(); ++i)
    {
        if (s.substring (i).startsWith ("bpm"))
        {
            // number before
            int j = i - 1;
            while (j >= 0 && (s[j] == ' ' || s[j] == '_' || s[j] == '-')) --j;
            int end = j + 1;
            while (j >= 0 && (juce::CharacterFunctions::isDigit (s[j]) || s[j] == '.')) --j;
            auto num = s.substring (j + 1, end).getDoubleValue();
            if (num >= 50 && num <= 250) return num;
            // number after
            int k = i + 3;
            while (k < s.length() && (s[k] == ' ' || s[k] == '_' || s[k] == '-')) ++k;
            int st = k;
            while (k < s.length() && juce::CharacterFunctions::isDigit (s[k])) ++k;
            num = s.substring (st, k).getDoubleValue();
            if (num >= 50 && num <= 250) return num;
        }
    }
    // 2) a standalone token of 2-3 digits between 60 and 200, e.g. "Loop_128_Am"
    auto tokens = juce::StringArray::fromTokens (s, " _-().[]", "");
    for (auto& t : tokens)
        if (t.length() >= 2 && t.length() <= 3 && t.containsOnly ("0123456789"))
        {
            auto v = t.getIntValue();
            if (v >= 60 && v <= 200) return v;
        }
    return 0.0;
}

static MusicalKey findKey (const juce::String& raw)
{
    // Candidates are separate tokens: "Am", "F#m", "Cmaj", "C#min", "Bb", "(F#)", "A_minor", "8A"
    auto tokens = juce::StringArray::fromTokens (raw, " _-().[],", "");
    for (int i = tokens.size(); --i >= 0;)  // keys are usually near the end
    {
        auto t = tokens[i].trim();
        if (t.isEmpty() || t.length() > 6) continue;
        auto first = t[0];
        const bool noteLetter = (first >= 'A' && first <= 'G') || (first >= 'a' && first <= 'g' && t.length() >= 2);
        if (! noteLetter) continue;
        // merge with following "minor"/"major"/"min"/"maj"
        auto next = tokens[i + 1].toLowerCase();
        juce::String cand = t;
        if (next == "minor" || next == "min" || next == "moll") cand << "m";
        else if (next == "major" || next == "maj" || next == "dur") cand << "maj";

        // a bare single letter ("Snare_A") is usually a variation, not a key
        if (t.length() == 1 && cand == t) continue;

        // reject common words that start with a note letter
        static const juce::StringArray stop { "bass", "back", "beat", "face", "fade", "deep", "drum", "gate", "dead", "aged", "add", "bad",
                                              "cab", "dab", "egg", "fab", "gag", "ace", "bed", "bee", "cafe", "edge", "feed", "def", "dec", "efx", "fx" };
        if (stop.contains (t.toLowerCase())) continue;

        auto k = MusicalKey::fromString (cand);
        if (k.isValid())
        {
            // lower-case letter only accepted with explicit mode marker ("am" is ambiguous, "amin" ok)
            if (juce::CharacterFunctions::isLowerCase (first) && ! (cand.containsIgnoreCase ("min") || cand.containsIgnoreCase ("maj")))
                continue;
            return k;
        }
    }
    return {};
}

FilenameHints parse (const juce::String& name, const juce::String& parentFolders)
{
    FilenameHints h;
    h.bpm = findBpm (name);
    h.key = findKey (name);
    auto lower = (name + " " + parentFolders).toLowerCase();
    auto nameLower = name.toLowerCase();
    if (nameLower.contains ("one shot") || nameLower.contains ("oneshot") || nameLower.contains ("one-shot")
        || nameLower.contains ("one_shot") || lower.contains ("one shots") || lower.contains ("oneshots")
        || lower.contains ("one_shots") || lower.contains ("one-shots"))
        h.typeHint = 2;
    else if (nameLower.contains ("loop") || lower.contains ("loops"))
        h.typeHint = 1;
    return h;
}
}
