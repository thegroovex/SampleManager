#pragma once
#include <juce_core/juce_core.h>

namespace sm
{
/** A musical key. root = pitch class 0..11 (C..B), -1 = unknown.
    mode: Major, Minor or Note (a single pitched one-shot without mode). */
struct MusicalKey
{
    enum Mode { Major = 0, Minor = 1, Note = 2 };

    int root = -1;
    Mode mode = Major;

    bool isValid() const noexcept { return root >= 0 && root < 12; }
    bool operator== (const MusicalKey& o) const noexcept { return root == o.root && mode == o.mode; }
    bool operator!= (const MusicalKey& o) const noexcept { return ! (*this == o); }

    /** "C", "C#m", "F#" ... */
    juce::String toString() const;
    /** Camelot wheel code like "8A" (empty for Note/unknown). */
    juce::String toCamelot() const;
    /** Parses "Am", "A min", "A minor", "F#maj", "Bb", "Gbm", "8A" ... */
    static MusicalKey fromString (const juce::String& text);

    /** Stores as single int for the database: -1 unknown, else root + 12*mode. */
    int toInt() const noexcept { return isValid() ? root + 12 * (int) mode : -1; }
    static MusicalKey fromInt (int v) noexcept;
};

namespace theory
{
    juce::String pitchClassName (int pc, bool preferFlats = false);
    /** Smallest signed semitone shift (-6..+5) that moves 'from' onto 'to' (mode-aware: relative keys share material). */
    int semitonesBetween (const MusicalKey& from, const MusicalKey& to);
    /** Returns the 24 keys + 12 notes as list for menus. */
    juce::Array<MusicalKey> allKeys();
    /** Pitch class for a MIDI note number. */
    inline int pitchClassOf (int midiNote) { return ((midiNote % 12) + 12) % 12; }
}
} // namespace sm
