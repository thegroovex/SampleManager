#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/SearchQuery.h"
#include "Core/Database.h"

namespace sm
{
/** Row of search tabs: several searches in one instance. */
class SearchTabs : public juce::Component
{
public:
    void setTabs (const juce::StringArray& titles, int current);
    std::function<void (int)> onSelect, onClose;
    std::function<void()> onAdd;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    juce::Rectangle<int> tabBounds (int i) const;
    juce::Rectangle<int> addBounds() const;
    juce::StringArray titles;
    int current = 0;
};

/** Text search, filters and the action buttons for the current tab. */
class SearchBar : public juce::Component
{
public:
    explicit SearchBar (bool standalone);

    void setQuery (const SearchQuery& q, const std::vector<TagRow>& allTags);
    /** Writes the widgets' state into q (keeps tags/collection/etc.). */
    void readInto (SearchQuery& q) const;
    void setTempoDisplay (double bpm, bool linkEnabled, int peers, bool hostControlled);
    void focusSearch() { searchBox.grabKeyboardFocus(); }

    std::function<void()> onQueryChanged;
    std::function<void()> onRandom, onRandomBack, onSimilar, onSaveSearch, onSettings;
    std::function<void (juce::int64 tagId)> onRemoveTagChip;
    std::function<void (double)> onTempoEdited;
    std::function<void (bool)> onLinkToggled;

    SearchTabs tabs;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Chip { juce::int64 id; juce::String name; int type; bool excluded; juce::Rectangle<int> bounds; };
    class ChipArea : public juce::Component
    {
    public:
        std::vector<Chip> chips;
        std::function<void (juce::int64)> onRemove;
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void layout();
    };

    bool standalone;
    juce::TextEditor searchBox, bpmMin, bpmMax, tempoBox;
    juce::ComboBox typeBox, keyBox;
    juce::TextButton matchButton { "AND" }, favButton, dupButton, randomButton, randomBackButton, similarButton,
                     saveButton, settingsButton, linkButton { "Link" };
    juce::Label tempoLabel;
    ChipArea chipArea;
    bool updating = false;
};
}
