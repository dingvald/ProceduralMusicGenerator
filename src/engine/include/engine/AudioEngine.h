#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include "engine/Mixer.h"
#include "engine/ParameterBus.h"

// Forward-declared so consumers of this header don't need to pull in the
// (large) miniaudio.h; the complete types are only needed in AudioEngine.cpp.
struct ma_device;
struct ma_context;

namespace pmg {

struct AudioEngineConfig {
    uint32_t sampleRate = 48000;
    uint32_t channels = 2;       // device output channel count; the mono mixer sum is duplicated across channels
    bool useNullBackend = false; // headless smoke-test path (see README) - no real audio hardware required
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
    void Start();
    void Stop();
    void Shutdown();

    Mixer& GetMixer() { return m_mixer; }
    ParameterBus& GetParameterBus() { return m_parameterBus; }

    uint32_t GetSampleRate() const { return m_config.sampleRate; }
    uint64_t GetFramesProcessed() const { return m_framesProcessed.load(std::memory_order_relaxed); }

private:
    static void DataCallback(ma_device* device, void* output, const void* input, uint32_t frameCount);
    void RenderFrames(float* output, uint32_t frameCount);

    AudioEngineConfig m_config;
    Mixer m_mixer;
    ParameterBus m_parameterBus;
    std::atomic<uint64_t> m_framesProcessed{0};
    std::unique_ptr<ma_context> m_context;
    std::unique_ptr<ma_device> m_device;
    bool m_initialized = false;
};

} // namespace pmg
