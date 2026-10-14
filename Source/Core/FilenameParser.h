#pragma once
#include "MusicTheory.h"

namespace sm
{
/** Extracts hints that sample vendors put into file and folder names:
    "Loop_128_Am_Bass.wav", "Kick 01 (F#).wav", "125bpm", "Cmaj", "one shot" ... */
struct FilenameHints
{
    double bpm = 0.0;        // 0 = none found
    MusicalKey key;          // invalid = none found
    int typeHint = 0;        // 0 unknown, 1 loop, 2 one-shot
};

namespace filename
{
    FilenameHints parse (const juce::String& fileNameWithoutExtension, const juce::String& parentFolders = {});
    /** Lower-case word tokens, splitting on separators and camelCase ("HiHatOpen" -> hi hat open). */
    juce::StringArray tokenize (const juce::String& text);
}
}
