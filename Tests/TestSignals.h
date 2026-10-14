#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace sm::test
{
    /** Sum of harmonic tones for the given MIDI notes, with a gentle envelope per chord. */
    juce::AudioBuffer<float> chordProgression (const std::vector<std::vector<int>>& chords, double secondsPerChord, double sampleRate);
    /** Kick-like clicks on every beat (accent on bar start), sample exact length = bars*4 beats. */
    juce::AudioBuffer<float> drumLoop (double bpm, int bars, double sampleRate);
    /** Single decaying tone (one-shot). */
    juce::AudioBuffer<float> pluck (int midiNote, double seconds, double sampleRate);
    juce::AudioBuffer<float> noiseBurst (double seconds, double sampleRate, float decay = 30.0f);
}
