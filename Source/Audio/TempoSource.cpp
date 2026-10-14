#include "TempoSource.h"

#if SM_WITH_LINK
 #include <ableton/Link.hpp>
#endif

namespace sm
{
struct TempoSource::LinkImpl
{
   #if SM_WITH_LINK
    ableton::Link link { 120.0 };
   #endif
};

TempoSource::TempoSource() : link (std::make_unique<LinkImpl>()) {}
TempoSource::~TempoSource()
{
   #if SM_WITH_LINK
    link->link.enable (false);
   #endif
}

bool TempoSource::linkAvailable()
{
   #if SM_WITH_LINK
    return true;
   #else
    return false;
   #endif
}

void TempoSource::setLinkEnabled (bool b)
{
   #if SM_WITH_LINK
    if (b)
    {
        auto st = link->link.captureAppSessionState();
        st.setTempo (manualTempo.load(), link->link.clock().micros());
        link->link.commitAppSessionState (st);
    }
    link->link.enable (b);
   #else
    juce::ignoreUnused (b);
   #endif
}

bool TempoSource::isLinkEnabled() const
{
   #if SM_WITH_LINK
    return link->link.isEnabled();
   #else
    return false;
   #endif
}

int TempoSource::getNumPeers() const
{
   #if SM_WITH_LINK
    return (int) link->link.numPeers();
   #else
    return 0;
   #endif
}

void TempoSource::setTempo (double bpm)
{
    bpm = juce::jlimit (20.0, 999.0, bpm);
    manualTempo = bpm;
   #if SM_WITH_LINK
    if (link->link.isEnabled())
    {
        auto st = link->link.captureAppSessionState();
        st.setTempo (bpm, link->link.clock().micros());
        link->link.commitAppSessionState (st);
    }
   #endif
}

double TempoSource::getTempo() const
{
   #if SM_WITH_LINK
    if (link->link.isEnabled())
        return link->link.captureAppSessionState().tempo();
   #endif
    return manualTempo.load();
}

TransportState TempoSource::advance (int numSamples, double sampleRate, double latency)
{
    TransportState t;
    t.valid = true;
    t.playing = true;
   #if SM_WITH_LINK
    if (link->link.isEnabled())
    {
        const auto state = link->link.captureAudioSessionState();
        const auto time = link->link.clock().micros() + std::chrono::microseconds ((long long) (latency * 1.0e6));
        t.bpm = state.tempo();
        t.beatAtBlockStart = state.beatAtTime (time, 4.0);
        if (t.beatAtBlockStart < 0) t.beatAtBlockStart += 4.0 * std::ceil (-t.beatAtBlockStart / 4.0);
        manualTempo = t.bpm;
        return t;
    }
   #endif
    juce::ignoreUnused (latency);
    t.bpm = manualTempo.load();
    t.beatAtBlockStart = internalBeat;
    internalBeat += numSamples * t.bpm / (60.0 * sampleRate);
    if (internalBeat > 1.0e6) internalBeat = std::fmod (internalBeat, 4.0);
    return t;
}
}
