#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <functional>

namespace sm
{
/** MIDI-mappable controls (random, filters, fades, gain ...).
    The audio thread pushes incoming CCs into a lock-free FIFO; the message
    thread drains it, handles "learn" and dispatches actions. */
class MidiLearn
{
public:
    enum class Action
    {
        None = 0, Random, RandomBack, PlayStop, Next, Previous,
        HighPass, LowPass, FadeIn, FadeOut, Gain, NumActions
    };

    static juce::String actionName (Action a);
    static bool isContinuous (Action a);

    // Audio thread
    void pushFromAudioThread (const juce::MidiBuffer& midi);

    // Message thread
    void startLearning (Action a) { learning = a; }
    void cancelLearning() { learning = Action::None; }
    Action getLearning() const { return learning; }
    /** Drains the FIFO. 'handler' gets (action, value 0..1). */
    void dispatch (const std::function<void (Action, float)>& handler);

    void setMapping (int controller, Action a);
    void clearMapping (Action a);
    int controllerFor (Action a) const;
    std::map<int, Action> getMappings() const { return mappings; }

    juce::String serialise() const;      // "cc:action;cc:action"
    void deserialise (const juce::String& s);

    std::function<void()> onMappingsChanged;

private:
    struct Event { int controller; int value; };
    juce::AbstractFifo fifo { 256 };
    std::array<Event, 256> events {};
    std::map<int, Action> mappings;      // controller (channel*128 + cc) -> action
    Action learning = Action::None;
};
}
