#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/Database.h"

namespace sm
{
/** Left panel: libraries, collections, saved searches and the tag browser.
    Collections accept samples dragged from the table. */
class SidePanel : public juce::Component,
                  public juce::DragAndDropTarget,
                  private juce::ListBoxModel
{
public:
    enum class Kind { Header, All, Favourites, Duplicates, Library, Collection, SavedSearch, Tag };

    struct Item
    {
        Kind kind = Kind::Header;
        juce::int64 id = 0;
        juce::String text;
        int count = 0;
        int section = 0;     // header: 0 libraries, 1 collections, 2 saved, 3+ tag type
        int tagType = 0;
    };

    SidePanel();

    void refresh (Database& db);
    void setActive (juce::int64 libraryId, juce::int64 collectionId, bool favourites,
                    const juce::Array<juce::int64>& includeTags, const juce::Array<juce::int64>& excludeTags);

    std::function<void()> onShowAll, onShowFavourites, onShowDuplicates, onAddLibrary, onCreateCollection;
    std::function<void (juce::int64)> onSelectLibrary, onSelectCollection, onOpenSavedSearch;
    std::function<void (juce::int64, bool exclude)> onTagClicked;
    std::function<void (const Item&, const juce::MouseEvent&)> onItemMenu;
    std::function<void (juce::int64 collectionId)> onDropOnCollection;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    int getNumRows() override { return (int) items.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void rebuild();

    juce::ListBox list { "Side", this };
    juce::TextEditor tagFilter;
    std::vector<Item> allItems, items;
    juce::int64 activeLibrary = 0, activeCollection = 0;
    bool activeFavourites = false;
    juce::Array<juce::int64> included, excluded;
    std::set<int> collapsed;
    int dropHighlightRow = -1;
};
}
