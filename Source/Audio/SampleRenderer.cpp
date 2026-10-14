#include "SampleRenderer.h"
#include <juce_dsp/juce_dsp.h>
#include <SoundTouch.h>

namespace sm
{
bool ProcessSettings::operator== (const ProcessSettings& o) const
{
    return startNorm == o.startNorm && endNorm == o.endNorm && direction == o.direction
        && sourceBpm == o.sourceBpm && targetBpm == o.targetBpm && keepPitch == o.keepPitch
        && transposeSemitones == o.transposeSemitones && highPassHz == o.highPassHz && lowPassHz == o.lowPassHz
        && fadeInMs == o.fadeInMs && fadeOutMs == o.fadeOutMs && gainDb == o.gainDb && normalize == o.normalize;
}

double ProcessSettings::tempoRatio() const
{
    if (sourceBpm <= 0 || targetBpm <= 0) return 1.0;
    return juce::jlimit (0.25, 4.0, targetBpm / sourceBpm);
}

bool ProcessSettings::isNeutral() const
{
    return startNorm <= 0 && endNorm >= 1 && direction == Forward && std::abs (tempoRatio() - 1.0) < 1e-6
        && transposeSemitones == 0 && highPassHz <= 0 && lowPassHz <= 0 && fadeInMs <= 0 && fadeOutMs <= 0
        && gainDb == 0 && ! normalize;
}

bool SampleRenderer::load (juce::AudioFormatManager& fm, const juce::File& f, juce::AudioBuffer<float>& out,
                           double& sampleRate, double maxSeconds)
{
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->sampleRate <= 0) return false;
    const auto n = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxSeconds * reader->sampleRate));
    const int ch = (int) juce::jlimit<unsigned int> (1, 2, reader->numChannels);
    out.setSize (ch, juce::jmax (1, n));
    out.clear();
    if (n > 0) reader->read (&out, 0, n, 0, true, ch > 1);
    sampleRate = reader->sampleRate;
    return true;
}

juce::AudioBuffer<float> SampleRenderer::stretch (const juce::AudioBuffer<float>& in, double sampleRate,
                                                  double tempo, double pitchSemitones, double rate)
{
    const int ch = in.getNumChannels();
    const int n = in.getNumSamples();
    soundtouch::SoundTouch st;
    st.setSampleRate ((unsigned int) juce::jmax (8000.0, sampleRate));
    st.setChannels ((unsigned int) ch);
    st.setTempo (tempo);
    st.setRate (rate);
    st.setPitchSemiTones (pitchSemitones);
    st.setSetting (SETTING_USE_QUICKSEEK, 0);
    st.setSetting (SETTING_USE_AA_FILTER, 1);

    const int expected = (int) std::llround (n / (tempo * rate));
    std::vector<float> interleaved;
    std::vector<float> outInterleaved;
    outInterleaved.reserve ((size_t) (expected + 8192) * (size_t) ch);

    constexpr int chunk = 4096;
    interleaved.resize ((size_t) chunk * (size_t) ch);
    std::vector<float> recv ((size_t) chunk * (size_t) ch);

    auto drain = [&]
    {
        unsigned int got;
        while ((got = st.receiveSamples (recv.data(), (unsigned int) chunk)) > 0)
            outInterleaved.insert (outInterleaved.end(), recv.begin(), recv.begin() + (long) got * ch);
    };

    for (int pos = 0; pos < n; pos += chunk)
    {
        const int len = juce::jmin (chunk, n - pos);
        for (int i = 0; i < len; ++i)
            for (int c = 0; c < ch; ++c)
                interleaved[(size_t) (i * ch + c)] = in.getSample (c, pos + i);
        st.putSamples (interleaved.data(), (unsigned int) len);
        drain();
    }
    st.flush();
    drain();

    juce::AudioBuffer<float> out (ch, juce::jmax (1, expected));
    out.clear();
    const int avail = (int) (outInterleaved.size() / (size_t) ch);
    const int copyN = juce::jmin (avail, expected);
    for (int i = 0; i < copyN; ++i)
        for (int c = 0; c < ch; ++c)
            out.setSample (c, i, outInterleaved[(size_t) (i * ch + c)]);
    return out;
}

void SampleRenderer::applyFades (juce::AudioBuffer<float>& b, double sr, double inMs, double outMs)
{
    const int n = b.getNumSamples();
    const int fi = juce::jmin (n, (int) (inMs * 0.001 * sr));
    const int fo = juce::jmin (n, (int) (outMs * 0.001 * sr));
    if (fi > 0) for (int c = 0; c < b.getNumChannels(); ++c) b.applyGainRamp (c, 0, fi, 0.0f, 1.0f);
    if (fo > 0) for (int c = 0; c < b.getNumChannels(); ++c) b.applyGainRamp (c, n - fo, fo, 1.0f, 0.0f);
}

void SampleRenderer::applyFilters (juce::AudioBuffer<float>& b, double sr, double hp, double lp)
{
    const double nyq = sr * 0.5;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto* d = b.getWritePointer (c);
        if (hp > 10.0 && hp < nyq * 0.95)
        {
            juce::IIRFilter f;
            f.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, hp, 0.7071));
            f.processSamples (d, b.getNumSamples());
        }
        if (lp > 10.0 && lp < nyq * 0.95)
        {
            juce::IIRFilter f;
            f.setCoefficients (juce::IIRCoefficients::makeLowPass (sr, lp, 0.7071));
            f.processSamples (d, b.getNumSamples());
        }
    }
}

void SampleRenderer::normalise (juce::AudioBuffer<float>& b, float target)
{
    const float peak = b.getMagnitude (0, b.getNumSamples());
    if (peak > 1.0e-6f) b.applyGain (target / peak);
}

juce::AudioBuffer<float> SampleRenderer::render (const juce::AudioBuffer<float>& source, double srcRate,
                                                 const ProcessSettings& s, double outRate)
{
    // 1) trim
    const int n = source.getNumSamples();
    int a = juce::jlimit (0, n, (int) std::floor (s.startNorm * n));
    int e = juce::jlimit (0, n, (int) std::ceil (s.endNorm * n));
    if (e - a < 16) { a = 0; e = n; }
    juce::AudioBuffer<float> work (2, juce::jmax (1, e - a));
    for (int c = 0; c < 2; ++c)
        work.copyFrom (c, 0, source, juce::jmin (c, source.getNumChannels() - 1), a, e - a);

    // 2) tempo / pitch / sample-rate in one SoundTouch pass
    const double tempoRatio = s.tempoRatio();
    const double srRatio = srcRate / outRate;
    double tempo = 1.0, rate = srRatio, pitch = s.transposeSemitones;
    if (s.keepPitch) tempo = tempoRatio;
    else             rate *= tempoRatio; // varispeed: faster = higher
    const bool needsStretch = std::abs (tempo - 1.0) > 1e-6 || std::abs (rate - 1.0) > 1e-6 || std::abs (pitch) > 1e-6;
    if (needsStretch)
    {
        // keep loops sample-exact: target length = source length / (tempo * rate)
        work = stretch (work, srcRate, tempo, pitch, rate);
    }

    // 3) filters, 4) direction, 5) fades, 6) level
    applyFilters (work, outRate, s.highPassHz, s.lowPassHz);
    if (s.direction == ProcessSettings::Reverse)
        work.reverse (0, work.getNumSamples());
    else if (s.direction == ProcessSettings::PingPong)
    {
        const int len = work.getNumSamples();
        juce::AudioBuffer<float> pp (2, len * 2);
        for (int c = 0; c < 2; ++c)
        {
            pp.copyFrom (c, 0, work, c, 0, len);
            pp.copyFrom (c, len, work, c, 0, len);
        }
        pp.reverse (len, len);
        work = std::move (pp);
    }
    applyFades (work, outRate, s.fadeInMs, s.fadeOutMs);
    if (s.normalize) normalise (work);
    if (s.gainDb != 0.0) work.applyGain (juce::Decibels::decibelsToGain ((float) s.gainDb, -100.0f));
    return work;
}

bool SampleRenderer::writeWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& dest, int bitDepth)
{
    dest.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (dest.createOutputStream());
    if (os == nullptr) return false;
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (sampleRate)
                    .withNumChannels (buffer.getNumChannels()).withBitsPerSample (bitDepth);
    auto writer = wav.createWriterFor (os, opts);
    if (writer == nullptr) return false;
    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}
}
