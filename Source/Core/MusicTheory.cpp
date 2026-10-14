#include "MusicTheory.h"

namespace sm
{
static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

juce::String theory::pitchClassName (int pc, bool preferFlats)
{
    if (pc < 0 || pc > 11) return {};
    return preferFlats ? flatNames[pc] : sharpNames[pc];
}

juce::String MusicalKey::toString() const
{
    if (! isValid()) return {};
    return theory::pitchClassName (root) + (mode == Minor ? "m" : "");
}

juce::String MusicalKey::toCamelot() const
{
    if (! isValid() || mode == Note) return {};
    // Camelot: 8B = C major, 8A = A minor. Moving a fifth up adds 1.
    static const int majorNum[] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 };
    if (mode == Major) return juce::String (majorNum[root]) + "B";
    const int relMajor = (root + 3) % 12;
    return juce::String (majorNum[relMajor]) + "A";
}

MusicalKey MusicalKey::fromInt (int v) noexcept
{
    MusicalKey k;
    if (v < 0 || v >= 36) return k;
    k.root = v % 12;
    k.mode = (Mode) (v / 12);
    return k;
}

MusicalKey MusicalKey::fromString (const juce::String& textIn)
{
    MusicalKey k;
    auto text = textIn.trim();
    if (text.isEmpty()) return k;

    // Camelot notation: 1A..12B
    {
        auto digits = text.dropLastCharacters (1);
        auto last = juce::CharacterFunctions::toUpperCase (text.getLastCharacter());
        if (text.length() >= 2 && text.length() <= 3 && digits.containsOnly ("0123456789")
            && (last == 'A' || last == 'B'))
        {
            const int num = digits.getIntValue();
            static const int majorNum[] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 };
            for (int pc = 0; pc < 12; ++pc)
                if (majorNum[pc] == num)
                {
                    if (last == 'B') { k.root = pc; k.mode = Major; }
                    else             { k.root = (pc + 9) % 12; k.mode = Minor; }
                    return k;
                }
            return k;
        }
    }

    auto c = juce::CharacterFunctions::toUpperCase (text[0]);
    static const int base[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    if (c < 'A' || c > 'G') return k;
    int pc = base[c - 'A'];
    int i = 1;
    if (i < text.length())
    {
        auto acc = text[i];
        if (acc == '#' || acc == 0x266F) { ++pc; ++i; }
        else if (acc == 'b' || acc == 0x266D)
        {
            // "b" could also start "bass"... only treat as flat if followed by mode text or end
            auto rest = text.substring (i + 1).trim().toLowerCase();
            if (rest.isEmpty() || rest.startsWith ("m") || rest.startsWith ("min") || rest.startsWith ("maj"))
            { --pc; ++i; }
        }
    }
    pc = (pc + 12) % 12;
    auto rest = text.substring (i).trim().toLowerCase();
    k.root = pc;
    if (rest.isEmpty())                                         k.mode = Major;
    else if (rest.startsWith ("maj") || rest == "major" || rest == "dur") k.mode = Major;
    else if (rest.startsWith ("min") || rest == "m" || rest == "moll" || rest.startsWith ("m ")) k.mode = Minor;
    else { k.root = -1; }
    return k;
}

int theory::semitonesBetween (const MusicalKey& from, const MusicalKey& to)
{
    if (! from.isValid() || ! to.isValid()) return 0;
    auto rootOf = [] (const MusicalKey& k, bool wantMinorFrame)
    {
        // Convert to a comparable root: compare relative keys (Am ~ C) when modes differ.
        if (k.mode == MusicalKey::Minor && ! wantMinorFrame) return (k.root + 3) % 12;
        if (k.mode != MusicalKey::Minor && wantMinorFrame)   return (k.root + 9) % 12;
        return k.root;
    };
    const bool minorFrame = (to.mode == MusicalKey::Minor);
    int a = rootOf (from, minorFrame);
    int b = rootOf (to, minorFrame);
    if (from.mode == MusicalKey::Note || to.mode == MusicalKey::Note) { a = from.root; b = to.root; }
    int d = ((b - a) % 12 + 12) % 12;
    if (d > 6) d -= 12;
    if (d == 6) d = -6;
    return d;
}

juce::Array<MusicalKey> theory::allKeys()
{
    juce::Array<MusicalKey> keys;
    for (int m = 0; m < 2; ++m)
        for (int pc = 0; pc < 12; ++pc)
            keys.add ({ pc, (MusicalKey::Mode) m });
    return keys;
}
} // namespace sm
