#include "TagEditor.h"
#include "LookAndFeel.h"

namespace sm
{
TagEditor::TagEditor (Database& d, juce::Array<juce::int64> s) : db (d), ids (std::move (s))
{
    title.setText (ids.size() == 1 ? TRANS ("Tags") : TRANS ("Tags for") + " " + juce::String (ids.size()) + " " + TRANS ("samples"), juce::dontSendNotification);
    title.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (title);
    addAndMakeVisible (input);
    addAndMakeVisible (typeBox);
    addAndMakeVisible (addButton);
    addAndMakeVisible (suggestionList);
    input.setTextToShowWhenEmpty (TRANS ("New tag..."), colours::textDim);
    typeBox.addItem (TRANS ("My Tag"), 4);
    typeBox.addItem (TRANS ("Sound Type"), 1);
    typeBox.addItem (TRANS ("Genre"), 2);
    typeBox.addItem (TRANS ("Label"), 3);
    typeBox.setSelectedId (4);
    addButton.setButtonText (TRANS ("Add"));
    suggestionList.setRowHeight (20);
    input.onTextChange = [this] { updateSuggestions(); };
    input.onReturnKey = [this] { addTag (input.getText(), (TagType) (typeBox.getSelectedId() - 1)); };
    addButton.onClick = input.onReturnKey;
    reload();
    setSize (380, 300);
}

void TagEditor::reload()
{
    current = ids.isEmpty() ? std::vector<TagRow>() : db.tagsFor (ids[0]);
    all = db.tags (false);
    updateSuggestions();
    repaint();
}

void TagEditor::updateSuggestions()
{
    suggestions.clear();
    const auto t = input.getText().trim();
    for (auto& tag : all)
    {
        if (t.isNotEmpty() && ! tag.name.containsIgnoreCase (t)) continue;
        bool already = false;
        for (auto& c : current) already |= c.id == tag.id;
        if (! already) suggestions.push_back (tag);
        if (suggestions.size() > 60) break;
    }
    suggestionList.updateContent();
    suggestionList.repaint();
}

void TagEditor::addTag (const juce::String& name, TagType type)
{
    if (name.trim().isEmpty()) return;
    auto id = db.tagId (name.trim(), type, true);
    db.addTag (ids, id);
    input.clear();
    reload();
    if (onChanged) onChanged();
}

void TagEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    chipBounds.clear();
    int x = 10, y = 34;
    g.setFont (juce::FontOptions (12.0f));
    juce::Font f (juce::FontOptions (12.0f));
    for (auto& t : current)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (f, t.name) + 26;
        if (x + w > getWidth() - 10) { x = 10; y += 24; }
        juce::Rectangle<int> r (x, y, w, 20);
        chipBounds.push_back (r);
        auto col = colours::forTagType ((int) t.type);
        g.setColour (col.withAlpha (0.25f));
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (col);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
        g.setColour (colours::text);
        auto tr = r.reduced (9, 0);
        g.drawText (juce::String::fromUTF8 ("\xc3\x97"), tr.removeFromRight (10), juce::Justification::centred);
        g.drawText (t.name, tr, juce::Justification::centredLeft, true);
        x += w + 4;
    }
    if (current.empty())
    {
        g.setColour (colours::textDim);
        g.drawText (TRANS ("No tags yet"), 10, y, getWidth() - 20, 20, juce::Justification::centredLeft);
    }
}

void TagEditor::mouseUp (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < chipBounds.size() && i < current.size(); ++i)
        if (chipBounds[i].contains (e.getPosition()))
        {
            db.removeTag (ids, current[i].id);
            reload();
            if (onChanged) onChanged();
            return;
        }
}

void TagEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    title.setBounds (r.removeFromTop (20));
    r.removeFromTop (60); // chip area
    auto row = r.removeFromTop (26);
    addButton.setBounds (row.removeFromRight (70));
    row.removeFromRight (4);
    typeBox.setBounds (row.removeFromRight (110));
    row.removeFromRight (4);
    input.setBounds (row);
    r.removeFromTop (6);
    suggestionList.setBounds (r);
}

void TagEditor::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) suggestions.size()) return;
    if (selected) g.fillAll (colours::selection);
    const auto& t = suggestions[(size_t) row];
    g.setColour (colours::forTagType ((int) t.type));
    g.fillEllipse (6.0f, (float) h * 0.5f - 3.0f, 6.0f, 6.0f);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (t.name, 18, 0, w - 70, h, juce::Justification::centredLeft, true);
    g.setColour (colours::textDim);
    g.drawText (TRANS (tagTypeName (t.type)), w - 70, 0, 64, h, juce::Justification::centredRight);
}

void TagEditor::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) suggestions.size()) return;
    const auto t = suggestions[(size_t) row];
    db.addTag (ids, t.id);
    reload();
    if (onChanged) onChanged();
}
}
