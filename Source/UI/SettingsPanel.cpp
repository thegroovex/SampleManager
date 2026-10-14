#include "SettingsPanel.h"
#include "LookAndFeel.h"
#include "Core/AppPaths.h"

namespace sm
{
SettingsPanel::SettingsPanel (SampleManagerProcessor& p) : proc (p)
{
    auto& db = proc.getDatabase();
    langLabel.setText (TRANS ("Language"), juce::dontSendNotification);
    addAndMakeVisible (langLabel);
    languageBox.addItem ("English", 1);
    languageBox.addItem ("Deutsch", 2);
    languageBox.setSelectedId (db.getSetting ("language", "en") == "de" ? 2 : 1, juce::dontSendNotification);
    languageBox.onChange = [this]
    {
        proc.getDatabase().setSetting ("language", languageBox.getSelectedId() == 2 ? "de" : "en");
        if (onLanguageChanged) onLanguageChanged();
    };
    addAndMakeVisible (languageBox);

    autoPlay.setButtonText (TRANS ("Play samples when selected"));
    autoPlay.setToggleState (proc.getPreviewSettings().autoPlay, juce::dontSendNotification);
    autoPlay.onClick = [this] { proc.getPreviewSettings().autoPlay = autoPlay.getToggleState(); };
    addAndMakeVisible (autoPlay);

    rescanOnStart.setButtonText (TRANS ("Rescan libraries on startup"));
    rescanOnStart.setToggleState (db.getSetting ("rescanOnStartup", "1") == "1", juce::dontSendNotification);
    rescanOnStart.onClick = [this] { proc.getDatabase().setSetting ("rescanOnStartup", rescanOnStart.getToggleState() ? "1" : "0"); };
    addAndMakeVisible (rescanOnStart);

    rescanAll.setButtonText (TRANS ("Rescan all libraries"));
    rescanAll.onClick = [this] { proc.getScanner().requestScan(); };
    addAndMakeVisible (rescanAll);

    reanalyseAll.setButtonText (TRANS ("Re-analyse everything"));
    reanalyseAll.onClick = [this]
    {
        juce::Array<juce::int64> all;
        SearchQuery q; q.limit = 10000000;
        for (auto& r : proc.getDatabase().search (q)) all.add (r.id);
        proc.getDatabase().requestReanalysis (all);
        proc.getScanner().requestAnalysis();
    };
    addAndMakeVisible (reanalyseAll);

    dbLabel.setText (TRANS ("Library database (shared by app and plugin):") + "\n" + db.getFile().getFullPathName(), juce::dontSendNotification);
    dbLabel.setFont (juce::FontOptions (11.5f));
    dbLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (dbLabel);
    showDb.setButtonText (TRANS ("Show"));
    showDb.onClick = [this] { proc.getDatabase().getFile().revealToUser(); };
    addAndMakeVisible (showDb);

    if (proc.isStandalone())
    {
        audioSettings.setButtonText (TRANS ("Audio / MIDI Settings..."));
        audioSettings.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
        addAndMakeVisible (audioSettings);
    }

    midiLabel.setText (TRANS ("MIDI Learn"), juce::dontSendNotification);
    midiLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (midiLabel);
    for (int a = 1; a < (int) MidiLearn::Action::NumActions; ++a)
    {
        MidiRow row;
        row.action = (MidiLearn::Action) a;
        row.name = std::make_unique<juce::Label> (juce::String(), TRANS (MidiLearn::actionName (row.action)));
        row.cc = std::make_unique<juce::Label>();
        row.cc->setColour (juce::Label::textColourId, colours::textDim);
        row.learn = std::make_unique<juce::TextButton> (TRANS ("Learn"));
        row.clear = std::make_unique<juce::TextButton> (juce::String::fromUTF8 ("\xc3\x97"));
        const auto act = row.action;
        row.learn->onClick = [this, act]
        {
            auto& ml = proc.getMidiLearn();
            if (ml.getLearning() == act) ml.cancelLearning(); else ml.startLearning (act);
            refreshMidi();
        };
        row.clear->onClick = [this, act]
        {
            proc.getMidiLearn().clearMapping (act);
            proc.getDatabase().setSetting ("midiMappings", proc.getMidiLearn().serialise());
            refreshMidi();
        };
        addAndMakeVisible (*row.name); addAndMakeVisible (*row.cc); addAndMakeVisible (*row.learn); addAndMakeVisible (*row.clear);
        midiRows.push_back (std::move (row));
    }

    versionLabel.setText ("SampleManager " SM_VERSION "  -  open source (AGPLv3)", juce::dontSendNotification);
    versionLabel.setFont (juce::FontOptions (11.0f));
    versionLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (versionLabel);

    refreshMidi();
    startTimerHz (5);
    setSize (480, 300 + (int) midiRows.size() * 26);
}

SettingsPanel::~SettingsPanel() { proc.getMidiLearn().cancelLearning(); }

void SettingsPanel::timerCallback() { refreshMidi(); }

void SettingsPanel::refreshMidi()
{
    auto& ml = proc.getMidiLearn();
    for (auto& r : midiRows)
    {
        const int cc = ml.controllerFor (r.action);
        r.cc->setText (cc < 0 ? "-" : "Ch " + juce::String (cc / 128 + 1) + "  CC " + juce::String (cc % 128), juce::dontSendNotification);
        const bool learning = ml.getLearning() == r.action;
        r.learn->setToggleState (learning, juce::dontSendNotification);
        r.learn->setButtonText (learning ? TRANS ("Move a control...") : TRANS ("Learn"));
    }
}

void SettingsPanel::paint (juce::Graphics& g) { g.fillAll (colours::panel); }

void SettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto row = r.removeFromTop (26);
    langLabel.setBounds (row.removeFromLeft (120));
    languageBox.setBounds (row.removeFromLeft (150));
    r.removeFromTop (8);
    autoPlay.setBounds (r.removeFromTop (24));
    rescanOnStart.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);
    row = r.removeFromTop (26);
    rescanAll.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (8);
    reanalyseAll.setBounds (row.removeFromLeft (200));
    if (proc.isStandalone())
    {
        r.removeFromTop (6);
        audioSettings.setBounds (r.removeFromTop (26).removeFromLeft (220));
    }
    r.removeFromTop (8);
    row = r.removeFromTop (34);
    showDb.setBounds (row.removeFromRight (70).withSizeKeepingCentre (70, 24));
    dbLabel.setBounds (row);
    r.removeFromTop (8);
    midiLabel.setBounds (r.removeFromTop (22));
    for (auto& m : midiRows)
    {
        auto line = r.removeFromTop (26);
        m.name->setBounds (line.removeFromLeft (150));
        m.cc->setBounds (line.removeFromLeft (110));
        m.clear->setBounds (line.removeFromRight (26).reduced (1));
        line.removeFromRight (4);
        m.learn->setBounds (line.reduced (0, 1));
    }
    versionLabel.setBounds (r.removeFromBottom (18));
}
}
