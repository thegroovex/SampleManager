#pragma once
#include "PreviewEngine.h"
#include <memory>

namespace sm
{
/** Clock for the standalone app: Ableton Link when enabled, otherwise an
    internal free-running clock at the manual tempo. (The plugin follows the host.) */
class TempoSource
{
public:
    TempoSource();
    ~TempoSource();

    static bool linkAvailable();
    void setLinkEnabled (bool);
    bool isLinkEnabled() const;
    int  getNumPeers() const;

    /** Sets the tempo (and pushes it to the Link session when enabled). */
    void setTempo (double bpm);
    double getTempo() const;

    /** Audio thread: state for the block that starts now. */
    TransportState advance (int numSamples, double sampleRate, double outputLatencySeconds = 0.0);

private:
    struct LinkImpl;
    std::unique_ptr<LinkImpl> link;
    std::atomic<double> manualTempo { 120.0 };
    double internalBeat = 0.0;
};
}
