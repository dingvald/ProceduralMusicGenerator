#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "engine/AudioEngine.h"
#include "engine/ConfigLoader.h"
#include "engine/MarkovChainVariationStrategy.h"
#include "engine/Mixer.h"
#include "engine/Pattern.h"
#include "engine/RandomSource.h"
#include "engine/SampleLoader.h"
#include "engine/Sequencer.h"
#include "engine/VariationEngine.h"

// Only the encoder half of miniaudio.h is used here (ma_encoder_*); the
// implementation itself is compiled exactly once, in
// src/engine/src/miniaudio_impl.cpp, so this translation unit just needs the
// declarations.
#include "miniaudio/miniaudio.h"

namespace {

// Renders the composition offline (no audio device at all -- see
// AudioEngine::InitializeOffline) and writes it straight to a WAV file via
// miniaudio's encoder, chunk by chunk, so there's no need to hold the whole
// render in memory. Frames are counted the same way real-time playback would
// count them (AudioEngine::GetFramesProcessed()), just advanced by this
// loop's own RenderFrames() calls instead of a real-time device callback --
// so the exact same Sequencer/VariationEngine logic that drives normal
// playback drives the render, just as fast as the CPU can go rather than
// paced to a wall clock.
bool RenderCompositionToWav(pmg::AudioEngine& audioEngine, pmg::Sequencer& sequencer, const std::string& outputPath,
                             double seconds, uint32_t sampleRate, uint32_t channels) {
    // s16 (not f32) for maximum player compatibility -- some players/browsers
    // don't handle WAVE_FORMAT_IEEE_FLOAT. ma_encoder_write_pcm_frames writes
    // whatever bytes it's handed as-is (no implicit format conversion), so
    // this loop converts RenderFrames()'s f32 output (matching the real-time
    // device path) to s16 itself before each write.
    ma_encoder_config encoderConfig =
        ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, channels, sampleRate);
    ma_encoder encoder;
    if (ma_encoder_init_file(outputPath.c_str(), &encoderConfig, &encoder) != MA_SUCCESS) {
        std::cerr << "Failed to open '" << outputPath << "' for WAV output.\n";
        return false;
    }

    constexpr uint32_t kChunkFrames = 256;
    std::vector<float> floatBuffer(static_cast<size_t>(kChunkFrames) * channels);
    std::vector<int16_t> intBuffer(static_cast<size_t>(kChunkFrames) * channels);
    uint64_t totalFrames = static_cast<uint64_t>(seconds * sampleRate);

    for (uint64_t rendered = 0; rendered < totalFrames; rendered += kChunkFrames) {
        uint32_t framesThisChunk = static_cast<uint32_t>(std::min<uint64_t>(kChunkFrames, totalFrames - rendered));
        sequencer.Update();
        audioEngine.RenderFrames(floatBuffer.data(), framesThisChunk);

        size_t sampleCount = static_cast<size_t>(framesThisChunk) * channels;
        for (size_t i = 0; i < sampleCount; ++i) {
            float clamped = std::clamp(floatBuffer[i], -1.0f, 1.0f);
            intBuffer[i] = static_cast<int16_t>(clamped * 32767.0f);
        }

        ma_uint64 framesWritten = 0;
        ma_encoder_write_pcm_frames(&encoder, intBuffer.data(), framesThisChunk, &framesWritten);
    }

    ma_encoder_uninit(&encoder);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    using namespace pmg;

    bool useNullBackend = false;
    std::string configPath = "composition_demo.json";
    std::string renderWavPath;
    double renderSeconds = 30.0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--null-audio") {
            useNullBackend = true;
        } else if (arg == "--render-wav" && i + 1 < argc) {
            renderWavPath = argv[++i];
        } else if (arg == "--seconds" && i + 1 < argc) {
            renderSeconds = std::stod(argv[++i]);
        } else {
            configPath = arg;
        }
    }

    std::cout.setf(std::ios::unitbuf); // flush eagerly so redirected/piped output stays live during the playback loop

    std::cout << "Procedural Music Generator - demo\n";

    CompositionConfig config;
    try {
        config = ConfigLoader::LoadFromFile(configPath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load " << configPath << ": " << e.what() << "\n";
        return 1;
    }

    std::vector<Pattern> patterns;
    patterns.reserve(config.patterns.size());
    for (const PatternConfig& patternConfig : config.patterns) {
        patterns.push_back(ResolvePattern(patternConfig));
    }

    AudioEngine audioEngine;
    AudioEngineConfig audioConfig;
    audioConfig.sampleRate = config.sampleRate;
    audioConfig.channels = 2;
    audioConfig.useNullBackend = useNullBackend;
    audioConfig.loFi = config.loFi;
    audioConfig.delay = config.delay;
    bool renderingToWav = !renderWavPath.empty();
    bool initOk = renderingToWav ? audioEngine.InitializeOffline(audioConfig) : audioEngine.Initialize(audioConfig);
    if (!initOk) {
        std::cerr << "Failed to initialize audio device.\n";
        return 1;
    }

    Mixer& mixer = audioEngine.GetMixer();
    for (const InstrumentConfig& instrument : config.instruments) {
        if (instrument.type == InstrumentType::Synth) {
            SynthInstrumentDef def;
            def.waveform = instrument.waveform;
            def.dutyCycle = instrument.dutyCycle;
            def.arpeggio = instrument.arpeggio;
            def.envelope = instrument.envelope;
            def.vibrato = instrument.vibrato;
            def.fm = instrument.fm;
            def.gain = instrument.gain;
            def.pan = instrument.pan;
            mixer.AddSynthInstrument(instrument.id, def);
        } else {
            try {
                SampleInstrumentDef def;
                def.asset = LoadSampleAsset(instrument.file, config.sampleRate);
                def.gain = instrument.gain;
                def.pan = instrument.pan;
                mixer.AddSampleInstrument(instrument.id, def);
            } catch (const std::exception& e) {
                std::cerr << "Failed to load sample instrument '" << instrument.id << "': " << e.what() << "\n";
                return 1;
            }
        }
    }

    std::unique_ptr<IVariationStrategy> variationStrategy;
    if (config.variationStrategy == VariationStrategyKind::MarkovChain) {
        variationStrategy = std::make_unique<MarkovChainVariationStrategy>(config.markovChain);
    }

    RandomSource randomSource(std::random_device{}());
    VariationEngine variationEngine(config.variationRules, std::move(variationStrategy));
    Sequencer sequencer(audioEngine, variationEngine, randomSource, config, std::move(patterns));
    sequencer.SetVariationLogCallback([](const std::string& message) { std::cout << message << "\n"; });

    if (renderingToWav) {
        std::cout << "Rendering " << configPath << " -> " << renderWavPath << " (" << renderSeconds << "s)...\n";
        if (!RenderCompositionToWav(audioEngine, sequencer, renderWavPath, renderSeconds, config.sampleRate,
                                     audioConfig.channels)) {
            return 1;
        }
        std::cout << "Wrote " << renderWavPath << "\n";
        return 0;
    }

    audioEngine.Start();
    std::cout << "Playing " << configPath << ". Press Ctrl+C to stop.\n";

    while (true) {
        sequencer.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
