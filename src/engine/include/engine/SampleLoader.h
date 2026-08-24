#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "engine/SampleAsset.h"

namespace pmg {

// Decodes a WAV (or any other miniaudio-supported format) file to mono
// float PCM at the given sample rate, once, on the control thread at
// startup. Throws std::runtime_error on failure.
std::shared_ptr<SampleAsset> LoadSampleAsset(const std::string& path, uint32_t targetSampleRate);

} // namespace pmg
