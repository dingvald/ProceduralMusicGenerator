#include "engine/GainCrossfader.h"

#include <algorithm>
#include <cmath>

namespace pmg {

void GainCrossfader::Configure(const GainCrossfadeConfig& config) {
    m_config = config;
    m_currentGain = 0.0f;
    m_initialized = false;
}

float GainCrossfader::NextGain(float parameterValue, double deltaSeconds) {
    float range = m_config.paramAtGainMax - m_config.paramAtGainMin;
    float t = range != 0.0f ? (parameterValue - m_config.paramAtGainMin) / range
                             : (parameterValue >= m_config.paramAtGainMin ? 1.0f : 0.0f);
    t = std::clamp(t, 0.0f, 1.0f);
    float target = m_config.gainAtMin + t * (m_config.gainAtMax - m_config.gainAtMin);

    // The very first tick jumps straight to the target rather than ramping
    // up from an arbitrary starting gain -- a track shouldn't audibly fade
    // in from silence just because this is the first time NextGain() ran;
    // it should start wherever the current parameter value already implies.
    if (!m_initialized) {
        m_currentGain = target;
        m_initialized = true;
        return m_currentGain;
    }

    if (m_config.smoothingSeconds <= 0.0f) {
        m_currentGain = target;
        return m_currentGain;
    }

    float maxStep =
        std::fabs(m_config.gainAtMax - m_config.gainAtMin) / m_config.smoothingSeconds * static_cast<float>(deltaSeconds);
    float delta = target - m_currentGain;
    if (std::fabs(delta) <= maxStep) {
        m_currentGain = target;
    } else {
        m_currentGain += delta > 0.0f ? maxStep : -maxStep;
    }
    return m_currentGain;
}

} // namespace pmg
