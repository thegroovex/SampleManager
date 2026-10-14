#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace sm
{
/** A rendered (already processed) sample ready for playback at the output rate. */
struct RenderedSample : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RenderedSample>;
    juce::AudioBuffer<float> buffer;    // 2 channels at outputRate
    double outputRate = 44100.0;
    bool isLoop = false;
    double lengthInBeats = 0.0;         // for synced loops, 0 otherwise
    juce::int64 sampleId = 0;
    int generation = 0;
};

/** Where the music is: host transport, Ableton Link or the internal clock. */
struct TransportState
{
    bool   valid = false;       // tempo known
    bool   playing = false;     // transport running
    double bpm = 120.0;
    double beatAtBlockStart = 0.0; // quarter notes
};

/** Real-time player: one preview voice plus polyphonic MIDI-triggered voices.
    Runs entirely on the audio thread, lock-free except a try-lock for the buffer swap. */
class PreviewEngine
{
public:
    enum class Quantize { Off = 0, Beat = 1, Bar = 2 };

    PreviewEngine();

    void prepare (double sampleRate, int maxBlockSize);

    // Message / worker threads (never the audio thread) ------------------------------------------------------
    void setSample (RenderedSample::Ptr s, bool keepPlaying);
    RenderedSample::Ptr getSample() const;
    /** Frees rendered buffers the audio thread has let go of (call from a timer). */
    void releaseUnused();
    void play();
    void stop();
    bool isPlaying() const noexcept { return playing.load() || pendingStart.load(); }
    void setLooping (bool b) noexcept { looping = b; }
    bool isLooping() const noexcept { return looping.load(); }
    void setQuantize (Quantize q) noexcept { quantize = (int) q; }
    void setMidiRootNote (int note) noexcept { midiRoot = note; }
    void setVolume (float g) noexcept { volume = g; }
    /** 0..1 position of the preview voice for the UI (-1 when stopped). */
    float getPlayheadNormalised() const noexcept { return playhead.load(); }
    float getPeakLeft()  noexcept { return peakL.exchange (0.0f); }
    float getPeakRight() noexcept { return peakR.exchange (0.0f); }

    // Audio thread --------------------------------------------------------
    void process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const TransportState& t);

    /** Pure helper (tests): samples until the next quantise boundary, or -1 if none within 'blockLength'. */
    static int samplesUntilBoundary (double beat, double bpm, double sampleRate, double unitBeats, int blockLength);

private:
    struct Voice
    {
        bool active = false;
        int note = -1;
        double pos = 0.0, increment = 1.0;
        float gain = 1.0f, release = 1.0f, releaseStep = 0.0f;
        bool releasing = false;
    };

    void renderVoice (Voice& v, const RenderedSample& s, juce::AudioBuffer<float>& out, int start, int num, bool loop);
    void startPreviewAt (double sourcePos);

    double sampleRate = 44100.0;
    juce::SpinLock swapLock;
    RenderedSample::Ptr current, pending;
    juce::ReferenceCountedArray<RenderedSample> retained; // never touched by the audio thread
    juce::CriticalSection retainedLock;
    std::atomic<bool> hasPending { false };

    std::atomic<bool> playing { false }, pendingStart { false }, stopRequested { false }, looping { true };
    std::atomic<bool> keepPositionOnSwap { false };
    std::atomic<int> quantize { 0 }, midiRoot { 60 };
    std::atomic<float> volume { 1.0f }, playhead { -1.0f }, peakL { 0.0f }, peakR { 0.0f };

    Voice preview;
    std::array<Voice, 16> voices;
};
}
