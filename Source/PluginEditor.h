#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/SearchBar.h"
#include "UI/SidePanel.h"
#include "UI/SampleTable.h"
#include "UI/WaveformView.h"
#include "UI/ControlStrip.h"

namespace sm
{
class SampleManagerEditor : public juce::AudioProcessorEditor,
                            public juce::DragAndDropContainer,
                            private juce::Timer
{
public:
    explicit SampleManagerEditor (SampleManagerProcessor&);
    ~SampleManagerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

private:
    void timerCallback() override;

    // searching
    SearchQuery& query() { return tabs[(size_t) currentTab]; }
    void runSearch (bool keepSelection);
    void queryEdited();
    void updateTabs();
    void newTab (const SearchQuery& q);
    void closeTab (int index);
    void selectTab (int index);
    void toggleTag (juce::int64 tagId, bool exclude);
    void showDuplicates (const juce::String& onlyFingerprint);

    // samples
    void sampleSelected (const SampleRow& row);
    void pickRandom();
    void randomBack();
    void showSample (juce::int64 id);
    void findSimilar();
    void toggleFavourite (juce::int64 id);
    void showSampleMenu();
    void showSideMenu (const SidePanel::Item& item);
    void editTags();
    void askText (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done);
    void addLibrary();
    void startExternalDrag();
    void refreshLibraryViews();

    // state
    void saveUiState();
    void restoreUiState();
    void showSettings();

    SampleManagerProcessor& proc;
    Database& db;
    SMLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    SearchBar searchBar;
    SidePanel sidePanel;
    SampleTable table;
    WaveformView waveform;
    ControlStrip controls;
    juce::Label statusLabel;
    juce::TextButton sideToggle;

    juce::StretchableLayoutManager vLayout, hLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> vBar, hBar;
    bool sideVisible = true;
    bool showingSpecialList = false; // duplicates view: don't auto-refresh

    std::vector<SearchQuery> tabs;
    int currentTab = 0;
    std::vector<juce::int64> randomHistory;
    juce::int64 lastDataVersion = -1, lastLocalVersion = -1;
    int lastBatch = -1, tick = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleManagerEditor)
};
}
