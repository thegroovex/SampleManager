#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MusicTheory.h"
#include "FilenameParser.h"

namespace sm
{
/** Compact overview of a sample for drawing: per bin min/max and the energy
    in three frequency bands (low/mid/high), used for the spectral colouring. */
struct WaveformOverview
{
    static constexpr int numBins = 400;
    std::vector<uint8_t> data; // numBins * 5 bytes: min, max (offset 128), low, mid, high (0..255)

    bool isEmpty() const noexcept { return data.size() != (size_t) numBins * 5; }
    juce::MemoryBlock toMemoryBlock() const { return { data.data(), data.size() }; }
    static WaveformOverview fromMemoryBlock (const void* d, size_t n)
    {
        WaveformOverview w;
        if (n == (size_t) numBins * 5) w.data.assign ((const uint8_t*) d, (const uint8_t*) d + n);
        return w;
    }
};

struct AnalysisResult
{
    double durationSeconds = 0;
    double effectiveSeconds = 0;  // length without trailing silence
    double sampleRate = 0;
    int    bitDepth = 0;
    int    channels = 0;
    juce::String codec;

    double bpm = 0;               // 0 = none (one-shot)
    double bpmConfidence = 0;
    MusicalKey key;
    double keyConfidence = 0;
    bool   isLoop = false;
    float  peak = 0, rms = 0;
    float  spectralCentroid = 0;  // Hz
    float  lowRatio = 0;          // share of energy below 150 Hz
    float  attackSeconds = 0;     // time to peak
    juce::String fingerprint;     // equal for identical audio
    WaveformOverview overview;
};

struct AnalyzerOptions
{
    double maxSecondsToAnalyse = 90.0;
};

/** Pure analysis functions. Thread-safe, no global state. */
class AudioAnalyzer
{
public:
    using Options = AnalyzerOptions;

    /** Analyse an in-memory buffer. hints from the file name override weak detections. */
    static AnalysisResult analyse (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                   const FilenameHints& hints = {}, Options options = {});

    /** Reads a file with the given format manager and analyses it. Returns false if unreadable. */
    static bool analyseFile (juce::AudioFormatManager& fm, const juce::File& file,
                             AnalysisResult& result, Options options = {});

    // Building blocks (exposed for tests)
    static std::array<double, 12> chromagram (const float* mono, int numSamples, double sampleRate);
    static MusicalKey keyFromChroma (const std::array<double, 12>& chroma, bool singleNote, double* confidence = nullptr);
    static double estimateTempo (const float* mono, int numSamples, double sampleRate, double* confidence = nullptr);
    /** If the duration is a whole number of bars at a plausible tempo, returns that tempo, else 0. */
    static double tempoFromLoopLength (double seconds, double tempoGuess);
    /** Spectral-flux onset strength (local mean removed), envRate = frames per second. */
    static std::vector<double> onsetEnvelope (const float* mono, int numSamples, double sampleRate, double& envRate);
    /** Best whole-bar tempo for a loop of 'seconds' by beat-grid fit. score > ~0.25 means a clear grid. */
    static double tempoFromBeatGrid (const std::vector<double>& onset, double envRate, double seconds, double* score = nullptr);
    static juce::String fingerprint (const float* mono, int numSamples, double sampleRate);
    static WaveformOverview makeOverview (const juce::AudioBuffer<float>& buffer, double sampleRate);
    /** Finds the nearest zero crossing to 'position' (in samples) within +-searchRadius. */
    static int nearestZeroCrossing (const float* data, int numSamples, int position, int searchRadius = 2048);
};
}
