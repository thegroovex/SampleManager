#include "SampleTable.h"
#include "LookAndFeel.h"

namespace sm
{
juce::String SampleTable::columnKey (int id)
{
    switch (id)
    {
        case colFav: return "fav";       case colName: return "name";      case colType: return "type";
        case colKey: return "key";       case colBpm: return "bpm";        case colTags: return "tags";
        case colLength: return "length"; case colEffective: return "effective"; case colSize: return "size";
        case colDate: return "date";     case colLibrary: return "library"; case colPath: return "path";
        case colBitDepth: return "bitdepth"; case colSampleRate: return "samplerate";
        case colChannels: return "channels"; case colCodec: return "codec";
        default: return "name";
    }
}

juce::String SampleTable::formatSize (juce::int64 b)
{
    if (b < 1024) return juce::String (b) + " B";
    if (b < 1024 * 1024) return juce::String ((double) b / 1024.0, 1) + " KB";
    return juce::String ((double) b / (1024.0 * 1024.0), 1) + " MB";
}

juce::String SampleTable::formatSeconds (double s)
{
    if (s <= 0) return "-";
    if (s < 60) return juce::String (s, s < 10 ? 2 : 1) + " s";
    return juce::String ((int) s / 60) + ":" + juce::String ((int) s % 60).paddedLeft ('0', 2);
}

SampleTable::SampleTable()
{
    addAndMakeVisible (table);
    table.setRowHeight (24);
    table.setMultipleSelectionEnabled (true);
    table.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    auto& h = table.getHeader();
    using F = juce::TableHeaderComponent::ColumnPropertyFlags;
    const int def = F::visible | F::resizable | F::draggable | F::sortable;
    const int hidden = F::resizable | F::draggable | F::sortable;
    h.addColumn (juce::String::fromUTF8 ("\xe2\x98\x85"), colFav, 26, 26, 26, F::visible | F::sortable);
    h.addColumn (TRANS ("Name"), colName, 280, 80, 900, def);
    h.addColumn (TRANS ("Type"), colType, 70, 40, 120, def);
    h.addColumn (TRANS ("Key"), colKey, 50, 36, 100, def);
    h.addColumn (TRANS ("BPM"), colBpm, 58, 40, 100, def);
    h.addColumn (TRANS ("Tags"), colTags, 220, 60, 600, def);
    h.addColumn (TRANS ("Length"), colLength, 64, 40, 120, def);
    h.addColumn (TRANS ("Effective Length"), colEffective, 64, 40, 120, hidden);
    h.addColumn (TRANS ("Size"), colSize, 70, 40, 120, hidden);
    h.addColumn (TRANS ("Date Added"), colDate, 96, 50, 200, hidden);
    h.addColumn (TRANS ("Library"), colLibrary, 140, 50, 400, def);
    h.addColumn (TRANS ("File Path"), colPath, 300, 60, 1200, hidden);
    h.addColumn (TRANS ("Bit Depth"), colBitDepth, 60, 40, 100, hidden);
    h.addColumn (TRANS ("Sample Rate"), colSampleRate, 76, 40, 120, hidden);
    h.addColumn (TRANS ("Channels"), colChannels, 60, 40, 100, hidden);
    h.addColumn (TRANS ("Codec"), colCodec, 60, 40, 120, hidden);
    h.setStretchToFitActive (false);
    h.setPopupMenuActive (true); // right-click the header to show/hide columns
    suppressSortCallback = true;
    h.setSortColumnId (colName, true);
    suppressSortCallback = false;
}

void SampleTable::resized() { table.setBounds (getLocalBounds()); }
void SampleTable::paint (juce::Graphics& g) { g.fillAll (colours::background); }

void SampleTable::setRows (std::vector<SampleRow> newRows, bool keepSelection)
{
    auto selectedIds = keepSelection ? getSelectedIds() : juce::Array<juce::int64>();
    const int scroll = table.getViewport()->getViewPositionY();
    rows = std::move (newRows);
    suppressSelectionCallback = true;
    table.updateContent();
    juce::SparseSet<int> sel;
    if (keepSelection)
        for (int i = 0; i < (int) rows.size(); ++i)
            if (selectedIds.contains (rows[(size_t) i].id)) sel.addRange ({ i, i + 1 });
    table.setSelectedRows (sel, juce::dontSendNotification);
    table.getViewport()->setViewPosition (0, keepSelection ? scroll : 0);
    suppressSelectionCallback = false;
    table.repaint();
}

juce::Array<juce::int64> SampleTable::getSelectedIds() const
{
    juce::Array<juce::int64> ids;
    auto sel = table.getSelectedRows();
    for (int i = 0; i < sel.size(); ++i)
    {
        const int r = sel[i];
        if (r >= 0 && r < (int) rows.size()) ids.add (rows[(size_t) r].id);
    }
    return ids;
}

const SampleRow* SampleTable::getFirstSelected() const
{
    const int r = table.getSelectedRow (0);
    return (r >= 0 && r < (int) rows.size()) ? &rows[(size_t) r] : nullptr;
}

void SampleTable::selectId (juce::int64 id, bool scrollTo)
{
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].id == id)
        {
            table.selectRow (i, ! scrollTo, true);
            return;
        }
}

void SampleTable::selectRelative (int delta)
{
    if (rows.empty()) return;
    const int cur = table.getSelectedRow (0);
    const int next = juce::jlimit (0, (int) rows.size() - 1, cur < 0 ? 0 : cur + delta);
    table.selectRow (next);
}

void SampleTable::setSort (const juce::String& column, bool asc)
{
    for (int id = colFav; id <= colCodec; ++id)
        if (columnKey (id) == column)
        {
            const juce::ScopedValueSetter<bool> svs (suppressSortCallback, true);
            table.getHeader().setSortColumnId (id, asc);
            return;
        }
}

juce::String SampleTable::getColumnLayout() const { return table.getHeader().toString(); }

void SampleTable::restoreColumnLayout (const juce::String& s)
{
    if (s.isEmpty()) return;
    const juce::ScopedValueSetter<bool> svs (suppressSortCallback, true);
    table.getHeader().restoreFromString (s);
}

void SampleTable::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    if (selected) g.fillAll (colours::selection);
    else if (row % 2) g.fillAll (colours::panel.withAlpha (0.45f));
}

void SampleTable::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) rows.size()) return;
    const auto& r = rows[(size_t) row];
    g.setFont (juce::FontOptions (13.0f));
    auto textCol = selected ? colours::text : colours::text.withAlpha (0.9f);
    juce::String s;
    auto just = juce::Justification::centredLeft;
    switch (col)
    {
        case colFav:
            g.setColour (r.favourite ? colours::accent : colours::outline);
            g.drawText (juce::String::fromUTF8 (r.favourite ? "\xe2\x98\x85" : "\xe2\x98\x86"), 0, 0, w, h, juce::Justification::centred);
            return;
        case colName:
        {
            g.setColour (textCol);
            auto area = juce::Rectangle<int> (6, 0, w - 8, h);
            if (r.hasDuplicate)
            {
                g.setColour (colours::textDim);
                g.setFont (juce::FontOptions (10.0f));
                g.drawText ("DUP", area.removeFromRight (28), juce::Justification::centredRight);
                g.setFont (juce::FontOptions (13.0f));
                g.setColour (textCol);
            }
            g.drawText (r.filename, area, just, true);
            return;
        }
        case colType:
            if (! r.analysed) { s = "..."; break; }
            g.setColour (r.isLoop ? colours::accent2 : colours::accent);
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
            g.drawText (r.isLoop ? TRANS ("Loop") : TRANS ("One-Shot"), 6, 0, w - 8, h, just, true);
            return;
        case colKey:     s = r.key.toString(); break;
        case colBpm:     s = r.bpm > 0 ? juce::String (r.bpm, std::abs (r.bpm - std::round (r.bpm)) < 0.01 ? 0 : 1) : juce::String(); break;
        case colTags:    g.setColour (colours::textDim); g.drawText (r.tags, 6, 0, w - 8, h, just, true); return;
        case colLength:  s = formatSeconds (r.duration); break;
        case colEffective: s = formatSeconds (r.effectiveLength); break;
        case colSize:    s = formatSize (r.fileSize); break;
        case colDate:    s = juce::Time (r.addedAt).formatted ("%Y-%m-%d"); break;
        case colLibrary: s = r.libraryName; break;
        case colPath:    s = r.path; break;
        case colBitDepth: s = r.bitDepth > 0 ? juce::String (r.bitDepth) : juce::String(); break;
        case colSampleRate: s = r.sampleRate > 0 ? juce::String (r.sampleRate / 1000.0, 1) + " k" : juce::String(); break;
        case colChannels: s = r.channels == 1 ? "Mono" : r.channels == 2 ? "Stereo" : (r.channels > 0 ? juce::String (r.channels) : juce::String()); break;
        case colCodec:   s = r.codec; break;
        default: break;
    }
    g.setColour (textCol);
    g.drawText (s, 6, 0, w - 8, h, just, true);
}

void SampleTable::sortOrderChanged (int col, bool fwd)
{
    if (! suppressSortCallback && onSortChanged) onSortChanged (columnKey (col), fwd);
}

void SampleTable::selectedRowsChanged (int last)
{
    if (suppressSelectionCallback) return;
    if (last >= 0 && last < (int) rows.size() && onSelectionChanged)
        onSelectionChanged (rows[(size_t) last]);
}

void SampleTable::cellClicked (int row, int col, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (! table.isRowSelected (row)) table.selectRow (row);
        if (onContextMenu) onContextMenu (e);
        return;
    }
    if (col == colFav && row >= 0 && row < (int) rows.size() && onToggleFavourite)
        onToggleFavourite (rows[(size_t) row].id);
}

void SampleTable::cellDoubleClicked (int row, int, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) rows.size() && onDoubleClick) onDoubleClick (rows[(size_t) row]);
}

juce::var SampleTable::getDragSourceDescription (const juce::SparseSet<int>&)
{
    return "sm-samples";
}

juce::String SampleTable::getCellTooltip (int row, int col)
{
    if (row < 0 || row >= (int) rows.size()) return {};
    const auto& r = rows[(size_t) row];
    if (col == colKey && r.key.isValid()) return r.key.toString() + "  (Camelot " + r.key.toCamelot() + ")";
    if (col == colTags) return r.tags;
    return r.path;
}
}
