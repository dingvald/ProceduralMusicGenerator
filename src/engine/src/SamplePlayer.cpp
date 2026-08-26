#include "engine/SamplePlayer.h"

#include <utility>

namespace pmg {

void SamplePlayer::Trigger(std::shared_ptr<const SampleAsset> asset, float gain, float pan) {
    m_asset = std::move(asset);
    m_frameIndex = 0;
    m_gain = gain;
    m_pan = pan;
    m_active = m_asset != nullptr && !m_asset->interleavedPCM.empty();
}

float SamplePlayer::RenderSample() {
    if (!m_active || !m_asset) {
        return 0.0f;
    }

    if (m_frameIndex >= m_asset->interleavedPCM.size()) {
        m_active = false;
        return 0.0f;
    }

    float sample = m_asset->interleavedPCM[m_frameIndex] * m_gain;
    ++m_frameIndex;
    if (m_frameIndex >= m_asset->interleavedPCM.size()) {
        m_active = false;
    }
    return sample;
}

bool SamplePlayer::IsActive() const {
    return m_active;
}

} // namespace pmg
