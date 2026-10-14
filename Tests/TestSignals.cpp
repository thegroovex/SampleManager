#include "TestSignals.h"

namespace sm::test
{
static double mtof (int n) { return 440.0 * std::pow (2.0, (n - 69) / 12.0); }

juce::AudioBuffer<float> chordProgression (const std::vector<std::vector<int>>& chords, double spc, double sr)
{
    const int per = (int) (spc * sr);
    juce::AudioBuffer<float> b (2, per * (int) chords.size());
    b.clear();
    for (size_t c = 0; c < chords.size(); ++c)
        for (int note : chords[c])
            for (int h = 1; h <= 4; ++h)
            {
                const double f = mtof (note) * h;
                const float amp = 0.15f / (float) h;
                for (int i = 0; i < per; ++i)
                {
                    const float env = std::min (1.0f, i / 400.0f) * std::exp (-1.5f * (float) i / per);
                    const float v = amp * env * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * i / sr);
                    b.addSample (0, (int) c * per + i, v);
                    b.addSample (1, (int) c * per + i, v);
                }
            }
    return b;
}

juce::AudioBuffer<float> drumLoop (double bpm, int bars, double sr)
{
    const int n = (int) std::llround (bars * 4 * 60.0 / bpm * sr);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (42);
    const double spb = 60.0 / bpm * sr;
    for (int beat = 0; beat < bars * 4; ++beat)
    {
        const int start = (int) std::llround (beat * spb);
        const float accent = (beat % 4 == 0) ? 1.0f : 0.7f;
        for (int i = 0; i < (int) (0.15 * sr) && start + i < n; ++i)
        {
            const float env = std::exp (-(float) i / (float) (0.03 * sr));
            const float f = 60.0f + 100.0f * std::exp (-(float) i / (float) (0.01 * sr));
            const float v = accent * 0.8f * env * std::sin (2.0f * juce::MathConstants<float>::pi * f * (float) i / (float) sr);
            b.addSample (0, start + i, v); b.addSample (1, start + i, v);
        }
        // off-beat hats keep energy until the end like real loops
        const int hat = (int) std::llround ((beat + 0.5) * spb);
        for (int i = 0; i < (int) (0.05 * sr) && hat + i < n; ++i)
        {
            const float v = 0.2f * std::exp (-(float) i / (float) (0.008 * sr)) * (rng.nextFloat() * 2.0f - 1.0f);
            b.addSample (0, hat + i, v); b.addSample (1, hat + i, v);
        }
    }
    return b;
}

juce::AudioBuffer<float> pluck (int note, double seconds, double sr)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (1, n);
    for (int i = 0; i < n; ++i)
    {
        float v = 0;
        for (int h = 1; h <= 5; ++h)
            v += (0.5f / (float) h) * (float) std::sin (2.0 * juce::MathConstants<double>::pi * mtof (note) * h * i / sr);
        b.setSample (0, i, v * std::exp (-4.0f * (float) i / (float) n) * std::min (1.0f, i / 50.0f));
    }
    return b;
}

juce::AudioBuffer<float> noiseBurst (double seconds, double sr, float decay)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (1, n);
    juce::Random rng (7);
    for (int i = 0; i < n; ++i)
        b.setSample (0, i, (rng.nextFloat() * 2 - 1) * std::exp (-decay * (float) i / (float) sr));
    return b;
}
}
