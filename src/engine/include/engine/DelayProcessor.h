#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pmg {

// A single delay/echo tap in the shared post-mix chain, modeled on the kind
// of digital echo buffer real chip-era consoles had on-die (e.g. the SNES
// S-DSP's echo buffer) rather than a generic modern reverb effect. mix == 0
// (the default) is a no-op. feedback is clamped below 1.0 so the feedback
// loop can never grow unbounded.
struct DelayConfig {
    float delayTimeSeconds = 0.0f;
    float feedback = 0.0f; // clamped to [0, 0.98] in Configure
    float mix = 0.0f;      // 0 (dry only, disabled) - 1 (wet only)
};

// Audio-thread safe: no allocation after Configure(), no locking -- the
// backing buffer is a fixed-size array sized for the longest delay this
// class supports, regardless of the delay actually configured.
class DelayProcessor {
public:
    // 2.0s @ 48kHz; a longer requested delayTimeSeconds is silently clamped.
    static constexpr size_t kMaxDelayBufferSamples = 96000;

    void Configure(const DelayConfig& config, uint32_t sampleRate);

    float Process(float input);

private:
    DelayConfig m_config;
    std::array<float, kMaxDelayBufferSamples> m_buffer{};
    uint32_t m_delaySamples = 0;
    uint32_t m_readIndex = 0;
};

} // namespace pmg
