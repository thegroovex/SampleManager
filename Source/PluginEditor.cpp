#include "PluginEditor.h"
#include "UI/TagEditor.h"
#include "UI/SettingsPanel.h"
#include "UI/Translations.h"
#if JucePlugin_Build_Standalone
 #include <juce_audio_devices/juce_audio_devices.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace sm
{
SampleManagerEditor::SampleManagerEditor (SampleManagerProcessor& p)
    : AudioProcessorEditor (p), proc (p), db (p.getDatabase()), searchBar (p.isStandalone()), controls (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);

    addAndMakeVisible (searchBar);
    addAndMakeVisible (sidePanel);
    addAndMakeVisible (table);
    addAndMakeVisible (waveform);
    addAndMakeVisible (controls);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (sideToggle);
    statusLabel.setFont (juce::FontOptions (11.5f));
    statusLabel.setColour (juce::Label::textColourId, colours::textDim);
    sideToggle.setButtonText (juce::String::fromUTF8 ("\xe2\x97\x80"));
    sideToggle.setTooltip (TRANS ("Show / hide the side panel"));
    sideToggle.onClick = [this]
    {
        sideVisible = ! sideVisible;
        sidePanel.setVisible (sideVisible);
        hBar->setVisible (sideVisible);
        sideToggle.setButtonText (juce::String::fromUTF8 (sideVisible ? "\xe2\x97\x80" : "\xe2\x96\xb6"));
        resized();
        saveUiState();
    };

    // resizable split: table / waveform
    vLayout.setItemLayout (0, 120, -1.0, -0.62);
    vLayout.setItemLayout (1, 6, 6, 6);
    vLayout.setItemLayout (2, 60, 500, -0.38);
    vBar = std::make_unique<juce::StretchableLayoutResizerBar> (&vLayout, 1, false);
    addAndMakeVisible (*vBar);
    // resizable side panel
    hLayout.setItemLayout (0, 150, 420, 220);
    hLayout.setItemLayout (1, 5, 5, 5);
    hLayout.setItemLayout (2, 300, -1.0, -1.0);
    hBar = std::make_unique<juce::StretchableLayoutResizerBar> (&hLayout, 1, true);
    addAndMakeVisible (*hBar);

    // --- search bar
    searchBar.onQueryChanged = [this] { queryEdited(); };
    searchBar.onRandom = [this] { pickRandom(); };
    searchBar.onRandomBack = [this] { randomBack(); };
    searchBar.onSimilar = [this] { findSimilar(); };
    searchBar.onSettings = [this] { showSettings(); };
    searchBar.onRemoveTagChip = [this] (juce::int64 id)
    {
        query().includeTags.removeFirstMatchingValue (id);
        query().excludeTags.removeFirstMatchingValue (id);
        runSearch (true);
    };
    searchBar.onSaveSearch = [this]
    {
        askText (TRANS ("Save search as"), query().describe(), [this] (juce::String name)
        {
            db.saveSearch (name, query());
            query().title = name;
            refreshLibraryViews();
            updateTabs();
        });
    };
    searchBar.onTempoEdited = [this] (double bpm)
    {
        if (bpm >= 20 && proc.isStandalone())
        {
            proc.getTempoSource().setTempo (bpm);
            db.setSetting ("manualTempo", juce::String (bpm));
        }
    };
    searchBar.onLinkToggled = [this] (bool on) { proc.getTempoSource().setLinkEnabled (on); db.setSetting ("linkEnabled", on ? "1" : "0"); };
    if (proc.isStandalone() && db.getSetting ("linkEnabled", "0") == "1")
        proc.getTempoSource().setLinkEnabled (true);
    searchBar.tabs.onSelect = [this] (int i) { selectTab (i); };
    searchBar.tabs.onClose = [this] (int i) { closeTab (i); };
    searchBar.tabs.onAdd = [this] { newTab ({}); };

    // --- side panel
    sidePanel.onShowAll = [this]
    {
        query().libraryId = 0; query().collectionId = 0; query().favouritesOnly = false; query().similarTo = 0;
        runSearch (false);
    };
    sidePanel.onShowFavourites = [this] { query().favouritesOnly = ! query().favouritesOnly; runSearch (false); };
    sidePanel.onShowDuplicates = [this] { showDuplicates ({}); };
    sidePanel.onSelectLibrary = [this] (juce::int64 id) { query().libraryId = query().libraryId == id ? 0 : id; query().collectionId = 0; runSearch (false); };
    sidePanel.onSelectCollection = [this] (juce::int64 id) { query().collectionId = query().collectionId == id ? 0 : id; query().libraryId = 0; runSearch (false); };
    sidePanel.onOpenSavedSearch = [this] (juce::int64 id)
    {
        for (auto& s : db.savedSearches())
            if (s.id == id) newTab (SearchQuery::fromJson (s.json));
    };
    sidePanel.onTagClicked = [this] (juce::int64 id, bool exclude) { toggleTag (id, exclude); };
    sidePanel.onAddLibrary = [this] { addLibrary(); };
    sidePanel.onCreateCollection = [this]
    {
        askText (TRANS ("New collection"), {}, [this] (juce::String name) { db.createCollection (name); refreshLibraryViews(); });
    };
    sidePanel.onItemMenu = [this] (const SidePanel::Item& it, const juce::MouseEvent&) { showSideMenu (it); };
    sidePanel.onDropOnCollection = [this] (juce::int64 col) { db.addToCollection (col, table.getSelectedIds()); refreshLibraryViews(); };

    // --- table
    table.onSelectionChanged = [this] (const SampleRow& r) { sampleSelected (r); };
    table.onDoubleClick = [this] (const SampleRow& r) { proc.selectSample (r, true); };
    table.onSortChanged = [this] (juce::String col, bool asc) { query().sortColumn = col; query().ascending = asc; runSearch (true); saveUiState(); };
    table.onContextMenu = [this] (const juce::MouseEvent&) { showSampleMenu(); };
    table.onToggleFavourite = [this] (juce::int64 id) { toggleFavourite (id); };
    table.onReturnKey = [this] { proc.togglePlay(); };

    // --- waveform + controls
    waveform.onTrimChanged = [this] (double s, double e) { proc.setTrim (s, e); };
    waveform.onClicked = [this] { proc.togglePlay(); };
    controls.onPlayStop = [this] { proc.togglePlay(); };
    controls.onZoom = [this] (double f) { waveform.zoomBy (f); };
    controls.getSnapBox().onChange = [this] { waveform.setSnap ((WaveformView::Snap) (controls.getSnapBox().getSelectedId() - 1)); saveUiState(); };
    controls.getDragHandle().onDragStart = [this] { startExternalDrag(); };

    proc.onSampleLoaded = [this]
    {
        const auto& r = proc.getSelectedSample();
        waveform.setAudio (proc.getSourceAudio(), proc.getSourceSampleRate(), r.isLoop ? r.bpm : 0.0);
    };
    proc.onMidiAction = [this] (MidiLearn::Action a, float)
    {
        switch (a)
        {
            case MidiLearn::Action::Random:     pickRandom(); break;
            case MidiLearn::Action::RandomBack: randomBack(); break;
            case MidiLearn::Action::Next:       table.selectRelative (1); break;
            case MidiLearn::Action::Previous:   table.selectRelative (-1); break;
            case MidiLearn::Action::None: case MidiLearn::Action::PlayStop: case MidiLearn::Action::HighPass:
            case MidiLearn::Action::LowPass: case MidiLearn::Action::FadeIn: case MidiLearn::Action::FadeOut:
            case MidiLearn::Action::Gain: case MidiLearn::Action::NumActions:
                controls.refreshFromSettings(); break;
        }
    };

    restoreUiState();
    // remember the library state we display, so any later change (scanner, other instances) triggers a refresh
    lastDataVersion = db.dataVersion();
    lastLocalVersion = db.localVersion();
    lastBatch = proc.getScanner().getBatchCounter();
    refreshLibraryViews();
    runSearch (false);
    if (proc.hasSelection())
    {
        table.selectId (proc.getSelectedSample().id);
        waveform.setOverview (db.overview (proc.getSelectedSample().id));
        proc.onSampleLoaded();
        controls.updateSampleInfo (&proc.getSelectedSample());
    }

    setResizable (true, true);
    setResizeLimits (860, 560, 4000, 3000);
    setSize (proc.editorWidth, proc.editorHeight);
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

SampleManagerEditor::~SampleManagerEditor()
{
    stopTimer();
    saveUiState();
    proc.onSampleLoaded = nullptr;
    proc.onMidiAction = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void SampleManagerEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void SampleManagerEditor::resized()
{
    auto r = getLocalBounds();
    proc.editorWidth = getWidth();
    proc.editorHeight = getHeight();

    searchBar.setBounds (r.removeFromTop (102));
    auto status = r.removeFromBottom (22);
    sideToggle.setBounds (status.removeFromLeft (26).reduced (3));
    statusLabel.setBounds (status.reduced (6, 0));
    controls.setBounds (r.removeFromBottom (96));

    if (sideVisible)
    {
        juce::Component* hc[] = { &sidePanel, hBar.get(), nullptr };
        hLayout.layOutComponents (hc, 3, r.getX(), r.getY(), r.getWidth(), r.getHeight(), false, true);
        r = r.withTrimmedLeft (sidePanel.getWidth() + 5);
    }
    juce::Component* vc[] = { &table, vBar.get(), &waveform };
    vLayout.layOutComponents (vc, 3, r.getX(), r.getY(), r.getWidth(), r.getHeight(), true, true);
}

//==============================================================================
void SampleManagerEditor::refreshLibraryViews()
{
    sidePanel.refresh (db);
    sidePanel.setActive (query().libraryId, query().collectionId, query().favouritesOnly, query().includeTags, query().excludeTags);
}

void SampleManagerEditor::runSearch (bool keepSelection)
{
    showingSpecialList = false;
    auto& q = query();
    auto rows = db.search (q);
    const int n = (int) rows.size();
    table.setRows (std::move (rows), keepSelection);
    searchBar.setQuery (q, db.tags (false));
    sidePanel.setActive (q.libraryId, q.collectionId, q.favouritesOnly, q.includeTags, q.excludeTags);
    statusLabel.setText (juce::String (n) + (n >= q.limit ? "+" : "") + " " + TRANS ("samples"), juce::dontSendNotification);
    updateTabs();
}

void SampleManagerEditor::queryEdited()
{
    searchBar.readInto (query());
    query().similarTo = 0;
    runSearch (true);
}

void SampleManagerEditor::updateTabs()
{
    juce::StringArray titles;
    for (auto& t : tabs) titles.add (TRANS (t.describe()));
    searchBar.tabs.setTabs (titles, currentTab);
}

void SampleManagerEditor::newTab (const SearchQuery& q)
{
    tabs.push_back (q);
    currentTab = (int) tabs.size() - 1;
    table.setSort (query().sortColumn, query().ascending);
    runSearch (false);
    saveUiState();
}

void SampleManagerEditor::closeTab (int i)
{
    if (tabs.size() <= 1 || i < 0 || i >= (int) tabs.size()) return;
    tabs.erase (tabs.begin() + i);
    if (currentTab >= i) currentTab = juce::jmax (0, currentTab - 1);
    currentTab = juce::jlimit (0, (int) tabs.size() - 1, currentTab);
    table.setSort (query().sortColumn, query().ascending);
    runSearch (false);
    saveUiState();
}

void SampleManagerEditor::selectTab (int i)
{
    if (i < 0 || i >= (int) tabs.size()) return;
    currentTab = i;
    table.setSort (query().sortColumn, query().ascending);
    runSearch (false);
    saveUiState();
}

void SampleManagerEditor::toggleTag (juce::int64 id, bool exclude)
{
    auto& q = query();
    auto& primary = exclude ? q.excludeTags : q.includeTags;
    auto& other = exclude ? q.includeTags : q.excludeTags;
    other.removeFirstMatchingValue (id);
    if (primary.contains (id)) primary.removeFirstMatchingValue (id);
    else primary.add (id);
    runSearch (false);
}

void SampleManagerEditor::showDuplicates (const juce::String& onlyFingerprint)
{
    SearchQuery q;
    q.title = TRANS ("Duplicates");
    newTab (q);
    auto rows = db.search (q);
    rows.erase (std::remove_if (rows.begin(), rows.end(), [&] (const SampleRow& r)
    {
        return ! r.hasDuplicate || (onlyFingerprint.isNotEmpty() && r.fingerprint != onlyFingerprint);
    }), rows.end());
    std::stable_sort (rows.begin(), rows.end(), [] (const SampleRow& a, const SampleRow& b) { return a.fingerprint < b.fingerprint; });
    const auto n = rows.size();
    table.setRows (std::move (rows), false);
    showingSpecialList = true;
    statusLabel.setText (juce::String ((int) n) + " " + TRANS ("samples with duplicates"), juce::dontSendNotification);
}

//==============================================================================
void SampleManagerEditor::sampleSelected (const SampleRow& row)
{
    proc.selectSample (row, proc.getPreviewSettings().autoPlay);
    waveform.clear();
    waveform.setOverview (db.overview (row.id));
    controls.updateSampleInfo (&row);
}

void SampleManagerEditor::showSample (juce::int64 id)
{
    table.selectId (id);
    if (table.getFirstSelected() == nullptr || table.getFirstSelected()->id != id)
        if (auto row = db.sample (id)) sampleSelected (*row); // outside the visible result limit
}

void SampleManagerEditor::pickRandom()
{
    const auto cur = proc.hasSelection() ? proc.getSelectedSample().id : 0;
    const auto id = db.randomSample (query(), cur);
    if (id == 0) return;
    if (cur != 0) randomHistory.push_back (cur);
    if (randomHistory.size() > 200) randomHistory.erase (randomHistory.begin());
    showSample (id);
}

void SampleManagerEditor::randomBack()
{
    if (randomHistory.empty()) return;
    const auto id = randomHistory.back();
    randomHistory.pop_back();
    showSample (id);
}

void SampleManagerEditor::findSimilar()
{
    auto* sel = table.getFirstSelected();
    if (sel == nullptr) return;
    SearchQuery q;
    q.similarTo = sel->id;
    q.title = TRANS ("Like") + " " + sel->filename;
    newTab (q);
}

void SampleManagerEditor::toggleFavourite (juce::int64 id)
{
    if (auto r = db.sample (id))
        db.setFavourite ({ id }, ! r->favourite);
    runSearch (true);
}

void SampleManagerEditor::askText (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("text", initial);
    w->addButton (TRANS ("OK"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (TRANS ("Cancel"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int result)
    {
        const auto text = w->getTextEditorContents ("text").trim();
        if (result == 1 && text.isNotEmpty()) done (text);
    }), true);
}

void SampleManagerEditor::editTags()
{
    auto ids = table.getSelectedIds();
    if (ids.isEmpty()) return;
    auto editor = std::make_unique<TagEditor> (db, ids);
    editor->onChanged = [this] { refreshLibraryViews(); runSearch (true); };
    auto area = table.getBounds().withHeight (30).translated (0, 30);
    juce::CallOutBox::launchAsynchronously (std::move (editor), area, this);
}

void SampleManagerEditor::showSampleMenu()
{
    auto ids = table.getSelectedIds();
    auto* first = table.getFirstSelected();
    if (ids.isEmpty() || first == nullptr) return;
    const auto row = *first;

    juce::PopupMenu m, keyMenu, colMenu;
    m.addItem (row.favourite ? TRANS ("Remove from Favourites") : TRANS ("Add to Favourites"),
               [this, ids, fav = ! row.favourite] { db.setFavourite (ids, fav); runSearch (true); });
    m.addItem (TRANS ("Edit Tags..."), [this] { editTags(); });
    m.addSeparator();
    m.addItem (TRANS ("Set BPM..."), [this, ids, bpm = row.bpm]
    {
        askText (TRANS ("BPM"), bpm > 0 ? juce::String (bpm) : juce::String(), [this, ids] (juce::String t)
        {
            db.setBpm (ids, t.getDoubleValue());
            runSearch (true);
        });
    });
    for (auto k : theory::allKeys())
        keyMenu.addItem (k.toString() + "  (" + k.toCamelot() + ")", [this, ids, k] { db.setKey (ids, k); runSearch (true); });
    keyMenu.addSeparator();
    for (int pc = 0; pc < 12; ++pc)
    {
        MusicalKey note { pc, MusicalKey::Note };
        keyMenu.addItem (TRANS ("Note") + " " + note.toString(), [this, ids, note] { db.setKey (ids, note); runSearch (true); });
    }
    keyMenu.addSeparator();
    keyMenu.addItem (TRANS ("No Key"), [this, ids] { db.setKey (ids, {}); runSearch (true); });
    m.addSubMenu (TRANS ("Set Key"), keyMenu);
    m.addItem (row.isLoop ? TRANS ("Mark as One-Shot") : TRANS ("Mark as Loop"), [this, ids, loop = ! row.isLoop] { db.setLoop (ids, loop); runSearch (true); });
    m.addSeparator();
    for (auto& c : db.collections())
        colMenu.addItem (c.name, [this, ids, id = c.id] { db.addToCollection (id, ids); refreshLibraryViews(); });
    colMenu.addSeparator();
    colMenu.addItem (TRANS ("New Collection..."), [this, ids]
    {
        askText (TRANS ("New collection"), {}, [this, ids] (juce::String name)
        {
            db.addToCollection (db.createCollection (name), ids);
            refreshLibraryViews();
        });
    });
    m.addSubMenu (TRANS ("Add to Collection"), colMenu);
    if (query().collectionId != 0)
        m.addItem (TRANS ("Remove from this Collection"), [this, ids] { db.removeFromCollection (query().collectionId, ids); refreshLibraryViews(); runSearch (true); });
    m.addSeparator();
    m.addItem (TRANS ("Find Similar"), [this] { findSimilar(); });
    if (row.hasDuplicate)
        m.addItem (TRANS ("Show Duplicates"), [this, fp = row.fingerprint] { showDuplicates (fp); });
    m.addItem (TRANS ("Show in Finder / Explorer"), [f = row.file()] { f.revealToUser(); });
    m.addItem (TRANS ("Copy Path"), [p = row.path] { juce::SystemClipboard::copyTextToClipboard (p); });
    m.addItem (TRANS ("Re-analyse"), [this, ids] { db.requestReanalysis (ids); proc.getScanner().requestAnalysis(); });
    m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this).withMousePosition());
}

void SampleManagerEditor::showSideMenu (const SidePanel::Item& it)
{
    juce::PopupMenu m;
    using K = SidePanel::Kind;
    switch (it.kind)
    {
        case K::Library:
            m.addItem (TRANS ("Rescan"), [this, id = it.id] { proc.getScanner().requestScan (id); });
            m.addItem (TRANS ("Remove Library"), [this, id = it.id] { db.removeLibrary (id); query().libraryId = 0; refreshLibraryViews(); runSearch (false); });
            break;
        case K::Collection:
            m.addItem (TRANS ("Rename..."), [this, id = it.id, name = it.text]
            { askText (TRANS ("Rename collection"), name, [this, id] (juce::String n) { db.renameCollection (id, n); refreshLibraryViews(); }); });
            m.addItem (TRANS ("Delete Collection"), [this, id = it.id]
            { db.deleteCollection (id); if (query().collectionId == id) query().collectionId = 0; refreshLibraryViews(); runSearch (false); });
            break;
        case K::SavedSearch:
            m.addItem (TRANS ("Open in New Tab"), [this, id = it.id] { sidePanel.onOpenSavedSearch (id); });
            m.addItem (TRANS ("Delete Saved Search"), [this, id = it.id] { db.deleteSavedSearch (id); refreshLibraryViews(); });
            break;
        case K::Tag:
            m.addItem (TRANS ("Include"), [this, id = it.id] { toggleTag (id, false); });
            m.addItem (TRANS ("Exclude"), [this, id = it.id] { toggleTag (id, true); });
            m.addItem (TRANS ("Search in New Tab"), [this, id = it.id, name = it.text] { SearchQuery q; q.includeTags.add (id); q.title = name; newTab (q); });
            m.addSeparator();
            m.addItem (TRANS ("Rename Tag..."), [this, id = it.id, name = it.text]
            { askText (TRANS ("Rename tag"), name, [this, id] (juce::String n) { db.renameTag (id, n); refreshLibraryViews(); runSearch (true); }); });
            m.addItem (TRANS ("Delete Tag"), [this, id = it.id] { db.deleteTag (id); refreshLibraryViews(); runSearch (true); });
            break;
        case K::Header: case K::All: case K::Favourites: case K::Duplicates:
            return;
    }
    m.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this).withMousePosition());
}

void SampleManagerEditor::addLibrary()
{
    chooser = std::make_unique<juce::FileChooser> (TRANS ("Add a sample folder (Splice, Loopcloud, Noiiz and any other folder work)"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
    {
        auto dir = fc.getResult();
        if (! dir.isDirectory()) return;
        const auto id = db.addLibrary (dir);
        proc.getScanner().requestScan (id);
        refreshLibraryViews();
    });
}

//==============================================================================
bool SampleManagerEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details,
                                                                 juce::StringArray& files, bool& canMove)
{
    if (details.description.toString() != "sm-samples") return false;
    canMove = false;
    auto ids = table.getSelectedIds();
    const bool processed = proc.getPreviewSettings().dragProcessed;
    for (auto& r : table.getRows())
    {
        if (! ids.contains (r.id)) continue;
        if (processed && proc.hasSelection() && proc.getSelectedSample().id == r.id)
            files.add (proc.renderSelectedToFile().getFullPathName());
        else
            files.add (r.path);
    }
    return ! files.isEmpty();
}

void SampleManagerEditor::startExternalDrag()
{
    if (! proc.hasSelection()) return;
    auto file = proc.getPreviewSettings().dragProcessed ? proc.renderSelectedToFile() : proc.getSelectedSample().file();
    if (file.existsAsFile())
        performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

//==============================================================================
bool SampleManagerEditor::keyPressed (const juce::KeyPress& key)
{
    const bool cmd = key.getModifiers().isCommandDown();
    const bool typing = dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) != nullptr;
    if (key == juce::KeyPress::spaceKey && ! typing) { proc.togglePlay(); return true; }
    if (cmd && key.getKeyCode() == 'T') { newTab ({}); return true; }
    if (cmd && key.getKeyCode() == 'W') { closeTab (currentTab); return true; }
    if (cmd && key.getKeyCode() == 'F') { searchBar.focusSearch(); return true; }
    if (! typing && ! cmd)
    {
        const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
        if (c == 'r') { pickRandom(); return true; }
        if (c == 'b') { randomBack(); return true; }
        if (c == 'f') { if (auto* s = table.getFirstSelected()) toggleFavourite (s->id); return true; }
        if (c == 't') { editTags(); return true; }
        if (c == 's') { findSimilar(); return true; }
    }
    return false;
}

void SampleManagerEditor::timerCallback()
{
    auto& engine = proc.getEngine();
    float ph = engine.getPlayheadNormalised();
    if (ph >= 0)
    {
        const auto dir = proc.getPreviewSettings().direction;
        if (dir == ProcessSettings::Reverse) ph = 1.0f - ph;
        else if (dir == ProcessSettings::PingPong) ph = ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
    }
    waveform.setPlayhead (ph);
    waveform.setTrim (proc.getTrimStart(), proc.getTrimEnd());
    controls.setPlaying (engine.isPlaying());
    controls.getMeter().setLevels (engine.getPeakLeft(), engine.getPeakRight());

    if (++tick % 10 != 0) return;

    auto& ts = proc.getTempoSource();
    if (proc.isStandalone())
        searchBar.setTempoDisplay (ts.getTempo(), ts.isLinkEnabled(), ts.getNumPeers(), false);
    else
        searchBar.setTempoDisplay (proc.getProjectTempo(), false, 0, true);

    // library changes from the scanner, another plugin instance or the standalone app
    const auto dv = db.dataVersion();
    const auto lv = db.localVersion();
    const auto bc = proc.getScanner().getBatchCounter();
    if (dv != lastDataVersion || lv != lastLocalVersion || bc != lastBatch)
    {
        lastDataVersion = dv; lastLocalVersion = lv; lastBatch = bc;
        refreshLibraryViews();
        if (! showingSpecialList) runSearch (true);
    }

    auto prog = proc.getScanner().getProgress();
    if (prog.running)
        statusLabel.setText (juce::String ((int) table.getRows().size()) + " " + TRANS ("samples") + "   |   "
                             + TRANS ("Scanning") + ": " + juce::String (prog.filesFound) + " " + TRANS ("files") + ", "
                             + TRANS ("analysed") + " " + juce::String (prog.analysed) + "/" + juce::String (prog.toAnalyse)
                             + "   " + prog.currentFile, juce::dontSendNotification);
}

//==============================================================================
void SampleManagerEditor::saveUiState()
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> t;
    for (auto& q : tabs) t.add (q.toVar());
    o->setProperty ("tabs", t);
    o->setProperty ("current", currentTab);
    o->setProperty ("columns", table.getColumnLayout());
    o->setProperty ("side", sideVisible);
    o->setProperty ("sideWidth", sidePanel.getWidth());
    o->setProperty ("waveHeight", waveform.getHeight());
    o->setProperty ("snap", controls.getSnapBox().getSelectedId());
    proc.uiStateJson = juce::JSON::toString (juce::var (o), true);
    db.setSetting ("lastUiState", proc.uiStateJson); // new instances start where you left off
}

void SampleManagerEditor::restoreUiState()
{
    auto json = proc.uiStateJson.isNotEmpty() ? proc.uiStateJson : db.getSetting ("lastUiState");
    auto v = juce::JSON::parse (json);
    tabs.clear();
    if (auto* arr = v["tabs"].getArray())
        for (auto& q : *arr) tabs.push_back (SearchQuery::fromVar (q));
    if (tabs.empty()) tabs.push_back ({});
    currentTab = juce::jlimit (0, (int) tabs.size() - 1, (int) v.getProperty ("current", 0));
    table.restoreColumnLayout (v["columns"].toString());
    table.setSort (query().sortColumn, query().ascending);
    sideVisible = v.getProperty ("side", true);
    sidePanel.setVisible (sideVisible);
    hBar->setVisible (sideVisible);
    if ((int) v.getProperty ("sideWidth", 0) > 100) hLayout.setItemLayout (0, 150, 420, (int) v["sideWidth"]);
    if ((int) v.getProperty ("waveHeight", 0) > 50) vLayout.setItemLayout (2, 60, 500, (int) v["waveHeight"]);
    const int snap = v.getProperty ("snap", 2);
    controls.getSnapBox().setSelectedId (snap, juce::dontSendNotification);
    waveform.setSnap ((WaveformView::Snap) (snap - 1));
}

void SampleManagerEditor::showSettings()
{
    auto panel = std::make_unique<SettingsPanel> (proc);
    panel->onLanguageChanged = [this]
    {
        applyLanguage (db.getSetting ("language", "en"));
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "SampleManager",
                                                TRANS ("Close and reopen the window to apply the new language."));
    };
    panel->onAudioSettings = []
    {
       #if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            holder->showAudioSettingsDialog();
       #endif
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), searchBar.getBounds().removeFromRight (40).removeFromTop (70), this);
}
}
