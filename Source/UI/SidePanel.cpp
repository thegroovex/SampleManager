#include "SidePanel.h"
#include "LookAndFeel.h"

namespace sm
{
SidePanel::SidePanel()
{
    addAndMakeVisible (tagFilter);
    addAndMakeVisible (list);
    list.setRowHeight (22);
    list.setColour (juce::ListBox::backgroundColourId, colours::panel);
    tagFilter.setTextToShowWhenEmpty (TRANS ("Filter tags..."), colours::textDim);
    tagFilter.setFont (juce::FontOptions (12.5f));
    tagFilter.onTextChange = [this] { rebuild(); };
}

void SidePanel::refresh (Database& db)
{
    allItems.clear();
    allItems.push_back ({ Kind::All, 0, TRANS ("All Samples"), db.sampleCount() });
    allItems.push_back ({ Kind::Favourites, 0, TRANS ("Favourites") });
    allItems.push_back ({ Kind::Duplicates, 0, TRANS ("Duplicates") });

    allItems.push_back ({ Kind::Header, 0, TRANS ("LIBRARIES"), 0, 0 });
    for (auto& l : db.libraries()) allItems.push_back ({ Kind::Library, l.id, l.name, l.sampleCount, 0 });

    allItems.push_back ({ Kind::Header, 0, TRANS ("COLLECTIONS"), 0, 1 });
    for (auto& c : db.collections()) allItems.push_back ({ Kind::Collection, c.id, c.name, c.count, 1 });

    allItems.push_back ({ Kind::Header, 0, TRANS ("SAVED SEARCHES"), 0, 2 });
    for (auto& s : db.savedSearches()) allItems.push_back ({ Kind::SavedSearch, s.id, s.name, 0, 2 });

    auto tags = db.tags (true);
    static const char* typeHeaders[] = { "SOUND TYPE", "GENRE", "LABEL / PACK", "MY TAGS" };
    for (int type = 0; type < 4; ++type)
    {
        allItems.push_back ({ Kind::Header, 0, TRANS (typeHeaders[type]), 0, 3 + type, type });
        for (auto& t : tags)
            if ((int) t.type == type) allItems.push_back ({ Kind::Tag, t.id, t.name, t.count, 3 + type, type });
    }
    rebuild();
}

void SidePanel::rebuild()
{
    const auto filter = tagFilter.getText().trim();
    items.clear();
    for (auto& it : allItems)
    {
        const bool collapsible = it.kind == Kind::Library || it.kind == Kind::Collection || it.kind == Kind::SavedSearch || it.kind == Kind::Tag;
        if (collapsible && collapsed.count (it.section) > 0) continue;
        if (it.kind == Kind::Tag && filter.isNotEmpty() && ! it.text.containsIgnoreCase (filter)) continue;
        items.push_back (it);
    }
    list.updateContent();
    list.repaint();
}

void SidePanel::setActive (juce::int64 lib, juce::int64 col, bool fav, const juce::Array<juce::int64>& inc, const juce::Array<juce::int64>& exc)
{
    activeLibrary = lib; activeCollection = col; activeFavourites = fav; included = inc; excluded = exc;
    list.repaint();
}

void SidePanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::outline);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void SidePanel::resized()
{
    auto r = getLocalBounds().withTrimmedRight (1);
    tagFilter.setBounds (r.removeFromTop (34).reduced (6));
    list.setBounds (r);
}

void SidePanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) items.size()) return;
    const auto& it = items[(size_t) row];

    if (it.kind == Kind::Header)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        const bool isCollapsed = collapsed.count (it.section) > 0;
        g.drawText (juce::String::fromUTF8 (isCollapsed ? "\xe2\x96\xb8 " : "\xe2\x96\xbe ") + it.text, 8, 4, w - 40, h - 4, juce::Justification::centredLeft);
        if (it.section <= 1)
        {
            g.setFont (juce::FontOptions (16.0f));
            g.drawText ("+", w - 26, 2, 20, h - 2, juce::Justification::centred);
        }
        return;
    }

    bool active = false, isExcluded = false;
    switch (it.kind)
    {
        case Kind::All:        active = activeLibrary == 0 && activeCollection == 0 && ! activeFavourites; break;
        case Kind::Favourites: active = activeFavourites; break;
        case Kind::Library:    active = it.id == activeLibrary; break;
        case Kind::Collection: active = it.id == activeCollection; break;
        case Kind::Tag:        active = included.contains (it.id); isExcluded = excluded.contains (it.id); break;
        case Kind::Header: case Kind::Duplicates: case Kind::SavedSearch: break;
    }
    if (row == dropHighlightRow) g.fillAll (colours::accent.withAlpha (0.35f));
    else if (active) g.fillAll (colours::selection);

    int x = 14;
    if (it.kind == Kind::Tag)
    {
        g.setColour (colours::forTagType (it.tagType).withAlpha (isExcluded ? 0.3f : 1.0f));
        g.fillEllipse ((float) x, (float) h * 0.5f - 3.0f, 6.0f, 6.0f);
        x += 12;
    }
    g.setColour (isExcluded ? juce::Colours::red.withAlpha (0.8f) : (active ? colours::text : colours::text.withAlpha (0.85f)));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ((isExcluded ? "- " : "") + it.text, x, 0, w - x - 44, h, juce::Justification::centredLeft, true);
    if (it.count > 0)
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (it.count), w - 50, 0, 42, h, juce::Justification::centredRight);
    }
}

void SidePanel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) items.size()) return;
    const auto it = items[(size_t) row];
    if (e.mods.isPopupMenu() && it.kind != Kind::Header)
    {
        if (onItemMenu) onItemMenu (it, e);
        return;
    }
    switch (it.kind)
    {
        case Kind::Header:
            if (it.section <= 1 && e.x > list.getWidth() - 34)
            {
                if (it.section == 0 && onAddLibrary) onAddLibrary();
                if (it.section == 1 && onCreateCollection) onCreateCollection();
            }
            else
            {
                if (collapsed.count (it.section)) collapsed.erase (it.section); else collapsed.insert (it.section);
                rebuild();
            }
            break;
        case Kind::All:         if (onShowAll) onShowAll(); break;
        case Kind::Favourites:  if (onShowFavourites) onShowFavourites(); break;
        case Kind::Duplicates:  if (onShowDuplicates) onShowDuplicates(); break;
        case Kind::Library:     if (onSelectLibrary) onSelectLibrary (it.id); break;
        case Kind::Collection:  if (onSelectCollection) onSelectCollection (it.id); break;
        case Kind::SavedSearch: if (onOpenSavedSearch) onOpenSavedSearch (it.id); break;
        case Kind::Tag:         if (onTagClicked) onTagClicked (it.id, e.mods.isAltDown() || e.mods.isCommandDown()); break;
    }
}

bool SidePanel::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString() == "sm-samples";
}

void SidePanel::itemDragMove (const SourceDetails& d)
{
    const auto p = list.getLocalPoint (this, d.localPosition);
    const int row = list.getRowContainingPosition (p.x, p.y);
    const int newRow = (row >= 0 && row < (int) items.size() && items[(size_t) row].kind == Kind::Collection) ? row : -1;
    if (newRow != dropHighlightRow) { dropHighlightRow = newRow; list.repaint(); }
}

void SidePanel::itemDragExit (const SourceDetails&)
{
    dropHighlightRow = -1;
    list.repaint();
}

void SidePanel::itemDropped (const SourceDetails& d)
{
    itemDragMove (d);
    if (dropHighlightRow >= 0 && onDropOnCollection)
        onDropOnCollection (items[(size_t) dropHighlightRow].id);
    dropHighlightRow = -1;
    list.repaint();
}
}
