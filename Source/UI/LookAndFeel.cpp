#include "LookAndFeel.h"

namespace sm
{
juce::Colour colours::forTagType (int type)
{
    switch (type)
    {
        case 0: return juce::Colour (0xffff8a3d); // sound type
        case 1: return juce::Colour (0xff3da5ff); // genre
        case 2: return juce::Colour (0xffb07cff); // label
        default: return juce::Colour (0xff4cd38a); // custom
    }
}

SMLookAndFeel::SMLookAndFeel()
{
    using namespace colours;
    setColourScheme ({ background, panel, panelLight, outline, text, accent, text, selection, text });
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, panelLight);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, panelLight);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, selection);
    setColour (juce::TextEditor::backgroundColourId, panelLight);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::TableListBox::backgroundColourId, background);
    setColour (juce::TableHeaderComponent::backgroundColourId, panel);
    setColour (juce::TableHeaderComponent::textColourId, textDim);
    setColour (juce::TableHeaderComponent::outlineColourId, outline);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, outline);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ScrollBar::thumbColourId, outline);
    setColour (juce::TooltipWindow::backgroundColourId, panelLight);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::TreeView::backgroundColourId, panel);
}

void SMLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? colours::accent : bg;
    if (down) c = c.brighter (0.15f);
    else if (over) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    if (! b.getToggleState())
    {
        g.setColour (colours::outline);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

void SMLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float box = juce::jmin (14.0f, r.getHeight() - 4.0f);
    auto boxR = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - box / 2, box, box);
    g.setColour (b.getToggleState() ? colours::accent : colours::panelLight.brighter (over ? 0.1f : 0.0f));
    g.fillRoundedRectangle (boxR, 3.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (boxR, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (boxR.getX() + box * 0.22f, boxR.getCentreY());
        tick.lineTo (boxR.getX() + box * 0.42f, boxR.getBottom() - box * 0.25f);
        tick.lineTo (boxR.getRight() - box * 0.2f, boxR.getY() + box * 0.25f);
        g.setColour (juce::Colours::black);
        g.strokePath (tick, juce::PathStrokeType (1.8f));
    }
    g.setColour (b.isEnabled() ? colours::text : colours::textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (box + 7).toNearestInt(), juce::Justification::centredLeft, 1);
}

void SMLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (r.getWidth(), r.getHeight()) / 2.0f;
    auto c = r.getCentre();
    const float angle = start + pos * (end - start);
    juce::Path bg, val;
    bg.addCentredArc (c.x, c.y, radius, radius, 0.0f, start, end, true);
    g.setColour (colours::outline);
    g.strokePath (bg, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // bipolar sliders fill from the centre
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    val.addCentredArc (c.x, c.y, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.isEnabled() ? colours::accent : colours::textDim);
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Line<float> needle (c, c.getPointOnCircumference (radius - 4.0f, angle));
    g.setColour (colours::text);
    g.drawLine (needle.withShortenedStart (radius * 0.35f), 2.0f);
}

void SMLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float mn, float mx,
                                      juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, mn, mx, style, s);
        return;
    }
    auto track = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (colours::outline);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (colours::accent);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pos, track.getCentreY() }));
}

void SMLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent : colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (colours::textDim);
    g.fillPath (arrow);
}

void SMLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
}

void SMLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? colours::accent : colours::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
}

void SMLookAndFeel::drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& h)
{
    g.fillAll (colours::panel);
    g.setColour (colours::outline);
    g.fillRect (0, h.getHeight() - 1, h.getWidth(), 1);
}

void SMLookAndFeel::drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent&, const juce::String& name, int,
                                           int w, int h, bool over, bool, int flags)
{
    if (over) g.fillAll (colours::panelLight);
    g.setColour (colours::outline);
    g.fillRect (w - 1, 4, 1, h - 8);
    auto r = juce::Rectangle<int> (6, 0, w - 12, h);
    if ((flags & (juce::TableHeaderComponent::sortedForwards | juce::TableHeaderComponent::sortedBackwards)) != 0)
    {
        juce::Path p;
        const bool fwd = (flags & juce::TableHeaderComponent::sortedForwards) != 0;
        const float cx = (float) r.getRight() - 4.0f, cy = (float) h * 0.5f;
        if (fwd) p.addTriangle (cx - 4, cy + 2, cx + 4, cy + 2, cx, cy - 3);
        else     p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (colours::accent);
        g.fillPath (p);
        r.removeFromRight (12);
    }
    g.setColour (colours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (name, r, juce::Justification::centredLeft, 1);
}
}
