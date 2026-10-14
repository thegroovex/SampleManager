#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace sm
{
namespace colours
{
    const juce::Colour background   { 0xff16181c };
    const juce::Colour panel        { 0xff1f2227 };
    const juce::Colour panelLight   { 0xff2a2e35 };
    const juce::Colour outline      { 0xff363b43 };
    const juce::Colour text         { 0xffe4e6ea };
    const juce::Colour textDim      { 0xff8b9099 };
    const juce::Colour accent       { 0xffff8a3d };   // orange
    const juce::Colour accent2      { 0xff3da5ff };   // blue
    const juce::Colour selection    { 0xff3a3f48 };
    const juce::Colour good         { 0xff4cd38a };
    const juce::Colour bandLow      { 0xff2f7dff };   // waveform colours: bass blue
    const juce::Colour bandMid      { 0xffff9a3d };   // mids orange
    const juce::Colour bandHigh     { 0xfff4f4f4 };   // highs white

    juce::Colour forTagType (int type);
}

class SMLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SMLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTableHeaderBackground (juce::Graphics&, juce::TableHeaderComponent&) override;
    void drawTableHeaderColumn (juce::Graphics&, juce::TableHeaderComponent&, const juce::String&, int id,
                                int w, int h, bool over, bool down, int flags) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::FontOptions (juce::jmin (14.0f, (float) h * 0.55f)); }
};
}
