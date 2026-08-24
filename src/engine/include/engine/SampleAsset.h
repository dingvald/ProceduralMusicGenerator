#pragma once

#include <cstdint>
#include <vector>

namespace pmg {

// Decoded, immutable PCM sample data. Loaded once on the control thread at
// startup, then shared into the audio thread via shared_ptr<const
// SampleAsset> — safe with no locking since it's never mutated after load.
//
// v1 always decodes to mono (channels == 1) to keep SamplePlayer/Mixer
// simple; the `channels` field is kept for future multi-channel support.
struct SampleAsset {
    std::vector<float> interleavedPCM;
    uint32_t channels = 1;
    uint32_t sampleRate = 0;
};

} // namespace pmg
