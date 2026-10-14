#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace sm
{
/** Small stereo level meter. */
class LevelMeter : public juce::Component
{
public:
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;
private:
    float levelL = 0, levelR = 0;
};

/** The handle you drag into the DAW (raw or processed sample). */
class DragHandle : public juce::Component
{
public:
    std::function<void()> onDragStart;
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { started = false; }
private:
    bool started = false;
};

/** Transport + sound shaping controls under the waveform. */
class ControlStrip : public juce::Component
{
public:
    explicit ControlStrip (SampleManagerProcessor&);

    void refreshFromSettings();
    void updateSampleInfo (const SampleRow* row);
    void setPlaying (bool);
    LevelMeter& getMeter() { return meter; }
    DragHandle& getDragHandle() { return dragHandle; }
    juce::ComboBox& getSnapBox() { return snapBox; }

    std::function<void()> onSettingsChanged; // already applied to the processor
    std::function<void()> onPlayStop;
    std::function<void (double)> onZoom;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Knob : juce::Component
    {
        explicit Knob (const juce::String& name);
        juce::Slider slider;
        juce::Label label;
        void resized() override;
    };

    void apply();

    SampleManagerProcessor& proc;
    juce::TextButton playButton { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::TextButton loopButton { "Loop" };
    juce::ComboBox directionBox, quantizeBox, targetKeyBox, snapBox, dragModeBox;
    juce::ToggleButton syncTempoButton, keepPitchButton, syncKeyButton, normalizeButton;
    juce::TextButton zoomIn { "+" }, zoomOut { "-" };
    Knob transpose { "Transpose" }, highPass { "HP" }, lowPass { "LP" }, fadeIn { "Fade In" }, fadeOut { "Fade Out" }, gain { "Gain" };
    juce::Label infoLabel;
    LevelMeter meter;
    DragHandle dragHandle;
    bool updating = false;
};
}
