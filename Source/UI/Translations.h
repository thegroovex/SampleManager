#pragma once
#include <juce_core/juce_core.h>

namespace sm
{
    /** "en" or "de". Applies to every window opened afterwards (process wide). */
    void applyLanguage (const juce::String& code);
}
