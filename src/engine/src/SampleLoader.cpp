#include "engine/SampleLoader.h"

#include <stdexcept>

#include "miniaudio/miniaudio.h"

namespace pmg {

std::shared_ptr<SampleAsset> LoadSampleAsset(const std::string& path, uint32_t targetSampleRate) {
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 1, targetSampleRate);

    void* pcmFrames = nullptr;
    ma_uint64 frameCount = 0;
    ma_result result = ma_decode_file(path.c_str(), &decoderConfig, &frameCount, &pcmFrames);
    if (result != MA_SUCCESS) {
        throw std::runtime_error("LoadSampleAsset: failed to decode '" + path +
                                  "' (miniaudio result " + std::to_string(static_cast<int>(result)) + ")");
    }

    auto asset = std::make_shared<SampleAsset>();
    asset->channels = 1;
    asset->sampleRate = targetSampleRate;
    const float* samples = static_cast<const float*>(pcmFrames);
    asset->interleavedPCM.assign(samples, samples + frameCount);

    ma_free(pcmFrames, nullptr);

    return asset;
}

} // namespace pmg
