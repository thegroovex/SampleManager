#include "SearchBar.h"
#include "LookAndFeel.h"

namespace sm
{
//==============================================================================
juce::Rectangle<int> SearchTabs::tabBounds (int i) const
{
    const int n = juce::jmax (1, titles.size());
    const int w = juce::jlimit (90, 200, (getWidth() - 34) / n);
    return { i * w, 0, w - 2, getHeight() };
}

juce::Rectangle<int> SearchTabs::addBounds() const
{
    auto last = tabBounds (titles.size());
    return { last.getX(), 2, 28, getHeight() - 4 };
}

void SearchTabs::setTabs (const juce::StringArray& t, int cur)
{
    titles = t; current = cur;
    repaint();
}

void SearchTabs::paint (juce::Graphics& g)
{
    const auto mouse = getMouseXYRelative();
    for (int i = 0; i < titles.size(); ++i)
    {
        auto r = tabBounds (i);
        const bool sel = i == current;
        g.setColour (sel ? colours::panelLight : colours::panel);
        g.fillRoundedRectangle (r.toFloat().withTrimmedBottom (-4), 5.0f);
        if (sel)
        {
            g.setColour (colours::accent);
            g.fillRect (r.getX() + 6, r.getBottom() - 2, r.getWidth() - 12, 2);
        }
        g.setColour (sel ? colours::text : colours::textDim);
        g.setFont (juce::FontOptions (12.5f));
        auto textR = r.reduced (10, 0);
        auto closeR = textR.removeFromRight (titles.size() > 1 ? 14 : 0);
        g.drawText (titles[i], textR, juce::Justification::centredLeft, true);
        if (titles.size() > 1)
        {
            g.setColour (closeR.expanded (3).contains (mouse) ? colours::text : colours::textDim.withAlpha (0.6f));
            g.drawText (juce::String::fromUTF8 ("\xc3\x97"), closeR, juce::Justification::centred);
        }
    }
    auto a = addBounds();
    g.setColour (a.contains (mouse) ? colours::text : colours::textDim);
    g.setFont (juce::FontOptions (18.0f));
    g.drawText ("+", a, juce::Justification::centred);
}

void SearchTabs::mouseUp (const juce::MouseEvent& e)
{
    if (addBounds().contains (e.getPosition())) { if (onAdd) onAdd(); return; }
    for (int i = 0; i < titles.size(); ++i)
    {
        auto r = tabBounds (i);
        if (! r.contains (e.getPosition())) continue;
        auto closeR = r.reduced (10, 0).removeFromRight (14).expanded (3);
        if ((titles.size() > 1 && closeR.contains (e.getPosition())) || e.mods.isMiddleButtonDown())
        { if (onClose) onClose (i); }
        else if (onSelect) onSelect (i);
        return;
    }
}

//==============================================================================
void SearchBar::ChipArea::layout()
{
    int x = 0;
    juce::Font f (juce::FontOptions (12.0f));
    for (auto& c : chips)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (f, (c.excluded ? "-" : "") + c.name) + 26;
        c.bounds = { x, 2, w, getHeight() - 4 };
        x += w + 4;
    }
}

void SearchBar::ChipArea::paint (juce::Graphics& g)
{
    layout();
    g.setFont (juce::FontOptions (12.0f));
    for (auto& c : chips)
    {
        auto col = colours::forTagType (c.type);
        auto r = c.bounds.toFloat();
        g.setColour (c.excluded ? colours::panelLight : col.withAlpha (0.25f));
        g.fillRoundedRectangle (r, r.getHeight() / 2);
        g.setColour (c.excluded ? juce::Colours::red.withAlpha (0.7f) : col);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() / 2, 1.0f);
        g.setColour (colours::text);
        auto tr = c.bounds.reduced (9, 0);
        g.drawText (juce::String::fromUTF8 ("\xc3\x97"), tr.removeFromRight (10), juce::Justification::centred);
        g.drawText ((c.excluded ? "-" : "") + c.name, tr, juce::Justification::centredLeft, true);
    }
}

void SearchBar::ChipArea::mouseUp (const juce::MouseEvent& e)
{
    for (auto& c : chips)
        if (c.bounds.contains (e.getPosition()) && onRemove) { onRemove (c.id); return; }
}

SearchBar::SearchBar (bool isStandalone) : standalone (isStandalone)
{
    addAndMakeVisible (tabs);
    for (auto* c : std::initializer_list<juce::Component*> { &searchBox, &bpmMin, &bpmMax, &typeBox, &keyBox, &matchButton, &favButton,
                                                             &dupButton, &randomButton, &randomBackButton, &similarButton, &saveButton,
                                                             &settingsButton, &tempoLabel, &tempoBox, &chipArea })
        addAndMakeVisible (c);
    if (standalone) addAndMakeVisible (linkButton);

    searchBox.setTextToShowWhenEmpty (TRANS ("Search samples, tags, folders...  (-word excludes)"), colours::textDim);
    searchBox.setFont (juce::FontOptions (14.0f));
    searchBox.setIndents (8, 6);
    bpmMin.setTextToShowWhenEmpty (TRANS ("BPM min"), colours::textDim);
    bpmMax.setTextToShowWhenEmpty (TRANS ("max"), colours::textDim);
    for (auto* e : { &bpmMin, &bpmMax, &tempoBox })
    {
        e->setInputRestrictions (6, "0123456789.");
        e->setJustification (juce::Justification::centred);
        e->setFont (juce::FontOptions (13.0f));
    }

    typeBox.addItem (TRANS ("All Types"), 1);
    typeBox.addItem (TRANS ("Loops"), 2);
    typeBox.addItem (TRANS ("One-Shots"), 3);
    keyBox.addItem (TRANS ("Any Key"), 1);
    for (auto k : theory::allKeys()) keyBox.addItem (k.toString() + "  (" + k.toCamelot() + ")", k.toInt() + 2);

    matchButton.setTooltip (TRANS ("Selected tags: match ALL (AND) or ANY (OR)"));
    matchButton.setClickingTogglesState (true);
    favButton.setButtonText (juce::String::fromUTF8 ("\xe2\x98\x85"));
    favButton.setTooltip (TRANS ("Favourites only"));
    favButton.setClickingTogglesState (true);
    dupButton.setButtonText (TRANS ("Hide Dupes"));
    dupButton.setTooltip (TRANS ("Hide duplicate samples"));
    dupButton.setClickingTogglesState (true);
    randomButton.setButtonText (TRANS ("Random"));
    randomButton.setTooltip (TRANS ("Instant inspiration: pick a random sample from the current search (R)"));
    randomBackButton.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xa9"));
    randomBackButton.setTooltip (TRANS ("Back to the previous random sample"));
    similarButton.setButtonText (TRANS ("Similar"));
    similarButton.setTooltip (TRANS ("Find sounds with similar tags in a new tab"));
    saveButton.setButtonText (TRANS ("Save"));
    saveButton.setTooltip (TRANS ("Save this search"));
    settingsButton.setButtonText (juce::String::fromUTF8 ("\xe2\x9a\x99"));
    settingsButton.setTooltip (TRANS ("Settings"));
    linkButton.setClickingTogglesState (true);
    linkButton.setTooltip (TRANS ("Ableton Link: sync tempo with other apps and devices on the network"));
    tempoLabel.setFont (juce::FontOptions (11.0f));
    tempoLabel.setColour (juce::Label::textColourId, colours::textDim);
    tempoLabel.setJustificationType (juce::Justification::centredRight);
    tempoBox.setTooltip (TRANS ("Tempo previews sync to"));

    auto changed = [this] { if (! updating && onQueryChanged) onQueryChanged(); };
    searchBox.onTextChange = changed;
    bpmMin.onTextChange = changed;
    bpmMax.onTextChange = changed;
    typeBox.onChange = changed;
    keyBox.onChange = changed;
    matchButton.onClick = [this, changed] { matchButton.setButtonText (matchButton.getToggleState() ? "OR" : "AND"); changed(); };
    favButton.onClick = changed;
    dupButton.onClick = changed;
    randomButton.onClick = [this] { if (onRandom) onRandom(); };
    randomBackButton.onClick = [this] { if (onRandomBack) onRandomBack(); };
    similarButton.onClick = [this] { if (onSimilar) onSimilar(); };
    saveButton.onClick = [this] { if (onSaveSearch) onSaveSearch(); };
    settingsButton.onClick = [this] { if (onSettings) onSettings(); };
    linkButton.onClick = [this] { if (onLinkToggled) onLinkToggled (linkButton.getToggleState()); };
    tempoBox.onReturnKey = [this] { if (onTempoEdited) onTempoEdited (tempoBox.getText().getDoubleValue()); };
    tempoBox.onFocusLost = tempoBox.onReturnKey;
    chipArea.onRemove = [this] (juce::int64 id) { if (onRemoveTagChip) onRemoveTagChip (id); };
}

void SearchBar::setQuery (const SearchQuery& q, const std::vector<TagRow>& allTags)
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    if (searchBox.getText() != q.text) searchBox.setText (q.text, false);
    if (bpmMin.getText().getDoubleValue() != q.bpmMin) bpmMin.setText (q.bpmMin > 0 ? juce::String (q.bpmMin, 0) : juce::String(), false);
    if (bpmMax.getText().getDoubleValue() != q.bpmMax) bpmMax.setText (q.bpmMax > 0 ? juce::String (q.bpmMax, 0) : juce::String(), false);
    typeBox.setSelectedId (q.type + 1, juce::dontSendNotification);
    keyBox.setSelectedId (q.keys.isEmpty() ? 1 : q.keys[0] + 2, juce::dontSendNotification);
    matchButton.setToggleState (! q.tagsMatchAll, juce::dontSendNotification);
    matchButton.setButtonText (q.tagsMatchAll ? "AND" : "OR");
    favButton.setToggleState (q.favouritesOnly, juce::dontSendNotification);
    dupButton.setToggleState (q.hideDuplicates, juce::dontSendNotification);

    chipArea.chips.clear();
    auto addChip = [&] (juce::int64 id, bool excluded)
    {
        for (auto& t : allTags)
            if (t.id == id) { chipArea.chips.push_back ({ id, t.name, (int) t.type, excluded, {} }); return; }
    };
    for (auto id : q.includeTags) addChip (id, false);
    for (auto id : q.excludeTags) addChip (id, true);
    chipArea.repaint();
}

void SearchBar::readInto (SearchQuery& q) const
{
    q.text = searchBox.getText().trim();
    q.bpmMin = bpmMin.getText().getDoubleValue();
    q.bpmMax = bpmMax.getText().getDoubleValue();
    q.type = typeBox.getSelectedId() - 1;
    q.keys.clear();
    if (keyBox.getSelectedId() > 1) q.keys.add (keyBox.getSelectedId() - 2);
    q.tagsMatchAll = ! matchButton.getToggleState();
    q.favouritesOnly = favButton.getToggleState();
    q.hideDuplicates = dupButton.getToggleState();
}

void SearchBar::setTempoDisplay (double bpm, bool linkEnabled, int peers, bool hostControlled)
{
    if (! tempoBox.hasKeyboardFocus (false))
        tempoBox.setText (juce::String (bpm, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."), false);
    tempoBox.setReadOnly (hostControlled);
    linkButton.setToggleState (linkEnabled, juce::dontSendNotification);
    const juce::String src = hostControlled ? TRANS ("Host") : (linkEnabled ? "Link (" + juce::String (peers) + ")" : TRANS ("Tempo"));
    tempoLabel.setText (src, juce::dontSendNotification);
}

void SearchBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void SearchBar::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    tabs.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    auto row = r.removeFromTop (30);
    settingsButton.setBounds (row.removeFromRight (30));
    row.removeFromRight (6);
    if (standalone) { linkButton.setBounds (row.removeFromRight (46)); row.removeFromRight (4); }
    tempoBox.setBounds (row.removeFromRight (60));
    tempoLabel.setBounds (row.removeFromRight (60));
    row.removeFromRight (10);
    saveButton.setBounds (row.removeFromRight (60));
    row.removeFromRight (4);
    similarButton.setBounds (row.removeFromRight (66));
    row.removeFromRight (4);
    randomBackButton.setBounds (row.removeFromRight (30));
    randomButton.setBounds (row.removeFromRight (70));
    row.removeFromRight (10);
    searchBox.setBounds (row);

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (26);
    typeBox.setBounds (row2.removeFromLeft (110));
    row2.removeFromLeft (6);
    keyBox.setBounds (row2.removeFromLeft (110));
    row2.removeFromLeft (6);
    bpmMin.setBounds (row2.removeFromLeft (64));
    row2.removeFromLeft (2);
    bpmMax.setBounds (row2.removeFromLeft (48));
    row2.removeFromLeft (6);
    favButton.setBounds (row2.removeFromLeft (30));
    row2.removeFromLeft (4);
    dupButton.setBounds (row2.removeFromLeft (96));
    row2.removeFromLeft (10);
    matchButton.setBounds (row2.removeFromLeft (44));
    row2.removeFromLeft (6);
    chipArea.setBounds (row2);
}
}
