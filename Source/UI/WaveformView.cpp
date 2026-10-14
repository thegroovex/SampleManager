#include "WaveformView.h"
#include "LookAndFeel.h"

namespace sm
{
void WaveformView::setOverview (const WaveformOverview& o) { overview = o; repaint(); }

void WaveformView::setAudio (std::shared_ptr<const juce::AudioBuffer<float>> a, double sr, double tempo)
{
    audio = std::move (a);
    audioRate = sr;
    bpm = tempo;
    repaint();
}

void WaveformView::clear()
{
    overview = {};
    audio.reset();
    trimStart = 0; trimEnd = 1;
    resetZoom();
}

void WaveformView::setTrim (double s, double e)
{
    if (s != trimStart || e != trimEnd) { trimStart = s; trimEnd = e; repaint(); }
}

void WaveformView::setPlayhead (float p)
{
    if (std::abs (p - playhead) > 0.0005f) { playhead = p; repaint(); }
}

void WaveformView::resetZoom() { viewStart = 0; viewEnd = 1; repaint(); }

void WaveformView::zoomBy (double factor, double anchor)
{
    if (anchor < 0) anchor = (viewStart + viewEnd) * 0.5;
    const double span = juce::jlimit (0.002, 1.0, (viewEnd - viewStart) / factor);
    double s = anchor - (anchor - viewStart) / (viewEnd - viewStart) * span;
    s = juce::jlimit (0.0, 1.0 - span, s);
    viewStart = s; viewEnd = s + span;
    repaint();
}

double WaveformView::xToNorm (float x) const
{
    return viewStart + juce::jlimit (0.0, 1.0, (double) x / juce::jmax (1, getWidth())) * (viewEnd - viewStart);
}

float WaveformView::normToX (double n) const
{
    return (float) ((n - viewStart) / (viewEnd - viewStart) * getWidth());
}

double WaveformView::applySnap (double n) const
{
    n = juce::jlimit (0.0, 1.0, n);
    if (audio == nullptr || audio->getNumSamples() == 0) return n;
    const int len = audio->getNumSamples();
    switch (snap)
    {
        case Snap::ZeroCrossing:
        {
            const int pos = AudioAnalyzer::nearestZeroCrossing (audio->getReadPointer (0), len, (int) (n * len));
            return (double) pos / len;
        }
        case Snap::Sixteenth:
        case Snap::Beat:
        {
            if (bpm <= 0) return n;
            const double unit = (snap == Snap::Beat ? 1.0 : 0.25) * 60.0 / bpm * audioRate / len;
            return juce::jlimit (0.0, 1.0, std::round (n / unit) * unit);
        }
        case Snap::Off: break;
    }
    return n;
}

juce::Colour WaveformView::colourAt (double n) const
{
    if (overview.isEmpty()) return colours::bandMid;
    const int bins = WaveformOverview::numBins;
    const int b = juce::jlimit (0, bins - 1, (int) (n * bins));
    const float lo = overview.data[(size_t) b * 5 + 2] / 255.0f;
    const float mi = overview.data[(size_t) b * 5 + 3] / 255.0f;
    const float hi = overview.data[(size_t) b * 5 + 4] / 255.0f;
    const float sum = lo + mi + hi + 1.0e-6f;
    const float r = (colours::bandLow.getFloatRed() * lo + colours::bandMid.getFloatRed() * mi + colours::bandHigh.getFloatRed() * hi) / sum;
    const float g = (colours::bandLow.getFloatGreen() * lo + colours::bandMid.getFloatGreen() * mi + colours::bandHigh.getFloatGreen() * hi) / sum;
    const float bl = (colours::bandLow.getFloatBlue() * lo + colours::bandMid.getFloatBlue() * mi + colours::bandHigh.getFloatBlue() * hi) / sum;
    return juce::Colour::fromFloatRGBA (r, g, bl, 1.0f);
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (colours::panel);
    const float mid = (float) bounds.getHeight() * 0.5f;
    const float amp = (float) bounds.getHeight() * 0.46f;
    const int w = bounds.getWidth();

    // beat grid when the tempo is known
    if (audio != nullptr && bpm > 0 && audio->getNumSamples() > 0)
    {
        const double beatNorm = 60.0 / bpm * audioRate / audio->getNumSamples();
        if (beatNorm * w / (viewEnd - viewStart) > 6)
            for (int b = (int) std::floor (viewStart / beatNorm); b * beatNorm <= viewEnd; ++b)
            {
                const float x = normToX (b * beatNorm);
                g.setColour (colours::outline.withAlpha (b % 4 == 0 ? 0.9f : 0.4f));
                g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
            }
    }

    g.setColour (colours::outline.withAlpha (0.6f));
    g.drawHorizontalLine ((int) mid, 0.0f, (float) w);

    if (audio != nullptr && audio->getNumSamples() > 0)
    {
        const int len = audio->getNumSamples();
        const int chs = audio->getNumChannels();
        for (int x = 0; x < w; ++x)
        {
            const double n0 = xToNorm ((float) x), n1 = xToNorm ((float) x + 1);
            const int a = juce::jlimit (0, len - 1, (int) (n0 * len));
            const int b = juce::jlimit (a + 1, len, (int) (n1 * len));
            float mn = 0, mx = 0;
            for (int c = 0; c < chs; ++c)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax (audio->getReadPointer (c, a), b - a);
                mn = juce::jmin (mn, range.getStart()); mx = juce::jmax (mx, range.getEnd());
            }
            g.setColour (colourAt (n0));
            g.drawVerticalLine (x, mid - mx * amp, mid - mn * amp + 1.0f);
        }
    }
    else if (! overview.isEmpty())
    {
        const int bins = WaveformOverview::numBins;
        for (int x = 0; x < w; ++x)
        {
            const double n = xToNorm ((float) x);
            const int b = juce::jlimit (0, bins - 1, (int) (n * bins));
            const float mn = (float) (overview.data[(size_t) b * 5] - 128) / 127.0f;
            const float mx = (float) (overview.data[(size_t) b * 5 + 1] - 128) / 127.0f;
            g.setColour (colourAt (n));
            g.drawVerticalLine (x, mid - mx * amp, mid - mn * amp + 1.0f);
        }
    }
    else
    {
        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (TRANS ("Select a sample to preview"), bounds, juce::Justification::centred);
        return;
    }

    // dim outside the trim range
    const float xs = normToX (trimStart), xe = normToX (trimEnd);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    if (xs > 0) g.fillRect (0.0f, 0.0f, xs, (float) getHeight());
    if (xe < (float) w) g.fillRect (xe, 0.0f, (float) w - xe, (float) getHeight());
    g.setColour (colours::accent);
    g.fillRect (xs - 1.0f, 0.0f, 2.0f, (float) getHeight());
    g.fillRect (xe - 1.0f, 0.0f, 2.0f, (float) getHeight());
    juce::Path hs, he;
    hs.addTriangle (xs, 0, xs + 9, 0, xs, 9);
    he.addTriangle (xe, 0, xe - 9, 0, xe, 9);
    g.fillPath (hs); g.fillPath (he);

    if (playhead >= 0.0f)
    {
        const double pn = trimStart + playhead * (trimEnd - trimStart);
        g.setColour (juce::Colours::white);
        g.fillRect (normToX (pn) - 0.5f, 0.0f, 1.5f, (float) getHeight());
    }

    if (viewEnd - viewStart < 0.999)
    {
        g.setColour (colours::textDim.withAlpha (0.6f));
        g.fillRect ((float) (viewStart * w), (float) getHeight() - 3.0f, (float) ((viewEnd - viewStart) * w), 3.0f);
    }
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const float xs = normToX (trimStart), xe = normToX (trimEnd);
    const bool nearHandle = std::abs (e.position.x - xs) < 6 || std::abs (e.position.x - xe) < 6;
    setMouseCursor (nearHandle ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    const float xs = normToX (trimStart), xe = normToX (trimEnd);
    if (std::abs (e.position.x - xs) < 6)      dragging = Drag::Start;
    else if (std::abs (e.position.x - xe) < 6) dragging = Drag::End;
    else if (e.mods.isAltDown() || e.mods.isMiddleButtonDown()) { dragging = Drag::Pan; dragAnchorView = viewStart; dragAnchorX = e.position.x; }
    else dragging = Drag::None;
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Drag::Pan)
    {
        const double span = viewEnd - viewStart;
        const double delta = (dragAnchorX - e.position.x) / juce::jmax (1, getWidth()) * span;
        viewStart = juce::jlimit (0.0, 1.0 - span, dragAnchorView + delta);
        viewEnd = viewStart + span;
        repaint();
        return;
    }
    if (dragging == Drag::None) return;
    const double n = e.mods.isShiftDown() ? xToNorm (e.position.x) : applySnap (xToNorm (e.position.x));
    if (dragging == Drag::Start) trimStart = juce::jlimit (0.0, trimEnd - 0.001, n);
    else trimEnd = juce::jlimit (trimStart + 0.001, 1.0, n);
    repaint();
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (dragging == Drag::Start || dragging == Drag::End)
    {
        if (onTrimChanged) onTrimChanged (trimStart, trimEnd);
    }
    else if (dragging == Drag::None && e.mouseWasClicked() && onClicked)
        onClicked();
    dragging = Drag::None;
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    trimStart = 0; trimEnd = 1;
    resetZoom();
    if (onTrimChanged) onTrimChanged (0, 1);
}

void WaveformView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown() || std::abs (w.deltaY) > std::abs (w.deltaX) * 2)
        zoomBy (w.deltaY > 0 ? 1.25 : 0.8, xToNorm (e.position.x));
    else
    {
        const double span = viewEnd - viewStart;
        viewStart = juce::jlimit (0.0, 1.0 - span, viewStart - w.deltaX * span * 0.5);
        viewEnd = viewStart + span;
        repaint();
    }
}

void WaveformView::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    zoomBy (scale, xToNorm (e.position.x));
}
}
