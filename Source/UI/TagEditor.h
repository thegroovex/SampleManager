#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/Database.h"

namespace sm
{
/** Add / remove tags for the selected samples, with auto-complete. */
class TagEditor : public juce::Component,
                  private juce::ListBoxModel
{
public:
    TagEditor (Database& db, juce::Array<juce::int64> sampleIds);

    std::function<void()> onChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int getNumRows() override { return (int) suggestions.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemClicked (int, const juce::MouseEvent&) override;
    void reload();
    void updateSuggestions();
    void addTag (const juce::String& name, TagType type);

    Database& db;
    juce::Array<juce::int64> ids;
    std::vector<TagRow> current, all, suggestions;
    std::vector<juce::Rectangle<int>> chipBounds;
    juce::TextEditor input;
    juce::ComboBox typeBox;
    juce::TextButton addButton { "Add" };
    juce::ListBox suggestionList { "Suggestions", this };
    juce::Label title;
};
}
