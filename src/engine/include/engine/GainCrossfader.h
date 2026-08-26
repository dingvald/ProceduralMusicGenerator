#pragma once

#include "engine/CompositionConfig.h"

namespace pmg {

// Smoothly ramps one instrument's Mixer track gain toward a target derived
// linearly from a GameParameters value, instead of snapping to it -- e.g. a
// tension pad's gain climbing gradually as "danger" rises rather than
// popping in at full volume the instant a threshold is crossed. Same
// Configure()-then-tick shape as LoFiProcessor/DelayProcessor, but ticked
// once per control-thread Sequencer::Update() (with a variable
// deltaSeconds) rather than once per audio sample.
class GainCrossfader {
public:
    void Configure(const GainCrossfadeConfig& config);

    // Advances the current gain toward the target derived from
    // parameterValue by at most a linear rate of
    // |gainAtMax - gainAtMin| / smoothingSeconds per second (so
    // smoothingSeconds is "time to cross the full configured gain range"),
    // clamping to the target rather than overshooting it. smoothingSeconds
    // <= 0 snaps straight to the target. Returns the new current gain (also
    // available via CurrentGain()).
    float NextGain(float parameterValue, double deltaSeconds);

    float CurrentGain() const { return m_currentGain; }

private:
    GainCrossfadeConfig m_config;
    float m_currentGain = 0.0f;
    bool m_initialized = false;
};

} // namespace pmg
