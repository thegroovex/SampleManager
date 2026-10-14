#include "MidiLearn.h"

namespace sm
{
juce::String MidiLearn::actionName (Action a)
{
    switch (a)
    {
        case Action::Random:     return "Random";
        case Action::RandomBack: return "Random Back";
        case Action::PlayStop:   return "Play / Stop";
        case Action::Next:       return "Next Sample";
        case Action::Previous:   return "Previous Sample";
        case Action::HighPass:   return "High-Pass";
        case Action::LowPass:    return "Low-Pass";
        case Action::FadeIn:     return "Fade In";
        case Action::FadeOut:    return "Fade Out";
        case Action::Gain:       return "Gain";
        default: break;
    }
    return {};
}

bool MidiLearn::isContinuous (Action a)
{
    return a == Action::HighPass || a == Action::LowPass || a == Action::FadeIn || a == Action::FadeOut || a == Action::Gain;
}

void MidiLearn::pushFromAudioThread (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isController()) continue;
        const juce::AbstractFifo::ScopedWrite w (fifo, 1);
        if (w.blockSize1 > 0) events[(size_t) w.startIndex1] = { (m.getChannel() - 1) * 128 + m.getControllerNumber(), m.getControllerValue() };
    }
}

void MidiLearn::dispatch (const std::function<void (Action, float)>& handler)
{
    const int ready = fifo.getNumReady();
    if (ready == 0) return;
    const juce::AbstractFifo::ScopedRead r (fifo, ready);
    auto handle = [&] (int start, int size)
    {
        for (int i = start; i < start + size; ++i)
        {
            const auto e = events[(size_t) i];
            if (learning != Action::None)
            {
                setMapping (e.controller, learning);
                learning = Action::None;
                continue;
            }
            auto it = mappings.find (e.controller);
            if (it == mappings.end()) continue;
            const auto a = it->second;
            // buttons fire on press (value >= 64), knobs pass the value through
            if (isContinuous (a)) handler (a, e.value / 127.0f);
            else if (e.value >= 64) handler (a, 1.0f);
        }
    };
    handle (r.startIndex1, r.blockSize1);
    handle (r.startIndex2, r.blockSize2);
}

void MidiLearn::setMapping (int controller, Action a)
{
    clearMapping (a);
    mappings[controller] = a;
    if (onMappingsChanged) onMappingsChanged();
}

void MidiLearn::clearMapping (Action a)
{
    for (auto it = mappings.begin(); it != mappings.end();)
        it = (it->second == a) ? mappings.erase (it) : std::next (it);
}

int MidiLearn::controllerFor (Action a) const
{
    for (auto& [cc, act] : mappings) if (act == a) return cc;
    return -1;
}

juce::String MidiLearn::serialise() const
{
    juce::StringArray parts;
    for (auto& [cc, a] : mappings) parts.add (juce::String (cc) + ":" + juce::String ((int) a));
    return parts.joinIntoString (";");
}

void MidiLearn::deserialise (const juce::String& s)
{
    mappings.clear();
    for (auto& p : juce::StringArray::fromTokens (s, ";", ""))
    {
        auto cc = p.upToFirstOccurrenceOf (":", false, false).getIntValue();
        auto a = p.fromFirstOccurrenceOf (":", false, false).getIntValue();
        if (a > 0 && a < (int) Action::NumActions) mappings[cc] = (Action) a;
    }
}
}
