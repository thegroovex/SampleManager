#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/AudioAnalyzer.h"

namespace sm
{
/** Spectrally coloured waveform (bass blue, mids orange, highs white) with
    zoom, trim handles, snapping (zero crossing / note division) and playhead. */
class WaveformView : public juce::Component
{
public:
    enum class Snap { Off = 0, ZeroCrossing = 1, Sixteenth = 2, Beat = 3 };

    WaveformView() = default;

    void setOverview (const WaveformOverview& o);
    void setAudio (std::shared_ptr<const juce::AudioBuffer<float>> audio, double sampleRate, double bpm);
    void clear();
    void setTrim (double startNorm, double endNorm);
    void setPlayhead (float normalisedWithinTrim);
    void setSnap (Snap s) { snap = s; }
    Snap getSnap() const { return snap; }
    void zoomBy (double factor, double anchorNorm = -1.0);
    void resetZoom();

    std::function<void (double start, double end)> onTrimChanged;
    std::function<void()> onClicked;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scale) override;

private:
    double xToNorm (float x) const;
    float normToX (double n) const;
    double applySnap (double norm) const;
    juce::Colour colourAt (double norm) const;

    WaveformOverview overview;
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double audioRate = 44100.0, bpm = 0.0;
    double trimStart = 0.0, trimEnd = 1.0;
    double viewStart = 0.0, viewEnd = 1.0;
    float playhead = -1.0f;
    Snap snap = Snap::ZeroCrossing;
    enum class Drag { None, Start, End, Pan } dragging = Drag::None;
    double dragAnchorView = 0.0;
    float dragAnchorX = 0;
};
}
