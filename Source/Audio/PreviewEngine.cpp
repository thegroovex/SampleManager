#include "PreviewEngine.h"

namespace sm
{
PreviewEngine::PreviewEngine() {}

void PreviewEngine::prepare (double sr, int)
{
    sampleRate = sr;
}

void PreviewEngine::setSample (RenderedSample::Ptr s, bool keepPlaying)
{
    {
        const juce::ScopedLock rl (retainedLock);
        if (s != nullptr) retained.add (s); // keeps the audio thread from ever freeing memory
    }
    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        pending = s;
        keepPositionOnSwap = keepPlaying;
        hasPending = true;
    }
    releaseUnused();
}

void PreviewEngine::releaseUnused()
{
    // only our list still references these buffers: safe to free on the message thread
    const juce::ScopedLock rl (retainedLock);
    for (int i = retained.size(); --i >= 0;)
        if (retained.getObjectPointerUnchecked (i)->getReferenceCount() == 1)
            retained.remove (i);
}

RenderedSample::Ptr PreviewEngine::getSample() const
{
    const juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (swapLock));
    return pending != nullptr ? pending : current;
}

void PreviewEngine::play()
{
    stopRequested = false;
    pendingStart = true;
}

void PreviewEngine::stop()
{
    pendingStart = false;
    stopRequested = true;
}

int PreviewEngine::samplesUntilBoundary (double beat, double bpm, double sr, double unit, int blockLength)
{
    if (bpm <= 0 || unit <= 0) return 0;
    const double samplesPerBeat = sr * 60.0 / bpm;
    double next = std::ceil (beat / unit - 1e-9) * unit;
    const double offset = (next - beat) * samplesPerBeat;
    if (offset < 0) return 0;
    const int off = (int) std::floor (offset);
    return off < blockLength ? off : -1;
}

void PreviewEngine::renderVoice (Voice& v, const RenderedSample& s, juce::AudioBuffer<float>& out, int start, int num, bool loop)
{
    const int len = s.buffer.getNumSamples();
    if (len <= 1) { v.active = false; return; }
    auto* l = s.buffer.getReadPointer (0);
    auto* r = s.buffer.getReadPointer (s.buffer.getNumChannels() > 1 ? 1 : 0);
    auto* ol = out.getWritePointer (0);
    auto* orr = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);
    const float vol = volume.load() * v.gain;
    const bool mono = out.getNumChannels() == 1;

    for (int i = start; i < start + num; ++i)
    {
        if (v.pos >= len)
        {
            if (loop) v.pos = std::fmod (v.pos, (double) len);
            else { v.active = false; return; }
        }
        const int i0 = (int) v.pos;
        const int i1 = i0 + 1 < len ? i0 + 1 : (loop ? 0 : i0);
        const float frac = (float) (v.pos - i0);
        float g = vol;
        if (v.releasing)
        {
            v.release -= v.releaseStep;
            if (v.release <= 0.0f) { v.active = false; return; }
            g *= v.release;
        }
        const float sl = (l[i0] + frac * (l[i1] - l[i0])) * g;
        const float sr = (r[i0] + frac * (r[i1] - r[i0])) * g;
        if (mono) ol[i] += 0.5f * (sl + sr);
        else { ol[i] += sl; orr[i] += sr; }
        v.pos += v.increment;
    }
}

void PreviewEngine::startPreviewAt (double sourcePos)
{
    preview.active = true;
    preview.releasing = false;
    preview.release = 1.0f;
    preview.gain = 1.0f;
    preview.pos = sourcePos;
    playing = true;
}

void PreviewEngine::process (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const TransportState& t)
{
    const int numSamples = out.getNumSamples();

    // pick up a newly rendered sample
    if (hasPending.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (swapLock);
        if (tl.isLocked())
        {
            auto old = current;
            const double oldLen = old != nullptr ? (double) old->buffer.getNumSamples() : 0.0;
            current = pending;
            pending = nullptr;
            hasPending = false;
            if (current != nullptr && keepPositionOnSwap.load() && preview.active && oldLen > 0)
                preview.pos = preview.pos / oldLen * current->buffer.getNumSamples(); // keep musical position
            else if (! keepPositionOnSwap.load())
                preview.active = false, playing = false;
            for (auto& v : voices) v.active = false;
        }
    }

    auto* s = current.get();
    if (s == nullptr) { playhead = -1.0f; return; }

    const double rateRatio = s->outputRate / sampleRate; // 1 normally
    const int len = s->buffer.getNumSamples();

    // stop with a short fade to avoid clicks
    if (stopRequested.exchange (false) && preview.active)
    {
        preview.releasing = true;
        preview.release = 1.0f;
        preview.releaseStep = 1.0f / (float) juce::jmax (1.0, sampleRate * 0.008);
    }

    int startOffset = 0;
    if (pendingStart.load())
    {
        const int q = quantize.load();
        if (q != 0 && t.valid && t.playing)
        {
            const double unit = q == 1 ? 1.0 : 4.0;
            const int off = samplesUntilBoundary (t.beatAtBlockStart, t.bpm, sampleRate, unit, numSamples);
            if (off >= 0)
            {
                pendingStart = false;
                startOffset = off;
                double pos = 0.0;
                if (s->isLoop && s->lengthInBeats > 0)
                {
                    // start in phase with the transport so the loop lines up with the beat
                    const double beatAtStart = t.beatAtBlockStart + off * t.bpm / (60.0 * sampleRate);
                    const double phase = std::fmod (beatAtStart, s->lengthInBeats) / s->lengthInBeats;
                    pos = juce::jlimit (0.0, (double) len - 1, phase * len);
                }
                startPreviewAt (pos);
                preview.increment = rateRatio;
            }
        }
        else
        {
            pendingStart = false;
            startPreviewAt (0.0);
            preview.increment = rateRatio;
        }
    }

    // MIDI notes trigger the sample chromatically around the root note
    int pos = 0;
    auto renderSpan = [&] (int from, int to)
    {
        if (to <= from) return;
        if (preview.active)
        {
            const int s0 = juce::jmax (from, startOffset);
            if (s0 < to) renderVoice (preview, *s, out, s0, to - s0, looping.load());
            if (! preview.active) playing = false;
        }
        for (auto& v : voices)
            if (v.active) renderVoice (v, *s, out, from, to - from, false);
    };

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderSpan (pos, at);
        pos = at;
        if (m.isNoteOn())
        {
            Voice* v = nullptr;
            for (auto& cand : voices) if (! cand.active) { v = &cand; break; }
            if (v == nullptr) v = &voices[0];
            v->active = true;
            v->note = m.getNoteNumber();
            v->pos = 0.0;
            v->increment = rateRatio * std::pow (2.0, (m.getNoteNumber() - midiRoot.load()) / 12.0);
            v->gain = m.getFloatVelocity();
            v->releasing = false;
            v->release = 1.0f;
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices)
                if (v.active && v.note == m.getNoteNumber() && ! v.releasing)
                {
                    v.releasing = true;
                    v.releaseStep = 1.0f / (float) juce::jmax (1.0, sampleRate * 0.03);
                }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) v.active = false;
    }
    renderSpan (pos, numSamples);

    playhead = preview.active ? (float) (preview.pos / juce::jmax (1, len)) : -1.0f;
    peakL = juce::jmax (peakL.load(), out.getMagnitude (0, 0, numSamples));
    if (out.getNumChannels() > 1) peakR = juce::jmax (peakR.load(), out.getMagnitude (1, 0, numSamples));
}
}
