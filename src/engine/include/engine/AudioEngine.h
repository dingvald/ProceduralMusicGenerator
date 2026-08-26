#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "engine/DelayProcessor.h"
#include "engine/LoFiProcessor.h"
#include "engine/Mixer.h"
#include "engine/ParameterBus.h"

// Forward-declared so consumers of this header don't need to pull in the
// (large) miniaudio.h; the complete types are only needed in AudioEngine.cpp.
struct ma_device;
struct ma_context;

namespace pmg {

struct AudioEngineConfig {
    uint32_t sampleRate = 48000;
    // Device output channel count. channels == 1 downmixes stereo to mono;
    // channels >= 2 writes real left/right into slots 0/1 (per-instrument
    // pan, see InstrumentConfig::pan) and duplicates the right channel into
    // any slot beyond 2 (untested beyond stereo -- no demo asset uses it).
    uint32_t channels = 2;
    bool useNullBackend = false; // headless smoke-test path (see README) - no real audio hardware required
    LoFiConfig loFi;             // post-mix bit-depth/sample-hold quantization; defaults to a no-op
    DelayConfig delay;           // post-mix echo, applied before loFi; defaults to a no-op
};

// The only class that touches raw miniaudio device APIs. Owns the Mixer and
// ParameterBus (both audio-thread-exclusive) and the atomic frame counter
// Sequencer uses to derive elapsed beats without wall-clock drift.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool Initialize(const AudioEngineConfig& config);

    // Configures the Mixer/LoFiProcessor exactly like Initialize(), but never
    // touches ma_context/ma_device -- no audio hardware (real or null-backend)
    // is opened. For driving RenderFrames() manually from an offline caller
    // (e.g. a WAV-rendering tool) that supplies its own frame counting rather
    // than a real-time device callback. Start()/Stop() are meaningless after
    // this and are no-ops since m_device stays null.
    bool InitializeOffline(const AudioEngineConfig& config);

    void Start();
    void Stop();
    void Shutdown();

    Mixer& GetMixer() { return m_mixer; }
    ParameterBus& GetParameterBus() { return m_parameterBus; }

    uint32_t GetSampleRate() const { return m_config.sampleRate; }
    uint64_t GetFramesProcessed() const { return m_framesProcessed.load(std::memory_order_relaxed); }

    // Drains ParameterBus and renders frameCount frames (interleaved,
    // m_config.channels per frame) into output, advancing GetFramesProcessed()
    // by frameCount. Normally invoked only via DataCallback from the
    // real-time device thread; exposed publicly so an offline caller using
    // InitializeOffline() can drive rendering directly from its own loop.
    void RenderFrames(float* output, uint32_t frameCount);

private:
    static void DataCallback(ma_device* device, void* output, const void* input, uint32_t frameCount);

    AudioEngineConfig m_config;
    Mixer m_mixer;
    // One independent instance per channel so each channel's echo/lofi state
    // stays separate and the stereo image from Mixer::RenderNextStereoSample
    // survives the post-mix effects chain instead of collapsing to a mono
    // pre-effects sum.
    DelayProcessor m_delayProcessorL;
    DelayProcessor m_delayProcessorR;
    LoFiProcessor m_loFiProcessorL;
    LoFiProcessor m_loFiProcessorR;
    ParameterBus m_parameterBus;
    std::atomic<uint64_t> m_framesProcessed{0};
    std::unique_ptr<ma_context> m_context;
    std::unique_ptr<ma_device> m_device;
    bool m_initialized = false;
};

} // namespace pmg
