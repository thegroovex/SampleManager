#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/Database.h"

namespace sm
{
/** The results list. Sortable columns, show/hide via header right-click,
    multi-select, drag to collections (internal) or into the DAW (external). */
class SampleTable : public juce::Component,
                    private juce::TableListBoxModel
{
public:
    enum ColumnId
    {
        colFav = 1, colName, colType, colKey, colBpm, colTags, colLength, colEffective, colSize, colDate,
        colLibrary, colPath, colBitDepth, colSampleRate, colChannels, colCodec
    };

    SampleTable();

    void setRows (std::vector<SampleRow> rows, bool keepSelection);
    const std::vector<SampleRow>& getRows() const { return rows; }
    juce::Array<juce::int64> getSelectedIds() const;
    const SampleRow* getFirstSelected() const;
    void selectId (juce::int64 id, bool scrollTo = true);
    void selectRelative (int delta);
    void setSort (const juce::String& column, bool ascending);
    juce::String getColumnLayout() const;
    void restoreColumnLayout (const juce::String& s);
    juce::TableListBox& getTable() { return table; }

    std::function<void (const SampleRow&)> onSelectionChanged;
    std::function<void (const SampleRow&)> onDoubleClick;
    std::function<void (juce::String column, bool ascending)> onSortChanged;
    std::function<void (const juce::MouseEvent&)> onContextMenu;
    std::function<void (juce::int64 id)> onToggleFavourite;
    std::function<void()> onReturnKey;

    static juce::String columnKey (int columnId);
    static juce::String formatSize (juce::int64 bytes);
    static juce::String formatSeconds (double s);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    int getNumRows() override { return (int) rows.size(); }
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void sortOrderChanged (int newSortColumnId, bool isForwards) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void cellClicked (int row, int col, const juce::MouseEvent&) override;
    void cellDoubleClicked (int row, int col, const juce::MouseEvent&) override;
    void backgroundClicked (const juce::MouseEvent&) override {}
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rowsToDescribe) override;
    juce::String getCellTooltip (int row, int col) override;
    void returnKeyPressed (int) override { if (onReturnKey) onReturnKey(); }

    juce::TableListBox table { "Samples", this };
    std::vector<SampleRow> rows;
    bool suppressSelectionCallback = false;
    bool suppressSortCallback = false;
};
}
