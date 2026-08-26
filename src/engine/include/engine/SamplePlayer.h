#pragma once

#include <cstddef>
#include <memory>

#include "engine/SampleAsset.h"

namespace pmg {

// One sample-playback slot: an asset pointer plus a read cursor. State is
// mutated only on the audio thread (via commands drained from ParameterBus),
// never called directly from the control thread.
class SamplePlayer {
public:
    void Trigger(std::shared_ptr<const SampleAsset> asset, float gain, float pan = 0.0f);

    float RenderSample();
    bool IsActive() const;
    float GetPan() const { return m_pan; }

private:
    std::shared_ptr<const SampleAsset> m_asset;
    size_t m_frameIndex = 0;
    float m_gain = 1.0f;
    float m_pan = 0.0f;
    bool m_active = false;
};

} // namespace pmg
