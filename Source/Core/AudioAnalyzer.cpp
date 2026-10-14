#include "AudioAnalyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <map>

namespace sm
{
namespace
{
    std::vector<float> mixToMono (const juce::AudioBuffer<float>& b, int maxSamples)
    {
        const int n = juce::jmin (b.getNumSamples(), maxSamples);
        std::vector<float> m ((size_t) n, 0.0f);
        const int ch = b.getNumChannels();
        if (ch == 0) return m;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = b.getReadPointer (c);
            for (int i = 0; i < n; ++i) m[(size_t) i] += d[i];
        }
        const float g = 1.0f / (float) ch;
        for (auto& v : m) v *= g;
        return m;
    }

    // Krumhansl-Kessler key profiles
    constexpr double majorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    constexpr double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double pearson (const double* a, const double* b, int n)
    {
        double ma = 0, mb = 0;
        for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
        ma /= n; mb /= n;
        double num = 0, da = 0, db = 0;
        for (int i = 0; i < n; ++i)
        {
            num += (a[i] - ma) * (b[i] - mb);
            da  += (a[i] - ma) * (a[i] - ma);
            db  += (b[i] - mb) * (b[i] - mb);
        }
        return (da > 0 && db > 0) ? num / std::sqrt (da * db) : 0.0;
    }
}

//==============================================================================
std::array<double, 12> AudioAnalyzer::chromagram (const float* mono, int numSamples, double sampleRate)
{
    std::array<double, 12> chroma {};
    if (numSamples <= 0 || sampleRate <= 0) return chroma;

    constexpr int order = 13;
    constexpr int fftSize = 1 << order;
    const int hop = fftSize / 2;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false);

    // Precompute bin -> pitch class with a weight that peaks at the semitone centre
    std::vector<int> binPc (fftSize / 2, -1);
    std::vector<float> binW (fftSize / 2, 0.0f);
    for (int k = 1; k < fftSize / 2; ++k)
    {
        const double f = k * sampleRate / fftSize;
        if (f < 50.0 || f > 5000.0) continue;
        const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
        const double nearest = std::round (midi);
        const double dist = std::abs (midi - nearest); // 0..0.5
        binPc[(size_t) k] = ((int) nearest % 12 + 12) % 12;
        binW[(size_t) k]  = (float) std::pow (std::cos (dist * juce::MathConstants<double>::pi), 2.0);
    }

    std::vector<float> frame ((size_t) fftSize * 2);
    for (int start = 0; start < juce::jmax (1, numSamples - fftSize / 4); start += hop)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        const int len = juce::jmin (fftSize, numSamples - start);
        if (len <= 0) break;
        std::copy (mono + start, mono + start + len, frame.begin());
        window.multiplyWithWindowingTable (frame.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (frame.data(), true);

        std::array<double, 12> fc {};
        for (int k = 1; k < fftSize / 2; ++k)
            if (binPc[(size_t) k] >= 0)
                fc[(size_t) binPc[(size_t) k]] += binW[(size_t) k] * frame[(size_t) k];
        double frameSum = 0;
        for (auto v : fc) frameSum += v;
        if (frameSum > 1e-9)
            for (int i = 0; i < 12; ++i) chroma[(size_t) i] += fc[(size_t) i] / frameSum;
    }

    double total = 0;
    for (auto v : chroma) total += v;
    if (total > 0) for (auto& v : chroma) v /= total;
    return chroma;
}

MusicalKey AudioAnalyzer::keyFromChroma (const std::array<double, 12>& chroma, bool allowSingleNote, double* confidence)
{
    MusicalKey best;
    if (confidence) *confidence = 0;

    double maxV = 0, sum = 0;
    int maxI = 0;
    for (int i = 0; i < 12; ++i) { sum += chroma[(size_t) i]; if (chroma[(size_t) i] > maxV) { maxV = chroma[(size_t) i]; maxI = i; } }
    if (sum <= 0) return best;
    const double mean = sum / 12.0;
    if (maxV < mean * 1.35) return best; // flat chroma: noise or unpitched

    if (allowSingleNote)
    {
        double second = 0;
        for (int i = 0; i < 12; ++i)
            if (i != maxI) second = juce::jmax (second, chroma[(size_t) i]);
        // a single pitched sound: the root dominates; its fifth (3rd harmonic) is the usual runner up
        if (maxV > second * 1.5)
        {
            best.root = maxI;
            best.mode = MusicalKey::Note;
            if (confidence) *confidence = juce::jlimit (0.0, 1.0, (maxV - second) / maxV);
            return best;
        }
    }

    double bestScore = -2, secondScore = -2;
    for (int mode = 0; mode < 2; ++mode)
        for (int root = 0; root < 12; ++root)
        {
            double rotated[12];
            for (int i = 0; i < 12; ++i)
                rotated[i] = chroma[(size_t) ((i + root) % 12)];
            const double score = pearson (rotated, mode == 0 ? majorProfile : minorProfile, 12);
            if (score > bestScore)
            {
                secondScore = bestScore;
                bestScore = score;
                best.root = root;
                best.mode = mode == 0 ? MusicalKey::Major : MusicalKey::Minor;
            }
            else if (score > secondScore)
                secondScore = score;
        }
    if (confidence) *confidence = juce::jlimit (0.0, 1.0, (bestScore - secondScore) * 5.0 + juce::jmax (0.0, bestScore) * 0.5);
    if (bestScore < 0.3) best.root = -1;
    return best;
}

//==============================================================================
std::vector<double> AudioAnalyzer::onsetEnvelope (const float* mono, int numSamples, double sampleRate, double& envRate)
{
    constexpr int order = 10;
    constexpr int fftSize = 1 << order;
    constexpr int hop = 256;
    envRate = sampleRate / hop;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false);

    std::vector<float> frame ((size_t) fftSize * 2), prev ((size_t) fftSize / 2, 0.0f);
    std::vector<int> bandOf ((size_t) fftSize / 2, 0);
    int bandCount[3] = { 0, 0, 0 };
    for (int k = 1; k < fftSize / 2; ++k)
    {
        const double f = k * sampleRate / fftSize;
        bandOf[(size_t) k] = f < 200.0 ? 0 : (f < 2000.0 ? 1 : 2);
        bandCount[bandOf[(size_t) k]]++;
    }
    const double bandWeight[3] = { 1.0 / juce::jmax (1, bandCount[0]), 0.5 / juce::jmax (1, bandCount[1]), 0.25 / juce::jmax (1, bandCount[2]) };

    std::vector<double> env;
    for (int start = -fftSize / 2; start + fftSize / 2 < numSamples; start += hop)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = start + i;
            if (idx >= 0 && idx < numSamples) frame[(size_t) i] = mono[idx];
        }
        window.multiplyWithWindowingTable (frame.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (frame.data(), true);
        // band-weighted flux: broadband hats must not drown the kick/snare that carry the beat
        double band[3] = { 0, 0, 0 };
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const float mag = std::log1p (frame[(size_t) k] * 100.0f);
            const float d = mag - prev[(size_t) k];
            if (d > 0) band[bandOf[(size_t) k]] += d;
            prev[(size_t) k] = mag;
        }
        env.push_back (band[0] * bandWeight[0] + band[1] * bandWeight[1] + band[2] * bandWeight[2]);
    }

    // remove local mean (adaptive threshold) and half-wave rectify
    const int w = juce::jmax (1, (int) (envRate * 0.1));
    std::vector<double> onset (env.size(), 0.0);
    for (size_t i = 0; i < env.size(); ++i)
    {
        double m = 0; int c = 0;
        for (int j = (int) i - w; j <= (int) i + w; ++j)
            if (j >= 0 && j < (int) env.size()) { m += env[(size_t) j]; ++c; }
        onset[i] = juce::jmax (0.0, env[i] - m / c);
    }
    return onset;
}

double AudioAnalyzer::estimateTempo (const float* mono, int numSamples, double sampleRate, double* confidence)
{
    if (confidence) *confidence = 0;
    if (numSamples < (int) (sampleRate * 1.0)) return 0.0;
    double envRate = 0;
    auto onset = onsetEnvelope (mono, numSamples, sampleRate, envRate);
    if (onset.size() < 16) return 0.0;

    const int minLag = (int) std::floor (envRate * 60.0 / 200.0);
    const int maxLag = juce::jmin ((int) onset.size() / 2, (int) std::ceil (envRate * 60.0 / 60.0));
    if (maxLag <= minLag + 2) return 0.0;

    auto ac = [&] (int lag)
    {
        double s = 0;
        for (size_t i = (size_t) lag; i < onset.size(); ++i) s += onset[i] * onset[i - (size_t) lag];
        return s / (double) (onset.size() - (size_t) lag);
    };
    const double ac0 = ac (0);
    if (ac0 <= 0) return 0.0;

    std::vector<double> acv ((size_t) (2 * maxLag + 2), 0.0);
    for (int lag = 1; lag < (int) acv.size() && lag < (int) onset.size() - 1; ++lag) acv[(size_t) lag] = ac (lag);

    int bestLag = 0; double bestScore = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        const double bpm = 60.0 * envRate / lag;
        // perceptual prior around 130 BPM (log-gaussian, ~1 octave): only breaks half/double ties
        const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 130.0) / 0.9, 2.0));
        // comb over multiples: a true beat period repeats at 1x..4x its lag
        double s = 0; int terms = 0;
        for (int k = 1; k <= 4; ++k)
            if (k * lag < (int) acv.size()) { s += acv[(size_t) (k * lag)]; ++terms; }
        if (terms == 0) continue;
        s = s / terms * prior;
        if (s > bestScore) { bestScore = s; bestLag = lag; }
    }
    if (bestLag == 0) return 0.0;

    double lag = bestLag;
    if (bestLag > 1 && bestLag + 1 < (int) acv.size())
    {
        const double a = acv[(size_t) bestLag - 1], b = acv[(size_t) bestLag], c = acv[(size_t) bestLag + 1];
        const double denom = a - 2 * b + c;
        if (std::abs (denom) > 1e-12) lag += juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / denom);
    }
    if (confidence) *confidence = juce::jlimit (0.0, 1.0, acv[(size_t) bestLag] / ac0);
    return 60.0 * envRate / lag;
}

double AudioAnalyzer::tempoFromLoopLength (double seconds, double guess)
{
    if (seconds < 0.9 || guess <= 0) return 0.0;
    static const int beatCounts[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 128 };
    double best = 0, bestScore = 1e9, bestErr = 1e9;
    for (auto beats : beatCounts)
    {
        const double bpm = 60.0 * beats / seconds;
        if (bpm < 55 || bpm > 210) continue;
        for (double factor : { 1.0, 2.0, 0.5 })
        {
            const double err = std::abs (std::log2 (bpm / (guess * factor)));
            // prefer the guess itself, and tempos in the common 80-180 range
            const double score = err + (factor != 1.0 ? 0.02 : 0.0) + ((bpm < 80 || bpm >= 180) ? 0.05 : 0.0);
            if (score < bestScore) { bestScore = score; bestErr = err; best = bpm; }
        }
    }
    // accept within ~3.5 %
    return bestErr < 0.05 ? best : 0.0;
}

double AudioAnalyzer::tempoFromBeatGrid (const std::vector<double>& onset, double envRate, double seconds, double* score)
{
    if (score) *score = 0;
    if (onset.size() < 16 || seconds < 0.9) return 0.0;
    // loops start on the downbeat and last whole bars: test every bar-length tempo and
    // keep the grid that explains most of the onset energy (squared, so kicks/snares dominate)
    double total = 0;
    for (auto v : onset) total += v * v;
    if (total <= 0) return 0.0;
    const int n = (int) onset.size();
    const int w = 3;
    static const int beatCounts[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 128 };
    double best = 0, bestScore = -1;
    int bestBeats = 0;
    std::map<int, std::pair<double, double>> shares; // beats -> (share, score)
    std::map<int, std::vector<double>> pointEnergy;  // beats -> onset energy at each grid point
    for (auto beats : beatCounts)
    {
        const double bpm = 60.0 * beats / seconds;
        if (bpm < 55 || bpm > 210) continue;
        const double framesPerBeat = envRate * 60.0 / bpm;
        std::vector<char> used ((size_t) n, 0);
        double captured = 0; int covered = 0;
        auto& pts = pointEnergy[beats];
        for (int b = 0; b < beats; ++b)
        {
            const int centre = (int) std::llround (b * framesPerBeat);
            double e = 0;
            for (int i = centre - w; i <= centre + w; ++i)
                if (i >= 0 && i < n && ! used[(size_t) i]) { used[(size_t) i] = 1; e += onset[(size_t) i] * onset[(size_t) i]; ++covered; }
            captured += e;
            pts.push_back (e);
        }
        const double share = captured / total;
        const double chance = (double) covered / n;
        const double prior = std::exp (-0.5 * std::pow (std::log2 (bpm / 120.0) / 0.9, 2.0));
        const double sc = share - chance + 0.05 * prior;
        shares[beats] = { share, sc };
        if (sc > bestScore) { bestScore = sc; best = bpm; bestBeats = beats; }
    }
    // Half-time check: if the extra (off-beat) grid points of the faster tempo only catch
    // weak onsets such as eighth-note hats, the slower tempo is the musical one (90, not 180).
    // Medians (without the downbeat) keep one big transient, e.g. a bass attack on beat 1, from deciding.
    auto median = [] (std::vector<double> v)
    {
        if (v.empty()) return 0.0;
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    while (bestBeats % 2 == 0 && shares.count (bestBeats / 2) > 0)
    {
        const auto& pts = pointEnergy[bestBeats];
        std::vector<double> on, off;
        for (size_t i = 1; i < pts.size(); ++i) (i % 2 == 0 ? on : off).push_back (pts[i]); // skip the downbeat
        const double mOn = median (on), mOff = median (off);
        if (on.empty() || mOff >= 0.6 * mOn) break;
        bestBeats /= 2;
        best = 60.0 * bestBeats / seconds;
        bestScore = shares[bestBeats].second;
    }
    if (score) *score = bestScore;
    return best;
}

//==============================================================================
juce::String AudioAnalyzer::fingerprint (const float* mono, int numSamples, double sampleRate)
{
    // Coarse loudness contour (64 segments, 16 levels) + length in 1/20 s.
    // Identical audio in different containers/bit depths yields the same print.
    constexpr int segments = 64;
    juce::String fp = juce::String::toHexString ((int) std::round (numSamples / sampleRate * 20.0)) + ":";
    if (numSamples <= 0) return fp;
    std::array<double, segments> e {};
    double maxE = 0;
    for (int s = 0; s < segments; ++s)
    {
        const int a = (int) ((int64_t) numSamples * s / segments);
        const int b = juce::jmax (a + 1, (int) ((int64_t) numSamples * (s + 1) / segments));
        double sum = 0;
        for (int i = a; i < b && i < numSamples; ++i) sum += mono[i] * mono[i];
        e[(size_t) s] = std::sqrt (sum / (b - a));
        maxE = juce::jmax (maxE, e[(size_t) s]);
    }
    for (auto v : e)
    {
        const int q = maxE > 0 ? juce::jlimit (0, 15, (int) std::round (std::sqrt (v / maxE) * 15.0)) : 0;
        fp << juce::String::charToString ("0123456789abcdef"[q]);
    }
    return fp;
}

WaveformOverview AudioAnalyzer::makeOverview (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    WaveformOverview w;
    const int n = buffer.getNumSamples();
    if (n <= 0) return w;
    auto mono = mixToMono (buffer, n);

    // simple band split with one-pole filters
    const float aLow  = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 200.0f  / (float) sampleRate);
    const float aHigh = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 2500.0f / (float) sampleRate);
    float lp = 0, lp2 = 0;

    constexpr int bins = WaveformOverview::numBins;
    std::vector<float> mn (bins, 0), mx (bins, 0), lo (bins, 0), mi (bins, 0), hi (bins, 0);
    std::vector<int> cnt (bins, 0);
    for (int i = 0; i < n; ++i)
    {
        const float x = mono[(size_t) i];
        lp  += aLow  * (x - lp);
        lp2 += aHigh * (x - lp2);
        const float low = lp, high = x - lp2, mid = lp2 - lp;
        const int b = (int) ((int64_t) i * bins / n);
        mn[(size_t) b] = juce::jmin (mn[(size_t) b], x);
        mx[(size_t) b] = juce::jmax (mx[(size_t) b], x);
        lo[(size_t) b] += low * low; mi[(size_t) b] += mid * mid; hi[(size_t) b] += high * high;
        cnt[(size_t) b]++;
    }
    float maxBand = 1e-12f, peak = 1e-9f;
    for (int b = 0; b < bins; ++b)
    {
        const float c = (float) juce::jmax (1, cnt[(size_t) b]);
        lo[(size_t) b] = std::sqrt (lo[(size_t) b] / c);
        mi[(size_t) b] = std::sqrt (mi[(size_t) b] / c) * 1.6f; // mids/highs carry less energy, weight up for display
        hi[(size_t) b] = std::sqrt (hi[(size_t) b] / c) * 2.5f;
        maxBand = juce::jmax (maxBand, lo[(size_t) b], mi[(size_t) b], hi[(size_t) b]);
        peak = juce::jmax (peak, std::abs (mn[(size_t) b]), std::abs (mx[(size_t) b]));
    }
    w.data.resize ((size_t) bins * 5);
    for (int b = 0; b < bins; ++b)
    {
        auto toByte = [] (float v) { return (uint8_t) juce::jlimit (0, 255, (int) std::round (v)); };
        w.data[(size_t) b * 5 + 0] = toByte (128.0f + mn[(size_t) b] / peak * 127.0f);
        w.data[(size_t) b * 5 + 1] = toByte (128.0f + mx[(size_t) b] / peak * 127.0f);
        w.data[(size_t) b * 5 + 2] = toByte (lo[(size_t) b] / maxBand * 255.0f);
        w.data[(size_t) b * 5 + 3] = toByte (mi[(size_t) b] / maxBand * 255.0f);
        w.data[(size_t) b * 5 + 4] = toByte (hi[(size_t) b] / maxBand * 255.0f);
    }
    return w;
}

int AudioAnalyzer::nearestZeroCrossing (const float* data, int numSamples, int position, int radius)
{
    position = juce::jlimit (0, juce::jmax (0, numSamples - 1), position);
    for (int d = 0; d <= radius; ++d)
    {
        for (int sgn : { -1, 1 })
        {
            const int i = position + sgn * d;
            if (i <= 0 || i >= numSamples) continue;
            if ((data[i - 1] <= 0.0f && data[i] > 0.0f) || (data[i - 1] >= 0.0f && data[i] < 0.0f) || data[i] == 0.0f)
                return i;
        }
    }
    return position;
}

//==============================================================================
AnalysisResult AudioAnalyzer::analyse (const juce::AudioBuffer<float>& buffer, double sampleRate,
                                       const FilenameHints& hints, Options options)
{
    AnalysisResult r;
    r.sampleRate = sampleRate;
    r.channels = buffer.getNumChannels();
    if (r.durationSeconds <= 0) r.durationSeconds = buffer.getNumSamples() / juce::jmax (1.0, sampleRate);
    if (buffer.getNumSamples() == 0 || sampleRate <= 0) return r;

    const int maxN = (int) juce::jmin ((double) buffer.getNumSamples(), options.maxSecondsToAnalyse * sampleRate);
    auto mono = mixToMono (buffer, maxN);
    const int n = (int) mono.size();

    // levels, attack, effective length
    double sumSq = 0; int peakIdx = 0; float peak = 0;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (mono[(size_t) i]);
        sumSq += (double) a * a;
        if (a > peak) { peak = a; peakIdx = i; }
    }
    r.peak = peak;
    r.rms = (float) std::sqrt (sumSq / n);
    r.attackSeconds = (float) (peakIdx / sampleRate);
    const float silence = juce::jmax (1.0e-4f, peak * 0.001f); // -60 dB relative
    int last = n - 1;
    while (last > 0 && std::abs (mono[(size_t) last]) < silence) --last;
    r.effectiveSeconds = (double) (last + 1) / sampleRate;
    if (buffer.getNumSamples() > n) r.effectiveSeconds = r.durationSeconds; // not fully analysed

    // spectrum summary (centroid, low share) from one big averaged FFT pass
    {
        constexpr int order = 11, size = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> win ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> frame ((size_t) size * 2);
        std::vector<double> spec ((size_t) size / 2, 0.0);
        for (int s = 0; s < juce::jmax (1, n - size / 2); s += size)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            const int len = juce::jmin (size, n - s);
            std::copy (mono.begin() + s, mono.begin() + s + len, frame.begin());
            win.multiplyWithWindowingTable (frame.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (frame.data(), true);
            for (int k = 0; k < size / 2; ++k) spec[(size_t) k] += frame[(size_t) k] * frame[(size_t) k];
        }
        double num = 0, den = 0, low = 0;
        for (int k = 1; k < size / 2; ++k)
        {
            const double f = k * sampleRate / size;
            num += f * spec[(size_t) k]; den += spec[(size_t) k];
            if (f < 150.0) low += spec[(size_t) k];
        }
        r.spectralCentroid = den > 0 ? (float) (num / den) : 0.0f;
        r.lowRatio = den > 0 ? (float) (low / den) : 0.0f;
    }

    // tempo / loop detection
    double tempoConf = 0;
    const double est = estimateTempo (mono.data(), n, sampleRate, &tempoConf);
    const double fullLen = r.durationSeconds;
    double gridScore = 0;
    double lengthTempo = 0;
    if (hints.bpm > 0)
        lengthTempo = tempoFromLoopLength (fullLen, hints.bpm);
    else if (fullLen >= 1.5 && buffer.getNumSamples() <= n)
    {
        double envRate = 0;
        auto onset = onsetEnvelope (mono.data(), n, sampleRate, envRate);
        lengthTempo = tempoFromBeatGrid (onset, envRate, fullLen, &gridScore);
        if (gridScore < 0.25) lengthTempo = 0;
    }
    else
        lengthTempo = tempoFromLoopLength (fullLen, est);

    // how much is still going on at the end? loops don't decay to silence
    double tailRms = 0;
    {
        const int tailStart = (int) (n * 0.85);
        double s = 0;
        for (int i = tailStart; i < n; ++i) s += (double) mono[(size_t) i] * mono[(size_t) i];
        tailRms = std::sqrt (s / juce::jmax (1, n - tailStart));
    }
    const bool sustainsToEnd = r.rms > 0 && tailRms / r.rms > 0.12;

    if (hints.typeHint == 1)       r.isLoop = true;
    else if (hints.typeHint == 2)  r.isLoop = false;
    else if (hints.bpm > 0 && fullLen >= 1.5) r.isLoop = true;
    else r.isLoop = fullLen >= 1.5 && lengthTempo > 0 && sustainsToEnd && tempoConf > 0.05;

    if (r.isLoop)
    {
        if (hints.bpm > 0)          r.bpm = hints.bpm, r.bpmConfidence = 1.0;
        else if (lengthTempo > 0)   r.bpm = lengthTempo, r.bpmConfidence = juce::jmax (0.6, tempoConf);
        else                        r.bpm = est, r.bpmConfidence = tempoConf;
        r.bpm = std::round (r.bpm * 100.0) / 100.0;
    }

    // key
    if (hints.key.isValid())
    {
        r.key = hints.key;
        r.keyConfidence = 1.0;
    }
    else
    {
        auto chroma = chromagram (mono.data(), n, sampleRate);
        r.key = keyFromChroma (chroma, ! r.isLoop, &r.keyConfidence);
        // very short/noisy hits rarely carry a usable key
        if (! r.isLoop && r.spectralCentroid > 4000.0f) r.key = {};
    }

    r.fingerprint = fingerprint (mono.data(), n, sampleRate);
    r.overview = makeOverview (buffer, sampleRate);
    return r;
}

bool AudioAnalyzer::analyseFile (juce::AudioFormatManager& fm, const juce::File& file, AnalysisResult& result, Options options)
{
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0) return false;

    const auto total = reader->lengthInSamples;
    const int toRead = (int) std::min<juce::int64> (total, (juce::int64) (options.maxSecondsToAnalyse * reader->sampleRate));
    const int chans = (int) juce::jlimit<unsigned int> (1, 2, reader->numChannels);
    juce::AudioBuffer<float> buf (chans, juce::jmax (1, toRead));
    buf.clear();
    if (toRead > 0) reader->read (&buf, 0, toRead, 0, true, chans > 1);

    auto hints = filename::parse (file.getFileNameWithoutExtension(),
                                  file.getParentDirectory().getFileName() + " " + file.getParentDirectory().getParentDirectory().getFileName());
    result = analyse (buf, reader->sampleRate, hints, options);
    result.durationSeconds = (double) total / reader->sampleRate;
    if (toRead < total) result.effectiveSeconds = result.durationSeconds;
    result.channels = (int) reader->numChannels;
    result.bitDepth = (int) reader->bitsPerSample;
    result.codec = reader->getFormatName();
    // loop decisions that depend on full length
    if (toRead < total && result.isLoop && hints.bpm <= 0)
    {
        auto lt = tempoFromLoopLength (result.durationSeconds, result.bpm);
        if (lt > 0) result.bpm = std::round (lt * 100.0) / 100.0;
    }
    return true;
}
}
