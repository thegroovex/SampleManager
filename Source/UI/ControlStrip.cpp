#include "ControlStrip.h"
#include "LookAndFeel.h"

namespace sm
{
void LevelMeter::setLevels (float l, float r)
{
    // simple decay ballistics
    levelL = juce::jmax (l, levelL * 0.85f);
    levelR = juce::jmax (r, levelR * 0.85f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (r, 2.0f);
    auto draw = [&] (juce::Rectangle<float> a, float lvl)
    {
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = a.removeFromBottom (a.getHeight() * frac);
        g.setColour (db > -1.0f ? juce::Colours::red : (db > -9.0f ? colours::accent : colours::good));
        g.fillRect (fill);
    };
    auto inner = r.reduced (2);
    draw (inner.removeFromLeft (inner.getWidth() / 2 - 1), levelL);
    draw (inner.withTrimmedLeft (2), levelR);
}

void DragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (isMouseOver() ? colours::accent.brighter (0.1f) : colours::accent);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (TRANS ("Drag to DAW"), r.toNearestInt(), juce::Justification::centred, 2);
}

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (! started && e.getDistanceFromDragStart() > 4)
    {
        started = true;
        if (onDragStart) onDragStart();
    }
}

ControlStrip::Knob::Knob (const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    label.setText (TRANS (name), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
}

void ControlStrip::Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

ControlStrip::ControlStrip (SampleManagerProcessor& p) : proc (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &loopButton, &directionBox, &quantizeBox, &targetKeyBox,
                                                             &snapBox, &dragModeBox, &syncTempoButton, &keepPitchButton, &syncKeyButton,
                                                             &normalizeButton, &zoomIn, &zoomOut, &transpose, &highPass, &lowPass,
                                                             &fadeIn, &fadeOut, &gain, &infoLabel, &meter, &dragHandle })
        addAndMakeVisible (c);

    playButton.setTooltip (TRANS ("Play / Stop (Space)"));
    playButton.onClick = [this] { if (onPlayStop) onPlayStop(); };
    loopButton.setButtonText (TRANS ("Loop"));
    loopButton.setClickingTogglesState (true);
    loopButton.setTooltip (TRANS ("Loop the preview"));

    directionBox.addItem (TRANS ("Forward"), 1);
    directionBox.addItem (TRANS ("Reverse"), 2);
    directionBox.addItem (TRANS ("Ping-Pong"), 3);
    directionBox.setTooltip (TRANS ("Playback direction"));

    quantizeBox.addItem (TRANS ("Start: Free"), 1);
    quantizeBox.addItem (TRANS ("Start: Beat"), 2);
    quantizeBox.addItem (TRANS ("Start: Bar"), 3);
    quantizeBox.setTooltip (TRANS ("Quantised start points: previews start on the next beat or bar"));

    for (auto k : theory::allKeys())
        targetKeyBox.addItem (k.toString() + "  (" + k.toCamelot() + ")", k.toInt() + 1);
    targetKeyBox.setTooltip (TRANS ("Project key"));

    snapBox.addItem (TRANS ("Snap: Off"), 1);
    snapBox.addItem (TRANS ("Snap: Zero Crossing"), 2);
    snapBox.addItem (TRANS ("Snap: 1/16"), 3);
    snapBox.addItem (TRANS ("Snap: Beat"), 4);
    snapBox.setSelectedId (2, juce::dontSendNotification);
    snapBox.setTooltip (TRANS ("Snapping for start / end points (hold Shift to move freely)"));

    dragModeBox.addItem (TRANS ("Drag: Raw"), 1);
    dragModeBox.addItem (TRANS ("Drag: Processed"), 2);
    dragModeBox.setTooltip (TRANS ("Drag the original file or render it with tempo, key, fades and filters"));

    syncTempoButton.setButtonText (TRANS ("Sync BPM"));
    syncTempoButton.setTooltip (TRANS ("Stretch loops to the project tempo"));
    keepPitchButton.setButtonText (TRANS ("Keep Pitch"));
    keepPitchButton.setTooltip (TRANS ("On: time-stretch. Off: varispeed (pitch follows tempo)"));
    syncKeyButton.setButtonText (TRANS ("Sync Key"));
    syncKeyButton.setTooltip (TRANS ("Transpose samples to the project key"));
    normalizeButton.setButtonText (TRANS ("Normalize"));

    transpose.slider.setRange (-12.0, 12.0, 1.0);
    transpose.slider.setDoubleClickReturnValue (true, 0.0);
    transpose.slider.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String ((int) v) + " st"; };
    highPass.slider.setRange (0.0, 1.0, 0.0);
    highPass.slider.setDoubleClickReturnValue (true, 0.0);
    highPass.slider.textFromValueFunction = [] (double v) { return v < 0.01 ? juce::String ("Off") : juce::String ((int) (20.0 * std::pow (1000.0, v))) + " Hz"; };
    lowPass.slider.setRange (0.0, 1.0, 0.0);
    lowPass.slider.setDoubleClickReturnValue (true, 1.0);
    lowPass.slider.textFromValueFunction = [] (double v) { return v > 0.99 ? juce::String ("Off") : juce::String ((int) (20.0 * std::pow (1000.0, v))) + " Hz"; };
    for (auto* k : { &fadeIn, &fadeOut })
    {
        k->slider.setRange (0.0, 2000.0, 1.0);
        k->slider.setSkewFactorFromMidPoint (200.0);
        k->slider.setDoubleClickReturnValue (true, 0.0);
        k->slider.textFromValueFunction = [] (double v) { return juce::String ((int) v) + " ms"; };
    }
    gain.slider.setRange (-36.0, 12.0, 0.1);
    gain.slider.setDoubleClickReturnValue (true, 0.0);
    gain.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };

    infoLabel.setFont (juce::FontOptions (12.0f));
    infoLabel.setColour (juce::Label::textColourId, colours::textDim);

    auto changed = [this] { if (! updating) apply(); };
    for (auto* b : { &syncTempoButton, &keepPitchButton, &syncKeyButton, &normalizeButton }) b->onClick = changed;
    loopButton.onClick = changed;
    for (auto* c : { &directionBox, &quantizeBox, &targetKeyBox, &dragModeBox }) c->onChange = changed;
    for (auto* k : { &transpose, &highPass, &lowPass, &fadeIn, &fadeOut, &gain }) k->slider.onValueChange = changed;
    zoomIn.onClick = [this] { if (onZoom) onZoom (1.5); };
    zoomOut.onClick = [this] { if (onZoom) onZoom (1.0 / 1.5); };
    zoomIn.setTooltip (TRANS ("Zoom in (or pinch / Cmd+wheel)"));

    refreshFromSettings();
}

void ControlStrip::refreshFromSettings()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    auto& s = proc.getPreviewSettings();
    syncTempoButton.setToggleState (s.syncTempo, juce::dontSendNotification);
    keepPitchButton.setToggleState (s.keepPitch, juce::dontSendNotification);
    syncKeyButton.setToggleState (s.syncKey, juce::dontSendNotification);
    normalizeButton.setToggleState (s.normalize, juce::dontSendNotification);
    loopButton.setToggleState (s.loop, juce::dontSendNotification);
    directionBox.setSelectedId ((int) s.direction + 1, juce::dontSendNotification);
    quantizeBox.setSelectedId (s.quantize + 1, juce::dontSendNotification);
    targetKeyBox.setSelectedId (s.targetKey.toInt() + 1, juce::dontSendNotification);
    dragModeBox.setSelectedId (s.dragProcessed ? 2 : 1, juce::dontSendNotification);
    transpose.slider.setValue (s.transpose, juce::dontSendNotification);
    highPass.slider.setValue (s.highPassHz <= 0 ? 0.0 : std::log (s.highPassHz / 20.0) / std::log (1000.0), juce::dontSendNotification);
    lowPass.slider.setValue (s.lowPassHz <= 0 ? 1.0 : std::log (s.lowPassHz / 20.0) / std::log (1000.0), juce::dontSendNotification);
    fadeIn.slider.setValue (s.fadeInMs, juce::dontSendNotification);
    fadeOut.slider.setValue (s.fadeOutMs, juce::dontSendNotification);
    gain.slider.setValue (s.gainDb, juce::dontSendNotification);
    targetKeyBox.setEnabled (s.syncKey);
}

void ControlStrip::apply()
{
    auto& s = proc.getPreviewSettings();
    s.syncTempo = syncTempoButton.getToggleState();
    s.keepPitch = keepPitchButton.getToggleState();
    s.syncKey = syncKeyButton.getToggleState();
    s.normalize = normalizeButton.getToggleState();
    s.loop = loopButton.getToggleState();
    s.direction = (ProcessSettings::Direction) (directionBox.getSelectedId() - 1);
    s.quantize = quantizeBox.getSelectedId() - 1;
    s.targetKey = MusicalKey::fromInt (targetKeyBox.getSelectedId() - 1);
    s.dragProcessed = dragModeBox.getSelectedId() == 2;
    s.transpose = transpose.slider.getValue();
    const double hp = highPass.slider.getValue(), lp = lowPass.slider.getValue();
    s.highPassHz = hp < 0.01 ? 0.0 : 20.0 * std::pow (1000.0, hp);
    s.lowPassHz = lp > 0.99 ? 0.0 : 20.0 * std::pow (1000.0, lp);
    s.fadeInMs = fadeIn.slider.getValue();
    s.fadeOutMs = fadeOut.slider.getValue();
    s.gainDb = gain.slider.getValue();
    targetKeyBox.setEnabled (s.syncKey);
    proc.settingsChanged();
    if (onSettingsChanged) onSettingsChanged();
}

void ControlStrip::updateSampleInfo (const SampleRow* row)
{
    if (row == nullptr) { infoLabel.setText ({}, juce::dontSendNotification); return; }
    juce::StringArray parts;
    parts.add (row->isLoop ? TRANS ("Loop") : TRANS ("One-Shot"));
    if (row->key.isValid()) parts.add (row->key.toString());
    if (row->bpm > 0) parts.add (juce::String (row->bpm, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " BPM");
    parts.add (juce::String (row->duration, 2) + " s");
    if (row->sampleRate > 0) parts.add (juce::String (row->sampleRate / 1000.0, 1) + " kHz / " + juce::String (row->bitDepth) + " bit");
    infoLabel.setText (row->filename + "   " + parts.joinIntoString ("  |  "), juce::dontSendNotification);
}

void ControlStrip::setPlaying (bool p)
{
    playButton.setButtonText (juce::String::fromUTF8 (p ? "\xe2\x96\xa0" : "\xe2\x96\xb6"));
}

void ControlStrip::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::outline);
    g.fillRect (0, 0, getWidth(), 1);
}

void ControlStrip::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto top = r.removeFromTop (24);
    meter.setBounds (r.getRight() - 14, r.getY() + 2, 14, r.getHeight() - 2);
    dragHandle.setBounds (top.removeFromRight (110));
    top.removeFromRight (6);
    dragModeBox.setBounds (top.removeFromRight (130));
    top.removeFromRight (6);
    zoomIn.setBounds (top.removeFromRight (24));
    zoomOut.setBounds (top.removeFromRight (24));
    top.removeFromRight (4);
    snapBox.setBounds (top.removeFromRight (150));
    infoLabel.setBounds (top);

    r.removeFromTop (6);
    r.removeFromRight (20);
    auto row = r;
    playButton.setBounds (row.removeFromLeft (44).withSizeKeepingCentre (44, 32));
    row.removeFromLeft (6);
    auto column = [&row] (int w) { auto c = row.removeFromLeft (w); row.removeFromLeft (8); return c; };
    auto col1 = column (110);
    loopButton.setBounds (col1.removeFromTop (col1.getHeight() / 2).reduced (0, 2));
    directionBox.setBounds (col1.reduced (0, 2));
    auto col2 = column (120);
    quantizeBox.setBounds (col2.removeFromTop (col2.getHeight() / 2).reduced (0, 2));
    syncTempoButton.setBounds (col2.reduced (0, 2));
    auto col3 = column (120);
    keepPitchButton.setBounds (col3.removeFromTop (col3.getHeight() / 2).reduced (0, 2));
    normalizeButton.setBounds (col3.reduced (0, 2));
    auto col4 = column (120);
    syncKeyButton.setBounds (col4.removeFromTop (col4.getHeight() / 2).reduced (0, 2));
    targetKeyBox.setBounds (col4.reduced (0, 2));
    const int kw = juce::jlimit (44, 64, row.getWidth() / 6);
    for (auto* k : { &transpose, &highPass, &lowPass, &fadeIn, &fadeOut, &gain })
        k->setBounds (row.removeFromLeft (kw));
}
}
