#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace sm
{
/** Language, behaviour, MIDI learn and (standalone) audio settings. */
class SettingsPanel : public juce::Component, private juce::Timer
{
public:
    explicit SettingsPanel (SampleManagerProcessor&);
    ~SettingsPanel() override;

    std::function<void()> onLanguageChanged;
    std::function<void()> onAudioSettings;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshMidi();

    SampleManagerProcessor& proc;
    juce::Label langLabel, midiLabel, dbLabel, versionLabel;
    juce::ComboBox languageBox;
    juce::ToggleButton autoPlay, rescanOnStart;
    juce::TextButton showDb, audioSettings, rescanAll, reanalyseAll;
    struct MidiRow { MidiLearn::Action action; std::unique_ptr<juce::Label> name, cc; std::unique_ptr<juce::TextButton> learn, clear; };
    std::vector<MidiRow> midiRows;
};
}
