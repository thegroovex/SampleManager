#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SharedLibrary.h"
#include "Audio/PreviewEngine.h"
#include "Audio/SampleRenderer.h"
#include "Audio/MidiLearn.h"
#include "Audio/TempoSource.h"

namespace sm
{
/** What the user dialled in for previewing (independent of the sample). */
struct PreviewSettings
{
    bool   syncTempo = true;
    bool   keepPitch = true;
    bool   syncKey = false;
    MusicalKey targetKey { 0, MusicalKey::Minor };
    double transpose = 0.0;
    ProcessSettings::Direction direction = ProcessSettings::Forward;
    double highPassHz = 0.0, lowPassHz = 0.0;
    double fadeInMs = 0.0, fadeOutMs = 0.0;
    double gainDb = 0.0;
    bool   normalize = false;
    bool   loop = true;
    int    quantize = 1;        // 0 off, 1 beat, 2 bar
    bool   autoPlay = true;
    bool   dragProcessed = false;

    juce::var toVar() const;
    void fromVar (const juce::var&);
};

class SampleManagerProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    SampleManagerProcessor();
    ~SampleManagerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SampleManager"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---------------------------------------------------------------------
    Database& getDatabase() { return shared->db; }
    LibraryScanner& getScanner() { return shared->scanner; }
    PreviewEngine& getEngine() { return engine; }
    MidiLearn& getMidiLearn() { return midiLearn; }
    TempoSource& getTempoSource() { return tempoSource; }
    juce::AudioFormatManager& getFormats() { return formats; }

    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    /** Tempo the previews sync to: host, Link or manual. */
    double getProjectTempo() const { return lastTempo.load(); }
    bool hostTransportPlaying() const { return lastPlaying.load(); }

    /** Selects a sample for preview; loads and renders it in the background. */
    void selectSample (const SampleRow& row, bool play);
    const SampleRow& getSelectedSample() const { return selected; }
    bool hasSelection() const { return selected.id != 0; }
    /** The loaded source audio (for drawing). */
    std::shared_ptr<const juce::AudioBuffer<float>> getSourceAudio() const;
    double getSourceSampleRate() const { return sourceRate; }

    PreviewSettings& getPreviewSettings() { return settings; }
    /** Call after changing settings or the trim range. */
    void settingsChanged();
    void setTrim (double startNorm, double endNorm);
    double getTrimStart() const { return trimStart; }
    double getTrimEnd() const { return trimEnd; }

    ProcessSettings buildProcessSettings (const SampleRow& row) const;
    /** Writes the processed version of the selected sample to a temp WAV for drag & drop. */
    juce::File renderSelectedToFile();

    void togglePlay();

    /** UI state of this instance (tabs etc.), restored with the DAW project. */
    juce::String uiStateJson;
    int editorWidth = 1180, editorHeight = 760;

    /** Fired on the message thread for MIDI-learned actions the UI handles (random, next ...). */
    std::function<void (MidiLearn::Action, float)> onMidiAction;
    /** Fired on the message thread when the selected sample's audio finished loading. */
    std::function<void()> onSampleLoaded;

private:
    void timerCallback() override;
    void requestRender (bool keepPlaying);
    void loadSettingsFromDatabase();
    void saveSettingsToDatabase();
    RenderedSample::Ptr makeRendered (const juce::AudioBuffer<float>& src, double rate, const ProcessSettings& ps,
                                      double outRate, const SampleRow& row, int gen) const;

    juce::SharedResourcePointer<SharedLibrary> shared;
    juce::AudioFormatManager formats;
    PreviewEngine engine;
    MidiLearn midiLearn;
    TempoSource tempoSource;
    juce::ThreadPool renderPool { juce::ThreadPoolOptions().withNumberOfThreads (1).withThreadName ("SM Render") };

    PreviewSettings settings;
    SampleRow selected;
    double trimStart = 0.0, trimEnd = 1.0;
    std::shared_ptr<juce::AudioBuffer<float>> source;
    double sourceRate = 44100.0;
    juce::CriticalSection sourceLock;

    std::atomic<int> renderGeneration { 0 };
    std::atomic<double> lastTempo { 120.0 };
    std::atomic<bool> lastPlaying { false };
    std::atomic<double> currentSampleRate { 44100.0 };
    double renderedTempo = 0.0;
    juce::uint32 tempoChangedAt = 0;
    std::atomic<bool> settingsDirty { false };
    juce::uint32 settingsChangedAt = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (SampleManagerProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleManagerProcessor)
};
}
