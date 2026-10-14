#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

namespace sm
{
/** All the sound shaping applied to a preview, also used when dragging the
    processed sample into the DAW, so what you hear is what you drop. */
struct ProcessSettings
{
    enum Direction { Forward = 0, Reverse = 1, PingPong = 2 };

    double startNorm = 0.0, endNorm = 1.0;   // trim, 0..1 of the source
    Direction direction = Forward;
    double sourceBpm = 0.0;                  // detected/edited tempo of the sample
    double targetBpm = 0.0;                  // project tempo; stretching happens when both > 0
    bool   keepPitch = true;                 // true: time-stretch, false: varispeed (repitch)
    double transposeSemitones = 0.0;         // includes key sync
    double highPassHz = 0.0;                 // 0 = off
    double lowPassHz = 0.0;                  // 0 = off
    double fadeInMs = 0.0, fadeOutMs = 0.0;
    double gainDb = 0.0;
    bool   normalize = false;

    bool operator== (const ProcessSettings& o) const;
    bool operator!= (const ProcessSettings& o) const { return ! (*this == o); }
    double tempoRatio() const;               // >1 = faster
    bool isNeutral() const;
};

class SampleRenderer
{
public:
    /** Loads a file into a float buffer (max 2 channels). */
    static bool load (juce::AudioFormatManager& fm, const juce::File& f, juce::AudioBuffer<float>& out,
                      double& sampleRate, double maxSeconds = 600.0);

    /** Renders the processed sample at outSampleRate. Always returns 2 channels. */
    static juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& source, double sourceRate,
                                            const ProcessSettings& s, double outSampleRate);

    /** Pitch/tempo change via SoundTouch. tempo > 1 = shorter, rate scales pitch and length. */
    static juce::AudioBuffer<float> stretch (const juce::AudioBuffer<float>& in, double sampleRate,
                                             double tempo, double pitchSemitones, double rate);

    static bool writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& dest, int bitDepth = 24);

    /** Applies settings in-place helpers (exposed for tests). */
    static void applyFades (juce::AudioBuffer<float>& b, double sampleRate, double inMs, double outMs);
    static void applyFilters (juce::AudioBuffer<float>& b, double sampleRate, double hpHz, double lpHz);
    static void normalise (juce::AudioBuffer<float>& b, float targetPeak = 0.989f);
};
}
