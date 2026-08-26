#include "engine/DelayProcessor.h"

#include <algorithm>
#include <cmath>

namespace pmg {

void DelayProcessor::Configure(const DelayConfig& config, uint32_t sampleRate) {
    m_config = config;
    m_config.feedback = std::max(0.0f, std::min(0.98f, m_config.feedback));
    m_config.mix = std::max(0.0f, std::min(1.0f, m_config.mix));

    uint32_t sr = sampleRate > 0 ? sampleRate : 48000;
    double requestedSamples = std::max(0.0, std::round(static_cast<double>(m_config.delayTimeSeconds) * sr));
    m_delaySamples = static_cast<uint32_t>(
        std::min(requestedSamples, static_cast<double>(kMaxDelayBufferSamples)));

    m_buffer.fill(0.0f);
    m_readIndex = 0;
}

float DelayProcessor::Process(float input) {
    if (m_delaySamples == 0) {
        return input; // no buffer to read from yet
    }

    float delayed = m_buffer[m_readIndex];
    m_buffer[m_readIndex] = input + delayed * m_config.feedback;
    m_readIndex = (m_readIndex + 1) % m_delaySamples;

    return input * (1.0f - m_config.mix) + delayed * m_config.mix;
}

} // namespace pmg
