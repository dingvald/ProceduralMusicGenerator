#pragma once

namespace pmg {

// Real chip sound hardware outputs through a coarse, low-rate DAC rather
// than a modern high-resolution one; bitDepth == 16 and holdFactor == 1
// are both no-ops, so an unconfigured LoFiProcessor passes samples through
// unchanged.
struct LoFiConfig {
    int bitDepth = 16;  // amplitude quantization levels = 2^bitDepth; clamped to [1, 16]
    int holdFactor = 1; // sample-and-hold: output updates once every N samples; clamped to >= 1
};

// Applies bit-depth quantization and sample-and-hold downsampling to a
// stream of samples, one at a time. The hold step is a deliberately naive
// zero-order hold with no anti-alias filtering, matching how cheap chip
// DACs (and early digital audio generally) actually sound "steppy" rather
// than cleanly downsampled. Audio-thread safe: no allocation, no locking.
class LoFiProcessor {
public:
    void Configure(const LoFiConfig& config);

    float Process(float input);

private:
    LoFiConfig m_config;
    float m_heldSample = 0.0f;
    int m_holdCounter = 0;
};

} // namespace pmg
